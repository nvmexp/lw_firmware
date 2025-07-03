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

#pragma once
#ifndef VMIOPELWMGR_H
#define VMIOPELWMGR_H

#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/vmiop/vmiopelw.h"
#include <list>
#include <map>
#include <memory>
class  ArgDatabase;
class  GpuSubdevice;
struct VmiopModsCallbacks;
class  SObject;
extern SObject VmiopElwManager_Object;

class VmiopElwManager
{
public:
    static VmiopElwManager* Instance();
    static VmiopElwManager* GetInstance() { return s_pInstance.get(); }
    virtual ~VmiopElwManager() {}

    void   SetConfigFile(const string& file) { m_ConfigFile = file; }
    const string& GetConfigFile() const      { return m_ConfigFile; }
    void   SetVfCount(UINT32 vfCount)        { m_VfCount = vfCount; }
    UINT32 GetVfCount() const                { return m_VfCount; }
    UINT32 GetVfProcessCount() const
        { return static_cast<UINT32>(m_VfConfigs.size()); }

    virtual RC Init();
    virtual void ShutDown();
    virtual void ShutDownMinimum();
    bool IsInitialized() const { return m_IsInitialized; }

    UINT32 GetSessionId() const { return m_SessionId; }
    VmiopElw* GetVmiopElw(UINT32 gfid) const;
    VmiopElw::PcieBarInfo GetBarInfo(UINT32 gfid) const;

    UINT32 GetGfidAttachedToProcess() const { return m_GfidAttachedToProcess; }
    RC CreateGfidToBdfMapping(UINT32 domain, UINT32 bus,
                              UINT32 device, UINT32 function);
    RC GetDomainBusDevFun(UINT32 gfid, UINT32* pDomain, UINT32* pBus,
                          UINT32* pDevice, UINT32* pFunction);
    RC FindVfEmuRegister(PHYSADDR physAddr,
                         VmiopElw** ppVmiopElw, UINT32* pRegister);

    struct VfConfig    // Settings read by ParseVfConfigFile (PF only)
    {
        string vftestDir;
        string vftestFileName;
        UINT32 seqId;

        UINT32 gfid;
    };

    struct PluginHandlerParam
    {
        VfConfig            vfConfig;
        UINT32              pluginHandle;
        VmiopModsCallbacks* pModsCallbacks;
        StickyRC            testRc;
        // Unload vGpu plugin before or after VmiopElw exit.
        // If earlyPluginUnload, it will unload plugin before VmiopElw exit.
        bool                earlyPluginUnload;
    };
    PluginHandlerParam* GetPluginHandlerParam(UINT32 gfid);

    virtual RC SetupAllVfTests(ArgDatabase*);
    virtual RC RulwfTests()  { return RulwfTests(m_VfConfigs); }
    RC RulwfTests(const vector<VfConfig>& vfConfigs);
    RC WaitPluginThreadDone(UINT32 gfid);
    RC WaitAllPluginThreadsDone();
    RC SyncPfVfs(UINT32 timeoutMs, bool syncPf, bool handlerTerminated, bool pfTestRunComplete);
    bool AreAllVfsWaiting();

    // Unload vGpu plugin before or after VmiopElw exit.
    // If EarlyPluginUnload, it will unload plugin before VmiopElw exit.
    void SetEarlyPluginUnload() { m_bEarlyPluginUnload = true; }
    bool IsEarlyPluginUnload() const { return m_bEarlyPluginUnload; }
    RC SetGpuSubdevInstance(GpuSubdevice *pGpuSubdevice);
    GpuSubdevice* GetGpuSubdevInstance() const { return m_pGpuSubdevice; }

    // TestLogging code to keep track of VF tests
    RC LogTestStart(UINT32 gfid, INT32 testNumber);
    RC LogTestComplete(UINT32 gfid, INT32 testNumber, UINT32 errorCode);
    void PrintVfTestLog();

protected:
    struct ConstructorTag {};  // Prevents calling constructor publicly
public:
    explicit VmiopElwManager(ConstructorTag) {}
protected:
    VmiopElwManager()                                  = delete;
    VmiopElwManager(const VmiopElwManager&)            = delete;
    VmiopElwManager& operator=(const VmiopElwManager&) = delete;
    static unique_ptr<VmiopElwManager> s_pInstance;
    virtual unique_ptr<VmiopElw> AllocVmiopElwPhysFun(UINT32 gfid,
                                                      UINT32 seqId);
    virtual unique_ptr<VmiopElw> AllocVmiopElwVirtFun(UINT32 gfid,
                                                      UINT32 seqId);
    virtual RC RemoveVmiopElw(UINT32 gfid);
    virtual RC ConfigGfids();

    vector<VfConfig> m_VfConfigs;
    map<UINT32, unique_ptr<VmiopElw>> m_GfidToVmiopElw;

private:
    RC ParseVfConfigFile();
    RC SetupVmiopElw(UINT32 gfid, UINT32 seqId);
    static void PFHandler(void* pArgs);
    RC PFHandlerImpl(PluginHandlerParam* pParam);
    RC ReleaseWaitingPfVfs(bool handlerTerminated, bool pfTestRunComplete);

    void ExitVmiopElw(PluginHandlerParam* pParam, bool bRemoteStarted);

    // For now, only one GFID per process
    UINT32 m_GfidAttachedToProcess = _UINT32_MAX;

    struct Bdf
    {
        UINT32 domain;
        UINT32 bus;
        UINT32 device;
        UINT32 function;
    };
    map<UINT32, Bdf> m_GfidToBdf; // GFID includes GPU instance in high bits

    struct VfTestState
    {
        UINT32 firstError;
        INT32 lwrrTest;
        vector<pair<INT32, UINT32>> completedTests;
    };
    map<UINT32, VfTestState> m_VfTestLog;

    bool   m_IsInitialized = false;
    UINT32 m_SessionId = ~0;
    UINT32 m_VfCount   = 0;
    UINT32 m_WaitingPfVfsCount = 0;
    UINT32 m_RunningVfsHandlerCount = 0;
    bool m_SyncPfRequested = false;
    bool m_HasPfCompletedTestRun = false;
    string m_ConfigFile;
    map<UINT32, VmiopElw::PcieBarInfo> m_GfidToBarInfo;
    list<PluginHandlerParam> m_PluginHandlerParams;
    map<UINT32, Tasker::ThreadID> m_PluginThreads; //!< Threads indexed by gfid
    bool m_bEarlyPluginUnload = false;
    GpuSubdevice* m_pGpuSubdevice = nullptr;
    Tasker::Mutex m_SyncMutex = Tasker::CreateMutex("SyncMutex", Tasker::mtxUnchecked);
    ModsCondition m_SyncCond;
};

#endif
