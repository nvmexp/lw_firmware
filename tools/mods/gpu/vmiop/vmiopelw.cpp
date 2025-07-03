/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/vmiop/vmiopelw.h"
#include "core/include/mgrmgr.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/registry.h"
#include "core/utility/sharedsysmem.h"
#include "gpu/include/gpumgr.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "vgpu/dev_vgpu.h"
#include "vgpu/mods/vmiop-external-mods.h"
#include <algorithm>
#include <iterator>

/* virtual */ RC VmiopElw::Initialize()
{
    RC rc;

    // Allocate shared process memory
    CHECK_RC(CreateProcSharedBuf(PCIE_BAR_INFO, "PCIeBarInfo", sizeof(PcieBarInfo)));
    CHECK_RC(CreateProcSharedBuf(RPC_MAILBOX,   "RpcMailBox",  sizeof(RpcMailbox)));
    CHECK_RC(CreateProcSharedBuf(MODS_CHAN,     "ModsChannel", sizeof(ModsChannel)));

    CHECK_RC(CreateVmiopConfigValues());
    return rc;
}

/* virtual */ RC VmiopElw::Shutdown()
{
    RC rc;
    CHECK_RC(DestroyProcSharedBufs());
    return rc;
}

RC VmiopElw::SetRemotePid(UINT32 pid)
{
    MASSERT(pid > 2);
    RC rc;

    m_RemotePid = pid;
    CHECK_RC(CreateVmiopConfigValue(true, VMIOP_CONFIG_PID_KEY,
                                    Utility::StrPrintf("%u", pid)));
    Printf(Tee::PriDebug, "%s: Remote process PID %u for GFID %u.\n",
           __FUNCTION__, pid, GetGfid());
    return rc;
}

RC VmiopElw::RunRemoteProcess(const string& cmdLine)
{
    SharedSysmem::PidBox pidbox;
    const UINT32 gfid = GetGfid();
    RC rc;

    m_pRemoteProcess = Xp::AllocProcess();
    CHECK_RC(SetElw(m_pRemoteProcess.get()));

    CHECK_RC(m_pRemoteProcess->Run(cmdLine));
    Printf(Tee::PriDebug, "SRIOV started VF process \"SRIOV_GFID %u %s\"\n",
           gfid, cmdLine.c_str());

    // Exchange PID with remote process.  If the PID fails
    UINT32 remotePid;
    pidbox.SetAbortCheck([&]{ return !m_pRemoteProcess->IsRunning(); });
    CHECK_RC_MSG(pidbox.ReceivePid(&remotePid),
                 "SRIOV: Failed to get child PID");
    CHECK_RC_MSG(SetRemotePid(remotePid), "SRIOV: Failed to set child PID");
    CHECK_RC_MSG(pidbox.Disconnect(),
                 "SRIOV: Failed handshake with child process");
    Printf(Tee::PriDebug, "SRIOV received remote PID %d\n", remotePid);

    return rc;
}

bool VmiopElw::IsRemoteProcessRunning() const
{
    if (m_pRemoteProcess)
        return m_pRemoteProcess->IsRunning();
    else if (m_RemotePid != 0)
        return Xp::IsPidRunning(m_RemotePid);
    else
        return false;
}

RC VmiopElw::WaitRemoteProcessDone() const
{
    RC rc;
    if (m_pRemoteProcess)
    {
        INT32 status = 0;
        // Need calling IsRunning() + Yield() to sim clock chiplib/fmod.
        // Otherwise VF may be blocked due stopped simtime when simply m_pRemoteProcess->Wait().
        while (m_pRemoteProcess->IsRunning())
        {
            Tasker::Yield();
        }
        CHECK_RC(m_pRemoteProcess->Wait(&status));
        if (status != 0)
        {
            Printf(Tee::PriError,
                   "SRIOV: VF process at GFID %u failed with status %d\n",
                   GetGfid(), status);
            return RC::SCRIPT_FAILED_TO_EXELWTE;
        }
    }
    return OK;
}

void VmiopElw::KillRemoteProcess() const
{
    if (m_pRemoteProcess)
    {
        m_pRemoteProcess->Kill();
    }
}

void VmiopElw::AbortRpcMailbox()
{
    const ProcSharedBufDesc* pMailBoxDesc = &m_ProcSharedBufs[RPC_MAILBOX];
    RpcMailbox* pMailbox = static_cast<RpcMailbox*>(pMailBoxDesc->pBuf);

    if (!pMailbox)
    {
        MASSERT(!"RpcMailbox shared memory is missing");
    }

    Printf(Tee::PriNormal,
           "SRIOV: %s GFID %u abort RPC mailbox.\n",
            __FUNCTION__,
           GetGfid());
    pMailbox->Commit(RpcMailbox::State::ABORT);
}

RC VmiopElw::AbortModsChannel()
{
    const ProcSharedBufDesc* pDesc = &m_ProcSharedBufs[MODS_CHAN];
    ModsChannel* pChan = static_cast<ModsChannel*>(pDesc->pBuf);
    if (!pChan)
    {
        MASSERT(!"Mods channel shared memory is missing");
    }

    Printf(Tee::PriNormal,
           "SRIOV: %s GFID %u abort MODS Channel.\n",
            __FUNCTION__,
           GetGfid());
    return pChan->Commit(ModsChannel::State::ABORT);
}

RC VmiopElw::CreateProcSharedBuf(UINT32 index, const char* name, size_t size)
{
    RC rc;

    if (index >= m_ProcSharedBufs.size())
    {
        m_ProcSharedBufs.resize(index + 1);
    }

    ProcSharedBufDesc* pDesc = &m_ProcSharedBufs[index];
    if (pDesc->pBuf != nullptr)
    {
        // Only create it once for each vgpu plugin
        if (pDesc->bufName != name || pDesc->bufSize != size)
        {
            MASSERT(!"Recreated ProcSharedBuf with different name or size");
            return RC::SOFTWARE_ERROR;
        }
        return rc;
    }

    pDesc->bufName = name;
    pDesc->bufSize = size;
    CHECK_RC(MapProcSharedBuf(pDesc));
    MASSERT(pDesc->pBuf != nullptr);
    return rc;
}

