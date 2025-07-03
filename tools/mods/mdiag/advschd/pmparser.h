/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PMPARSER_H
#define INCLUDED_PMPARSER_H

#ifndef INCLUDED_JSAPI_H
#include "jsapi.h"
#define INCLUDED_JSAPI_H
#endif

#ifndef INCLUDED_STL_MAP
#include <map>
#define INCLUDED_STL_MAP
#endif

#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_REGEXP_H
#include "core/include/regexp.h"
#endif

#ifndef INCLUDED_STL_STACK
#include <stack>
#define INCLUDED_STL_STACK
#endif

#ifndef INCLUDED_FPICKER_H
#include "core/include/fpicker.h"
#endif

class PmAction_Goto;
class PmTrigger_OnMethodExelwte;
class PmTrigger_OnPercentMethodsExelwted;

//--------------------------------------------------------------------
//! \brief Class that parses a policy file (.pcy file)
//!
//! This class parses a policy file.  It uses the JavaScript parser to
//! do much of the front-end work: the Parse() method ilwokes the file
//! as javascript, and as each javascript statement
//! (e.g. Policy.Start()) is exelwted, the JS->C++ glue code in
//! js_pcyfl.cpp call a corresponding token-processing method in this
//! class (e.g. PolicyStartToken()).
//!
//! Most of the token-processing methods take jsvals as arguments.
//! Note that it is unusual for C++ code to handle jsvals directly
//! (usually we handle that in the JS -> C++ glue code) However, in
//! the case of this parser it seems cleaner to take care of the
//! number of arguments in the glue code but leave the interpretation
//! of those arugments to the parser code.
//!
class PmParser
{
public:
    PmParser(PolicyManager *pPolicyManager);

    RC Parse(const string &fileName);
    string GetFileName() const { return m_FileName; }
    void SetErrInfo(JSContext *pContext);
    string GetErrInfo() const;
    bool PreProcessToken(JSContext *pContext, UINT32 correctNumArgs,
                         UINT32 actualNumArgs, const char *usage);
    JSBool PostProcessToken(JSContext *pContext, jsval *pReturlwalue,
                            JSBool status, const char *usage);

