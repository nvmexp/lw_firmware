/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTL_H
#define INCLUDED_UTL_H

#include <memory>
#include <stack>
#include "core/include/rc.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"

class ArgDatabase;
class ArgReader;
class LWGpuChannel;
class Test;
class GpuPartition;
class Trep;
class LWGpuTsg;
class MdiagSurf;
class TraceOp;
class UtlGpu;
class LWGpuResource;
class SmcEngine;
class UtlTest;

// This class represents the entry point from Mdiag for running UTL scripts.
// There should only ever be one instance of this class, which is created
// by calling the static function Utl::CreateInstance.
//
// IMPORTANT: this file can be included by source files that don't have
// access to either Python or pybind11 include files.  Do not make references
// to Python objects or APIs in this header file.  Python references can
// be used in utl.cpp, however.
//
// ALSO IMPORTANT: UTL lwrrently isn't supported on Windows because the compiler
// used to build Windows MODS is not compatible with the pybind11 header files.
// INCLUDE_MDIAGUTL is used to control whether to include UTL support or not.
// For Windows build, this environment variable will be set to False. 
//
// There are 2 implementations of utl.h. For Windows builds, the stub implementation
// will be used (utlstub.cpp). For Linux builds, utl.cpp will serve as the
// implementation. No inline functions should be used unless it supports in both the
// implementations. Any new function that is added to utl.h should be implemented in
// both utl.cpp and utlstub.cpp.
//
class Utl
{
public:
    // This enum is used to represent the different phases that Utl can be in.
    // Some functions are handled differently depending on the Utl phase.
    enum UtlPhase : UINT32
    {
        InitializationPhase = 0x01,
        ImportScriptsPhase  = 0x02,
        InitEventPhase      = 0x04,
        RunPhase            = 0x08,
        EndEventPhase       = 0x10
    };

    ~Utl();
    RC Init();
    RC Run();
    RC ReportTestResults();
    RC TriggerEndEvent();
    RC CheckScripts();
    RC PrintHelp();

    ArgDatabase* GetArgDatabase();
    Trep* GetTrep();
    void Abort();
    bool IsAborting() const;
    UtlPhase Phase() const;
    void SetPhase(UtlPhase phase);
    const ArgReader* GetArgReader();

    static bool IsSupported();
    static void CreateInstance
    (
        ArgDatabase* argDatabase,
        const ArgReader* mdiagArgs,
        Trep *trep,
        bool skipDevCreation
    );
    static Utl* Instance();
    static bool HasInstance();
    static void Shutdown();
    static void CreateUtlVfTests(const vector<VmiopMdiagElwManager::VFSetting> & vfSettings);
    char * ListAllParams();
    static bool ControlsTests();

