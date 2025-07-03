/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "repair_module_mem.h"

#include "core/include/abort.h"
#include "core/include/framebuf.h"
#include "core/include/gpu.h"
#include "core/include/mgrmgr.h"
#include "core/include/tee.h"
#include "gpu/include/gpudev.h"
#include "gpu/js_gpu.h" // Gpu_Object, Gpu_Reset, ...
#include "gpu/include/hbmimpl.h"

#include "repair_module_hbm.h"
#include "repair_module_gddr.h"

using namespace Memory;

using CmdGroup = Repair::Command::Group;

//!
//! \def REPAIR_CHECK(f)
//! \brief Check the RC of the given expression, records the RC in \a repairRc,
//! and continues if an error oclwred.
//!
//! Intended to be used with \a repairRc being a sticky RC. If a single repair
//! fails, it doesn't mean others should not be attempted.
//!
//! NOTE: CreateGpuTestList checks Log.FirstError for a failure, and throws if
//! there is one. We want to continue running despite a failure on one GPU. The
//! first recorded failure should be in 'repairRc'.
//!
#define REPAIR_CHECK(f)                         \
    {                                           \
        rc = (f);                               \
        repairRc = rc;                          \
        if (rc != RC::OK)                       \
        {                                       \
            rc.Clear();                         \
            Log::ResetFirstError();             \
            continue; /* move to next GPU */    \
        }                                       \
        rc.Clear();                             \
    }

/******************************************************************************/
RC RepairModuleMem::ValidateSettings
(
    const Repair::Settings& settings,
    const RepairModule::CmdBufProperties& prop
) const
{
    RC rc;

    if (settings.modifiers.pseudoHardRepair)
    {
        if (!prop.isAttemptingHardRepair)
        {
            Printf(Tee::PriError, "-pseudo_hard_repair arg can only be used when"
                   " performing hard repairs.\n");
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }

        Printf(Tee::PriWarn, "Pseudo hard repair mode active. Hard repairs will"
               " have no effect.\n");
    }

    if (settings.modifiers.skipHbmFuseRepairCheck)
    {
        // TODO(stewarts): Determine exactly what will happen if an already
        // repaired fuse is repaired again. The new repair will not be burned to
        // the actual HBM fuses. Does it remap like SLR? Does it take effect
        // until a GPU reset? Will the original repair value be visible during
        // this run, or after a gpu reset?
        Printf(Tee::PriWarn,
               "Skipping HBM fuse repair check. Any repairs done to an "
               "already repaired fuse will not persist. Undefined "
               "repair behaviour may occur.\n");
    }

    return rc;
}

RC RepairModuleMem::GetExpectedInitialGpuConfiguration
(
    const Repair::Settings& settings,
    const CmdBuf& cmdBuf,
    InitializationConfig* pInitConfig
) const
{
    MASSERT(pInitConfig);
    InitializationConfig initConfig = {};

    if (cmdBuf.HasCommand(CmdT::ReconfigGpuLanes))
    {
        initConfig.hbmDeviceInitMethod = Gpu::DoNotInitHbm;
    }
    else
    {
        // Initialize the HBM
        if (settings.modifiers.forceHtoJ)
        {
            initConfig.hbmDeviceInitMethod = Gpu::InitHbmHostToJtag;
        }
        else
        {
            initConfig.hbmDeviceInitMethod = Gpu::InitHbmFbPrivToIeee;
        }
    }

    if (cmdBuf.HasCommand(CmdT::MemRepairSeq))
    {
        initConfig.initializeGpuTests = true;
        initConfig.skipRmStateInit = false;
    }
    else
    {
        initConfig.initializeGpuTests = false;
        initConfig.skipRmStateInit = true;
    }

    *pInitConfig = initConfig;

    return RC::OK;
}

RC RepairModuleMem::CheckedGetGpuRawEcid(GpuSubdevice* pSubdev, RawEcid* pGpuRawEcid) const
{
    MASSERT(pSubdev);
    MASSERT(pGpuRawEcid);

    RC rc = pSubdev->GetRawEcid(pGpuRawEcid);
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "No raw ECID available for GPU, but is "
               "required for repairs\n.");
    }

    return rc;
}

