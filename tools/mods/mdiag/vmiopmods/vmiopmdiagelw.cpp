/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vmiopmdiagelwmgr.h"
#include "vmiopmdiagelw.h"
#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "mdiag/utils/utils.h"
#include "lwmisc.h"
#include "core/utility/sharedsysmem.h"
#include "vgpu/mods/vmiop-external-mods.h"
#include "core/include/lwrm.h"
#include "mdiag/advschd/policymn.h"
#include "mdiag/advschd/pmvftest.h"
#include "mdiag/smc/smcresourcecontroller.h"
#include "mdiag/smc/gpupartition.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "mdiag/lwgpu.h"
#include "mdiag/smc/smcengine.h"

RC VmiopMdiagElw::Initialize()
{
    RC rc;
    CHECK_RC(VmiopElw::Initialize());
    CHECK_RC(InitPmProcSharedMemoryInfo());

    CHECK_RC(CreateProcSharedBuf(PM_PROC_MESSAGE, "PmProcessMessages",
                                 sizeof(PmProcSharedMemoryInfo) *
                                 m_PmProcSharedMemoryInfos.size()));
    CHECK_RC(CreateProcSharedBuf(TEST_SYNC, "TestSync", sizeof(TestSyncBuf)));

    TestSyncBuf* pBuf = GetTestSyncSharedBuf();
    pBuf->Init();
    GetVmiopMdiagElwMgr()->LinkUpTestSyncBuf(this, &pBuf->m_DataBuf);
    m_pLock = &pBuf->m_Lock;

    return rc;
}

RC VmiopMdiagElw::Shutdown()
{
    RC rc;

    TestSyncBuf* pBuf = GetTestSyncSharedBuf();
    pBuf->CleanUp();
    m_pLock = 0;

    CHECK_RC(VmiopElw::Shutdown());
    return rc;
}

VmiopMdiagElw::TestSyncBuf* VmiopMdiagElw::GetTestSyncSharedBuf()
{
    ProcSharedBufDesc* pDesc = &m_ProcSharedBufs[TEST_SYNC];
    return static_cast<TestSyncBuf*>(pDesc->pBuf);
}

void VmiopMdiagElw::TestSyncBuf::Init()
{
    // Only PF does Init() and CleanUp().
    if (Platform::IsPhysFunMode())
    {
        m_Lock.Init();
        m_Status.Init();
        m_DataBuf.maxNumTests = 1;
    }
}

void VmiopMdiagElw::TestSyncBuf::CleanUp()
{
    // Only PF does Init() and CleanUp().
    if (Platform::IsPhysFunMode())
    {
        m_Lock.CleanUp();
    }
}

void VmiopMdiagElw::ShmLock()
{
    if (IsRemoteProcessRunning())
    {
        MASSERT(m_pLock != 0);
        m_pLock->Lock();
    }
}

void VmiopMdiagElw::ShmUnlock()
{
    MASSERT(m_pLock != 0);
    m_pLock->Unlock();
}

/* VmiopMdiagElwPhysFun */
/*****************************************************************************/
RC VmiopMdiagElwPhysFun::CreateVmiopConfigValues()
{
    RC rc;

    CHECK_RC(VmiopElwPhysFun::CreateVmiopConfigValues());
    CHECK_RC(CreateVmiopConfigValue(false, "debug",
                Utility::StrPrintf("0x%llx", m_PluginDebugLevel)));
    CHECK_RC(CreateVmiopConfigValue(false, "loglevel",
                Utility::StrPrintf("0x%llx", m_PluginDebugMask)));
    CHECK_RC(CreateVmiopConfigValue(true, "framebufferlength",
                FormatFrameBufferLengthString()));
    CHECK_RC(CreateVmiopConfigValue(true, "vmmu_seg_bitmask",
                FormatVmmuBitmaskString()));
    // used by host RM to put pTE/PDEs for VF.
    CHECK_RC(CreateVmiopConfigValue(false, "reserved_fb",
                FormatReservedPdePteString()));
    CHECK_RC(CreateVmiopConfigValue(true, "swizzid", FormatSwizzidString()));

    return rc;
}

RC VmiopMdiagElwPhysFun::InitPmProcSharedMemoryInfo()
{
    RC rc;
    m_pReceive = RECEIVE_BUF;
    m_pSend = SEND_BUF;

    m_PmProcSharedMemoryInfos =
    {
        {PmProcSharedMemoryInfo("RECEIVE_BUF")},
        {PmProcSharedMemoryInfo("SEND_BUF")}
    };

    return rc;
}

