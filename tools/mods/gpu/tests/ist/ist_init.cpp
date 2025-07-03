/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/mgrmgr.h"                        // For DevMgrMgr
#include "core/include/platform.h"                      // For platform abstraction
#include "gpu/include/gpumgr.h"                         // For GpuDevMgr
#include "gpu/include/gpudev.h"                         // For GpuDevice
#include "gpu/include/gpusbdev.h"                       // For GpuSubdevice
#include "gpu/include/testdevicemgr.h"                  // For TestDeviceMgr
#include "gpu/utility/lwlink/lwlinkdevif/lwl_topology_mgr.h"   // For TopologyManager
#include "gpu/utility/pcie/pexdev.h"                         // For PexDevMgr

#include "gpu/js_gpu.h"                 // For Gpu_Object and other JS symbols
#include "ist_utils.h"                  // For PRE_AMPERE_TESTING. Remove after GA100 Power On
#include "ist_init.h"

namespace Ist
{
namespace Init
{

// Function pointer to GfwEnterIstModeComplete
RC (*PollGFWIstBootComplete)
(
    GpuDevice* pGpuDevice,
    bool isPreIstScriptUsed,
    UINT64 timeoutMs
);


/*
 *  Polling on IST boot completion is quite differnt in Hopper and forward
 *  Hence a reimplementation of the functions
 *  TODO: Clean things up and move funtion into proper gpusubdevice class
 */
RC PollGFWGH100IstBootComplete
(
    GpuDevice* pGpuDevice,
    bool isPreIstScriptUsed,
    UINT64 timeoutMs
)
{

    auto& regs = pGpuDevice->GetSubdevice(0)->Regs();
    const UINT64 endTimeMs = Xp::GetWallTimeMS() + timeoutMs;

    if (isPreIstScriptUsed)
    {
        while (!regs.Test32(MODS_THERM_I2CS_SCRATCH_FSP_BOOT_COMPLETE_STATUS_SUCCESS))
        {
            if (Xp::GetWallTimeMS() > endTimeMs)
            {
                Printf(Tee::PriError,
                       "PreISTScript timeout (>%llums) while trying to enter IST mode.\n",
                       timeoutMs);
                return RC::TIMEOUT_ERROR;
            }
            // Polling for MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED
            // fails in TinyLinux. So I set the sleep to 10ms for more margin
            Tasker::Sleep(10);   // 10 ms
        }
    }
    else 
    {
        Printf(Tee::PriError, "No VBIOS support for GH100 IST mode.\n");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC PollGFWGA10xIstBootComplete
(
    GpuDevice* pGpuDevice,
    bool isPreIstScriptUsed,
    UINT64 timeoutMs
)
{
    auto& regs = pGpuDevice->GetSubdevice(0)->Regs();
    const UINT64 endTimeMs = Xp::GetWallTimeMS() + timeoutMs;

    // There lwrrently 2 paths, whether the pre IST script or VBIOS initiates
    // IST devinit. In both cases we poll to ensure the VBIOS is done exelwting
    if (isPreIstScriptUsed)
    {
        while (!regs.Test32(MODS_PGSP_FALCON_CPUCTL_HALTED_TRUE))
        {
            if (Xp::GetWallTimeMS() > endTimeMs)
            {
                Printf(Tee::PriError,
                       "PreISTScript timeout (>%llums) while trying to enter IST mode.\n",
                       timeoutMs);
                return RC::TIMEOUT_ERROR;
            }
            // Polling for MODS_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT_PROGRESS_COMPLETED
            // fails in TinyLinux. So I set the sleep to 10ms for more margin
            Tasker::Sleep(10);   // 10 ms
        }
    }
    else 
    {
        // Need two conditions to jump out the loop and continue
        // 1. IST VBIOS Devinit complete. 2. Falcon is halted
        while (!regs.Test32(MODS_PBUS_IST_FIRMWARE_PROGRESS_STATE_COMPLETE) ||
                !regs.Test32(MODS_PGSP_FALCON_CPUCTL_HALTED_TRUE))
        {
            if (regs.Test32(MODS_PBUS_IST_FIRMWARE_PROGRESS_STATE_NOT_SUPPORTED))
            {
                Printf(Tee::PriError,
                       "This VBIOS does not support IST mode. Please reflash your VBIOS.\n");
                return RC::WRONG_BIOS;
            }

            if (regs.Test32(MODS_PBUS_IST_FIRMWARE_PROGRESS_STATE_ABORTED))
            {
                Printf(Tee::PriError, "VBIOS failed to enter IST mode.\n");
                return RC::HW_STATUS_ERROR;
            }

            if (regs.Test32(MODS_PBUS_IST_FIRMWARE_PROGRESS_STATE_TEMP_ABORTED))
            {
                Printf(Tee::PriError, "Temperature exceeds limit. VBIOS fails to execute.\n");
                return RC::HW_STATUS_ERROR;
            }
            
            if (Xp::GetWallTimeMS() > endTimeMs)
            {
                Printf(Tee::PriError,
                       "VBIOS timeout (>%llums) while trying to enter IST mode.\n",
                       timeoutMs);
                return RC::TIMEOUT_ERROR;
            }
            Tasker::Sleep(1);   // 1 ms
        }
        regs.Write32Sync(MODS_PBUS_IST_FIRMWARE_PROGRESS, 0x00);
    }

    return RC::OK;
}

//--------------------------------------------------------------------
RC InitGpu
(
    GpuDevice** ppGpuDevice,
    const InitGpuArgs& initGpuArgs,
    const std::string& fsArgs
)
{
    Printf(Tee::PriDebug, "In InitGpu()\n");

    if (!Platform::IsInitialized())
    {
        Printf(Tee::PriError,
               "Cannot initialize Gpu until Platform is initialized\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    // TODO: early exit if we already have our subdevices populated and they
    // are accessible

    if (!DevMgrMgr::d_PexDevMgr)
    {
        DevMgrMgr::d_PexDevMgr = new PexDevMgr();
    }
    PexDevMgr* const pPexDevMgr = static_cast<PexDevMgr*>(DevMgrMgr::d_PexDevMgr);

    if (!DevMgrMgr::d_GraphDevMgr)
    {
        DevMgrMgr::d_GraphDevMgr = new GpuDevMgr();
    }
    GpuDevMgr* const pGpuDeviceMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);

    if (!DevMgrMgr::d_TestDeviceMgr)
    {
        DevMgrMgr::d_TestDeviceMgr = new TestDeviceMgr();
    }
    TestDeviceMgr* const pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);

    // Find all GPU chips: there should be only one we can see
    RC rc;
    CHECK_RC(pTestDeviceMgr->FindDevices());
    SW_ERROR((pGpuDeviceMgr->NumGpus() == 0),
             "Behaviour change in FindDevices(). "
             "It should have failed if no usable gpus are found.\n");

    // Check that there is only one GPU in the system. If the user has
    // selected one, it will also show up as if there were only one
    if (pGpuDeviceMgr->NumGpus() > 1)
    {
        Printf(Tee::PriError, "Only one GPU allowed in the system, "
                              "or use -only_pci_dev flag to select one\n");
        return RC::DEVICE_NOT_FOUND;
    }

    // Add GpuSubdevices to each GpuDevice
    CHECK_RC(pGpuDeviceMgr->PreCreateGpuDeviceObjects());

    // Pick the one and only GPU and sanity check it
    GpuDevice* const pGpuDevice = pGpuDeviceMgr->GetFirstGpuDevice();
    SW_ERROR((!pGpuDevice), "DeviceMgr has one GPU but returned a nullptr???");

    if (pGpuDevice->GetNumSubdevices() != 1)
    {
        Printf(Tee::PriError, "GpuDevice has multiple subdevices. Only one is allowed\n");
        return RC::DEVICE_NOT_FOUND;
    }
    GpuSubdevice* const pSubdevice = pGpuDevice->GetSubdevice(0);
    SW_ERROR((!pSubdevice), "GpuDevice has one subdevice but returned a nullptr???");

    CHECK_RC(pPexDevMgr->FindDevices());

    if (initGpuArgs.isFbBroken)
    {   
        GpuSubdevice::NamedProperty *pProp;
        CHECK_RC(pSubdevice->GetNamedProperty("IsFbBroken", &pProp));
        pProp->Set(true);
    }

    // The floorsweep will only work on unfused parts where there is no priv protection
    if (fsArgs.length())
    {
        GpuSubdevice::NamedProperty *pFsProp; 
        CHECK_RC(pSubdevice->GetNamedProperty("FermiFsArgs", &pFsProp));
        pFsProp->Set(fsArgs);
        CHECK_RC(pSubdevice->SetGFWBootHoldoff(true));
        // Write bit 30 of LW_PGC6_SCI_LW_SCRATCH for another time
        // Because it has been cleared by VBIOS last time when Dev init completes
        RegHal& regHal = pGpuDevice->GetSubdevice(0)->Regs();
        regHal.Write32Sync(MODS_PGC6_IST_DEVINIT_CONFIG_ENABLE_TRUE);
        CHECK_RC(pSubdevice->GetFsImpl()->SwReset(false));
        // use old sequence if devinit didn't run, for pre-slicon non-miata flow
        if (!pSubdevice->IsGFWBootStarted())
        {
            pSubdevice->SetGFWBootHoldoff(false);
        }
        CHECK_RC(pSubdevice->PreVBIOSSetup());
        // Release holdoff, letting the vbios ucode run devinit
        CHECK_RC(pSubdevice->SetGFWBootHoldoff(false));
    }

    CHECK_RC(pGpuDevice->InitializeWithoutRm());
    // RC GpuSubdevice::UnhookISR() // we might need this if interrupts are enabled by default
    switch(pGpuDevice->GetFamily())
    {
#if LWCFG(GLOBAL_GPU_IMPL_GA100) || \
    LWCFG(GLOBAL_GPU_IMPL_GA102) || \
    LWCFG(GLOBAL_GPU_IMPL_GA104) || \
    LWCFG(GLOBAL_GPU_IMPL_GA106) || \
    LWCFG(GLOBAL_GPU_IMPL_GA107)
        case GpuDevice::Ampere:
            PollGFWIstBootComplete = &PollGFWGA10xIstBootComplete;
            break;
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
        case GpuDevice::Hopper:
            PollGFWIstBootComplete = &PollGFWGH100IstBootComplete;
            break;
#endif  
        default:
            Printf(Tee::PriError, "Unsupported GPU family device\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    // Make sure VBIOS is done
    if (initGpuArgs.isIstMode)
    {
        CHECK_RC(PollGFWIstBootComplete(pGpuDevice, initGpuArgs.isPreIstScriptUsed,
                    initGpuArgs.vbiosTimeoutMs));
    }
    else
    {
        CHECK_RC(pSubdevice->PollGFWBootComplete(Tee::PriNormal));
    }

    if (ppGpuDevice)
    {
        *ppGpuDevice = pGpuDevice;
    }
    return rc;
}

} // namespace Init
} // namespace ist