RC VmiopElw::DestroyProcSharedBufs()
{
    for (const ProcSharedBufDesc& desc: m_ProcSharedBufs)
    {
        SharedSysmem::Free(desc.pBuf);
    }
    m_ProcSharedBufs.clear();
    return OK;
}

void VmiopElw::InitBdf()
{
    if (m_PluginHandle != 0)
    {
        return;
    }

    MASSERT(m_PcieDomain   == _UINT32_MAX);
    MASSERT(m_PcieBus      == _UINT32_MAX);
    MASSERT(m_PcieDevice   == _UINT32_MAX);
    MASSERT(m_PcieFunction == _UINT32_MAX);
    const RC rc = GetVmiopElwMgr()->GetDomainBusDevFun(GetGfid(),
                                                       &m_PcieDomain,
                                                       &m_PcieBus,
                                                       &m_PcieDevice,
                                                       &m_PcieFunction);
    if (rc != OK)
    {
        MASSERT(!"GetDomainBusDevFun failed");
        return;
    }
    m_PluginHandle = Pci::GetConfigAddress(m_PcieDomain, m_PcieBus,
                                           m_PcieDevice, m_PcieFunction, 1);
}

RC VmiopElwPhysFun::Initialize()
{
    RC rc;

    CHECK_RC(VmiopElw::Initialize());

    // Initialize shared process memory buffers
    *static_cast<PcieBarInfo*>(m_ProcSharedBufs[PCIE_BAR_INFO].pBuf) = {};
    *static_cast<RpcMailbox*>(m_ProcSharedBufs[RPC_MAILBOX].pBuf)    = {};
    *static_cast<ModsChannel*>(m_ProcSharedBufs[MODS_CHAN].pBuf)     = {};

    // Init BAR info
    PcieBarInfo barInfo = GetVmiopElwMgr()->GetBarInfo(GetGfid());
    for (UINT32 barIndex = 0; barIndex < PcieBarInfo::NUM_BARS; ++barIndex)
    {
        CHECK_RC(SendReceivePcieBarInfo(barIndex,
                                        &barInfo.bar[barIndex].base,
                                        &barInfo.bar[barIndex].size));
    }

    return rc;
}

RC VmiopElwPhysFun::SendReceivePcieBarInfo
(
    UINT32    barIndex,
    PHYSADDR* pBarBase,
    UINT64*   pBarSize
)
{
    MASSERT(Platform::IsPhysFunMode());
    MASSERT(barIndex < PcieBarInfo::NUM_BARS);

    void* pBarInfo = m_ProcSharedBufs[PCIE_BAR_INFO].pBuf;
    auto* pBar     = &static_cast<PcieBarInfo*>(pBarInfo)->bar[barIndex];

    pBar->base = *pBarBase;
    pBar->size = *pBarSize;

    Printf(Tee::PriDebug,
           "SRIOV PhysFun::%s: GFID %u PCIe Bar%u base 0x%llx size 0x%llx.\n",
           __FUNCTION__,
           GetGfid(),
           barIndex,
           pBar->base,
           pBar->size);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Used to implement VmiopElwManager::FindVfEmuRegister
//!
RC VmiopElwPhysFun::FindVfEmuRegister(PHYSADDR physAddr, UINT32* pRegister)
{
    MASSERT(!"FindVfEmuRegister is only valid for VF");
    return RC::SOFTWARE_ERROR;
}

RC VmiopElwPhysFun::CallPluginRegWrite
(
    UINT32      dataOffset,
    UINT32      dataSize,
    const void* pData
)
{
    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), dataOffset);
    vmiop_emul_state_t cacheable = vmiop_emul_noncacheable;
    const VmiopModsCallbacks *pModsCallbacks =
        GetVmiopElwMgr()->GetPluginHandlerParam(GetGfid())->pModsCallbacks;
    vmiop_error_t ret = pModsCallbacks->readWriteDevice(
            GetGfid(),
            vmiop_emul_op_write,
            vmiop_emul_space_mmio,
            dataOffset,
            dataSize,
            const_cast<void*>(pData),
            &cacheable);

    if (ret != vmiop_success)
    {
        Printf(Tee::PriError,
               "SRIOV %s failed on vgpu plugin call\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriDebug, "SRIOV %s dataSize = 0x%x %s = 0x%x\n",
           __FUNCTION__, dataSize,
           dataSize >= 4 ? "*(const UINT32*)pData" : "*(const UINT08*)pData",
           dataSize >= 4 ? *(const UINT32*)pData : *(const UINT08*)pData);
    return OK;
}

RC VmiopElwPhysFun::CallPluginRegRead
(
    UINT32 dataOffset,
    UINT32 dataSize,
    void*  pData
)
{
    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), dataOffset);
    vmiop_emul_state_t cacheable = vmiop_emul_noncacheable;
    const VmiopModsCallbacks *pModsCallbacks =
        GetVmiopElwMgr()->GetPluginHandlerParam(GetGfid())->pModsCallbacks;
    vmiop_error_t ret = pModsCallbacks->readWriteDevice(
            GetGfid(),
            vmiop_emul_op_read,
            vmiop_emul_space_mmio,
            dataOffset,
            dataSize,
            pData,
            &cacheable);

    if (ret != vmiop_success)
    {
        Printf(Tee::PriError,
               "SRIOV %s failed on vgpu plugin call\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriDebug, "SRIOV %s dataSize = 0x%x %s = 0x%x\n",
           __FUNCTION__, dataSize,
           dataSize >= 4 ? "*(const UINT32*)pData" : "*(const UINT08*)pData",
           dataSize >= 4 ? *(const UINT32*)pData : *(const UINT08*)pData);
    return OK;
}

