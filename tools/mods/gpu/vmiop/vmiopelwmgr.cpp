/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/vmiop/vmiopelwmgr.h"

#include "core/include/fileholder.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/include/xp.h"
#include "core/utility/sharedsysmem.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/vmiop/vmiopelw.h"
#include "hopper/gh100/dev_xtl_ep_pcfg_gpu.h"
#include "lwRmReg.h"
#include "vgpu/mods/vmiop-external-mods.h"
#include <set>

#define SRIOV_VGPUID_GPUINS  31:16
#define SRIOV_VGPUID_GFID    15:0

namespace
{
    const UINT32 UNINITIALIZED_SRIOVGFID = _UINT32_MAX;

    RC VmiopToRc(vmiop_error_t vmiopError)
    {
        switch (vmiopError)
        {
            case vmiop_success:
                return OK;
            case vmiop_error_ilwal:
            case vmiop_error_not_found:
                return RC::BAD_PARAMETER;
            case vmiop_error_resource:
                return RC::RESOURCE_IN_USE;
            case vmiop_error_range:
                return RC::ILWALID_ADDRESS;
            case vmiop_error_read_only:
                return RC::MEM_ACCESS_READ_PROTECTION_VIOLATION;
            case vmiop_error_no_address_space:
                return RC::CANNOT_ALLOCATE_MEMORY;
            case vmiop_error_timeout:
                return RC::TIMEOUT_ERROR;
            case vmiop_error_not_allowed_in_callback:
                return RC::SOFTWARE_ERROR;
            default:
                MASSERT(!"Unknown error code");
                return RC::SOFTWARE_ERROR;
        };
    }
};

/* static */ unique_ptr<VmiopElwManager> VmiopElwManager::s_pInstance;

#ifndef INCLUDE_MDIAG
/* static */ VmiopElwManager* VmiopElwManager::Instance()
{
    if (s_pInstance.get() == nullptr)
    {
        s_pInstance = make_unique<VmiopElwManager>(ConstructorTag());
    }
    return s_pInstance.get();
}
#endif // INCLUDE_MDIAG

/* virtual */ RC VmiopElwManager::Init()
{
    RC rc;

    if (IsInitialized())
    {
        return OK;
    }

    m_SessionId = (Platform::IsVirtFunMode() ?
                   SharedSysmem::GetParentModsPid() : Xp::GetPid());

    if (Platform::IsPhysFunMode())
    {
        m_GfidAttachedToProcess = 0;
        // disable pma
        CHECK_RC(Registry::Write("ResourceManager",
                                 LW_REG_STR_RM_ENABLE_PMA,
                                 LW_REG_STR_RM_ENABLE_PMA_NO));
        // enable sriov
        CHECK_RC(Registry::Write("ResourceManager",
                                 LW_REG_STR_RM_SET_SRIOV_MODE,
                                 LW_REG_STR_RM_SET_SRIOV_MODE_ENABLED));

        CHECK_RC(ParseVfConfigFile());
    }
    else
    {
        m_GfidAttachedToProcess = std::atoi(Xp::GetElw("SRIOV_GFID").c_str());
        CHECK_RC(SetupVmiopElw(GetGfidAttachedToProcess(), 1 /* seqId */));
        CHECK_RC(SharedSysmem::PidBox::SendPid(m_SessionId));
    }

    m_IsInitialized = true;
    return OK;
}

/* virtual */ void VmiopElwManager::ShutDown()
{
    if (s_pInstance)
    {
        for (auto& it : m_GfidToVmiopElw)
        {
            VmiopElw* pElw = it.second.get();
            pElw->Shutdown();
        }
    }
    ShutDownMinimum();
}

//--------------------------------------------------------------------
//! \brief kill all VFs process when PF crashes.
//!
//! bug200466961 bug2011455
//! There exist some cases that VF hangs on waiting PF response
//! when PF crashes/exits due to whatever reason. It may happen
//! when VF tries to allocate physical memory during crc check, which
//! is not covered by current remote process running check. This
//! function is designed to do some minimum cleanup when PF crashes
//! to avoid hang issue.
/* virtual */ void VmiopElwManager::ShutDownMinimum()
{
    if (s_pInstance)
    {
        for (auto& it : m_GfidToVmiopElw)
        {
            VmiopElw* pElw = it.second.get();
            if (Platform::IsPhysFunMode())
            {
                // m_VmiopElw objects should normally be removed in PFHandler.
                // Otherwise, kill the VF mods process.
                if (pElw->IsRemoteProcessRunning())
                {
                    pElw->KillRemoteProcess();
                }
            }
        }
        m_IsInitialized = false;
        s_pInstance.reset();
    }
}

VmiopElw* VmiopElwManager::GetVmiopElw(UINT32 gfid) const
{
    auto iter = m_GfidToVmiopElw.find(gfid);
    if (iter == m_GfidToVmiopElw.end())
    {
        Printf(Tee::PriDebug,
               "SRIOV %s: VmiopElw for GFID 0x%x doesn't exist\n",
               __FUNCTION__, gfid);
        return nullptr;
    }
    return iter->second.get();
}