const string VmiopMdiagElwPhysFun::FormatReservedPdePteString() const
{
    return Utility::StrPrintf("0x%llx", m_FbReserveSizePdePte);
}

const string VmiopMdiagElwPhysFun::FormatFrameBufferLengthString() const
{
    return Utility::StrPrintf("0x%llx", m_FbLength);
}

const string VmiopMdiagElwPhysFun::FormatVmmuBitmaskString() const
{
    string vmmustring;
    vmmustring.erase();
    string(vmmustring).swap(vmmustring);
    bool firstChar = true;
    for (auto it = m_VmmuSetting.begin(); it != m_VmmuSetting.end(); ++it)
    {
        if (firstChar)
        {
            vmmustring = Utility::StrPrintf("%llx", *it);
            firstChar = false;
        }
        else
        {
            vmmustring = Utility::StrPrintf("%s:%llx", vmmustring.c_str(), *it);
        }
    }

    return vmmustring;
}

const string VmiopMdiagElwPhysFun::FormatSwizzidString() const
{
    return Utility::StrPrintf("0x%x", m_Swizzid);
}

RC VmiopMdiagElwPhysFun::Initialize()
{
    RC rc;
    CHECK_RC(VmiopMdiagElw::Initialize());
    CHECK_RC(VmiopElwPhysFun::Initialize());
    return rc;
}

RC VmiopMdiagElwPhysFun::SetElw(Xp::Process *pProcess)
{
    RC rc;
    
    CHECK_RC(VmiopElwPhysFun::SetElw(pProcess)); 

    GpuSubdevice* pGpuSubdevice =
        static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr)->GetFirstGpu(); 
    if (pGpuSubdevice->IsSMCEnabled())
    {
        LWGpuResource * pRes = LWGpuResource::FindFirstResource();
        SmcResourceController *pSmcResourceController= pRes->GetSmcResourceController();
        GpuPartition * pGpuPartition = pSmcResourceController->GetGpuPartition(m_Swizzid);

        // Send GpuPartition and SmcEngine names only when -smc_mem is used
        // This will allow VFs to distinguish when PF uses -smc_mem vs -smc_partitioning 
        if (pSmcResourceController->IsSmcMemApi())
        {
            pProcess->SetElw("SRIOV_SMC_PART_NAME", pGpuPartition->GetName());
            string smcEngineNames;

            for (auto const & pSmcEngine : pGpuPartition->GetActiveSmcEngines())
            {
                smcEngineNames += pSmcEngine->GetName() + " ";
            }
            pProcess->SetElw("SRIOV_SMC_SMCENGINE_NAMES", smcEngineNames);
        }
        
        vector<SmcResourceController::SYS_PIPE> syspipes = pGpuPartition->GetAllSyspipes();
        string sriovSys;
        for (auto syspipe : syspipes)
        {
            sriovSys += Utility::StrPrintf("sys%d", syspipe); 
        } 
        //ToDo - set the unfloorsweeping SYSPIPE here
        pProcess->SetElw("SRIOV_SMC_SYS", sriovSys);
    }

    return rc;

}

bool VmiopMdiagElwPhysFun::SendReceiveTestStage(UINT32* pTestID, MDiagUtils::TestStage* pStage)
{
    MASSERT(Platform::IsPhysFunMode());
    MASSERT(m_pLock != 0);

    TestSyncBuf* pBuf = GetTestSyncSharedBuf();
    TestStatus data;
    bool ok = false;

    ShmLock();
    ok = pBuf->m_Status.Pop(&data);
    ShmUnlock();
    if (ok)
    {
        *pTestID = data.testID;
        *pStage = data.stage;
    }

    return ok;
}

/* VmiopMdiagElwVirtFun */
/*****************************************************************************/

RC VmiopMdiagElwVirtFun::InitPmProcSharedMemoryInfo()
{
    RC rc;
    m_pReceive = SEND_BUF;
    m_pSend = RECEIVE_BUF;

    m_PmProcSharedMemoryInfos =
    {
        {PmProcSharedMemoryInfo("SEND_BUF")},
        {PmProcSharedMemoryInfo("RECEIVE_BUF")}
    };

    return rc;
}

RC VmiopMdiagElwVirtFun::Initialize()
{
    RC rc;
    CHECK_RC(VmiopMdiagElw::Initialize());
    CHECK_RC(VmiopElwVirtFun::Initialize());
    return rc;
}