RC VmiopElwPhysFun::CallPluginPfRegRead
(
    ModsGpuRegAddress regAddr,
    UINT32* pVal
)
{
    MASSERT(pVal);

    const GpuSubdevice* pGpuSubdevice = GetVmiopElwMgr()->GetGpuSubdevInstance();
    MASSERT(pGpuSubdevice);
    const RegHal &regs = pGpuSubdevice->Regs();
    if (!regs.IsSupported(regAddr))
    {
        Printf(Tee::PriError,
               "Register read request from VF failed. Register not supported %s\n",
               regs.ColwertToString(regAddr));
        return RC::REGISTER_READ_WRITE_FAILED;
    }
    *pVal = regs.Read32(regAddr);

    return RC::OK;
}

RC VmiopElwPhysFun::CallPluginPfRegIsSupported
(
    ModsGpuRegAddress regAddr,
    bool* pIsSupported
)
{
    MASSERT(pIsSupported);
    *pIsSupported = GetVmiopElwMgr()->GetGpuSubdevInstance()->Regs().IsSupported(regAddr);
    return RC::OK;
}

RC VmiopElwPhysFun::CallPluginPfRegTest32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    RegHal::ArrayIndexes regIndexes,
    UINT32 value,
    bool* pResult
)
{
    MASSERT(pResult);
    *pResult = GetVmiopElwMgr()->GetGpuSubdevInstance()->Regs().Test32(domainIndex, field, regIndexes, value);
    return RC::OK;
}

RC VmiopElwPhysFun::CallPluginPfRegTest32
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    RegHal::ArrayIndexes regIndexes,
    bool* pResult
)
{
    MASSERT(pResult);
    *pResult = GetVmiopElwMgr()->GetGpuSubdevInstance()->Regs().Test32(domainIndex, value, regIndexes);
    return RC::OK;
}

RC VmiopElwPhysFun::CallPluginSyncPfVfs(UINT32 timeoutMs, bool syncPf)
{
    RC rc;
    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): timeoutMs = %u, syncPf = %s.\n",
           __FUNCTION__, GetGfid(), timeoutMs, syncPf ? "true" : "false");

    VmiopElwManager *pVmiopElwManager = GetVmiopElwMgr();
    const bool handlerTerminated = false;
    const bool pfTestRunComplete = false;
    CHECK_RC(pVmiopElwManager->SyncPfVfs(timeoutMs, syncPf, handlerTerminated, pfTestRunComplete));

    return rc;
}

RC VmiopElwPhysFun::CallPluginLocalGpcToHwGpc(UINT32 smcEngineIdx,
                                              UINT32 localGpcNum, UINT32* pHwGpcNum)
{
    const GpuSubdevice* pSubdev = GetVmiopElwMgr()->GetGpuSubdevInstance();
    MASSERT(pSubdev);
    MASSERT(pSubdev->IsSMCEnabled() && pSubdev->UsingMfgModsSMC());

    const UINT32 partIdx = pSubdev->GetUsablePartIdx(GetSeqId() - 1);
    return pSubdev->LocalGpcToHwGpc(partIdx, smcEngineIdx, localGpcNum, pHwGpcNum);
}

RC VmiopElwPhysFun::CallPluginGetTpcMask(UINT32 hwGpcNum, UINT32* pTpcMask)
{
    const GpuSubdevice* pSubdev = GetVmiopElwMgr()->GetGpuSubdevInstance();
    MASSERT(pSubdev);

    *pTpcMask = pSubdev->GetFsImpl()->TpcMask(hwGpcNum);
    return RC::OK;
}

