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

#ifndef VMIOP_MDIAG_ELW_H
#define VMIOP_MDIAG_ELW_H

#include  <boost/interprocess/shared_memory_object.hpp>
#include  <boost/interprocess/mapped_region.hpp>

#include <vector>
#include <map>

#include "mdiag/utils/types.h"
#include "mdiag/utils/utils.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/lwrm.h"
#include "gpu/vmiop/vmiopelw.h"

#pragma warning(push)
#pragma warning(disable:4250)

class VmiopMdiagElwManager;

class VmiopMdiagElw : virtual public VmiopElw
{
public:
    VmiopMdiagElw(VmiopMdiagElwManager *pVmiopMdiagElwMgr,
                  UINT32 gfid, UINT32 seqId) :
        VmiopElw(pVmiopMdiagElwMgr, gfid, seqId) {}

    RC Initialize() override;
    RC Shutdown() override;

    virtual bool SendReceiveTestStage(UINT32* pTestID, MDiagUtils::TestStage* pStage) = 0;

    RC CheckReceiveMessageReady() const;
 
    RC SendProcMessage(UINT64 data_size, const void * message);
    RC ReadProcMessage(string& message);

protected:
    enum MdiagProcSharedBufType
    {
        PM_PROC_MESSAGE = NUM_PROC_SHARED_BUF_TYPES,
        TEST_SYNC,
        NUM_MDIAG_PROC_SHARED_BUF_TYPES // Must be last
    };

    struct PmProcMessage
    {
        enum Status
        {
            DONE = 0,
            SEND = 1
        };
        UINT32 status;
        UINT32 size;
        UINT08 data[1];
    };

    enum PmProcSharedMemoryType
    {
        RECEIVE_BUF = 0,
        SEND_BUF = 1
    };

    struct PmProcSharedMemoryInfo
    {
        // VF send message to PF, it will redict to index 0
        // VF read message from PF, it will redict to index 1
        // PF send message to VF, it will redict to index 1
        // PF read message from VF, it will redit to index 0
        const char * m_Name;
        std::array<char, 1024> m_pMessage;
        PmProcSharedMemoryInfo(const char * name) :
           m_Name(name)
        {}
        ~PmProcSharedMemoryInfo()
        {
            Printf(Tee::PriDebug, "MdiagVmiop: ProcSharedMemory destroy in %s.\n", __FUNCTION__);
        }
    };

    vector<PmProcSharedMemoryInfo> m_PmProcSharedMemoryInfos;

    VmiopMdiagElwManager *GetVmiopMdiagElwMgr() const { return static_cast<VmiopMdiagElwManager*>(GetVmiopElwMgr()); }
    virtual RC InitPmProcSharedMemoryInfo() = 0;

    // PF MODS & VF MODS processess shared memory lock/unlock.
    void ShmLock();
    void ShmUnlock();

    struct TestStatus
    {
        // Test ID from Trace3DTest supporting multi tests per VF MODS.
        UINT32 testID;
        MDiagUtils::TestStage stage;
    };

    static const size_t NumVFTestStatusData = 5 * MDiagUtils::NumTestStage;

    // Data buffers in PF/VF shared buffer used by MdiagElwMgr::TestSync.
    struct TestSyncBuf
    {
        MDiagUtils::ShmLock m_Lock;
        MDiagUtils::LoopBuffer<TestStatus, NumVFTestStatusData> m_Status;
        // Data buffer for MdiagElwMgr::TestSync tests sync-up
        // from both PF & VF sides.
        VmiopMdiagElwManager::TestSyncDataBuf m_DataBuf;

        void Init();
        void CleanUp();
    };
    TestSyncBuf* GetTestSyncSharedBuf();

    // PF MODS & VF MODS processess shared memory lock.
    MDiagUtils::ShmLock* m_pLock = nullptr;
    UINT32 m_pReceive = 0;
    UINT32 m_pSend = 0;
};

class VmiopMdiagElwPhysFun: public VmiopMdiagElw, public VmiopElwPhysFun
{
public:
    VmiopMdiagElwPhysFun(VmiopMdiagElwManager *pVmiopMdiagElwMgr,
                         UINT32 gfid,
                         UINT32 seqId,
                         UINT64 pluginDebugLevel,
                         UINT64 pluginDebugMask,
                         UINT64 fbReserveSizePdePte,
                         UINT64 fbLength,
                         UINT32 swizzid,
                         vector<UINT64> vmmuSetting):
        VmiopElw(pVmiopMdiagElwMgr, gfid, seqId),
        VmiopMdiagElw(pVmiopMdiagElwMgr, gfid, seqId),
        VmiopElwPhysFun(pVmiopMdiagElwMgr, gfid, seqId),
        m_PluginDebugLevel(pluginDebugLevel),
        m_PluginDebugMask(pluginDebugMask),
        m_FbReserveSizePdePte(fbReserveSizePdePte),
        m_FbLength(fbLength),
        m_Swizzid(swizzid),
        m_VmmuSetting(move(vmmuSetting))
    {
    }

    RC Initialize() override;

    virtual bool SendReceiveTestStage(UINT32* pTestID, MDiagUtils::TestStage* pStage);

protected:
    virtual RC CreateVmiopConfigValues() override;
    virtual RC InitPmProcSharedMemoryInfo() override;
    virtual RC SetElw(Xp::Process * pProcess) override;

private:
    const string FormatVmmuBitmaskString() const;
    const string FormatFrameBufferLengthString() const;
    const string FormatReservedPdePteString() const;
    const string FormatSwizzidString() const;

    UINT64 m_PluginDebugLevel;
    UINT64 m_PluginDebugMask;
    UINT64 m_FbReserveSizePdePte;
    UINT64 m_FbLength;
    UINT32 m_Swizzid;
    vector<UINT64> m_VmmuSetting;
};

class VmiopMdiagElwVirtFun: public VmiopMdiagElw, public VmiopElwVirtFun
{
public:
    VmiopMdiagElwVirtFun(VmiopMdiagElwManager *pVmiopMdiagElwMgr,
                         UINT32 gfid, UINT32 seqId) :
        VmiopElw(pVmiopMdiagElwMgr, gfid, seqId),
        VmiopMdiagElw(pVmiopMdiagElwMgr, gfid, seqId),
        VmiopElwVirtFun(pVmiopMdiagElwMgr, gfid, seqId)
    {
    }

    RC Initialize() override;

    virtual bool SendReceiveTestStage(UINT32* pTestID, MDiagUtils::TestStage* pStage);

protected:
    virtual RC InitPmProcSharedMemoryInfo() override;
};

#pragma warning(pop)
#endif
