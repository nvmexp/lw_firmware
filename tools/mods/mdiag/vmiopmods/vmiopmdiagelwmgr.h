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

#ifndef VMIOP_MDIAG_ELW_MANAGER_SIM_H
#define VMIOP_MDIAG_ELW_MANAGER_SIM_H
#include <map>
#include <list>
#include "mdiag/utils/types.h"
#include "mdiag/utils/utils.h"
#include "core/include/rc.h"
#include "core/include/tasker.h"
#include "core/include/lwrm.h"
#include "core/include/argdb.h"
#include "gpu/vmiop/vmiopelwmgr.h"
#include "turing/tu106/dev_lw_xve.h"

// Tag shows up in log prints for finding out related debugging info easily.
#define SRIOVTestSyncTag       "[SRIOV TestSync] "

class VmiopMdiagElw;

class VmiopMdiagElwManager : public VmiopElwManager
{
public:
    explicit VmiopMdiagElwManager(ConstructorTag tag)
    : VmiopElwManager(tag) {}

    static VmiopMdiagElwManager * Instance();
    static VmiopMdiagElwManager * GetInstance();

    RC Init() override;
    void ShutDown() override;

    RC SetupAllVfTests(ArgDatabase * pArgs) override;

    VmiopMdiagElw* GetVmiopMdiagElw(UINT32 gfid) const;

    struct VFSetting
    {
        VfConfig* pConfig;
        vector<UINT64> vmmuSetting;
        UINT64 fbLength;
        UINT64 fbReserveSize;
        UINT64 fbReserveSizePdePte;
        UINT64 debugLevel;
        UINT64 debugMask;
        UINT32 swizzid;
    };

    vector<VFSetting *>  GetVFSetting(UINT32 GFID);

    RC RulwfTests() override;
    RC RulwfTests(const vector<VfConfig>& vfConfigs, vector<VFSetting*>& vfSettings);

    // Data buffer for tests sync-up from both PV & VF sides.
    // The data buffer may be in PF MODS & VF MODS processes shared buffer.
    struct TestSyncDataBuf
    {
        // Max number of VF tests or tests in PF MODS.
        size_t maxNumTests;
        // Blocked or unblocked status for each test stage.
        bool stageStatus[MDiagUtils::NumTestStage];
        bool needSync;
    };

    // Link up each MdiagElw's tests sync-up buffer to TestSync for use.
    // The buffers are in shared memory which PF/VF MdiagElw manages.
    void LinkUpTestSyncBuf(VmiopMdiagElw* pElw, TestSyncDataBuf* pBuf);
    void TestSynlwp(UINT32 testID, const MDiagUtils::TestStage stage);
    void SetNumT3dTests(size_t num);
    void StopTestSync();

    void ConfigFrameBufferLength(VFSetting * pVfSetting);
    UINT64 GetVmmuSegmentSize();
    RC SendVmmuInfo();

protected:
    unique_ptr<VmiopElw> AllocVmiopElwPhysFun(UINT32 gfid,
                                              UINT32 seqId) override;
    unique_ptr<VmiopElw> AllocVmiopElwVirtFun(UINT32 gfid,
                                              UINT32 seqId) override;
    RC RemoveVmiopElw(UINT32 gfid) override;
    RC ConfigGfids()               override;

private:
    RC RegisterToPolicyManager();
    RC RegisterToUtl();

    vector<VFSetting> m_VFSettings;
    RC ParseCommandLine(ArgDatabase * pArgs);
    RC ConfigVmmu();
    RC ConfigPluginDebugSettings();
    UINT64 GetFBLength(UINT32 swizzid);
    RC ConfigSwizzid();
    RC ConfigVf();
    RC SanityCheckVmmu();

    const UINT32 UNKOWN_GFID = ~0x0;