RC VmiopElwPhysFun::ProcessRpcMailbox()
{
    RC rc;
    const ProcSharedBufDesc* pMailBoxDesc = &m_ProcSharedBufs[RPC_MAILBOX];
    RpcMailbox* pMailbox = static_cast<RpcMailbox*>(pMailBoxDesc->pBuf);
    if (!pMailbox)
    {
        MASSERT(!"RpcMailbox shared memory is missing");
        return RC::SOFTWARE_ERROR;
    }

    // Enforce ordering of commit
    const RpcMailbox::State state = pMailbox->GetState();
    const UINT32 offset = pMailbox->offset;
    const UINT32 size   = pMailbox->size;
    void* pData         = &pMailbox->data[0];

    if (size > RpcMailbox::DATA_MAX_SIZE)
    {
        Printf(Tee::PriError, "SRIOV %s failed size check\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    else if (state == RpcMailbox::State::ABORT)
    {
        // No more read/write allowed.
        return RC::RESET_IN_PROGRESS;
    }
    else
    {
        switch (state)
        {
            case RpcMailbox::State::WRITE:
                rc = CallPluginRegWrite(offset, size, pData);
                if (rc != RC::OK)
                {
                    pMailbox->Commit(RpcMailbox::State::REQUEST_FAILED);
                    return rc;
                }
                pMailbox->Commit(RpcMailbox::State::DONE);
                break;
            case RpcMailbox::State::READ:
                rc = CallPluginRegRead(offset, size, pData);
                if (rc != RC::OK)
                {
                    pMailbox->Commit(RpcMailbox::State::REQUEST_FAILED);
                    return rc;
                }
                pMailbox->Commit(RpcMailbox::State::DONE);
                break;
            case RpcMailbox::State::DONE:
            case RpcMailbox::State::ABORT:
            case RpcMailbox::State::REQUEST_FAILED:
                break;
            default:
                MASSERT(!"Unknown operation!");
                return RC::SOFTWARE_ERROR;
        }
    }
    return rc;
}

RC VmiopElwPhysFun::ProcessModsChannel()
{
    using ChanState = ModsChannel::State;

    RC rc;
    const ProcSharedBufDesc* pDesc = &m_ProcSharedBufs[MODS_CHAN];
    ModsChannel* pChan = static_cast<ModsChannel*>(pDesc->pBuf);
    if (!pChan)
    {
        MASSERT(!"MODS channel shared memory is missing");
        return RC::SOFTWARE_ERROR;
    }

    if (pChan->GetState() != ChanState::VF_REQUEST_SENT)
    {
        return RC::OK;
    }

    const ModsChannel::Request req = pChan->request;
    switch (req)
    {
        case ModsChannel::Request::PF_REG_READ:
        {
            auto* pData = reinterpret_cast<ModsChannel::RegData*>(pChan->data);
            rc = CallPluginPfRegRead(pData->addr, &pData->value);
        }
        break;

        case ModsChannel::Request::PF_REG_IS_SUPPORTED:
        {
            auto* pData = reinterpret_cast<ModsChannel::RegIsSupported*>(pChan->data);
            rc = CallPluginPfRegIsSupported(pData->addr, &pData->isSupported);
        }
        break;

        case ModsChannel::Request::PF_REG_TEST_32:
        {
            // Dispatch to type of RegHal Test32 request
            using Test32Variant = ModsChannel::RegTest32::Variant;

            auto* pTest32Data = reinterpret_cast<ModsChannel::RegTest32*>(pChan->data);

            switch (pTest32Data->type)
            {
                case Test32Variant::REG_FIELD:
                {
                    auto* pData = static_cast<ModsChannel::RegTest32RegField*>(pTest32Data);

                    auto validIdxEndIter = pData->indexes.begin();
                    MASSERT(pData->numIndexes <= ModsChannel::RegTest32::MAX_REGHAL_INDEXES);
                    std::advance(validIdxEndIter, pData->numIndexes);
                    RegHal::ArrayIndexes_t indexes(pData->indexes.begin(),
                                                   validIdxEndIter);

                    rc = CallPluginPfRegTest32(pData->domain,
                                               pData->field,
                                               indexes,
                                               pData->value,
                                               &pData->result);
                }
                break;

                case Test32Variant::REG_VALUE:
                {
                    ModsChannel::RegTest32RegValue* pData
                        = static_cast<decltype(pData)>(pTest32Data);

                    auto validIdxEndIter = pData->indexes.begin();
                    MASSERT(pData->numIndexes <= ModsChannel::RegTest32::MAX_REGHAL_INDEXES);
                    std::advance(validIdxEndIter, pData->numIndexes);
                    RegHal::ArrayIndexes_t indexes(pData->indexes.begin(),
                                                   validIdxEndIter);

                    rc = CallPluginPfRegTest32(pData->domain,
                                               pData->value,
                                               indexes,
                                               &pData->result);
                }
                break;

                default:
                    Printf(Tee::PriError, "Unknown MODS channel RegTest32 type: %u\n",
                           static_cast<UINT32>(pTest32Data->type));
                    MASSERT(!"Unknown MODS channel RegTest32 type");
                    return RC::SOFTWARE_ERROR;
            }
        }
        break;

        case ModsChannel::Request::SYNC_PF_VF:
        {
            auto* pData = reinterpret_cast<ModsChannel::SyncPfVfs*>(pChan->data);
            rc = CallPluginSyncPfVfs(pData->timeoutMs, pData->syncPf);
        }
        break;

        case ModsChannel::Request::LOCAL_GPC_TO_HW_GPC:
        {
            auto* pData = reinterpret_cast<ModsChannel::LocalGpcToHwGpc*>(pChan->data);
            rc = CallPluginLocalGpcToHwGpc(pData->smcEngineIdx,
                                           pData->localGpcNum,
                                           &pData->hwGpcNum);
        }
        break;

        case ModsChannel::Request::GET_TPC_MASK:
        {
            auto* pData = reinterpret_cast<ModsChannel::GetTpcMask*>(pChan->data);
            rc = CallPluginGetTpcMask(pData->hwGpcNum, &pData->tpcMask);
        }
        break;

        case ModsChannel::Request::LOG_TEST:
        {
            auto* pData = reinterpret_cast<ModsChannel::TestLog*>(pChan->data);
            if (pData->bIsTestStart)
            {
                rc = VmiopElwPhysFun::CallPluginLogTestStart(pData->gfid, pData->testNumber);
            }
            else
            {
                rc = VmiopElwPhysFun::CallPluginLogTestEnd(pData->gfid, pData->testNumber, pData->errorCode);
            }
        }
        break;

        default:
            Printf(Tee::PriError, "Unknown MODS channel state: %u\n",
                   static_cast<UINT32>(req));
            MASSERT(!"Unknown MODS channel state!");
            return RC::SOFTWARE_ERROR;
    }

    if (rc != RC::OK)
    {
        CHECK_RC(pChan->Commit(ChanState::PF_REQUEST_FAILURE));
    }
    else
    {
        CHECK_RC(pChan->Commit(ChanState::PF_REQUEST_SUCCESS));
    }

    return rc;
}

RC VmiopElwPhysFun::ProcessingRequests()
{
    StickyRC rc;
    rc = ProcessRpcMailbox();
    rc = ProcessModsChannel();
    CHECK_RC(rc);

    if (IsFlrStarted() && GetVmiopElwMgr()->IsEarlyPluginUnload())
    {
        AbortRpcMailbox();
        CHECK_RC(AbortModsChannel());
        return RC::RESET_IN_PROGRESS;
    }

    return rc;
}

// log test status on VFs
RC VmiopElwPhysFun::CallPluginLogTestStart(UINT32 gfid, INT32 testNumber)
{
    // The string returned from ctime() is always 26 characters
    // and has this format: Sun Jan 01 12:34:56 1993\n\0
    time_t Now = time(0);
    Printf(Tee::PriNormal, "GFID %u - Entering test %d at %s", gfid, testNumber, ctime(&Now));

    return (GetVmiopElwMgr()->LogTestStart(gfid, testNumber));
}

RC VmiopElwPhysFun::CallPluginLogTestEnd(UINT32 gfid, INT32 testNumber, UINT32 errorCode)
{
    if (testNumber < 0)
    {
        // This error was logged outside the test thread / has no test number attached.
        Printf(Tee::PriNormal, "GFID %u - Encountered EC %012u\n", gfid, errorCode);
    }
    else
    {
        Printf(Tee::PriNormal, "GFID %u - Exiting test %d with EC %012u\n",
               gfid, testNumber, errorCode);
    }
    return (GetVmiopElwMgr()->LogTestComplete(gfid, testNumber, errorCode));
}

RC VmiopElwPhysFun::MapProcSharedBuf(ProcSharedBufDesc* pDesc)
{
    RC rc;

    rc = SharedSysmem::Alloc(pDesc->bufName, GetGfid(), pDesc->bufSize,
                             false, &pDesc->pBuf);
    if (rc != OK)
    {
        Printf(Tee::PriError,
               "SRIOV failed to create and map shared buffer for %s."
               "  Error message: %s.\n",
               pDesc->bufName.c_str(),
               rc.Message());
    }
    return rc;
}

RC VmiopElwPhysFun::CreateVmiopConfigValues()
{
    RC rc;

    CHECK_RC(CreateVmiopConfigValue(false, "debug",                  "0"));
    CHECK_RC(CreateVmiopConfigValue(false, "loglevel",               "0"));
    CHECK_RC(CreateVmiopConfigValue(false, "guest_bar1_length",
                                    "0x40"));       // TODO "0x100" 256m
    CHECK_RC(CreateVmiopConfigValue(false, "vgpu_type",              "lwdqro"));
    CHECK_RC(CreateVmiopConfigValue(false, "vdev_id",
                                    "0x213f:0"));   //TODO GetDID from GpuDev
    CHECK_RC(CreateVmiopConfigValue(false, "max_instance",
        Utility::StrPrintf("0x%x", GetVmiopElwMgr()->GetVfCount())));

    CHECK_RC(CreateVmiopConfigValue(false, "dev_instances",          "1"));
    CHECK_RC(CreateVmiopConfigValue(false, "sriov_enabled",          "1"));
    CHECK_RC(CreateVmiopConfigValue(false, "fb_scrubbing_enabled",   "0"));
    CHECK_RC(CreateVmiopConfigValue(false, "pte_blit_enabled",       "0"));
    CHECK_RC(CreateVmiopConfigValue(false, "frame_copy_engine",      "0"));
    CHECK_RC(CreateVmiopConfigValue(false, "use_per_vm_heap_memory", "1"));
    CHECK_RC(CreateVmiopConfigValue(true,  "gpu-pci-id",
        Utility::StrPrintf("%x:%x:%x.%x", GetPcieDomain(), GetPcieBus(),
                           GetPcieDevice(), GetPcieFunction())));

    // TODO update to work with multiple gpus
    const GpuSubdevice* pSubdev = GetVmiopElwMgr()->GetGpuSubdevInstance();
    const UINT64 pageSize   = Platform::GetPageSize();
    const UINT64 fbHeapSize = pSubdev->FbHeapSize();

    if (pSubdev->IsSMCEnabled() && pSubdev->UsingMfgModsSMC())
    {
        // Retrieve partition info
        const UINT32 partIdx = pSubdev->GetUsablePartIdx(GetSeqId() - 1);
        const auto&  part    = pSubdev->GetPartInfo(partIdx);
        const UINT32 swizzId = part.swizzId;
        const UINT64 partFbSizeBytes = part.memSize;
        const UINT32 engIdx  = pSubdev->GetSriovSmcEngineIdx(partIdx);
        MASSERT(engIdx < LW2080_CTRL_GPU_MAX_SMC_IDS);

        // Conservatively assume that we need 50MB of page tables per 16GB of framebuffer.
        // (this is copied from gpuutils.cpp)
        //
        // Reserve at least 10_MB since the scaling isn't quite linear
        //
        const UINT64 mapping16GB = 50_MB;
        UINT64 ptePdeSize = max((mapping16GB * (partFbSizeBytes)) / (1ULL << 34), 10_MB);
        ptePdeSize = ALIGN_UP(ptePdeSize, pageSize);

        // framebufferlength doesn't include the page tables
        // Make sure to also subtract out a little overhead for any FB allocated by the PF
        const UINT64 overheadBytes = 10_MB;
        const UINT64 fbSizeBytes = partFbSizeBytes - overheadBytes - ptePdeSize;

        // Set vmio plugin values
        CHECK_RC(CreateVmiopConfigValue(true, "swizzid",
            Utility::StrPrintf("0x%x", swizzId)));
        CHECK_RC(CreateVmiopConfigValue(true, "gr_idx",
            Utility::StrPrintf("0x%x", engIdx)));
        CHECK_RC(CreateVmiopConfigValue(true, "framebufferlength",
            Utility::StrPrintf("0x%llx", fbSizeBytes)));
        CHECK_RC(CreateVmiopConfigValue(true, "reserved_fb",
            Utility::StrPrintf("0x%llx", ptePdeSize)));

        Printf(Tee::PriNormal,
               "VMIO SeqID%d: partIdx %d swizzId 0x%x smcEngineIdx %d framebufferlength: %llu\n",
               GetSeqId(), partIdx, swizzId, engIdx, fbSizeBytes);
    }
    else
    {
        // Tell RM how much memory to reserve for PTEs/PDEs in VF.
        // Use 10% of the FB or 128 MB, whichever is smaller.
        // This is an absurd hack.
        //
        const UINT32 vfPfCount  = GetVmiopElwMgr()->GetVfCount() + 1;
        const UINT64 vfFbSize   = ALIGN_DOWN(fbHeapSize / vfPfCount, pageSize);
        const UINT64 ptePdeSize = ALIGN_DOWN(min<UINT64>(vfFbSize / 10, 128 << 20), pageSize);
        CHECK_RC(CreateVmiopConfigValue(true,  "framebufferlength",
            Utility::StrPrintf("0x%llx", vfFbSize - ptePdeSize)));
        CHECK_RC(CreateVmiopConfigValue(true, "reserved_fb",
            Utility::StrPrintf("0x%llx", ptePdeSize)));
    }
    return rc;
}

RC VmiopElwPhysFun::CreateVmiopConfigValue
(
    bool isPluginSpecific,
    const char *key,
    const string &value
)
{
    RC rc;
    if (isPluginSpecific)
    {
        string pluginKey = Utility::StrPrintf(
                VMIOP_CONFIG_PLUGIN_KEY(GetPluginHandle(), key));
        CHECK_RC(Registry::Write(VMIOP_CONFIG_NODE, pluginKey.c_str(), value));
    }
    else
    {
        CHECK_RC(Registry::Write(VMIOP_CONFIG_NODE, key, value));
    }
    return rc;
}

RC VmiopElwPhysFun::SetElw(Xp::Process * pProcess)
{
    RC rc;
    const UINT32 gfid = GetGfid();
    pProcess->SetElw("SRIOV_GFID", Utility::StrPrintf("%u", gfid));

    return rc;
}

void VmiopElwPhysFun::StartFlr()
{
    MASSERT(!IsExitStarted());
    m_bFlrStarted = true;
}

void VmiopElwPhysFun::StartExit()
{
    m_bExitStarted = true;
}

RC VmiopElwVirtFun::Initialize()
{
    RC rc;
    CHECK_RC(VmiopElw::Initialize());
    CHECK_RC(SetRemotePid(SharedSysmem::GetParentModsPid()));
    return rc;
}

RC VmiopElwVirtFun::SendReceivePcieBarInfo
(
    UINT32    barIndex,
    PHYSADDR* pBarBase,
    UINT64*   pBarSize
)
{
    MASSERT(Platform::IsVirtFunMode());
    MASSERT(barIndex < PcieBarInfo::NUM_BARS);

    void*       pBarInfo = m_ProcSharedBufs[PCIE_BAR_INFO].pBuf;
    const auto& bar      = static_cast<PcieBarInfo*>(pBarInfo)->bar[barIndex];

    *pBarBase = bar.base;
    *pBarSize = bar.size;

    Printf(Tee::PriDebug,
           "SRIOV VirtFun::%s: VF 0x%x PCIe Bar%u base 0x%llx size 0x%llx.\n",
           __FUNCTION__,
           GetGfid(),
           barIndex,
           *pBarBase,
           *pBarSize);
    return OK;
}

//--------------------------------------------------------------------
//! \brief Used to implement VmiopElwManager::FindVfEmuRegister
//!
RC VmiopElwVirtFun::FindVfEmuRegister(PHYSADDR physAddr, UINT32* pRegister)
{
    MASSERT(pRegister);
    const UINT32 fullBar0Size = 0x1000000; // 16M, for temporary WAR below

    // Cache the relevant ranges in BAR0 the first time this is called
    if (m_VgpuEmuRange[0] == 0)
    {
        PHYSADDR bar0;
        UINT64   bar0Size;
        RC rc;
        CHECK_RC(SendReceivePcieBarInfo(0, &bar0, &bar0Size));
        m_VgpuEmuRange[0]      = bar0 + DRF_BASE(LW_VGPU_EMU);
        m_VgpuEmuRange[1]      = bar0 + DRF_EXTENT(LW_VGPU_EMU);
        m_Bar0OverflowRange[0] = bar0 + bar0Size;
        m_Bar0OverflowRange[1] = bar0 + fullBar0Size - 1;
    }

    if (physAddr >= m_VgpuEmuRange[0] &&
        physAddr <= m_VgpuEmuRange[1])
    {
        *pRegister = (DRF_BASE(LW_VGPU_EMU) +
                      static_cast<UINT32>(physAddr - m_VgpuEmuRange[0]));
        return OK;
    }

    // Temporary WAR until all invalid register accesses in VF are
    // fixed: check whether the address falls within the full BAR0
    // window (16M) but outside the VF BAR0 window.  If so, then the
    // caller should ignore this register.
    //
    if (physAddr >= m_Bar0OverflowRange[0] &&
        physAddr <= m_Bar0OverflowRange[1])
    {
        *pRegister = (DRF_BASE(LW_VGPU_EMU) +
                      static_cast<UINT32>(physAddr - m_VgpuEmuRange[0]));
        Printf(Tee::PriWarn, "VF is trying to read PF register 0x%x\n",
               *pRegister);
        return RC::ILWALID_REGISTER_NUMBER;
    }

    // If we get here, the physAddr does not match any special range
    // on this VF
    //
    return RC::DEVICE_NOT_FOUND;
}

RC VmiopElwVirtFun::CallPluginRegWrite
(
    UINT32      dataOffset,
    UINT32      dataSize,
    const void* pData
)
{
    RC rc;
    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), dataOffset);

    RpcMailbox* pMailbox = nullptr;
    CHECK_RC(GetRpcMailbox(&pMailbox, dataSize));

    // Write shared memory to trigger request
    pMailbox->offset = dataOffset;
    pMailbox->size = dataSize;
    memcpy(&pMailbox->data[0], pData, dataSize);
    pMailbox->Commit(RpcMailbox::State::WRITE);

    CHECK_RC(WaitForRpc(pMailbox));

    Printf(Tee::PriDebug, "SRIOV %s dataSize = 0x%x %s = 0x%x\n",
           __FUNCTION__, dataSize,
           dataSize >= 4 ? "*(const UINT32*)pData" : "*(const UINT08*)pData",
           dataSize >= 4 ? *(const UINT32*)pData : *(const UINT08*)pData);

    return rc;
}