    JSBool Policy_Version(jsval version, jsval *pReturlwalue);
    JSBool Policy_Define(jsval trigger, jsval action, jsval *pReturlwalue);
    JSBool Policy_SetOutOfBand(jsval *pReturlwalue);
    JSBool Policy_SetInband(jsval *pReturlwalue);
    JSBool Policy_SetOutOfBandMemOp(jsval *pReturlwalue);
    JSBool Policy_SetInbandMemOp(jsval *pReturlwalue);
    JSBool Policy_SetOutOfBandPte(jsval *pReturlwalue);
    JSBool Policy_SetInbandPte(jsval *pReturlwalue);
    JSBool Policy_SetOutOfBandSurfaceMove(jsval *pReturlwalue);
    JSBool Policy_SetInbandSurfaceMove(jsval *pReturlwalue);
    JSBool Policy_SetOutOfBandSurfaceClear(jsval *pReturlwalue);
    JSBool Policy_SetInbandSurfaceClear(jsval *pReturlwalue);
    JSBool Policy_SetInbandChannel(jsval channelArg, jsval *pReturlwalue);
    JSBool Policy_SetOptimalAlloc(jsval *pReturlwalue);
    JSBool Policy_SetFramebufferAlloc(jsval *pReturlwalue);
    JSBool Policy_SetCoherentAlloc(jsval *pReturlwalue);
    JSBool Policy_SetNonCoherentAlloc(jsval *pReturlwalue);
    JSBool Policy_SetPhysContig(jsval *pReturlwalue);
    JSBool Policy_ClearPhysContig(jsval *pReturlwalue);
    JSBool Policy_SetAlignment(jsval alignment, jsval *pReturlwalue);
    JSBool Policy_ClearAlignment(jsval *pReturlwalue);
    JSBool Policy_SetDualPageSize(jsval *pReturlwalue);
    JSBool Policy_ClearDualPageSize(jsval *pReturlwalue);
    JSBool Policy_SetLoopBack(jsval *pReturlwalue);
    JSBool Policy_ClearLoopBack(jsval *pReturlwalue);
    JSBool Policy_SetDeleteMovedSurfaces(jsval *pReturlwalue);
    JSBool Policy_SetScrambleMovedSurfaces(jsval *pReturlwalue);
    JSBool Policy_SetDumpMovedSurfaces(jsval *pReturlwalue);
    JSBool Policy_SetCrcMovedSurfaces(jsval *pReturlwalue);
    JSBool Policy_SetLwrrentPageSize(jsval *pReturlwalue);
    JSBool Policy_SetAllPageSizes(jsval *pReturlwalue);
    JSBool Policy_SetBigPageSize(jsval *pReturlwalue);
    JSBool Policy_SetSmallPageSize(jsval *pReturlwalue);
    JSBool Policy_SetHugePageSize(jsval *pReturlwalue);
    JSBool Policy_SetPhysicalPageSize(jsval physPageSize,
                                      jsval *pReturlwalue);
    JSBool Policy_SetPowerWaitModelUs(jsval aPowerWaitModelUs,
                                      jsval *pReturlwalue);
    JSBool Policy_SetPowerWaitRTLUs(jsval aPowerWaitRTLUs,
                                    jsval *pReturlwalue);
    JSBool Policy_SetPowerWaitHWUs(jsval aPowerWaitHWUs,
                                   jsval *pReturlwalue);
    JSBool Policy_SetPowerWaitBusy(jsval *pReturlwalue);
    JSBool Policy_SetPowerWaitSleep(jsval *pReturlwalue);
    JSBool Policy_SetPStateFallbackError(jsval *pReturlwalue);
    JSBool Policy_SetPStateFallbackHigher(jsval *pReturlwalue);
    JSBool Policy_SetPStateFallbackLower(jsval *pReturlwalue);
    JSBool Policy_SetReqEventWaitMs(jsval aReqEventWaitMs,
                                    jsval *pReturlwalue);
    JSBool Policy_SetReqEventEnableSimTimeout(jsval aReqEventEnableSimTimeout,
                                              jsval *pReturlwalue);
    JSBool Policy_SetRandomSeedDefault(jsval *pReturlwalue);
    JSBool Policy_SetRandomSeedTime(jsval *pReturlwalue);
    JSBool Policy_SetRandomSeed(jsval aReqEventWaitMs,
                                jsval *pReturlwalue);
    JSBool Policy_SetClearNewSurfacesOn(jsval *pReturlwalue);
    JSBool Policy_SetClearNewSurfacesOff(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateTarget(jsval aperture, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidatePdbAll(jsval flag, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateGpc(jsval flag, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayNone(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayStart(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayCancelTargeted(jsval aGPCID,
                                                       jsval aClientUnitID,
                                                       jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayCancelGlobal(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayCancelVaGlobal(jsval aAccessType,
                                                       jsval aVEID,
                                                       jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateReplayStartAckAll(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateSysmembar(jsval aFlag, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateAckTypeNone(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateAckTypeGlobally(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateAckTypeIntranode(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateChannelPdb(jsval channelArg, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateBar1(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateBar2(jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateLevel(jsval aLevelNum, jsval *pReturlwalue);
    JSBool Policy_SetTlbIlwalidateIlwalScope(jsval aIlwalScope, jsval *pReturlwalue);
    JSBool Policy_SetGpuCacheable(jsval *pReturlwalue);
    JSBool Policy_SetNonGpuCacheable(jsval *pReturlwalue);
    JSBool Policy_SetDefaultGpuCacheMode(jsval *pReturlwalue);
    JSBool Policy_SetChannelSubdeviceMask(jsval mask, jsval *pReturlwalue);
    JSBool Policy_SetChannelAllSubdevicesMask(jsval *pReturlwalue);
    JSBool Policy_SetSurfaceAccessRemoteGpu(jsval  gpuDesc,
                                            jsval *pReturlwalue);
    JSBool Policy_SetSurfaceAccessLocal(jsval *pReturlwalue);
    JSBool Policy_SetSemaphoreReleaseWithTime(jsval *pReturlwalue);
    JSBool Policy_SetSemaphoreReleaseWithoutTime(jsval *pReturlwalue);
    JSBool Policy_SetSemaphoreReleaseWithWFI(jsval *pReturlwalue);
    JSBool Policy_SetSemaphoreReleaseWithoutWFI(jsval *pReturlwalue);
    JSBool Policy_SetBlockingCtxSwInt(jsval *pReturlwalue);
    JSBool Policy_SetNonBlockingCtxSwInt(jsval *pReturlwalue);
    JSBool Policy_SetDeferTlbIlwalidate(jsval flag, jsval *pReturlwalue);
    JSBool Policy_DisablePlcOnSurfaceMove(jsval flag, jsval *pReturlwalue);
    JSBool Policy_EnableNonStallInt(jsval enableSemaphore, jsval enableInt,
                                    jsval *pReturlwalue);
    JSBool Policy_SetSurfaceVirtualLocMove(jsval aSetSurfaceVirtualLocMove,
                                           jsval *pReturlwalue);
    JSBool Policy_SetGmmuVASpace(jsval *pReturlwalue);
    JSBool Policy_SetDefaultVASpace(jsval *pReturlwalue);
    JSBool Policy_SetSurfaceAllocationType(jsval aSurfAllocType,
                                           jsval *pReturlwalue);
    JSBool Policy_SetCEAllocAsync(jsval aCEInst,
                                  jsval *pReturlwalue);
    JSBool Policy_SetCEAllocDefault(jsval *pReturlwalue);
    JSBool Policy_SetVaSpace(jsval vaSpace, jsval *pReturlwalue);
    JSBool Policy_SetSmcEngine(jsval smcEngine, jsval *pReturlwalue);
    JSBool Policy_SetChannelEngine(jsval aEngineName, jsval *pReturlwalue);
    JSBool Policy_SetInbandCELaunchFlushEnable(jsval aFlag, jsval * pReturlwalue);
    JSBool Policy_SetTargetRunlist(jsval channel, jsval *pReturlwalue);
    JSBool Policy_SetFbCopyVerif(jsval *pReturlwalue);
    JSBool Policy_SetAccessCounterNotificationWaitMs(jsval aWaitMs, jsval *pReturlwalue);

    JSBool Trigger_OnChannelReset(jsval *pReturlwalue);
    JSBool Trigger_Start(jsval *pReturlwalue);
    JSBool Trigger_End(jsval *pReturlwalue);
    JSBool Trigger_OnPageFault(jsval channelDesc, jsval *pReturlwalue);
    JSBool Trigger_OnResetChannel(jsval channelDesc, jsval *pReturlwalue);
    JSBool Trigger_OnChannelRemoval(jsval channelDesc, jsval *pReturlwalue);
    JSBool Trigger_OnGrErrorSwNotify(jsval channelDesc, jsval *pReturlwalue);
    JSBool Trigger_OnGrFaultDuringCtxsw(jsval channelDesc,
                                        jsval *pReturlwalue);
    JSBool Trigger_OnCeError(jsval channelDesc, jsval ceNum, jsval *pReturlwalue);
    JSBool Trigger_OnNonStallInt(jsval intNames, jsval channelDesc,
                                 jsval *pReturlwalue);
    JSBool Trigger_OnSemaphoreRelease(jsval surfaceDesc, jsval offset,
                                      jsval payload, jsval *pReturlwalue);
    JSBool Trigger_OnWaitForChipIdle(jsval *pReturlwalue);
    JSBool Trigger_OnWaitForIdle(jsval channelDesc, jsval *pReturlwalue);
    JSBool Trigger_OnMethodWrite(jsval aChannels,
                                 jsval aPicker,
                                 jsval *pReturlwalue);
    JSBool Trigger_OnPercentMethodsWritten(jsval aChannels,
                                           jsval aPicker,
                                           jsval *pReturlwalue);
    JSBool Trigger_OnMethodExelwte(jsval aChannels,
                                   jsval aPicker,
                                   jsval *pReturlwalue);
    JSBool Trigger_OnMethodExelwteEx(jsval channelDesc,
                                     jsval aPicker,
                                     jsval aWfiOnRelease,
                                     jsval aWaitEventHandled,
                                     jsval *pReturlwalue);
    JSBool Trigger_OnPercentMethodsExelwted(jsval aChannels,
                                            jsval aPicker,
                                            jsval *pReturlwalue);
    JSBool Trigger_OnPercentMethodsExelwtedEx(jsval channelDesc,
                                              jsval aPicker,
                                              jsval aWfiOnRelease,
                                              jsval aWaitEventHandled,
                                              jsval *pReturlwalue);
    JSBool Trigger_OnMethodIdWrite(jsval aChannelDesc, jsval aClassIds,
                                   jsval aMethods, jsval aAfterWrite,
                                   jsval *pReturlwalue);
    JSBool Trigger_OnTimeUs(jsval aTimerName, jsval aHwPicker,
                            jsval aModelPicker, jsval aRtlPicker,
                            jsval *pReturlwalue);
    JSBool Trigger_OnPmuElpgEvent(jsval gpuDesc,
                                  jsval engineId,
                                  jsval interruptStatus,
                                  jsval *pReturlwalue);
    JSBool Trigger_OnRmEvent(jsval jsGpuDesc,
                             jsval jsEventId,
                             jsval *pReturlwalue);
    JSBool Trigger_OnTraceEventCpu(jsval jsTraceEventName,
                                   jsval jsAfterTraceEvent,
                                   jsval *pReturlwalue);
    JSBool Trigger_OnReplayablePageFault(jsval aGpuDesc,
        jsval *pReturlwalue);
    JSBool Trigger_OnCERecoverableFault(jsval aGpuDesc,
        jsval *pReturlwalue);
    JSBool Trigger_OnAccessCounterNotification(jsval aGpuDesc,
        jsval *pReturlwalue);
    JSBool Trigger_OnFaultBufferOverflow(jsval aGpuDesc, jsval *pReturlwalue);
    JSBool Trigger_PluginEventTrigger(jsval aName, jsval *pReturlwalue);
    JSBool Trigger_OnErrorLoggerInterrupt(jsval aGpuDesc, jsval aName,
        jsval *pReturlwalue);
    JSBool Trigger_OnT3dPluginEvent(jsval jsTraceEventName,
                                   jsval *pReturlwalue);
    JSBool Trigger_OnNonfatalPoisonError(jsval *arglist,
                                         uint32 numArgs,
                                         jsval *pReturlwalue);
    JSBool Trigger_OnFatalPoisonError(jsval *arglist,
                                      uint32 numArgs,
                                      jsval *pReturlwalue);

    JSBool ActionBlock_Define(jsval name, jsval *pReturlwalue);
    JSBool ActionBlock_End(jsval *pReturlwalue);
    JSBool ActionBlock_Else(jsval *pReturlwalue);
    JSBool ActionBlock_OnTriggerCount(jsval aPicker, jsval *pReturlwalue);
    JSBool ActionBlock_OnTestNum(jsval aPicker, jsval *pReturlwalue);
    JSBool ActionBlock_OnTestId(jsval aTestId, jsval *pReturlwalue);
    JSBool ActionBlock_OnSurfaceIsFaulting(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool ActionBlock_OlwEIDMatchesFault(jsval aTsgName, jsval aVEID, jsval *pReturlwalue);
    JSBool ActionBlock_OnGPCIDMatchesFault(jsval aGPCID, jsval *pReturlwalue);
    JSBool ActionBlock_OnClientIDMatchesFault(jsval aClientID, jsval *pReturlwalue);
    JSBool ActionBlock_OnFaultTypeMatchesFault(jsval aFaultType, jsval *pReturlwalue);
    JSBool ActionBlock_OnPageIsFaulting(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval *pReturlwalue);
    JSBool ActionBlock_OnNotificationFromSurface(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool ActionBlock_OnNotificationFromPage(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval *pReturlwalue);
    JSBool ActionBlock_AllowOverlappingTriggers(jsval *pReturlwalue);
    JSBool ActionBlock_OnAtsIsEnabledOnChannel(jsval channelDesc, jsval *pReturlwalue);
    JSBool ActionBlock_OnEngineIsExisting(jsval aEngineName, jsval *pReturlwalue);
    JSBool ActionBlock_OnMutex(jsval name, jsval *pReturlwalue);
    JSBool ActionBlock_TryMutex(jsval name, jsval *pReturlwalue);

    JSBool Policy_SetTlbIlwalidateReplayCancelTargetedFaulting(jsval *pReturlwalue);

    JSBool Action_ExitLwrrentVfProcess(jsval *pReturlwalue);
    JSBool Action_ClearFlrPendingBit(jsval pmvftestDesc, jsval *pReturlwalue);
    JSBool Action_WaitFlrDone(jsval pmvftestDesc, jsval *pReturlwalue);
    JSBool Action_VirtFunctionLevelReset(jsval pmvftestDesc, jsval *pReturlwalue);

    JSBool Action_Block(jsval name, jsval *pReturlwalue);
    JSBool Action_Print(jsval printString, jsval *pReturlwalue);
    JSBool Action_TriggerUtlUserEvent(jsval data, jsval *pReturlwalue);
    JSBool Action_UnmapSurfaces(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_UnmapPages(jsval surfaceDesc, jsval offset, jsval size,
                             jsval *pReturlwalue);
    JSBool Action_UnmapPde(jsval surfaceDesc, jsval offset, jsval size,
                             jsval levelNum, jsval *pReturlwalue);
    JSBool Action_SparsifyPages(jsval surfaceDesc, jsval offset, jsval size,
                                jsval *pReturlwalue);
    JSBool Action_RemapSurfaces(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_RemapPages(jsval surfaceDesc, jsval offset, jsval size,
                             jsval *pReturlwalue);
    JSBool Action_RemapPde(jsval surfaceDesc, jsval offset, jsval size,
                             jsval levelNum, jsval *pReturlwalue);
    JSBool Action_RandomRemapIlwalidPages(jsval  surfaceDesc, jsval  aPercentage,
                                          jsval *pReturlwalue);
    JSBool Action_ChangePageSize(jsval surfaceDesc, jsval offset,
                                 jsval size, jsval aPageSize,
                                 jsval *pReturlwalue);
    JSBool Action_RemapFaultingSurface(jsval *pReturlwalue);
    JSBool Action_RemapFaultingPage(jsval *pReturlwalue);
    JSBool Action_CreateVaSpace(jsval name, jsval gpuDesc,
                                jsval *pReturlwalue);
    JSBool Action_CreateSurface(jsval name, jsval size, jsval gpuDesc,
                                jsval *pReturlwalue);
    JSBool Action_MoveSurfaces(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_MovePages(jsval surfaceDesc, jsval offset, jsval size,
                            jsval *pReturlwalue);
    JSBool Action_MovePagesToDestSurf(jsval srcSurfaceDesc, jsval srcOffset, jsval srcSize,
                            jsval destSurfaceDesc, jsval destOffset, jsval *pReturlwalue);
    JSBool Action_MoveFaultingSurface(jsval *pReturlwalue);
    JSBool Action_MoveFaultingPage(jsval *pReturlwalue);
    JSBool Action_PreemptRunlist(jsval runlistDesc, jsval *pReturlwalue);
    JSBool Action_PreemptChannel(jsval channelDesc, jsval pollPreemptComplete,
                                 jsval timeoutMs, jsval *pReturlwalue);
    JSBool Action_RemoveEntriesFromRunlist(jsval runlistDesc, jsval aFirstEntry,
                                           jsval aNumEntries, jsval *pReturlwalue);
    JSBool Action_RestoreEntriesToRunlist(jsval runlistDesc, jsval aInsertPos,
                                          jsval aFirstEntry, jsval aNumEntries,
                                          jsval *pReturlwalue);
    JSBool Action_RemoveChannelFromRunlist(jsval channelDesc, jsval aFirstEntry,
                                           jsval aNumEntries, jsval *pReturlwalue);
    JSBool Action_RestoreChannelToRunlist(jsval channelDesc, jsval aInsertPos,
                                          jsval aFirstEntry, jsval aNumEntries,
                                          jsval *pReturlwalue);
    JSBool Action_MoveEntryInRunlist(jsval runlistDesc, jsval aEntryIndex,
                                     jsval aRelPri, jsval *pReturlwalue);
    JSBool Action_MoveChannelInRunlist(jsval channelDesc, jsval aEntryIndex,
                                       jsval aRelPri, jsval *pReturlwalue);
    JSBool Action_ResubmitRunlist(jsval runlistDesc, jsval *pReturlwalue);
    JSBool Action_FreezeRunlist(jsval runlistDesc, jsval *pReturlwalue);
    JSBool Action_UnfreezeRunlist(jsval runlistDesc, jsval *pReturlwalue);
    JSBool Action_WriteRunlistEntries(jsval runlistDesc, jsval numEntries,
                                      jsval *pReturlwalue);
    JSBool Action_Flush(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_ResetChannel(jsval channelDesc, jsval engineName,
                               jsval *pReturlwalue);
    JSBool Action_EnableEngine(jsval gpuDesc, jsval engineType, jsval hwinst,
                               jsval enable, jsval *pReturlwalue);
    JSBool Action_UnbindAndRebindChannel(jsval channelDesc,
                                         jsval *pReturlwalue);
    JSBool Action_DisableChannel(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_BlockChannelFlush(jsval channelDesc,
                                    jsval blocked,
                                    jsval *pReturlwalue);
    JSBool Action_TlbIlwalidate(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_TlbIlwalidateVA(jsval surfaceDesc, jsval offset, jsval *pReturlwalue);
    JSBool Action_L2Flush(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_L2SysmemIlwalidate(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_L2PeermemIlwalidate(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_L2WaitForSysPendingReads(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_L2VidmemIlwalidate(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_HostMembar(jsval gpuDesc, jsval aIsMembar, jsval *pReturlwalue);
    JSBool Action_ChannelWfi(jsval gpuDesc, jsval aWfiType, jsval *pReturlwalue);
    JSBool Action_SetReadOnly(jsval surfaceDesc, jsval offset, jsval size,
                              jsval *pReturlwalue);
    JSBool Action_ClearReadOnly(jsval surfaceDesc, jsval offset, jsval size,
                                jsval *pReturlwalue);
    JSBool Action_SetAtomicDisable(jsval surfaceDesc, jsval offset, jsval size,
                                   jsval *pReturlwalue);
    JSBool Action_ClearAtomicDisable(jsval surfaceDesc, jsval offset, jsval size,
                                     jsval *pReturlwalue);
    JSBool Action_SetShaderDefaultAccess(jsval surfaceDesc, jsval offset,
                                         jsval size, jsval *pReturlwalue);
    JSBool Action_SetShaderReadOnly(jsval surfaceDesc, jsval offset,
                                    jsval size, jsval *pReturlwalue);
    JSBool Action_SetShaderWriteOnly(jsval surfaceDesc, jsval offset,
                                     jsval size, jsval *pReturlwalue);
    JSBool Action_SetShaderReadWrite(jsval surfaceDesc, jsval offset,
                                     jsval size, jsval *pReturlwalue);
    JSBool Action_SetPdeSize(jsval surfaceDesc, jsval offset, jsval size,
                             jsval pdeSize, jsval *pReturlwalue);
    JSBool Action_SetPdeAperture(jsval surfaceDesc, jsval offset, jsval size,
                                 jsval aperture, jsval *pReturlwalue);
    JSBool Action_SparsifyPde(jsval surfaceDesc, jsval offset, jsval size,
                              jsval *pReturlwalue);
    JSBool Action_SparsifyMmuLevel(jsval surfaceDesc, jsval offset, jsval size,
                                   jsval aLevelNum, jsval *pReturlwalue);
    JSBool Action_SetPteAperture(jsval surfaceDesc, jsval offset, jsval size,
                                 jsval aperture, jsval *pReturlwalue);
    JSBool Action_SetVolatile(jsval surfaceDesc, jsval offset, jsval size,
                              jsval *pReturlwalue);
    JSBool Action_ClearVolatile(jsval surfaceDesc, jsval offset, jsval size,
                                jsval *pReturlwalue);
    JSBool Action_SetVolatileBit(jsval surfaceDesc, jsval offset, jsval size,
                              jsval *pReturlwalue);
    JSBool Action_ClearVolatileBit(jsval surfaceDesc, jsval offset, jsval size,
                                jsval *pReturlwalue);
    JSBool Action_SetKind(jsval surfaceDesc, jsval offset, jsval size,
                          jsval kind, jsval *pReturlwalue);
    JSBool Action_IlwalidatePdbTarget(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_ResetEngineContext(jsval channelDesc, jsval engineName,
                                     jsval *pReturlwalue);
    JSBool Action_ResetEngineContextNoPreempt(jsval channelDesc,
                                              jsval engineName,
                                              jsval *pReturlwalue);

    JSBool Action_SetPrivSurfaces(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_ClearPrivSurfaces(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_SetPrivPages(jsval surfaceDesc, jsval offset, jsval size,
                               jsval *pReturlwalue);
    JSBool Action_ClearPrivPages(jsval surfaceDesc, jsval offset, jsval size,
                                 jsval *pReturlwalue);
    JSBool Action_SetPrivChannels(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_ClearPrivChannels(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_HostSemaphoreAcquire(jsval channelDesc, jsval surfaceDesc,
                                       jsval offset, jsval payload,
                                       jsval *pReturlwalue);
    JSBool Action_HostSemaphoreRelease(jsval channelDesc, jsval surfaceDesc,
                                       jsval offset, jsval payload,
                                       jsval *pReturlwalue);
    JSBool Action_HostSemaphoreReduction(jsval channelDesc, jsval surfaceDesc,
                                         jsval offset, jsval payload, jsval reduction,
                                         jsval reductionType, jsval *pReturlwalue);
    JSBool Action_EngineSemaphoreRelease(jsval channelDesc, jsval surfaceDesc,
                                         jsval offset, jsval payload,
                                         jsval *pReturlwalue);
    JSBool Action_EngineSemaphoreAcquire(jsval channelDesc, jsval surfaceDesc,
                                         jsval offset, jsval payload,
                                         jsval *pReturlwalue);
    JSBool Action_NonStallInt(jsval intName, jsval channelDesc,
                              jsval *pReturlwalue);
    JSBool Action_EngineNonStallInt(jsval intName, jsval channelDesc,
                                    jsval *pReturlwalue);
    JSBool Action_InsertMethods(jsval channelDesc, jsval numMethods,
                                jsval aMethodPicker, jsval aDataPicker,
                                jsval *pReturlwalue);
    JSBool Action_InsertSubchMethods(jsval channelDesc, jsval subch,
                                     jsval numMethods, jsval aMethodPicker,
                                     jsval aDataPicker, jsval *pReturlwalue);
    JSBool Action_PowerWait(jsval *pReturlwalue);
    JSBool Action_EscapeRead(jsval path, jsval index, jsval size,
                             jsval *pReturlwalue);
    JSBool Action_EscapeWrite(jsval path, jsval index, jsval size,
                              jsval data, jsval *pReturlwalue);
    JSBool Action_GpuEscapeRead(jsval gpuDesc, jsval aPath, jsval aIndex,
                                jsval aSize, jsval *pReturlwalue);
    JSBool Action_GpuEscapeWrite(jsval gpuDesc, jsval aPath, jsval aIndex,
                                 jsval aSize, jsval aData,
                                 jsval *pReturlwalue);
    JSBool Action_SetPState(jsval gpuDesc, jsval pstate, jsval *pReturlwalue);
    JSBool Action_AbortTests(jsval testDesc, jsval *pReturlwalue);
    JSBool Action_AbortTest(jsval *arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Action_WaitForSemaphoreRelease(jsval  surfaceDesc, jsval aOffset,
                                          jsval  aValue,      jsval aTimeout,
                                          jsval *pReturlwalue);
    JSBool Action_FreezeHost(jsval gpuDesc, jsval aTimeout, jsval *pReturlwalue);
    JSBool Action_UnFreezeHost(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_SwReset(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_SecondaryBusReset(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_FundamentalReset(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_PhysWr32(jsval addr, jsval value, jsval *pReturlwalue);
    JSBool Action_PriWriteReg32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Action_PriWriteField32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Action_PriWriteMask32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Action_WaitPriReg32(jsval *arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Action_StartTimer(jsval TimerName, jsval *pReturlwalue);
    JSBool Action_AliasPages(jsval surfaceDesc, jsval aliasDstOffset,
                             jsval size, jsval aliasSrcOffset,
                             jsval *pReturlwalue);
    JSBool Action_AliasPagesFromSurface(jsval destSurfaceDesc,
        jsval aliasDstOffset, jsval size, jsval sourceSurfaceDesc,
        jsval aliasSrcOffset, jsval *pReturlwalue);
    JSBool Action_FillSurface(jsval surfaceDesc, jsval offset, jsval size,
                              jsval data, jsval *pReturlwalue);
    JSBool Action_CpuMapSurfacesDirect(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_CpuMapSurfacesReflected(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_CpuMapSurfacesDefault(jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_GpuSetClock(jsval gpuDesc, jsval aClkDomainName,
                              jsval aTargetHz, jsval *pReturlwalue);
    JSBool Action_StopFb(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_RestartFb(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_EnableElpg(jsval gpuDesc,
                             jsval aEngineId,
                             jsval aToEnableElpg,
                             jsval *pReturlwalue);

    JSBool Action_RailGateGpu(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_UnRailGateGpu(jsval gpuDesc, jsval *pReturlwalue);

    JSBool Action_SetGpioUp(jsval channelDesc, jsval aGpioPorts, jsval *pReturlwalue);
    JSBool Action_SetGpioDown(jsval channelDesc, jsval aGpioPorts, jsval *pReturlwalue);

    JSBool Action_CreateChannel(jsval aName, jsval aGpuDesc, jsval aTestDesc, jsval *returlwalue);
    JSBool Action_CreateSubchannel(jsval aChannelDesc, jsval aHwClass, jsval aSubchannelNumber, jsval *pReturlwalue);
    JSBool Action_CreateCESubchannel(jsval aChannelDesc, jsval *pReturlwalue);
    JSBool Action_SetUpdateFaultBufferGetPointer(jsval aFlag, jsval *pReturlwalue);
    JSBool Action_ClearFaultBuffer(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_SendTraceEvent(jsval aeventName, jsval surfaceDesc, jsval *pReturlwalue);
    JSBool Action_SetAtsReadPermission(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval aPermission, jsval *pReturlwalue);
    JSBool Action_SetAtsWritePermission(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval aPermission, jsval *pReturlwalue);
    JSBool Action_RemapAtsPages(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval *pReturlwalue);
    JSBool Action_UnmapAtsPages(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval *pReturlwalue);
    JSBool Action_RemapFaultingAtsPage(jsval *pReturlwalue);

    JSBool Action_RestartEngineFaultedChannel(jsval channelDesc, jsval *pReturlwalue);
    JSBool Action_RestartPbdmaFaultedChannel(jsval channelDesc, jsval *pReturlwalue);

    JSBool Action_ClearAccessCounter(jsval gpuDesc, jsval counterType, jsval *pReturlwalue);
    JSBool Action_ResetAccessCounterCachedGetPointer(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Action_SetPhysAddrBits(jsval surfaceDesc, jsval aOffset,
        jsval aSize, jsval aAddrValue, jsval aAddrMask, jsval *pReturlwalue);

    JSBool Action_ForceNonReplayableFaultBufferOverflow(jsval *pReturlwalue);
    JSBool Action_StartVFTest(jsval Vf, jsval *pReturlwalue);
    JSBool Action_WaitProcEvent(jsval message, jsval Vf, jsval *pReturlwalue);
    JSBool Action_SendProcEvent(jsval message, jsval Vf, jsval *pReturlwalue);
    JSBool Action_WaitErrorLoggerInterrupt(jsval gpuDesc, jsval name,
        jsval timeoutMs, jsval *pReturlwalue);
    JSBool Action_SaveCHRAM(jsval channelDesc, jsval filename, jsval *pReturlwalue);
    JSBool Action_RestoreCHRAM(jsval channelDesc, jsval filename, jsval *pReturlwalue);
    JSBool Action_SaveFB(jsval Vm, jsval aFile, jsval *pReturlwalue);
    JSBool Action_RestoreFB(jsval Vm, jsval aFile, jsval *pReturlwalue);
    JSBool Action_SaveRunlist(jsval channel, jsval *pReturlwalue);
    JSBool Action_RestoreRunlist(jsval channel, jsval *pReturlwalue);
    JSBool Action_DmaCopyFB(jsval VmFrom, jsval VmTo, jsval *pReturlwalue);
    JSBool Action_DmaSaveFB(jsval Vm, jsval *pReturlwalue);
    JSBool Action_DmaRestoreFB(jsval Vm, jsval *pReturlwalue);
    JSBool Action_SaveSmcPartFbInfo(jsval aFile, jsval *pReturlwalue);

    // Debugging purpose only
    JSBool Action_SaveFBSeg(jsval aFile, jsval aPa, jsval aSize, jsval *pReturlwalue);
    JSBool Action_RestoreFBSeg(jsval aFile, jsval aPa, jsval aSize, jsval *pReturlwalue);
    JSBool Action_SaveFBSegSurf(jsval aFile, jsval aPa, jsval aSize, jsval *pReturlwalue);
    JSBool Action_RestoreFBSegSurf(jsval aFile, jsval aPa, jsval aSize, jsval *pReturlwalue);
    JSBool Action_DmaSaveFBSeg(jsval aPa, jsval aSize, jsval *pReturlwalue);
    JSBool Action_DmaRestoreFBSeg(jsval aPa, jsval aSize, jsval *pReturlwalue);

    JSBool Surface_Define(jsval name, jsval *pReturlwalue);
    JSBool Surface_End(jsval *pReturlwalue);
    JSBool Surface_Defined(jsval name, jsval *pReturlwalue);
    JSBool Surface_Name(jsval * arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Surface_NotName(jsval regex, jsval *pReturlwalue);
    JSBool Surface_Type(jsval regex, jsval *pReturlwalue);
    JSBool Surface_NotType(jsval regex, jsval *pReturlwalue);
    JSBool Surface_Gpu(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Surface_Faulting(jsval *pReturlwalue);
    JSBool Surface_All(jsval *pReturlwalue);

    JSBool VaSpace_Name(jsval * aRegex, uint32 numArgs, jsval *pReturlwalue);

    JSBool VF_SeqId(jsval regex, jsval *pReturlwalue);
    JSBool VF_GFID(jsval regex, jsval *pReturlwalue);
    JSBool VF_All(jsval *pReturlwalue);

    JSBool SmcEngine_SysPipe(jsval regex, jsval *pReturlwalue);
    JSBool SmcEngine_Test(jsval aTestDesc, jsval *pReturlwalue);
    JSBool SmcEngine_Name(jsval regex, jsval *pReturlwalue);

    JSBool Channel_Define(jsval surfName, jsval *pReturlwalue);
    JSBool Channel_End(jsval *pReturlwalue);
    JSBool Channel_Defined(jsval surfName, jsval *pReturlwalue);
    JSBool Channel_Name(jsval * aRegex, uint32 numArgs, jsval *pReturlwalue);
    JSBool Channel_Number(jsval channelNumbers, jsval *pReturlwalue);
    JSBool Channel_Gpu(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Channel_Faulting(jsval *pReturlwalue);
    JSBool Channel_All(jsval *pReturlwalue);

    JSBool Gpu_Define(jsval surfName, jsval *pReturlwalue);
    JSBool Gpu_End(jsval *pReturlwalue);
    JSBool Gpu_Defined(jsval surfName, jsval *pReturlwalue);
    JSBool Gpu_Inst(jsval devRegex, jsval subdevRegex, jsval *pReturlwalue);
    JSBool Gpu_DevInst(jsval regex, jsval *pReturlwalue);
    JSBool Gpu_SubdevInst(jsval regex, jsval *pReturlwalue);
    JSBool Gpu_Faulting(jsval *pReturlwalue);
    JSBool Gpu_All(jsval *pReturlwalue);

    JSBool Runlist_Define(jsval surfName, jsval *pReturlwalue);
    JSBool Runlist_End(jsval *pReturlwalue);
    JSBool Runlist_Defined(jsval surfName, jsval *pReturlwalue);
    JSBool Runlist_Name(jsval regex, jsval *pReturlwalue);
    JSBool Runlist_Channel(jsval channelDesc, jsval *pReturlwalue);
    JSBool Runlist_Gpu(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Runlist_All(jsval *pReturlwalue);

    JSBool Test_Define(jsval surfName, jsval *pReturlwalue);
    JSBool Test_End(jsval *pReturlwalue);
    JSBool Test_Defined(jsval surfName, jsval *pReturlwalue);
    JSBool Test_Name(jsval regex, jsval *pReturlwalue);
    JSBool Test_Id(jsval * arglist, uint32 numArgs, jsval *pReturlwalue);
    JSBool Test_Type(jsval regex, jsval *pReturlwalue);
    JSBool Test_Channel(jsval channelDesc, jsval *pReturlwalue);
    JSBool Test_Gpu(jsval gpuDesc, jsval *pReturlwalue);
    JSBool Test_Faulting(jsval *pReturlwalue);
    JSBool Test_All(jsval *pReturlwalue);

    JSBool Aperture_Framebuffer(jsval *pReturlwalue);
    JSBool Aperture_Coherent(jsval *pReturlwalue);
    JSBool Aperture_NonCoherent(jsval *pReturlwalue);
    JSBool Aperture_Peer(jsval *pReturlwalue);
    JSBool Aperture_Ilwalid(jsval *pReturlwalue);
    JSBool Aperture_All(jsval *pReturlwalue);

private:
    enum Versions
    {
        LOWEST_VERSION = 2,
        HIGHEST_VERSION = 2
    };

    struct Scope
    {
        enum Type
        {
            ROOT,
            ACTION_BLOCK,
            CONDITION,
            ELSE_BLOCK,
            ALLOW_OVERLAPPING_TRIGGERS,
            SURFACE,
            CHANNEL,
            GPU,
            RUNLIST,
            TEST,
            VASPACE,
            VF,
            SMCENGINE,
            MUTEX,
            MUTEX_CONDITION
        };
        Type                      m_Type;
        PmActionBlock            *m_pActionBlock; // for ACTION_BLOCK scopes
        PmAction_Goto            *m_pGotoAction;  // for CONDITION and ELSE_BLOCK scopes
        PmSurfaceDesc            *m_pSurfaceDesc; // for SURFACE scopes
        PmChannelDesc            *m_pChannelDesc; // for CHANNEL scopes
        PmGpuDesc                *m_pGpuDesc;     // for GPU scopes
        PmRunlistDesc            *m_pRunlistDesc; // for RUNLIST scopes
        PmTestDesc               *m_pTestDesc;    // for TEST scopes
        bool                      m_InbandMemOp;
        bool                      m_InbandPte;
        bool                      m_InbandSurfaceMove;
        bool                      m_InbandSurfaceClear;
        PmChannelDesc            *m_pInbandChannel;
        Memory::Location          m_AllocLocation;
        bool                      m_PhysContig;
        UINT32                    m_Alignment;
        bool                      m_DualPageSize;
        PolicyManager::LoopBackPolicy m_LoopBackPolicy;
        PolicyManager::MovePolicy m_MovePolicy;
        PolicyManager::PageSize   m_PageSize;
        UINT32                    m_PowerWaitModelUs;
        UINT32                    m_PowerWaitRTLUs;
        UINT32                    m_PowerWaitHWUs;
        bool                      m_PowerWaitBusy;
        UINT32                    m_PStateFallback;
        bool                      m_bUseSeedString;
        UINT32                    m_RandomSeed;
        bool                      m_ClearNewSurfaces;
        bool                      m_TlbIlwalidateGpc;
        PolicyManager::TlbIlwalidateReplay m_TlbIlwalidateReplay;
        bool                      m_TlbIlwalidateSysmembar;
        PolicyManager::TlbIlwalidateAckType m_TlbIlwalidateAckType;
        Surface2D::GpuCacheMode   m_SurfaceGpuCacheMode;
        UINT32                    m_ChannelSubdevMask;
        PmGpuDesc                *m_pRemoteGpuDesc;    // for remote surface access
        UINT32                    m_SemRelFlags;
        bool                      m_DeferTlbIlwalidate;
        bool                      m_TlbIlwalidatePdbAll;
        bool                      m_DisablePlcOnSurfaceMove;
        bool                      m_EnableNsiSemaphore;
        bool                      m_EnableNsiInt;
        PolicyManager::AddressSpaceType  m_AddrSpaceType;
        Surface2D::VASpace        m_SurfaceAllocType;
        Surface2D::GpuSmmuMode    m_GpuSmmuMode;
        bool                      m_MoveSurfInDefaultMMU;
        FancyPicker               *m_pGPCID;
        FancyPicker               *m_pClientUnitID;

        FancyPicker               *m_pCEAllocInst;
        FancyPicker::FpContext     m_FpContext;
        PolicyManager::CEAllocMode m_CEAllocMode;

        PmChannelDesc             *m_pTlbIlwalidateChannel;
        bool                      m_TlbIlwalidateBar1;
        bool                      m_TlbIlwalidateBar2;
        PolicyManager::Level      m_TlbIlwalidateLevelNum;
        PolicyManager::TlbIlwalidateIlwalScope m_TlbIlwalidateIlwalScope;
        PolicyManager::PageSize   m_PhysPageSize;
        PmVaSpaceDesc             *m_pVaSpaceDesc;
        PmVfTestDesc                  *m_pVfDesc;
        PmSmcEngineDesc           *m_pSmcEngineDesc;
        PolicyManager::TlbIlwalidateAccessType   m_AccessType;
        UINT32                    m_VEID;
        string                    m_DefEngineName;
        string                    m_RegSpace;
        RegExp                    m_MutexName;
    };

    PmEventManager *GetEventManager() const
        { return m_pPolicyManager->GetEventManager(); }
    void PushScope(Scope::Type scopeType);
    RC CheckForIdentifier(const string &value, UINT32 argNum);
    RC CheckForRootScope(const char *tokenName);
    RC CheckForActionBlockScope(const char *tokenName);
    RC CheckForSurfaceScope(const char *tokenName);
    RC CheckForVaSpaceScope(const char *tokenName);
    RC CheckForVfScope(const char *tokenName);
    RC CheckForSmcEngineScope(const char *tokenName);
    RC CheckForChannelScope(const char *tokenName);
    RC CheckForGpuScope(const char *tokenName);
    RC CheckForRunlistScope(const char *tokenName);
    RC CheckForTestScope(const char *tokenName);

    RC FromJsval(jsval value, string         *pString,       UINT32 argNum);
    RC FromJsval(jsval value, bool           *pBool,         UINT32 argNum);
    RC FromJsval(jsval value, UINT32         *pUint32,       UINT32 argNum);
    RC FromJsval(jsval value, UINT64         *pUint64,       UINT32 argNum);
    RC FromJsval(jsval value, FLOAT64        *pFloat64,      UINT32 argNum);
    RC FromJsval(jsval value, PmTrigger     **ppTrigger,     UINT32 argNum);
    RC FromJsval(jsval value, PmAction      **ppAction,      UINT32 argNum);
    RC FromJsval(jsval value, PmActionBlock **ppActionBlock, UINT32 argNum);
    RC FromJsval(jsval value, PmSurfaceDesc **ppSurfaceDesc, UINT32 argNum);
    RC FromJsval(jsval value, PmChannelDesc **ppChannelDesc, UINT32 argNum);
    RC FromJsval(jsval value, PmGpuDesc     **ppGpuDesc,     UINT32 argNum);
    RC FromJsval(jsval value, PmRunlistDesc **ppRunlistDesc, UINT32 argNum);
    RC FromJsval(jsval value, PmTestDesc    **ppTestDesc,    UINT32 argNum);
    RC FromJsval(jsval value, PmVaSpaceDesc **ppVaSpaceDesc, UINT32 argNum);
    RC FromJsval(jsval value, PmVfTestDesc    **ppVfDesc,      UINT32 argNum);
    RC FromJsval(jsval value, PmSmcEngineDesc   **ppSmcEngineDesc,      UINT32 argNum);
    RC FromJsval(jsval value, RegExp         *pRegExp,       UINT32 argNum,
                                                             UINT32 flags);
    RC FromJsval(jsval value, vector<UINT32> *pArray,        UINT32 argNum);
    RC FromJsval(jsval value, vector<string> *pArray,        UINT32 argNum);
    RC FromJsval(jsval value, PolicyManager::Aperture *pAperture,
                                                             UINT32 argNum);
    RC ToJsval(PmTrigger     *pTrigger,          jsval *pValue);
    RC ToJsval(PmAction      *pAction,           jsval *pValue);
    RC ToJsval(PmActionBlock *ppActionBlock,     jsval *pValue);
    RC ToJsval(PmSurfaceDesc *pSurfaceDesc,      jsval *pValue);
    RC ToJsval(PmChannelDesc *pChannelDesc,      jsval *pValue);
    RC ToJsval(PmGpuDesc     *pGpuDesc,          jsval *pValue);
    RC ToJsval(PmRunlistDesc *pRunlistDesc,      jsval *pValue);
    RC ToJsval(PmVaSpaceDesc *pVaSpaceDesc,      jsval *pValue);
    RC ToJsval(PmVfTestDesc  *pVfDesc,           jsval *pValue);
    RC ToJsval(PmSmcEngineDesc  *pSmcEngineDesc,    jsval *pValue);
    RC ToJsval(PmTestDesc    *pTestDesc,         jsval *pValue);
    RC ToJsval(PolicyManager::Aperture aperture, jsval *pValue);
    RC ToJsvalWrapper(void *pObject, jsval *pValue, const char *className);
    RC FromJsvalWrapper(jsval value, void **ppObject, const char *className);
    RC ToJsvalEnum(UINT32 integer, jsval *pValue, const char *enumName);
    RC FromJsvalEnum(jsval value, UINT32 *pInteger, const char *enumName);

    RC ParsePriReg(string *pName, vector<UINT32> *pIndexes);
    void PrintRandomSeed(const char *tokenName);

    PmTrigger_OnMethodExelwte *CreateOnMethodExelwteObj
    (
        PmChannelDesc *pChannelDesc,
        FancyPicker   *pFancyPicker,
        bool           bUseSeedString,
        UINT32         randomSeed,
        bool           bWfiOnRelease,
        bool           bWaitEventHandled
    );
    PmTrigger_OnPercentMethodsExelwted *CreateOnPercentMethodsExelwtedObj
    (
        PmChannelDesc *pChannelDesc,
        FancyPicker   *pFancyPicker,
        bool           bUseSeedString,
        UINT32         randomSeed,
        bool           bWfiOnRelease,
        bool           bWaitEventHandled
    );

    static PolicyManager::Level ColwertStrToMmuLevelType(const string& levelName);
    static PolicyManager::TlbIlwalidateIlwalScope ColwertStrToMmuIlwalScope(const string& ilwalScopeName);

    PolicyManager *m_pPolicyManager;
    string m_FileName;
    UINT32 m_LineNum;
    UINT32 m_Version;
    stack<Scope> m_Scopes;

    vector<FancyPicker> m_ScopeFancyPickers;
};

#endif // INCLUDED_POLICY_FILE_PARSER_H