VmiopElw::PcieBarInfo VmiopElwManager::GetBarInfo(UINT32 gfid) const
{
    auto iter = m_GfidToBarInfo.find(gfid);
    if (iter == m_GfidToBarInfo.end())
    {
        MASSERT(!"Illegal gfid passed to VmiopElwManager::GetBarInfo");
        return VmiopElw::PcieBarInfo();
    }
    return iter->second;
}

RC VmiopElwManager::CreateGfidToBdfMapping
(
    UINT32 domain,
    UINT32 bus,
    UINT32 device,
    UINT32 function
)
{
    RC rc;

    // Get SRIOV header offset
    UINT16 capAddr = 0;
    UINT16 capSize = 0;
    if (Pci::GetExtendedCapInfo(domain, bus, device, function,
                                Pci::SRIOV, 0, &capAddr, &capSize) != RC::OK)
    {
        Printf(Tee::PriDebug,
               "SRIOV: Skip %04x:%02x:%02x:%02x because SRIOV is not supported\n",
               domain, bus, device, function);
        return RC::UNSUPPORTED_FUNCTION;
    }
    const INT32 capAdjust = capAddr - LW_EP_PCFG_GPU_SRIOV_HEADER;

    // Abort if VF is not enabled on this GPU
    UINT32 cfgHdr2 = 0;
    CHECK_RC(Platform::PciRead32( domain, bus, device, function,
                                  capAdjust
                                  + LW_EP_PCFG_GPU_SRIOV_CONTROL_AND_STATUS,
                                  &cfgHdr2));
    if (!FLD_TEST_DRF(_EP_PCFG_GPU, _SRIOV_CONTROL_AND_STATUS,
                      _VF_ENABLE, _TRUE, cfgHdr2))
    {
        Printf(Tee::PriDebug,
               "SRIOV: Skip %04x:%02x:%02x:%02x because VF is not enabled!\n",
               domain, bus, device, function);
        return RC::UNSUPPORTED_FUNCTION;
    }

    MASSERT(device == 0);  // PF should be at device 0 according to spec

    // Find physical Gpu Instance
    UINT32 gpuInstance = 0;
    set<UINT32> priorGpuInstances;
    for (const auto& elem: m_GfidToBdf)
    {
        priorGpuInstances.insert(REF_VAL(SRIOV_VGPUID_GPUINS, elem.first));
    }
    gpuInstance = static_cast<UINT32>(priorGpuInstances.size());

    VmiopElw::PcieBarInfo firstVfBarInfo;
    if (Platform::IsPhysFunMode())
    {
        for (UINT32 ibar = 0; ibar < VmiopElw::PcieBarInfo::NUM_BARS; ++ibar)
        {
            auto& bar = firstVfBarInfo.bar[ibar];
            CHECK_RC(Pci::GetBarInfo(domain, bus, device, function, ibar,
                                     &bar.base, &bar.size,
                                     capAdjust + LW_EP_PCFG_GPU_VF_BAR0));
        }
    }

    // Explore VFs
    UINT32 cfgHdr3 = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 capAdjust + LW_EP_PCFG_GPU_SRIOV_INIT_TOT_VF,
                                 &cfgHdr3));
    const UINT32 numVfs = DRF_VAL(_EP_PCFG_GPU, _SRIOV_INIT_TOT_VF,
                                  _TOTAL_VFS, cfgHdr3);

    UINT32 cfgHdr5 = 0;
    CHECK_RC(Platform::PciRead32(domain, bus, device, function,
                                 capAdjust
                                 + LW_EP_PCFG_GPU_SRIOV_FIRST_VF_STRIDE,
                                 &cfgHdr5));
    const UINT32 vfFirstOffset = DRF_VAL(_EP_PCFG_GPU, _SRIOV_FIRST_VF_STRIDE,
                                         _FIRST_VF_OFFSET, cfgHdr5);
    const UINT32 vfStride = DRF_VAL(_EP_PCFG_GPU, _SRIOV_FIRST_VF_STRIDE,
                                    _VF_STRIDE, cfgHdr5);

    for (UINT32 gfId = 0; gfId <= numVfs; ++gfId)
    {
        // Get the BDF address.  Colwert to configAddress and back to
        // overflow function to device to bus
        const UINT32 vfFunction =
            (gfId > 0) ? (vfFirstOffset + (gfId - 1) * vfStride) : 0;
        const UINT32 configAddress = Pci::GetConfigAddress(domain, bus, device,
                                                           vfFunction, 0);
        Bdf bdf = {};
        Pci::DecodeConfigAddress(configAddress, &bdf.domain, &bdf.bus,
                                 &bdf.device, &bdf.function, nullptr);

        // Store the BDF address
        UINT32 id = REF_NUM(SRIOV_VGPUID_GPUINS, gpuInstance) |
                    REF_NUM(SRIOV_VGPUID_GFID, gfId);
        m_GfidToBdf[id] = bdf;

        // Store the VF BAR addresses in the PF process
        if (Platform::IsPhysFunMode() && gfId > 0)
        {
            VmiopElw::PcieBarInfo barInfo = firstVfBarInfo;
            for (auto& bar: barInfo.bar)
            {
                bar.base += (gfId - 1) * bar.size;
            }
            m_GfidToBarInfo[id] = barInfo;
        }
    }

    return rc;
}