RC VmiopElwVirtFun::CallPluginRegRead
(
    UINT32 dataOffset,
    UINT32 dataSize,
    void*  pData
)
{
    RC rc;
    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), dataOffset);

    RpcMailbox* pMailbox = nullptr;
    CHECK_RC(GetRpcMailbox(&pMailbox, dataSize));

    // write shared memory to trigger request
    pMailbox->offset = dataOffset;
    pMailbox->size   = dataSize;
    pMailbox->Commit(RpcMailbox::State::READ);

    CHECK_RC(WaitForRpc(pMailbox));
    memcpy(pData, &pMailbox->data[0], dataSize); // Copy data back

    Printf(Tee::PriDebug, "SRIOV %s dataSize = 0x%x %s = 0x%x\n",
           __FUNCTION__, dataSize,
           dataSize >= 4 ? "*(const UINT32*)pData" : "*(const UINT08*)pData",
           dataSize >= 4 ? *(const UINT32*)pData : *(const UINT08*)pData);

    return rc;
}

RC VmiopElwVirtFun::MapProcSharedBuf(ProcSharedBufDesc* pDesc)
{
    RC rc;
    const UINT32 sessionId = GetVmiopElwMgr()->GetSessionId();
    CHECK_RC(SharedSysmem::Import(sessionId, pDesc->bufName, GetGfid(),
                                  false, &pDesc->pBuf, &pDesc->bufSize));
    return rc;
}