    // Each MdiagElwMgr has a local TestSyncData, and one
    // remote TestSyncData per each PF or VF MdiagElw.
    class TestSyncData
    {
    public:
        TestSyncData()
        {
            InitPendingTests();
        }
        void InitStages(bool needSync,
            const vector<MDiagUtils::TestStage>& stageList)
        {
            ClearStages();
            m_pDataBuf->needSync = needSync;
            for (const auto& stage : stageList)
            {
                m_pDataBuf->stageStatus[stage] = needSync;
            }
        }
        void Clear()
        {
            ClearStages();
            ClearPendingTests();
        }
        // Link up data buffer which may be from MdiagElw shared memory.
        void LinkUpDataBuf(TestSyncDataBuf* pBuf) { m_pDataBuf = pBuf; }

        bool IsPending(const MDiagUtils::TestStage stage) const
        {
            return m_pDataBuf->stageStatus[stage];
        }
        void Unblock(const MDiagUtils::TestStage stage)
        {
            m_pDataBuf->stageStatus[stage] = false;
        }
        void SetMaxNumTests(size_t num)
        {
            m_pDataBuf->maxNumTests = num;
        }
        void AddTest(const MDiagUtils::TestStage stage);

        virtual bool ReadyForRun(const MDiagUtils::TestStage stage) const;

    private:
        void ClearStages()
        {
            m_pDataBuf->needSync = false;
            fill(&m_pDataBuf->stageStatus[0],
                &m_pDataBuf->stageStatus[sizeof(m_pDataBuf->stageStatus) / sizeof(m_pDataBuf->stageStatus[0])],
                false);
        }

        void InitPendingTests()
        {
            ClearPendingTests();
        }
        void ClearPendingTests()
        {
            fill(&m_Tests[0], &m_Tests[sizeof(m_Tests) / sizeof(m_Tests[0])], 0);
        }

        size_t m_Tests[MDiagUtils::NumTestStage];
        // Per remote TestSyncData, the data buffer is from
        // shared memory which MdiagElw manages.
        TestSyncDataBuf* m_pDataBuf = 0;
    };

    class TestSyncRemoteData
        : public TestSyncData
    {
    public:
        virtual bool ReadyForRun(const MDiagUtils::TestStage stage) const;

        void SetGFID(UINT32 gfid) { m_GFID = gfid; }
        UINT32 GetGFID() const { return m_GFID; }
        void ReceiveStage();
        void SendStage(UINT32 testID, const MDiagUtils::TestStage stage);

    private:
        // The corresponding PF or VF MdiagElw ID if remote data.
        UINT32 m_GFID = 0;
    };

    class TestSync
    {
    public:
        TestSync();
        void CleanUp() { m_RemoteData.clear(); }
        void Disable() { m_NeedSync = false; }
        void MakeLocalData();
        TestSyncData* GetLocalData();
        TestSyncRemoteData* MakeRemoteData(UINT32 gfid, TestSyncDataBuf* pBuf);
        void SetRemoteMaxNumTests(size_t maxNum);
        void Synlwp(UINT32 testID, const MDiagUtils::TestStage stage);
        void Stop();
        void RemoveRemoteData(UINT32 gfid);

    private:
        bool ReadyForRun(const MDiagUtils::TestStage stage);
        void Unblock(const MDiagUtils::TestStage stage);
        void UpdateLocalData(UINT32 testID, const MDiagUtils::TestStage stage);
        void UpdateRemoteData(UINT32 testID, const MDiagUtils::TestStage stage);
        void SynlwpAll(const MDiagUtils::TestStage stage);

        TestSyncDataBuf m_LocalDataBuf;
        TestSyncData m_LocalData;
        // Remote data per GFID.
        map<UINT32, TestSyncRemoteData> m_RemoteData;
        vector<MDiagUtils::TestStage> m_StageList;
        bool m_NeedSync = true;
    };

    TestSync m_TestSync;

    string m_SriovGfids;
    string m_SriovVmmus;
    string m_PluginDebugSettings;
    string m_GFIDGpuPartitioningMappings;

    // FB reserved PDE/PTE value to be set by commandline argument. Size in MB.
    UINT32 m_OverrideFbReservedPteSizeMB = 0;
};

#endif