RC VmiopMdiagElw::CheckReceiveMessageReady() const
{
    const ProcSharedBufDesc* pMailBoxDesc = &m_ProcSharedBufs[PM_PROC_MESSAGE];
    auto pmShMem_p = static_cast<PmProcSharedMemoryInfo *>(pMailBoxDesc->pBuf);
    if (!pmShMem_p)
    {
        ErrPrintf("SRIOV %s message buffer not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    auto pMessage = reinterpret_cast<PmProcMessage*>(
            pmShMem_p[m_pReceive].m_pMessage.data());
    return (pMessage->status == PmProcMessage::SEND) ? OK : RC::RESOURCE_IN_USE;
}

RC VmiopMdiagElw::SendProcMessage
(
    UINT64 data_size,
    const void * data_p
)
{
    RC rc;

    const ProcSharedBufDesc* pMailBoxDesc = &m_ProcSharedBufs[PM_PROC_MESSAGE];
    size_t mailBoxSize = pMailBoxDesc->bufSize / m_PmProcSharedMemoryInfos.size();
    PmProcSharedMemoryInfo* pmShMem_p =
        static_cast<PmProcSharedMemoryInfo *>(pMailBoxDesc->pBuf);
    if (!pmShMem_p)
    {
        ErrPrintf("SRIOV %s message buffer not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    PmProcMessage* pMessage = reinterpret_cast<PmProcMessage*>(
            pmShMem_p[m_pSend].m_pMessage.data());

    // write shared memory to trigger request
    pMessage->size = static_cast<UINT32>(data_size);

    if (sizeof(*pMessage) - sizeof(pMessage->data) + data_size > mailBoxSize)
    {
        ErrPrintf("SRIOV %s failed on size check %s\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // check whether the data in shared memory has been fetched
    Tasker::PollHw(Tasker::NO_TIMEOUT,
        [&] { return (pMessage->status == PmProcMessage::DONE ||
                      !IsRemoteProcessRunning()); },
        __FUNCTION__);

    memcpy(&pMessage->data[0], data_p, data_size);
    pMessage->status = PmProcMessage::SEND;

    return rc;
}

RC VmiopMdiagElw::ReadProcMessage(string& message)
{
    RC rc;

    const ProcSharedBufDesc* pMailBoxDesc = &m_ProcSharedBufs[PM_PROC_MESSAGE];
    size_t mailBoxSize = pMailBoxDesc->bufSize / m_PmProcSharedMemoryInfos.size();
    PmProcSharedMemoryInfo* pmShMem_p =
        static_cast<PmProcSharedMemoryInfo *>(pMailBoxDesc->pBuf);
    if (!pmShMem_p)
    {
        ErrPrintf("SRIOV %s message buffer not initialized\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    PmProcMessage* pMessage = reinterpret_cast<PmProcMessage*>(
            pmShMem_p[m_pReceive].m_pMessage.data());

    // Check whether shared memory has been written some data
    Tasker::PollHw(Tasker::NO_TIMEOUT,
        [&] { return (pMessage->status == PmProcMessage::SEND ||
                      !IsRemoteProcessRunning()); },
        __FUNCTION__);

    const UINT32 incoming_size = pMessage->size;

    if (sizeof(*pMessage) - sizeof(pMessage->data) + incoming_size >
        mailBoxSize)
    {
        ErrPrintf("SRIOV %s failed on size check\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    string msg(reinterpret_cast<char*>(&pMessage->data[0]));
    message = msg.substr(0, incoming_size);
    pMessage->status = PmProcMessage::DONE;

    return rc;
}

bool VmiopMdiagElwVirtFun::SendReceiveTestStage(UINT32* pTestID, MDiagUtils::TestStage* pStage)
{
    MASSERT(Platform::IsVirtFunMode());
    MASSERT(m_pLock!= 0);
    MASSERT(pTestID != 0 && pStage != 0);

    RC rc;

    TestStatus data;
    data.testID = *pTestID;
    data.stage = *pStage;

    TestSyncBuf* pBuf = GetTestSyncSharedBuf();
    bool ok = true;
    do {
        ShmLock();
        ok = pBuf->m_Status.Push(data);
        ShmUnlock();
        if (!ok)
        {
            Tasker::Yield();
        }
    } while (!ok && IsRemoteProcessRunning());

    return ok;
}