RC VmiopElwVirtFun::GetRpcMailbox(RpcMailbox **ppRpcMailbox, UINT32 dataSize)
{
    MASSERT(ppRpcMailbox);

    const ProcSharedBufDesc* pMailboxDesc = &m_ProcSharedBufs[RPC_MAILBOX];
    RpcMailbox* pMailbox = static_cast<RpcMailbox*>(pMailboxDesc->pBuf);
    if (!pMailbox)
    {
        MASSERT(!"RpcMailbox shared memory is missing");
        return RC::SOFTWARE_ERROR;
    }

    if (pMailbox->GetState() == RpcMailbox::State::ABORT)
    {
        // No more read/write allowed.
        return RC::RESET_IN_PROGRESS;
    }
    if (dataSize > RpcMailbox::DATA_MAX_SIZE)
    {
        Printf(Tee::PriError, "SRIOV %s failed on size check\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    *ppRpcMailbox = pMailbox;

    return RC::OK;
}

RC VmiopElwVirtFun::GetModsChan(ModsChannel **ppChan)
{
    MASSERT(ppChan);
    const ProcSharedBufDesc* pDesc = &m_ProcSharedBufs[MODS_CHAN];
    ModsChannel* pChan = static_cast<ModsChannel*>(pDesc->pBuf);
    if (!pChan)
    {
        MASSERT(!"MODS channel shared memory is missing");
        return RC::SOFTWARE_ERROR;
    }

    // If the PF has aborted, we can no longer send requests
    if (pChan->GetState() == ModsChannel::State::ABORT)
    {
        return RC::RESET_IN_PROGRESS;
    }

    *ppChan = pChan;
    return RC::OK;
}

// Log test status on VFs
RC VmiopElwVirtFun::CallPluginLogTestStart(UINT32 gfid, INT32 testNumber)
{
    RC rc;
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    ModsChannel::TestLog* pTestLog = reinterpret_cast<ModsChannel::TestLog*>(pChan->data);

    // Write shared memory to trigger request
    pChan->request          = ModsChannel::Request::LOG_TEST;
    pTestLog->gfid          = gfid;
    pTestLog->testNumber    = testNumber;
    pTestLog->bIsTestStart  = true;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    return WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs());
}

RC VmiopElwVirtFun::CallPluginLogTestEnd(UINT32 gfid, INT32 testNumber, UINT32 errorCode)
{
    RC rc;
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    ModsChannel::TestLog* pTestLog = reinterpret_cast<ModsChannel::TestLog*>(pChan->data);

    // Write shared memory to trigger request
    pChan->request          = ModsChannel::Request::LOG_TEST;
    pTestLog->gfid          = gfid;
    pTestLog->testNumber    = testNumber;
    pTestLog->errorCode     = errorCode;
    pTestLog->bIsTestStart  = false;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    return WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs());
}

