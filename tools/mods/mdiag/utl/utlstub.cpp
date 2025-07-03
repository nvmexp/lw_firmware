/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// This file contains the stub implementation for utl.h. This implementation
// will be used in builds where UTL support is not included.

#include "utl.h"
#include "core/include/rc.h"

Utl::~Utl()
{
	// NOOP
}

RC Utl::Init() { return OK; }
RC Utl::Run() { return OK; }
RC Utl::ReportTestResults() { return OK; }
RC Utl::TriggerEndEvent() { return OK; }
RC Utl::CheckScripts() { return OK; }
RC Utl::PrintHelp() { return OK; }
ArgDatabase* Utl::GetArgDatabase() { return nullptr; }
Trep* Utl::GetTrep() { return nullptr; }
void Utl::Abort() { }
bool Utl::IsAborting() const { return false; }
Utl::UtlPhase Utl::Phase() const { return Utl::InitializationPhase; }
void Utl::SetPhase(UtlPhase phase) { }
const ArgReader* Utl::GetArgReader() { return nullptr; }

bool Utl::IsSupported() { return false; }
void Utl::CreateInstance
(
    ArgDatabase* argDatabase,
    const ArgReader* mdiagArgs,
    Trep *trep,
    bool skipDevCreation
)
{
    // NOOP
}

Utl* Utl::Instance() { return nullptr; }
bool Utl::HasInstance() { return false; }
void Utl::Shutdown() { }
void Utl::CreateUtlVfTests(const vector<VmiopMdiagElwManager::VFSetting> & vfSettings) { }
char * Utl::ListAllParams() { return nullptr; }
bool Utl::ControlsTests() { return false; }
void Utl::AddTest(Test* test) { }
void Utl::AddTestChannel(Test* test, LWGpuChannel* channel, const string& name, vector<UINT32>* pSubChCEInsts) { }
void Utl::RemoveChannel(LWGpuChannel* channel) { }
void Utl::AddTestTsg(Test* test, LWGpuTsg* tsg) { }
void Utl::RemoveTsg(LWGpuTsg* lwgpuTsg) { }
void Utl::AddTestSurface(Test* pTest, MdiagSurf* pSurface) { }
void Utl::RemoveSurface(MdiagSurf* mdiagSurface) { }
void Utl::RemoveTestSurface(Test* pTest, MdiagSurf* pSurface) { }
void Utl::AddGpuSurface(GpuSubdevice* pGpuSubdevice, MdiagSurf* pSurface) { }
vector<string>* Utl::GetScriptArgs() { return nullptr; }
void Utl::TriggerSmcPartitionCreatedEvent(GpuPartition* pPartition) { }
void Utl::TriggerTestMethodsDoneEvent(Test* pTest) { }
RC Utl::TriggerTrace3dEvent(Test* pTest,  const string& eventName) { return OK; }
void Utl::TriggerTraceFileReadEvent(Test* pTest, const string& fileName, void* data, size_t size) { }
void Utl::TriggerTablePrintEvent(string&& tableHeader, vector<vector<string>>&& tableData, bool isDebug) { }
void Utl::TriggerUserEvent(map<string, string>&& userData) { }
bool Utl::IsCheckUtlScripts() { return false; }
void Utl::TriggerBeforeSendPushBufHeaderEvent(Test* pTest,  const string& pbName, UINT32 pbOffset) { }
void Utl::TriggerTraceOpEvent(Test* pTest, TraceOp* pOp) { }
void Utl::TriggerSurfaceAllocationEvent(MdiagSurf* surface, Test* test) { }
void Utl::TriggerChannelResetEvent(LwRm::Handle hObject, LwRm* pLwRm) { }
void Utl::TriggerTestCleanupEvent(Test* test) { }
RC Utl::ReadTestSpecificScript(Test* test, string scriptParam, const string &scopeName) { return OK; }
void Utl::TriggerTestInitEvent(Test* test) { }
void Utl::TriggerVfLaunchedEvent(UINT32 gfid) { }
void Utl::TriggerTestParseEvent(Test* test, vector<char> *pBuf) { }
UINT32 Utl::GetScriptId() { return 0; }
UtlTest* Utl::GetTestFromScriptId() { return nullptr; }
void Utl::RegisterScriptIdToThread(UINT32 scriptId) { }
void Utl::UnregisterScriptIdFromThread() { }
void Utl::FreeSurfaceMmuLevels(MdiagSurf* pSurface) { }
void Utl::FreePreResetRecoveryObjects() { }

void Utl::InitVaSpace() { }
void Utl::FreeVaSpace(UINT32 testId, LwRm * pLwRm) { }
void Utl::FreeSubCtx(UINT32 testId) { }
void Utl::AddGpuPartition(LWGpuResource *pLWGpuResource, GpuPartition* pGpuPartition) { }
void Utl::AddSmcEngine(GpuPartition* pGpuPartition, SmcEngine* pSmcEngine) { }

void Utl::SetUpdateSurfaceInMmu(const string & name) { }
bool Utl::HasUpdateSurfaceInMmu(const string & name) { return false; }

RC Utl::Eval(const string & expression, const string & scope, Test * pTest) { return OK; }
RC Utl::Exec(const string & expression, const string & scope, Test * pTest) { return OK; }

RC Utl::SetupVfTests() { return OK; }

void Utl::AddVaSpace(LwRm * pLwRm, UtlGpu * pGpu) { }

void Utl::InitPython() { }
RC Utl::ImportScripts() { return OK; }
RC Utl::ImportScript(string scriptParam, UINT32 id, 
        const string & scopeName, 
        UtlTest * pTest) { return OK; }
void Utl::InitEvent() { }