RC RepairModuleMem::FindGpuSubdeviceFromRawEcid
(
    GpuDevMgr* pGpuDevMgr,
    const RawEcid& desiredGpuRawEcid,
    GpuSubdevice** ppSubdev
) const
{
    MASSERT(pGpuDevMgr);
    MASSERT(ppSubdev);
    RC rc;
    *ppSubdev = nullptr;

    for (GpuSubdevice* pSubdev = pGpuDevMgr->GetFirstGpu();
         pSubdev != nullptr;
         pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
    {
        RawEcid rawEcid;
        CHECK_RC(CheckedGetGpuRawEcid(pSubdev, &rawEcid));

        if (desiredGpuRawEcid.size() != rawEcid.size())
        {
            continue;
        }

        bool mismatch = false;
        for (UINT32 i = 0; i < desiredGpuRawEcid.size(); ++i)
        {
            if (desiredGpuRawEcid[i] != rawEcid[i])
            {
                mismatch = true;
                break;
            }
        }

        if (!mismatch)
        {
            // Found our subdevice, break
            *ppSubdev = pSubdev;
            break;
        }
    }

    return rc;
}

void RepairModuleMem::PrintGpuRepairHeader(const SystemState& sysState) const
{
    MASSERT(sysState.IsLwrrentGpuSet());

    Printf(Tee::PriNormal, "==== Starting raw ECID %s\n",
           Utility::FormatRawEcid(sysState.lwrrentGpu.rawEcid).c_str());
}

RC RepairModuleMem::DoGpuReset
(
    SystemState* pSysState,
    Gpu::ResetMode resetMode,
    const InitializationConfig& initConfig,
    bool printSystemInfo
) const
{
    RC rc;

    Gpu* pGpu = GpuPtr().Instance();

    // Shutdown GPU
    if (pSysState->init.isInitialized)
    {
        CHECK_RC(pGpu->ShutDown(Gpu::ShutDownMode::Normal));
    }

    // Print available system info after initializing Platform but before GPU
    if (printSystemInfo)
    {
        // TODO(stewarts): impl. See: JS g_InGpuInit = true;
        // Common_PrintSystemInfoWrapper
    }

    JavaScriptPtr pJs;
    CHECK_RC(pJs->SetProperty(Gpu_Object, Gpu_Reset,
                              static_cast<UINT32>(resetMode)));
    CHECK_RC(pJs->SetProperty(Gpu_Object, Gpu_HbmDeviceInitMethod,
                              static_cast<UINT32>(initConfig.hbmDeviceInitMethod)));
    CHECK_RC(pJs->SetProperty(Gpu_Object, Gpu_SkipRmStateInit,
                              static_cast<UINT32>(initConfig.skipRmStateInit)));

    CHECK_RC(pGpu->Initialize());

    pSysState->init.isInitialized = true;
    pSysState->init.config = initConfig;
    pSysState->init.resetMode = resetMode;

    // Set the GPU manager
    //
    pSysState->pGpuDevMgr = dynamic_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
    MASSERT(pSysState->pGpuDevMgr);

    // Update the subdevice
    //

    // If the current GPU is not set, don't update the subdevice
    if (!pSysState->IsLwrrentGpuSet())
    {
        Printf(Tee::PriLow,
               "DoGpuReset: System state current GPU is not set, will not "
               "update the system state current subdevice\n");
        return rc;
    }

    // Find the old subdevice
    //
    // NOTE: Since we shutdown the GPU, the GpuDevMgr has been destroyed, and
    // with it all original devices. The original device IDs may not correspond
    // to the same devices after re-initialization. We need to find our GPU
    // again by searching for its raw ECID amung the newly initialized GPUs.
    CHECK_RC(FindGpuSubdeviceFromRawEcid(pSysState->pGpuDevMgr,
                                     pSysState->lwrrentGpu.rawEcid,
                                     &pSysState->lwrrentGpu.pSubdev));

    // Ensure we found our subdevice
    if (pSysState->lwrrentGpu.pSubdev == nullptr)
    {
        Printf(Tee::PriError, "Could not find GPU post reset: raw ECID '%s'\n",
               Utility::FormatRawEcid(pSysState->lwrrentGpu.rawEcid).c_str());
        MASSERT(!"Panic! Could not find GPU post reset!");
        return RC::HW_ERROR;
    }

    // Check that the HBM is ready
    if ((pSysState->lwrrentGpu.pSubdev->GetNumHbmSites() > 0) &&
        (!pSysState->lwrrentGpu.pSubdev->GetHBMImpl() ||
         !pSysState->lwrrentGpu.pSubdev->GetHBMImpl()->GetHBMDev()))
    {
        Printf(Tee::PriError, "HBM device was not initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    return rc;
}

RC RepairModuleMem::PrepareSystemForNextGpuRepair
(
    SystemState* pSysState,
    const InitializationConfig& gpuConfigAtRepairStart
) const
{
    MASSERT(pSysState);
    RC rc;

    // Reset GPU state
    //
    pSysState->lwrrentGpu = {};
    MASSERT(!pSysState->IsLwrrentGpuSet());

    GpuDevMgr*& pGpuDevMgr = pSysState->pGpuDevMgr;
    MASSERT(pGpuDevMgr);

    // Setup the GPU to be in the expected start state
    //
    if (pSysState->init.config != gpuConfigAtRepairStart)
    {
        CHECK_RC(DoGpuReset(pSysState, Gpu::ResetMode::NoReset,
                            gpuConfigAtRepairStart, false));

        // Sanity check: same numbers of GPUs after reset
        const UINT32 lwrNumGpuSubdevs = pGpuDevMgr->NumGpus();
        if (pSysState->numGpuSubdevs != lwrNumGpuSubdevs)
        {
            Printf(Tee::PriError,
                   "Panic! Originally had %u GPU subdevices, now have %u\n",
                   pSysState->numGpuSubdevs, lwrNumGpuSubdevs);
            return RC::SOFTWARE_ERROR;
        }
    }

    // Find the next GPU not yet seen
    //
    RawEcid gpuRawEcid;
    for (auto pSubdev : *pGpuDevMgr)
    {
        CHECK_RC(CheckedGetGpuRawEcid(pSubdev, &gpuRawEcid));
        if (pSysState->processedRawEcids.find(gpuRawEcid)
            == pSysState->processedRawEcids.end())
        {
            // Found new GPU
            pSysState->lwrrentGpu.rawEcid = std::move(gpuRawEcid);
            pSysState->lwrrentGpu.pSubdev = pSubdev;
            break;
        }
    }

    return rc;
}

RC RepairModuleMem::DoInitialGpuInit
(
    const InitializationConfig &gpuConfigAtRepairStart,
    SystemState *pSysState
) const
{
    MASSERT(pSysState);
    RC rc;

    // Initialize the GPU for run information
    //
    // NOTE: We only care about getting the system information print out and the
    // number of subdevices on the machine. This requires InitializeGpuTests and
    // RM. We don't care about the init method. Use whatever the repair expects
    // the GPU to have for those values so we have the chance of not needing to
    // reboot the GPU for the first subdevice repair.
    //
    // NOTE: Other than gathering and printing all the GPU information, we don't
    // need to do the full MODS initialization.

    // NOTE: Reset type should be consistent with whatever reset type the user
    // set via the command line. For example, -hot_reset and -disable_ecc set
    // Gpu.Reset = Gpu.HotReset, but don't actually perform the reset. By
    // overriding that reset value we would be ignoring the arguments.
    Gpu::ResetMode initialResetMode;
    {
        JavaScriptPtr pJs;
        UINT32 jsGpuResetMode = 0;
        CHECK_RC(pJs->GetProperty(Gpu_Object, Gpu_Reset, &jsGpuResetMode));
        initialResetMode = static_cast<Gpu::ResetMode>(jsGpuResetMode);
    }

    Printf(Tee::PriLow, "Initial GPU reset type: %s\n",
           Gpu::ToString(initialResetMode).c_str());
    InitializationConfig initialGpuResetConfig = gpuConfigAtRepairStart;

    // IMPORTANT: Must initialize RM on the first reset to apply the repair HULK
    // license.
    initialGpuResetConfig.skipRmStateInit = false;
    CHECK_RC(DoGpuReset(pSysState, initialResetMode, initialGpuResetConfig,
                        true));

    // Store HBM / GDDR model details
    // This is important for GDDR, because we need RM to fetch GDDR model information
    // which might not be initialized while doing the repairs
    RawEcid gpuRawEcid;
    GpuDevMgr*& pGpuDevMgr = pSysState->pGpuDevMgr;
    for (auto pSubdev : *pGpuDevMgr)
    {
        CHECK_RC(CheckedGetGpuRawEcid(pSubdev, &gpuRawEcid));
        if (pSubdev->GetNumHbmSites() > 0)
        {
            HBMImpl* pHbmImpl = pSubdev->GetHBMImpl();
            MASSERT(pHbmImpl);
            HBMDevicePtr pHbmDev = pHbmImpl->GetHBMDev();
            if (pHbmDev == nullptr)
            {
                Printf(Tee::PriError, "HBM device not initialized\n");
                return RC::SOFTWARE_ERROR;
            }

            Hbm::Model hbmModel;
            CHECK_RC_MSG(pHbmDev->GetHbmModel(&hbmModel), "Unable to determine the HBM model\n");
            pSysState->hbmModels[gpuRawEcid] = hbmModel;
        }
        else
        {
            FrameBuffer *pFb = pSubdev->GetFB();
            if (pFb == nullptr)
            {
                Printf(Tee::PriError, "Framebuffer not initialized\n");
                return RC::SOFTWARE_ERROR;
            }

            FrameBuffer::RamVendorId vendorId    = pFb->GetVendorId();
            FrameBuffer::RamProtocol ramProtocol = pFb->GetRamProtocol();
            string vendorName                    = pFb->GetVendorName();
            string protocolName                  = pFb->GetRamProtocolString();
            Gddr::Model gddrModel(vendorId, ramProtocol, vendorName, protocolName);
            pSysState->gddrModels[gpuRawEcid] = gddrModel;
        }
    }

    return rc;
}

RC RepairModuleMem::ExelwteCommands
(
    const Repair::Settings& settings,
    const CmdBuf& cmdBuf,
    const RepairModule::CmdBufProperties& prop
) const
{
    RC rc;

    if (cmdBuf.Empty())
    {
        Printf(Tee::PriError, "No mem repair commands to execute\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    SystemState sysState = {};
    InitializationConfig gpuConfigAtRepairStart = {};
    CHECK_RC(GetExpectedInitialGpuConfiguration(settings, cmdBuf,
                                                &gpuConfigAtRepairStart));

    CHECK_RC(DoInitialGpuInit(gpuConfigAtRepairStart, &sysState));

    // Perform explicit repair validation
    GpuDevMgr*& pGpuDevMgr = sysState.pGpuDevMgr;
    if (pGpuDevMgr->NumGpus() > 1 && prop.hasExplicitRepairCommand)
    {
        Printf(Tee::PriError, "Explicit repairs are not supported when running "
               "MODS with multiple GPUs.\n");
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    // TODO(stewarts): Some commands don't actually need the gpu, for example
    // -lane_remapping_info. This should be considered using the
    // Command::Flags::NoGpuRequired flag.

    // Repair each GPU
    //
    // We have to jump through some hoops here to iterate over every GPU since
    // we reset RM and the GPU mid iteration. We can't assume that the
    // subdevices we are iterating over will still be valid once we have
    // reset. To account for this, we track the GPU raw ECIDs we have seen and only
    // process raw ECIDs we have not encountered before. We are done when we have
    // iterated over all the subdevices without seeing a new raw ECID.
    //
    sysState.numGpuSubdevs = pGpuDevMgr->NumGpus();
    bool isDone = false;
    StickyRC repairRc;

    while (!isDone)
    {
        CHECK_RC(Abort::Check());

        // If preparing the system fails, we are not able to get the system
        // setup for repair. Terminate.
        CHECK_RC(PrepareSystemForNextGpuRepair(&sysState, gpuConfigAtRepairStart));

        // Check for run completion
        //
        // NOTE: Need to check whether the current GPU is set instead of if
        // we've processed the number of GPUs seen when the repair started. It
        // is possible that we may not be seeing the same GPUs or the same
        // number of GPUs across resets. Unlikely, but possible.
        if (!sysState.IsLwrrentGpuSet())
        {
            // Found no new GPUs, done.
            isDone = true;

            // Sanity check
            if (sysState.processedRawEcids.size() != sysState.numGpuSubdevs)
            {
                Printf(Tee::PriError,
                       "Panic! Number of GPU subdevices processed (%u) does "
                       "not match the original number of GPU subdevices (%u)\n",
                       static_cast<UINT32>(sysState.processedRawEcids.size()),
                       sysState.numGpuSubdevs);
                return RC::HW_ERROR;
            }

            break;
        }

        // Process the GPU
        //
        PrintGpuRepairHeader(sysState);
        sysState.processedRawEcids.insert(sysState.lwrrentGpu.rawEcid); // Record GPU

        std::unique_ptr<RepairModuleMemImpl> pMemImpl;
        if (sysState.lwrrentGpu.pSubdev->GetNumHbmSites() > 0)
        {
            pMemImpl = std::make_unique<RepairModuleHbm>();
        }
        else
        {
            pMemImpl = std::make_unique<RepairModuleGddr>();
        }
        REPAIR_CHECK(pMemImpl->ExelwteCommandsOnLwrrentGpu(settings, &sysState, cmdBuf));
    }

    return repairRc;
}