RC VmiopElwVirtFun::CallPluginPfRegRead
(
    ModsGpuRegAddress regAddr,
    UINT32* pVal
)
{
    RC rc;
    MASSERT(pVal);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): register offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), static_cast<UINT32>(regAddr));

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    pChan->request = ModsChannel::Request::PF_REG_READ;
    pChan->size    = sizeof(ModsChannel::RegData);
    auto* pData    = reinterpret_cast<ModsChannel::RegData*>(pChan->data);
    pData->addr    = regAddr;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pVal = pData->value;

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): register data = 0x%x\n",
           __FUNCTION__, GetGfid(), pData->value);

    return rc;
}

RC VmiopElwVirtFun::CallPluginPfRegIsSupported
(
    ModsGpuRegAddress regAddr,
    bool* pIsSupported
)
{
    RC rc;
    MASSERT(pIsSupported);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): register offset = 0x%08x.\n",
           __FUNCTION__, GetGfid(), static_cast<UINT32>(regAddr));

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    using Payload      = ModsChannel::RegIsSupported;
    pChan->request     = ModsChannel::Request::PF_REG_IS_SUPPORTED;
    pChan->size        = sizeof(ModsChannel::RegIsSupported);
    auto* pData        = reinterpret_cast<Payload*>(pChan->data);
    pData->addr        = regAddr;
    pData->isSupported = false;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pIsSupported = pData->isSupported;

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u): register is supported = %s\n",
           __FUNCTION__, GetGfid(), Utility::ToString(*pIsSupported).c_str());

    return rc;
}

RC VmiopElwVirtFun::CallPluginPfRegTest32
(
    UINT32 domainIndex,
    ModsGpuRegField field,
    RegHal::ArrayIndexes regIndexes,
    UINT32 value,
    bool* pResult
)
{
    RC rc;
    MASSERT(pResult);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    using Payload  = ModsChannel::RegTest32RegField;
    pChan->request = ModsChannel::Request::PF_REG_TEST_32;
    pChan->size    = sizeof(Payload);
    auto* pData    = reinterpret_cast<Payload*>(pChan->data);
    pData->type    = ModsChannel::RegTest32::Variant::REG_FIELD;
    pData->domain  = domainIndex;
    pData->field   = field;

    if (regIndexes.size() > ModsChannel::RegTest32::MAX_REGHAL_INDEXES)
    {
        Printf(Tee::PriError, "Too many RegHal indices to pass to the PF."
               "\n\tgiven: %zu, max: %u\n", regIndexes.size(),
               ModsChannel::RegTest32::MAX_REGHAL_INDEXES);
        return RC::SOFTWARE_ERROR;
    }
    std::copy(regIndexes.begin(), regIndexes.end(), pData->indexes.begin());
    pData->numIndexes = static_cast<UINT32>(regIndexes.size());

    pData->value  = value;
    pData->result = false;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pResult = pData->result;

    return rc;
}