    void AddTest(Test* test);
    void AddTestChannel(Test* test, LWGpuChannel* channel, const string& name, vector<UINT32>* pSubChCEInsts);
    void RemoveChannel(LWGpuChannel* channel);
    void AddTestTsg(Test* test, LWGpuTsg* tsg);
    void RemoveTsg(LWGpuTsg* lwgpuTsg);
    void AddTestSurface(Test* pTest, MdiagSurf* pSurface);
    void RemoveSurface(MdiagSurf* mdiagSurface);
    void RemoveTestSurface(Test* pTest, MdiagSurf* pSurface);
    void AddGpuSurface(GpuSubdevice* pGpuSubdevice, MdiagSurf* pSurface);
    vector<string>* GetScriptArgs();
    void TriggerSmcPartitionCreatedEvent(GpuPartition* pPartition);
    void TriggerTestMethodsDoneEvent(Test* pTest);
    RC TriggerTrace3dEvent(Test* pTest,  const string& eventName);
    void TriggerTraceFileReadEvent(Test* pTest, const string& fileName, void* data, size_t size);
    void TriggerTablePrintEvent(string&& tableHeader, vector<vector<string>>&& tableData, bool isDebug);
    void TriggerUserEvent(map<string, string>&& userData);
    bool IsCheckUtlScripts();
    void TriggerBeforeSendPushBufHeaderEvent(Test* pTest,  const string& pbName, UINT32 pbOffset);
    void TriggerTraceOpEvent(Test* pTest, TraceOp* pOp);
    void TriggerSurfaceAllocationEvent(MdiagSurf* surface, Test* test);
    void TriggerChannelResetEvent(LwRm::Handle hObject, LwRm* pLwRm);
    void TriggerTestCleanupEvent(Test* test);
    RC ReadTestSpecificScript(Test* test, string scriptParam, const string &scopeName = "");
    void TriggerTestInitEvent(Test* test);
    void TriggerVfLaunchedEvent(UINT32 gfid);
    void TriggerTestParseEvent(Test* test, vector<char> *pBuf);
    UINT32 GetScriptId();
    UtlTest* GetTestFromScriptId();
    void RegisterScriptIdToThread(UINT32 scriptId);
    void UnregisterScriptIdFromThread();
    void FreeSurfaceMmuLevels(MdiagSurf* pSurface);
    void FreePreResetRecoveryObjects();

    void InitVaSpace();
    void FreeVaSpace(UINT32 testId, LwRm * pLwRm);
    void FreeSubCtx(UINT32 testId);
    void AddGpuPartition(LWGpuResource *pLWGpuResource, GpuPartition* pGpuPartition);
    void AddSmcEngine(GpuPartition* pGpuPartition, SmcEngine* pSmcEngine);

    void SetUpdateSurfaceInMmu(const string & name);
    bool HasUpdateSurfaceInMmu(const string & name);

    RC Eval(const string & expression, const string & scope, Test * pTest);
    RC Exec(const string & expression, const string & scope, Test * pTest);

    // Delete these default constructors and assignment operators.
    // The UTL object should only be created by CreateInstance.
    Utl() = delete;
    Utl(Utl&) = delete;
    Utl& operator=(Utl&) = delete;

private:
    // Private constructor; the UTL object should be created by CreateInstance.
    Utl
    (
        ArgDatabase* argDatabase,
        const ArgReader* mdiagArgs,
        Trep* trep,
        bool skipDevCreation
    );

    RC SetupVfTests();

    void AddVaSpace(LwRm * pLwRm, UtlGpu * pGpu);

    void InitPython();
    RC ImportScripts();
    RC ImportScript(string scriptParam, UINT32 id, 
            const string & scopeName, 
            UtlTest * pTest);
    void InitEvent();

    ArgDatabase* m_ArgDatabase;
    const ArgReader* m_MdiagArgs;
    Trep* m_Trep = nullptr;
    bool m_IsAborting = false;
    UtlPhase m_Phase = InitializationPhase;
    bool m_PythonRunning = false;
    vector<string> m_LwrrentScriptArgs;
    map<UINT32, vector<string>> m_ScriptArgs;
    wchar_t* m_NewPythonHome = nullptr;

    // Use a void pointer here to avoid having a Python object in this file.
    // (See above.)
    void* m_ThreadState = nullptr;

    static unique_ptr<Utl> s_Instance;
    static vector<char> m_ArgVec;
    bool m_IsCheckUtlScripts = false;
    vector<string> m_OwnedSurfacesMmu;
    UINT32 m_NextScriptId = 1;
    map<UINT32, Test*> m_IdTestMap;
    map<Tasker::ThreadID, stack<UINT32>> m_ThreadIdToScriptId;
    map<string, string> m_ExtractPathMap;
    UINT32 m_TestExtractCount = 0;
};

inline Utl::UtlPhase operator|(Utl::UtlPhase a, Utl::UtlPhase b)
{
    return static_cast<Utl::UtlPhase>(static_cast<UINT32>(a) |
        static_cast<UINT32>(b));
}

#endif