RC VmiopElwManager::GetDomainBusDevFun
(
    UINT32  gfid,
    UINT32* pDomain,
    UINT32* pBus,
    UINT32* pDevice,
    UINT32* pFunction
)
{
    if (m_GfidToBdf.empty())
    {
        MASSERT(Platform::IsPhysFunMode());

        GpuDevMgr* pDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        for (GpuSubdevice* pSubdev = pDevMgr->GetFirstGpu();
             pSubdev != NULL; pSubdev = pDevMgr->GetNextGpu(pSubdev))
        {
            const Pcie* pPcie = pSubdev->GetInterface<Pcie>();
            if (pPcie)
            {
                // Ignore bad RCs
                CreateGfidToBdfMapping(pPcie->DomainNumber(),
                                       pPcie->BusNumber(),
                                       pPcie->DeviceNumber(),
                                       pPcie->FunctionNumber());
            }
        }
    }

    const auto& pBdf = m_GfidToBdf.find(gfid);
    if (pBdf == m_GfidToBdf.end())
    {
        return RC::SOFTWARE_ERROR;
    }

    *pDomain   = pBdf->second.domain;
    *pBus      = pBdf->second.bus;
    *pDevice   = pBdf->second.device;
    *pFunction = pBdf->second.function;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Find a memory address within a LW_VGPU_EMU
//!
//! Given a physical memory address, determine whether it falls within
//! the LW_VGPU_EMU range within a VF's BAR0 aperture.  Register
//! accesses within this range should be forwarded to the PF instead
//! of being passed to BAR0.
//!
//! \param physAddr The physical address being checked
//! \param[out] ppVmiopElw On success, contains the VmiopElw object
//!     for the VF that contains physAddr
//! \param[out] pRegister On success, contains the register's BAR0 offset
//! \return OK = physAddr falls within a VF's LW_VGPU_EMU range.
//!     DEVICE_NOT_FOUND = physAddr does not fall within LW_VGPU_EMU
//!     for any VF.  ILWALID_REGISTER_NUMBER = physAddr looks like a
//!     PF register that was passed to a VF by mistake.
//!
RC VmiopElwManager::FindVfEmuRegister
(
    PHYSADDR   physAddr,
    VmiopElw** ppVmiopElw,
    UINT32*    pRegister
)
{
    MASSERT(ppVmiopElw != nullptr);
    MASSERT(pRegister != nullptr);
    RC rc;

    if (Platform::IsVirtFunMode())
    {
        for (const auto& iter: m_GfidToVmiopElw)
        {
            VmiopElw* pVmiopElw = iter.second.get();
            rc = pVmiopElw->FindVfEmuRegister(physAddr, pRegister);
            if (rc != RC::DEVICE_NOT_FOUND)
            {
                *ppVmiopElw = pVmiopElw;
                return rc;
            }
            rc.Clear();
        }
    }
    *ppVmiopElw = nullptr;
    *pRegister  = 0;
    return RC::DEVICE_NOT_FOUND;
}

/* static */ void VmiopElwManager::PFHandler(void* pArgs)
{
    PluginHandlerParam* pParam = static_cast<PluginHandlerParam*>(pArgs);
    pParam->testRc = VmiopElwManager::Instance()->PFHandlerImpl(pParam);
}

VmiopElwManager::PluginHandlerParam* VmiopElwManager::GetPluginHandlerParam
(
    UINT32 gfid
)
{
    for (auto& pluginHandlerParam: m_PluginHandlerParams)
    {
        if (pluginHandlerParam.vfConfig.gfid == gfid)
        {
            return &pluginHandlerParam;
        }
    }

    MASSERT(!"Can't find plugin handler param.");
    return nullptr;
}

/* virtual */ RC VmiopElwManager::SetupAllVfTests(ArgDatabase*)
{
    RC rc;
    CHECK_RC(ConfigGfids());

    // Allocate a test log for every valid gfid
    for (auto& vfConfig: m_VfConfigs)
    {
        if (vfConfig.gfid != UNINITIALIZED_SRIOVGFID)
        {
            m_VfTestLog[vfConfig.gfid] = {0};
        }
    }

    return rc;
}

RC VmiopElwManager::RulwfTests(const vector<VfConfig>& vfConfigs)
{
    MASSERT(Platform::IsPhysFunMode());
    RC rc;

    vector<UINT32> gfids;
    {
        Tasker::MutexHolder lock(m_SyncMutex);
        for (const auto& vfConfig: vfConfigs)
        {
            const UINT32 gfid = vfConfig.gfid;
            gfids.push_back(gfid);
            CHECK_RC(SetupVmiopElw(gfid, vfConfig.seqId));

            PluginHandlerParam pluginHandlerParam = {};
            pluginHandlerParam.vfConfig     = vfConfig;
            pluginHandlerParam.pluginHandle = GetVmiopElw(gfid)->GetPluginHandle();
            pluginHandlerParam.earlyPluginUnload = IsEarlyPluginUnload();
            m_PluginHandlerParams.push_back(pluginHandlerParam);

            const string threadName = Utility::StrPrintf("PluginThread_0x%x", gfid);
            m_PluginThreads[gfid] = Tasker::CreateThread(
                    PFHandler, GetPluginHandlerParam(gfid), 0, threadName.c_str());
            ++m_RunningVfsHandlerCount;
        }
    }

    // Wait until all vgpu plugins are initialized
    // and remote PID exchanged
    //
    CHECK_RC(Tasker::PollHw(
                    Tasker::GetDefaultTimeoutMs() * gfids.size(),
                    [&]() mutable
                    {
                        while (!gfids.empty())
                        {
                            VmiopElw* pElw = GetVmiopElw(gfids.back());
                            if (pElw && !pElw->IsRemoteProcessRunning())
                                return false;
                            gfids.pop_back();
                        }
                        return true;
                    },
                    __FUNCTION__));
    return rc;
}

RC VmiopElwManager::LogTestStart(UINT32 gfid, INT32 testNumber)
{
    RC rc;
    auto iter = m_VfTestLog.find(gfid);
    if (iter == m_VfTestLog.end())
    {
        Printf(Tee::PriError,
               "SRIOV: Testlog for GFID %u doesn't exist!\n", gfid);
        return RC::SOFTWARE_ERROR;
    }

    iter->second.lwrrTest = testNumber;
    return rc;
}

RC VmiopElwManager::LogTestComplete(UINT32 gfid, INT32 testNumber, UINT32 errorCode)
{
    RC rc;
    auto iter = m_VfTestLog.find(gfid);
    if (iter == m_VfTestLog.end())
    {
        Printf(Tee::PriError,
               "SRIOV: Testlog for GFID %u doesn't exist!\n", gfid);
        return RC::SOFTWARE_ERROR;
    }

    VfTestState* pTestState = &(iter->second);

    if ((pTestState->lwrrTest != testNumber) && (testNumber >= 0))
    {
        // Not returning error because nothing we can do anyway.
        // Also this could happen if we ever support parallel tests on VF
        Printf(Tee::PriError,
               "SRIOV: test %d on GFID %u did not return status\n", pTestState->lwrrTest, gfid);
    }

    // Only put actual test results in the list
    if (testNumber >= 0)
    {
        pTestState->completedTests.push_back(make_pair(testNumber, errorCode));
    }

    // Also log the first error. This may not be a test failure.
    if (pTestState->firstError == 0)
    {
        pTestState->firstError = errorCode;
    }

    return rc;
}

void VmiopElwManager::PrintVfTestLog()
{
    for (const auto &iter : m_VfTestLog)
    {
        UINT32 gfid = iter.first;
        vector<pair<INT32, UINT32>> completedTests = (iter.second).completedTests;

        Printf(Tee::PriNormal, "------------------------------------\n");
        Printf(Tee::PriNormal, "VF Test results for GFID %u:\n", gfid);
        Printf(Tee::PriNormal, "First Error: %012u\n", (iter.second).firstError);
        Printf(Tee::PriNormal, "\tTEST\tEC\n");
        Printf(Tee::PriNormal, "------------------------------------\n");
        for (const auto testItr : completedTests)
        {
            Printf(Tee::PriNormal, "\t%3d\t%012u\n", testItr.first, testItr.second);
        }
        Printf(Tee::PriNormal, "------------------------------------\n");
    }
}

RC VmiopElwManager::WaitPluginThreadDone(UINT32 gfid)
{
    const auto pThread = m_PluginThreads.find(gfid);
    if (pThread == m_PluginThreads.end())
    {
        Printf(Tee::PriError, "SRIOV: Can't find GFID %u in %s.\n",
               gfid, __FUNCTION__);
        return RC::BAD_PARAMETER;
    }
    Printf(Tee::PriDebug, "SRIOV: Joining thread for GFID %u in %s.\n",
           gfid, __FUNCTION__);
    Tasker::Join(pThread->second);
    Printf(Tee::PriDebug, "SRIOV: Join thread done for GFID %u in %s.\n",
           gfid, __FUNCTION__);
    m_PluginThreads.erase(pThread);

    for (auto pPlugin = m_PluginHandlerParams.begin();
         pPlugin != m_PluginHandlerParams.end(); ++pPlugin)
    {
        if (pPlugin->vfConfig.gfid == gfid)
        {
            m_PluginHandlerParams.erase(pPlugin);
            break;
        }
    }

    return OK;
}

RC VmiopElwManager::WaitAllPluginThreadsDone()
{
    RC rc;

    // If we have reached this point means PF thread has completed it's test list.
    // Release any waiting VF's and mark that PF test list exelwtion is complete
    // MArking test list completion ensures that no VF will again wait for PF
    const bool handlerTerminated = false;
    const bool hasPfCompletedTestRun = true;
    CHECK_RC(ReleaseWaitingPfVfs(handlerTerminated, hasPfCompletedTestRun));

    vector<Tasker::ThreadID> threadIds;
    for (const auto& thread: m_PluginThreads)
    {
        threadIds.push_back(thread.second);
    }
    Tasker::Join(threadIds);
    m_PluginThreads.clear();

    for (const auto& obj: m_PluginHandlerParams)
    {
        if (obj.testRc != OK)
        {
            Printf(Tee::PriError,
                   "SRIOV plugin for GFID %u failed with error %d: %s\n",
                   obj.vfConfig.gfid, obj.testRc.Get(), obj.testRc.Message());
            RC testRc = obj.testRc;
            m_PluginHandlerParams.clear();
            return testRc;
        }
    }
    m_PluginHandlerParams.clear();

    // Print the tests each VF ran and the status
    PrintVfTestLog();

    // Check if any VF failed. If so, return a failure
    for (const auto &iter : m_VfTestLog)
    {
        if ((iter.second).firstError != 0)
        {
            return RC::SRIOV_VF_MODS_FAILURE;
        }
    }
    return rc;
}

RC VmiopElwManager::SetGpuSubdevInstance(GpuSubdevice *pGpuSubdevice)
{
    MASSERT(pGpuSubdevice);
    m_pGpuSubdevice = pGpuSubdevice;
    return RC::OK;
}

RC VmiopElwManager::SyncPfVfs
(
    UINT32 timeoutMs,
    bool syncPf,
    bool handlerTerminated,
    bool pfTestRunComplete
)
{
    RC rc;

    Tasker::MutexHolder lock(m_SyncMutex);

    if (handlerTerminated && pfTestRunComplete)
    {
        MASSERT(!"handlerTerminated and pfTestRunComplete cannot be true at the same time");
    }

    // Save this value once for use in ReleaseWaitingPfVfs()
    if (!m_SyncPfRequested && syncPf)
    {
        m_SyncPfRequested = syncPf;
    }

    // Wait on PF only if it has not already completed it's test list (m_HasPfCompletedTestRun)
    // and the sync between PF and VF's was explicitly requested (m_SyncPfRequested)
    const UINT32 totalPfVfCount = m_RunningVfsHandlerCount
                      + ((!m_HasPfCompletedTestRun && m_SyncPfRequested) ? 1 : 0);

    // +1 for current VF/PF calling this function
    bool isLastVf = ((m_WaitingPfVfsCount + 1) == totalPfVfCount);
    if (isLastVf)
    {
        if (m_WaitingPfVfsCount)
        {
            m_WaitingPfVfsCount = 0;
            m_SyncCond.Broadcast();
        }
    }
    else if (!handlerTerminated && !pfTestRunComplete)
    {
        ++m_WaitingPfVfsCount;
        rc = m_SyncCond.Wait(m_SyncMutex.get(),
                             (timeoutMs ? timeoutMs : Tasker::NO_TIMEOUT));
        if (rc == RC::TIMEOUT_ERROR)
        {
            Printf(Tee::PriError,
                   "Wait condition failed. Unable to sync PF/VF's\n");
            return rc;
        }
        MASSERT(rc == RC::OK);
    }

    if (pfTestRunComplete)
    {
        m_HasPfCompletedTestRun = true;
    }
    else if (handlerTerminated)
    {
        --m_RunningVfsHandlerCount;
    }

    return rc;
}

bool VmiopElwManager::AreAllVfsWaiting()
{
    Tasker::MutexHolder lock(m_SyncMutex);
    return (m_WaitingPfVfsCount == m_RunningVfsHandlerCount);
}

RC VmiopElwManager::SetupVmiopElw(UINT32 gfid, UINT32 seqId)
{
    RC rc;

    if (m_GfidToVmiopElw.count(gfid) == 0)
    {
        unique_ptr<VmiopElw> pVmiopElw = (Platform::IsPhysFunMode() ?
                                          AllocVmiopElwPhysFun(gfid, seqId) :
                                          AllocVmiopElwVirtFun(gfid, seqId));
        CHECK_RC(pVmiopElw->Initialize());
        m_GfidToVmiopElw[gfid] = move(pVmiopElw);
    }
    Printf(Tee::PriDebug,
           "SRIOV %s: Creating VmiopElw object with GFID %u\n",
           __FUNCTION__, gfid);
    return rc;
}

/* virtual */ unique_ptr<VmiopElw> VmiopElwManager::AllocVmiopElwPhysFun
(
    UINT32 gfid,
    UINT32 seqId
)
{
    MASSERT(Platform::IsPhysFunMode());
    return make_unique<VmiopElwPhysFun>(this, gfid, seqId);
}

/* virtual */ unique_ptr<VmiopElw> VmiopElwManager::AllocVmiopElwVirtFun
(
    UINT32 gfid,
    UINT32 seqId
)
{
    MASSERT(Platform::IsVirtFunMode());
    return make_unique<VmiopElwVirtFun>(this, gfid, seqId);
}

/* virtual */ RC VmiopElwManager::RemoveVmiopElw(UINT32 gfid)
{
    RC rc;
    VmiopElw* pElw = GetVmiopElw(gfid);
    if (pElw)
    {
        CHECK_RC(pElw->Shutdown());
        m_GfidToVmiopElw.erase(gfid);
        Printf(Tee::PriDebug,
               "SRIOV %s: Removing VmiopElw object with GFID %u\n",
               __FUNCTION__, gfid);
    }
    return rc;
}

/* virtual */ RC VmiopElwManager::ConfigGfids()
{
    // TODO: Assign gfids sequentially for now, but do better later

    set<UINT32> gfidInUse = { 0, UNINITIALIZED_SRIOVGFID };
    for (auto& vfConfig: m_VfConfigs)
    {
        if (gfidInUse.count(vfConfig.gfid))
        {
            vfConfig.gfid = UNINITIALIZED_SRIOVGFID;
        }
        gfidInUse.insert(vfConfig.gfid);
    }

    UINT32 nextGfid = 0;
    for (auto& vfConfig: m_VfConfigs)
    {
        if (vfConfig.gfid == UNINITIALIZED_SRIOVGFID)
        {
            while (gfidInUse.count(nextGfid))
            {
                ++nextGfid;
            }
            vfConfig.gfid = nextGfid;
            gfidInUse.insert(nextGfid);
        }
    }

    return OK;
}

RC VmiopElwManager::ParseVfConfigFile()
{
    MASSERT(Platform::IsPhysFunMode());
    MASSERT(GetVfProcessCount() == 0);
    RC rc;

    if (m_ConfigFile.empty())
    {
        MASSERT(!"Please specify the command line -sriov_config_file.\n");
        return RC::SOFTWARE_ERROR;
    }

    FileHolder fileHolder;
    CHECK_RC(fileHolder.Open(m_ConfigFile, "r"));
    FILE *pFile = fileHolder.GetFile();

    long fileSize = 0;
    CHECK_RC(Utility::FileSize(pFile, &fileSize));
    vector<char> buffer(fileSize + 2);
    char* line = &buffer[0];

    while (fgets(line, (UINT32)buffer.size(), pFile) && !feof(pFile))
    {
        char* pStartComment = strchr(line, '#');
        if (pStartComment != nullptr)
        {
            *pStartComment = '\0';
        }

        const char* format = "- test_exelwtable: %[^/]/%s";
        constexpr UINT32 BUFFER_SIZE = 100;
        char path[BUFFER_SIZE] = {};
        char file[BUFFER_SIZE] = {};
        if (2 == sscanf(line, format, path, file))
        {
            VfConfig vfConfig = {};
            vfConfig.vftestDir = path;
            vfConfig.vftestFileName = file;
            vfConfig.seqId = GetVfProcessCount() + 1;
            vfConfig.gfid = UNINITIALIZED_SRIOVGFID;
            m_VfConfigs.push_back(vfConfig);
        }
        else
        {
            auto len = strlen(line);
            // Remove trailing spaces, CR, LF
            if (len && line[len - 1] == '\n')
            {
                line[--len] = 0;
            }
            if (len && line[len - 1] == '\r')
            {
                line[--len] = 0;
            }
            while (len && line[len - 1] == ' ')
            {
                line[--len] = 0;
            }

            // If line non-empty, print it
            if (len)
            {
                Printf(Tee::PriWarn, "Ignored line: %s\n", line);
            }
        }
    }

    // If no -sriov_vf_count arg, then use the number of sessions.
    //
    if (m_VfCount == 0)
    {
        m_VfCount = GetVfProcessCount();
    }
    else if (m_VfCount < GetVfProcessCount())
    {
        Printf(Tee::PriError,
               "VF sessions in %s exceeds num VFs passed in -sriov_vf_count\n",
               m_ConfigFile.c_str());
        return RC::BAD_COMMAND_LINE_ARGUMENT;
    }

    if (m_VfCount == 0)
    {
        Printf(Tee::PriError, "No VFs configured!\n");
        // Arch tests in AXL run with zero VFs, so excluding sim MODS, failing everywhere else
        if (Xp::GetOperatingSystem() != Xp::OS_LINUXSIM)
        {
            return RC::BAD_COMMAND_LINE_ARGUMENT;
        }
    }

    return rc;
}

void VmiopElwManager::ExitVmiopElw(PluginHandlerParam* pParam, bool bRemoteStarted)
{
    const UINT32 gfid = pParam->vfConfig.gfid;
    if (bRemoteStarted)
    {
        VmiopElw* pElw = GetVmiopElw(gfid);
        pElw->StartExit();
        pParam->testRc = pElw->WaitRemoteProcessDone();
        Printf(Tee::PriDebug, "SRIOV GFID %u remote process done.\n",
               gfid);
    }
    pParam->testRc = RemoveVmiopElw(gfid);
}

RC VmiopElwManager::PFHandlerImpl(PluginHandlerParam* pParam)
{
    MASSERT(Platform::IsPhysFunMode());
    MASSERT(pParam != nullptr);

    const string&        vftestDir      = pParam->vfConfig.vftestDir;
    const string&        vftestFileName = pParam->vfConfig.vftestFileName;
    const UINT32         gfid           = pParam->vfConfig.gfid;
    const vmiop_handle_t handle         = pParam->pluginHandle;
    VmiopElw*            pElw           = GetVmiopElw(gfid);
    RC                   rc;
    MASSERT(pElw != nullptr);

    Printf(Tee::PriDebug, "SRIOV %s Loading plugin library in thread %d, GFID %u\n",
           __FUNCTION__, Tasker::LwrrentThread(), gfid);

    string path = vftestDir + "/liblwidia-vgpu" + Xp::GetDLLSuffix();
    void * librm = 0;
    CHECK_RC_MSG(Xp::LoadDLL(path, &librm, Xp::UnloadDLLOnExit),
                 "SRIOV failed to load plugin library %s", path.c_str());

    // Unload the plugin on exit, and set testRc to SOFTWARE_ERROR if
    // this function aborts early.
    //
    bool unloadDll = false;
    bool shutdownPlugin = false;
    bool callExitPlugin = false;
    vmiop_plugin_t* plugin = nullptr;
    bool remoteStarted = false;
    DEFER
    {
        if (pParam->earlyPluginUnload)
        {
            // Early plugin unload flow for Tu10x/11x.
            pElw->AbortRpcMailbox();
            pElw->AbortModsChannel();
        }
        else
        {
            // Normal flow for non Tu10x/11x chips.
            // Delay vGPu plugin unload after VmiopElw exit.
            // Exit VmiopElw now.
            Printf(Tee::PriDebug,
                   "SRIOV Waiting for remote process done for GFID %u either non VFLR or non Tu10x/11x chips.\n", gfid);
            ExitVmiopElw(pParam, remoteStarted);
        }

        if (shutdownPlugin && plugin)
        {
            Printf(Tee::PriDebug, "SRIOV plugin->shutdown() handle = 0x%x in thread %d, GFID %u, earlyPluginUnload: %s.\n",
                   handle, Tasker::LwrrentThread(), gfid, pParam->earlyPluginUnload ? "Yes" : "No");
            pParam->testRc = VmiopToRc(plugin->shutdown(handle));
        }

        if (callExitPlugin && pParam->pModsCallbacks)
        {
            Printf(Tee::PriDebug, "SRIOV exitPlugin in thread %d, GFID %u, earlyPluginUnload: %s.\n",
                   Tasker::LwrrentThread(), gfid, pParam->earlyPluginUnload ? "Yes" : "No");
            pParam->testRc = VmiopToRc(
                    pParam->pModsCallbacks->exitPlugin(pParam->pluginHandle));
        }

        if (unloadDll)
        {
            Printf(Tee::PriDebug, "SRIOV UnloadDLL in thread %d, GFID %u, earlyPluginUnload: %s.\n",
                   Tasker::LwrrentThread(), gfid, pParam->earlyPluginUnload ? "Yes" : "No");
            pParam->testRc = Xp::UnloadDLL(librm);
        }
        
        if (pParam->earlyPluginUnload)
        {
            // Early plugin unload flow for Tu10x/11x.
            // Exit VmiopElw after plugin unload.
            Printf(Tee::PriDebug, "SRIOV unblock vFLR after vGpu plugin unload, Waiting for GFID %u remote process done.\n",
                   gfid);
            ExitVmiopElw(pParam, remoteStarted);
        }
        Printf(Tee::PriDebug, "SRIOV %s thread %d exiting, GFID %u\n",
               __FUNCTION__, Tasker::LwrrentThread(), gfid);
    };

    plugin = static_cast<vmiop_plugin_t *>(
            Xp::GetDLLProc(librm, "vmiop_display_vmiop_plugin"));
    if (plugin == NULL ||
        plugin->signature == NULL ||
        strcmp(plugin->signature, "VMIOP_PLUGIN_SIGNATURE") ||
        plugin->name == NULL)
    {

        Printf(Tee::PriError,
               "SRIOV Loaded plugin library doesn't match expectation\n");
        return RC::DLL_LOAD_FAILED;
    }
    unloadDll = true;
    Printf(Tee::PriDebug,
           "SRIOV Loaded vgpu plugin successfully (signature = %s name = %s)\n",
           plugin->signature, plugin->name);

    vmiop_plugin_elw_t* plugin_elw = static_cast<vmiop_plugin_elw_t*>(
            Xp::GetDLLProc(librm, "vmiop_display_vmiop_elw"));
    if (plugin_elw == NULL)
    {
        Printf(Tee::PriError,
            "Failed to find vmiop_display_vmiop_elw symbol in vGPU plugin\n");
        return RC::DLL_LOAD_FAILED;
    }

    plugin_elw->direct_sysmem_mapping_supported = vmiop_false;

    pParam->pModsCallbacks = static_cast<VmiopModsCallbacks*>(
            Xp::GetDLLProc(librm, "vmiopModsCallbacks"));
    if (pParam->pModsCallbacks == NULL)
    {
        Printf(Tee::PriError,
            "SRIOV: Failed to find vmiopModsCallbacks symbol in vGPU plugin\n");
        return RC::DLL_LOAD_FAILED;
    }

    // init the vgpu plugin
    static const VmiopShimFunctions shims =
    {
        ModsDrvAcquireMutex,
        ModsDrvAlloc,
        ModsDrvAllocEventAutoReset,
        ModsDrvAllocMutex,
        ModsDrvAtomicCmpXchg32,
        ModsDrvAtomicCmpXchg64,
        ModsDrvBreakPoint,
        ModsDrvCalloc,
        ModsDrvCreateThread,
        ModsDrvExitThread,
        ModsDrvFindSharedSysmem,
        ModsDrvFree,
        ModsDrvFreeEvent,
        ModsDrvFreeMutex,
        ModsDrvFreeSharedSysmem,
        ModsDrvGetLwrrentIrql,
        ModsDrvGetLwrrentThreadId,
        ModsDrvGetOsEvent,
        ModsDrvGetPageSize,
        ModsDrvGetTimeNS,
        ModsDrvImportSharedSysmem,
        ModsDrvJoinThread,
        ModsDrvMapDeviceMemory,
        ModsDrvMemCopy,
        ModsDrvMemRd08,
        ModsDrvMemRd16,
        ModsDrvMemRd32,
        ModsDrvMemSet,
        ModsDrvMemWr08,
        ModsDrvMemWr16,
        ModsDrvMemWr32,
        ModsDrvPrintString,
        ModsDrvReadRegistryString,
        ModsDrvReleaseMutex,
        ModsDrvSetEvent,
        ModsDrvTryAcquireMutex,
        ModsDrvUnMapDeviceMemory,
        ModsDrvVPrintf,
        ModsDrvWaitOnEvent,
        LwRmAlloc,
        LwRmAllocContextDma2,
        LwRmAllocMemory64,
        LwRmAllocObject,
        LwRmAllocRoot,
        LwRmControl,
        LwRmDupObject,
        LwRmDupObject2,
        LwRmFree,
        LwRmIdleChannels,
        LwRmMapMemory,
        LwRmMapMemoryDma,
        LwRmUnmapMemory,
        LwRmUnmapMemoryDma,
        LwRmVidHeapControl
    };
    CHECK_RC_MSG(VmiopToRc(pParam->pModsCallbacks->enterPlugin(handle, 1, &gfid,
                                                               &shims)),
                 "SRIOV: Failed to call enterPlugin mods callback");
    callExitPlugin = true;

    CHECK_RC_MSG(VmiopToRc(plugin->init_routine(handle)),
                 "SRIOV: Failed to call init_routine(), handle = 0x%x", handle);
    Printf(Tee::PriDebug, "SRIOV: plugin->init_routine(0x%x)\n", handle);
    shutdownPlugin = true;

    CHECK_RC_MSG(pElw->RunRemoteProcess(vftestDir + "/" + vftestFileName),
                 "SRIOV remote process failed.  Exiting...");
    remoteStarted = true;

    // Handling requests from VF
    // We should exit the handler only if RPC and mods channel status is set to ABORT.
    // If the status is set to ABORT, ProcessingRequests returns RESET_IN_PROGRESS.
    // All other error codes should be ignored and the handler should be kept running.
    CHECK_RC_MSG(
            Tasker::PollHw(Tasker::NO_TIMEOUT,
                           [&] { return (pElw->ProcessingRequests() == RC::RESET_IN_PROGRESS ||
                                         !pElw->IsRemoteProcessRunning()); },
                           __FUNCTION__),
            "SRIOV processing requests failed.  Exiting...");

    const bool handlerTerminated = true;
    const bool hasPfCompletedTestRun = false;
    CHECK_RC(ReleaseWaitingPfVfs(handlerTerminated, hasPfCompletedTestRun));
    Printf(Tee::PriDebug,
           "SRIOV Plugin thread for plugin GFID %u is done.\n", gfid);
    
    return OK;
}

//--------------------------------------------------------------------
//! \brief Release PF or other VF's on this VF thread
//!
//! This function is needed to handle the case where all of the VF's
//! and PF are waiting on 1 VF to reach the sync point and that VF
//! doesn't call SriovSync test or the handler for that VF terminates
//! before SriovSync test is called.
RC VmiopElwManager::ReleaseWaitingPfVfs(bool handlerTerminated, bool pfTestRunComplete)
{
    RC rc;
    const UINT32 timeoutMs = 0;
    return SyncPfVfs(timeoutMs, m_SyncPfRequested, handlerTerminated, pfTestRunComplete);
}