RC VmiopElwVirtFun::CallPluginPfRegTest32
(
    UINT32 domainIndex,
    ModsGpuRegValue value,
    RegHal::ArrayIndexes regIndexes,
    bool* pResult
)
{
    RC rc;
    MASSERT(pResult);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    using Payload  = ModsChannel::RegTest32RegValue;
    pChan->request = ModsChannel::Request::PF_REG_TEST_32;
    pChan->size    = sizeof(Payload);
    auto* pData    = reinterpret_cast<Payload*>(pChan->data);
    pData->type    = ModsChannel::RegTest32::Variant::REG_VALUE;
    pData->domain  = domainIndex;
    pData->value   = value;

    if (regIndexes.size() > ModsChannel::RegTest32::MAX_REGHAL_INDEXES)
    {
        Printf(Tee::PriError, "Too many RegHal indices to pass to the PF."
               "\n\tgiven: %zu, max: %u\n", regIndexes.size(),
               ModsChannel::RegTest32::MAX_REGHAL_INDEXES);
        return RC::SOFTWARE_ERROR;
    }
    std::copy(regIndexes.begin(), regIndexes.end(), pData->indexes.begin());
    pData->numIndexes = static_cast<UINT32>(regIndexes.size());

    pData->result  = false;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pResult = pData->result;

    return rc;
}

RC VmiopElwVirtFun::CallPluginSyncPfVfs(UINT32 timeoutMs, bool syncPf)
{
    RC rc;
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u) timeoutMs = %u, syncPf = %s.\n",
           __FUNCTION__, GetGfid(), timeoutMs, syncPf ? "true" : "false");

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // write shared memory to trigger request
    pChan->request   = ModsChannel::Request::SYNC_PF_VF;
    pChan->size      = sizeof(ModsChannel::SyncPfVfs);
    auto* pData      = reinterpret_cast<ModsChannel::SyncPfVfs*>(pChan->data);
    pData->timeoutMs = timeoutMs;
    pData->syncPf    = syncPf;
    CHECK_RC(pChan->Commit(ModsChannel::State::VF_REQUEST_SENT));

    CHECK_RC(WaitForModsChan(pChan, timeoutMs));

    Printf(Tee::PriDebug, "SRIOV %s(GFID %u) sync complete\n", __FUNCTION__, GetGfid());
    return rc;
}

RC VmiopElwVirtFun::CallPluginLocalGpcToHwGpc(UINT32 smcEngineIdx,
                                              UINT32 localGpcNum, UINT32* pHwGpcNum)
{
    RC rc;
    MASSERT(pHwGpcNum);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    pChan->request = ModsChannel::Request::LOCAL_GPC_TO_HW_GPC;
    pChan->size    = sizeof(ModsChannel::LocalGpcToHwGpc);
    auto* pData    = reinterpret_cast<ModsChannel::LocalGpcToHwGpc*>(pChan->data);
    pData->smcEngineIdx = smcEngineIdx;
    pData->localGpcNum  = localGpcNum;
    pChan->Commit(ModsChannel::State::VF_REQUEST_SENT);

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pHwGpcNum = pData->hwGpcNum;

    return rc;
}

RC VmiopElwVirtFun::CallPluginGetTpcMask(UINT32 hwGpcNum, UINT32* pTpcMask)
{
    RC rc;
    MASSERT(pTpcMask);
    Tasker::MutexHolder lock(m_pVfModsChannelMutex);

    ModsChannel* pChan = nullptr;
    CHECK_RC(GetModsChan(&pChan));

    // Write shared memory to trigger request
    pChan->request = ModsChannel::Request::GET_TPC_MASK;
    pChan->size    = sizeof(ModsChannel::GetTpcMask);
    auto* pData    = reinterpret_cast<ModsChannel::GetTpcMask*>(pChan->data);
    pData->hwGpcNum = hwGpcNum;
    pChan->Commit(ModsChannel::State::VF_REQUEST_SENT);

    CHECK_RC(WaitForModsChan(pChan, Tasker::GetDefaultTimeoutMs()));
    *pTpcMask = pData->tpcMask;

    return rc;
}

RC VmiopElwVirtFun::WaitForRpc(RpcMailbox *pMailbox)
{
    RC rc;
    MASSERT(pMailbox);

    // Wait service done
    RpcMailbox::State state = RpcMailbox::State::DONE;
    rc = Tasker::PollHw
    (
        Tasker::GetDefaultTimeoutMs(),
        [&]
        {
            state = pMailbox->GetState();
            return (state == RpcMailbox::State::DONE ||
                    state == RpcMailbox::State::ABORT ||
                    state == RpcMailbox::State::REQUEST_FAILED ||
                    !IsRemoteProcessRunning());
        },
        __FUNCTION__
    );
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "SR-IOV RPC request from VF->PF timed out!\n");
        return rc;
    }
    else if (state == RpcMailbox::State::ABORT)
    {
        return RC::RESET_IN_PROGRESS;
    }
    else if (state == RpcMailbox::State::REQUEST_FAILED)
    {
        return RC::SRIOV_REQUEST_FAILED;
    }

    return rc;
}

RC VmiopElwVirtFun::WaitForModsChan(ModsChannel *pChan, FLOAT64 timeoutMs)
{
    RC rc;
    MASSERT(pChan);
    MASSERT(Tasker::CheckMutexOwner(m_pVfModsChannelMutex.get()));

    // Wait service done
    ModsChannel::State state = ModsChannel::State::INIT;
    rc = Tasker::PollHw
    (
        (timeoutMs ? timeoutMs : Tasker::NO_TIMEOUT),
        [&]
        {
            state = pChan->GetState();
            return (state == ModsChannel::State::PF_REQUEST_SUCCESS ||
                    state == ModsChannel::State::PF_REQUEST_FAILURE ||
                    state == ModsChannel::State::ABORT ||
                    !IsRemoteProcessRunning());
        },
        __FUNCTION__
    );
    if (rc != RC::OK)
    {
        Printf(Tee::PriError, "SR-IOV MODS channel request from VF->PF timed out!\n");
        return rc;
    }
    else if (state == ModsChannel::State::PF_REQUEST_FAILURE)
    {
        return RC::SRIOV_REQUEST_FAILED;
    }
    else if (state == ModsChannel::State::ABORT)
    {
        return RC::RESET_IN_PROGRESS;
    }

    return RC::OK;
}
