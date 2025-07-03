/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tracechan.h
 * @brief  TraceChannel: per-channel resources used by Trace3DTest.
 */
#ifndef INCLUDED_TRACECHAN_H
#define INCLUDED_TRACECHAN_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_LIST
#include <list>
#define INCLUDED_STL_LIST
#endif
#ifndef INCLUDED_TYPES_H
#include "mdiag/utils/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_TEE_H
#include "core/include/tee.h"
#endif

#include "gputrace.h"
#include "tracemod.h"
#include "mdiag/utils/surf_types.h"
#include "mdiag/tests/gpu/lwgpu_ch_helper.h"
#include "tracesubchan.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"

class ModsEvent; // tasker.h

class BuffInfo;

class TraceTsg;
typedef vector<TraceSubChannel*>  TraceSubChannelList;

//------------------------------------------------------------------------------
// TraceChannel -- holds per-channel resources for Trace3DTest.
//
class TraceChannel
{
public:
    TraceChannel(string name, BuffInfo* inf, Trace3DTest *test);
    ~TraceChannel();

    // Parsing methods called by GpuTrace::ParseTestHdr.
    void AddModule(TraceModule * module);
    void SetTimeSlice(UINT32 val);

    // Alloc and free resources.
    RC AllocChannel
    (
        const ArgReader * params
    );
    RC   AllocObjects();
    RC   AllocNotifiers(const ArgReader * params);
    void ForceWfiMethod(WfiMethod m);
    void Free();

    // Trace playback methods.
    void BeginChannelSwitch();
    void EndChannelSwitch();
    void ClearNotifiers();
    RC   SendNotifiers();
    bool PollNotifiersDone();
    UINT16 PollNotifierValue();

    LWGpuChannel *           GetCh() const;
    GpuChannelHelper *       GetChHelper();
    LWGpuResource *          GetGpuResource();
    TraceModules::iterator   ModBegin();
    TraceModules::iterator   ModEnd();
    GpuVerif*            GetGpuVerif() { return m_pGpuVerif; }

    // Access functions
    string              GetName() const;
    WfiMethod           GetWfiMethod() const;

    // Subchannel related functions
    TraceSubChannel*         AddSubChannel(const string& name, const ArgReader* params);
    RC                       RemoveSubChannel(TraceSubChannel* subch);
    // return graphics subchannel if it contains one, otherwise return NULL
    TraceSubChannel*         GetGrSubChannel();
    // return compute subchannel if it contains one, otherwise return 0
    TraceSubChannel*         GetComputeSubChannel();
    // return copy subchannel if it contains one, otherwise return 0
    TraceSubChannel*         GetCopySubChannel();
    // return subchannel by name. If name is empty (""), return the first subchannel
    TraceSubChannel*         GetSubChannel(const string& name);
    // return subchannel with specified subchannel number
    TraceSubChannel*         GetSubChannel(UINT32 i, bool errOnNonExist=true);

    // Iterator for subchannel
    TraceSubChannelList::iterator SubChBegin() { return m_SubChList.begin(); }
    TraceSubChannelList::iterator SubChEnd() { return m_SubChList.end(); }

    MethodMassager           GetMassager(UINT32 subch, UINT32 method);
    bool                     HasMultipleSubChs() const;
    bool                     SubChSanityCheck(const char* fname) const;

    RC SetupCRCChecker(IGpuSurfaceMgr* sm, const ArgReader* params,
        ICrcProfile* profile, ICrcProfile* goldProfile, GpuSubdevice *pSubdev);
    RC SetupSelfgildChecker(SelfgildStates::const_iterator begin,
        SelfgildStates::const_iterator end, IGpuSurfaceMgr* sm,
        const ArgReader* params, ICrcProfile* profile, ICrcProfile* goldProfile,
        GpuSubdevice *pSubdev);
    TestEnums::TEST_STATUS VerifySurfaces(ITestStateReport *report, UINT32 subdev, const SurfaceSet *skipSurfaces);
    RC AddDmaBuffer(MdiagSurf *pDmaBuf, CHECK_METHOD check, const char *Filename,
                    UINT32 Offset, UINT32 Size, UINT32 Granularity,
                    const VP2TilingParams *pTilingParams,
                    BlockLinear* pBlockLinear, UINT32 Width, UINT32 Height, bool CrcMatch);
    RC AddAllocSurface(MdiagSurf *surface, const string &name,
        CHECK_METHOD check, UINT64 offset, UINT64 size, UINT32 granularity,
        bool crcMatch, const CrcRect& rect, bool rawCrc, UINT32 classNum);

    const CrcChain* GetCrcChain() const;

    // return the hwclass for the in-trace subchannel (0 if that subch non-existant)
    // in-trace subchannel means the subchannel in the pushbuffer data in the trace,
    // which is NOT (necessarily) the subchannel trace_3d writes methods to during
    // replay
    //
    UINT32 GetTraceSubChannelHwClass( UINT32 traceSubch );

    MdiagSurf &GetVABBuffer() { return m_VAB; }
    const string &GetVABPteKindName() const { return m_VABPteKindName; }

    // Get a list of all surfaces that this channel reads or writes,
    // based on the RELOC statements in the trace.
    //
    void GetRelocSurfaces(SurfaceSet *pSurfaces) const;

    RC SetTraceTsg(TraceTsg* pTraceTsg);
    TraceTsg* GetTraceTsg() const { return m_pTraceTsg; }

    RC SetGpuManagedChannel(bool isGpuManagedCh);
    bool IsGpuManagedChannel() const { return m_IsGpuManagedChannel; }
    bool IsHostOnlyChannel() const { return m_SubChList.empty(); }
    bool GetPreemptive() const { return m_Preemptive; }
    void SetPreemptive(bool b) { m_Preemptive = b; }
    bool GetVprMode() const { return m_VprMode; }
    void SetVprMode(bool b) { m_VprMode = b; }

    void SetGpFifoEntries(UINT32 val);
    void SetPushbufferSize(UINT64 val);
    void SetMaxCountedGpFifoEntries(UINT32 val) { m_MaxCountedGpFifoEntries = val; }
    UINT32 GetMaxCountedGpFifoEntries() const { return m_MaxCountedGpFifoEntries; }

    RC SetMethodCount();
    RC SetupClass();
    RC SetupSubChannelClass(TraceSubChannel* pTraceSubch);
    RC SetupNonThrottledC(LWGpuSubChannel * pTraceSubch);

    bool UseHostSemaphore();

    TraceFileMgr *GetTraceFileMgr() { return m_Trace->GetTraceFileMgr(); }

    bool MatchesChannelManagedMode(GpuTrace::ChannelManagedMode mode);
    RC SendPostTraceMethods();

    void SetIsDynamic() { m_IsDynamic = true; }
    bool IsDynamic() const { return m_IsDynamic; }
    void ClearIsValidInParsing() { m_IsValidInParsing = false; }
    bool IsValidInParsing() { return m_IsValidInParsing; }

    RC InjectMassageMethods();

    RC SetSCGType(LWGpuChannel::SCGType type);
    void SetPbdma(UINT32 pbdma);
    UINT32 GetPbdma() const { return m_Pbdma; }
    LwRm::Handle GetVASpaceHandle() const { return m_hVASpace; }
    void SetVASpace(const shared_ptr<VaSpace> &pVaSpace);
    const shared_ptr<VaSpace>& GetVASpace() const { return m_pVaSpace; }
    const shared_ptr<SubCtx>& GetSubCtx() const { return m_pSubCtx; }
    void SetSubCtx(const shared_ptr<SubCtx> &pSubCtx) { m_pSubCtx = pSubCtx; }
    UINT32 GetEngineId() const { return m_EngineId; };
    LwRm* GetLwRmPtr();
    SmcEngine* GetSmcEngine();

    enum CoarseShading : UINT32
    {
        COARSE_SHADING_1X2 = 0,
        COARSE_SHADING_2X1 = 1,
        COARSE_SHADING_2X2 = 2,
        COARSE_SHADING_2X4 = 3,
        COARSE_SHADING_4X2 = 4,
        COARSE_SHADING_4X4 = 5,
        COARSE_SHADING_FIRST = COARSE_SHADING_1X2,
        COARSE_SHADING_LAST = COARSE_SHADING_4X4
    };

    Trace3DTest * GetTest() { return m_Test; }
private:
    RC SetupKeplerCompute(LWGpuSubChannel* pSubch);
    RC SetupKeplerComputePreemptive(LWGpuSubChannel* pSubch);
    RC SetupKeplerComputeTrapHandle(LWGpuSubChannel* pSubch);
    RC SetupVoltaCompute(LWGpuSubChannel* pSubch);
    RC SetupVoltaComputeTrapHandler(LWGpuSubChannel* pSubch);

    RC SetupFermi(LWGpuSubChannel* pSubCh);
    RC SetupFermi_c(LWGpuSubChannel* pSubCh);
    RC SetupKepler(LWGpuSubChannel* pSubCh);
    RC SetupMaxwellA(LWGpuSubChannel* pSubCh);
    RC SetupMaxwellB(LWGpuSubChannel* pSubCh);
    RC SetupPascalA(LWGpuSubChannel* pSubCh);
    RC SetupVoltaA(LWGpuSubChannel* pSubCh);
    RC SetupTuringA(LWGpuSubChannel* pSubCh);
    RC SetupVoltaTrapHandle(LWGpuSubChannel * pSubCh);
    RC SetupAmpereA(LWGpuSubChannel* pSubCh);
    RC SetupAmpereB(LWGpuSubChannel* pSubCh);
    RC SetupHopperA(LWGpuSubChannel* pSubCh);
    RC FalconMethodWrite(LWGpuSubChannel* pSubch, UINT32 falconMethod, UINT32 data, UINT32 nopMethod);

    RC SetupFermiVabBuffer(LWGpuSubChannel* pSubch);
    RC SetupFermiCompute(LWGpuSubChannel* pSubch);
    RC SetupMsdec();

    RC SetFermiMethodCount(LWGpuChannel*);

    RC SetZbcXChannelReg(LWGpuSubChannel *subchannel);
    RC SetZbcXChannelRegFermi(LWGpuSubChannel *subchannel);
    RC SetZbcXChannelRegPascal(LWGpuSubChannel *subchannel);
    RC SendRenderTargetMethods(LWGpuSubChannel *subchannel);
    RC SendPostTraceMethodsKepler(LWGpuSubChannel *subchannel);
    RC SendPostTraceMethodsMaxwellB(LWGpuSubChannel *subchannel);

    void AddZLwllId(UINT32 id);

    // The channel itself.
    string              m_Name;             // Channel name from test.hdr.
    GpuChannelHelper*   m_pChHelper;
    LWGpuChannel *      m_pCh;
    TraceSubChannelList m_SubChList;
    UINT32              m_TimeSlice;

    // List of pushbuffer-data TraceModules that will be played through
    // this channel.
    TraceModules        m_PbufModules;

    GpuVerif* m_pGpuVerif;

    // XXX
    BuffInfo* m_BuffInfo;

    // VAB buffer
    MdiagSurf m_VAB;
    string m_VABPteKindName;
    bool m_IgnoreVAB;

    // indicate to pre-pascal setup functions to ignore the -pagepool_size argument
    bool m_bUsePascalPagePool;
    // indicat to pre-volta setup functions to use offset
    // after volta setup functions to use va
    bool m_bSetupTrapHanderVa;

    MdiagSurf m_AmodelCirlwlarBuffer;

    // Private methods
    bool SetupCrcChain(UINT32 Class, const ArgReader* params, GpuSubdevice *pSubdev);
    bool SetupPerfsimCrcChain(UINT32 Class);
    bool SetupAmodelCrcChain(UINT32 Class);
    bool BuildCrcChain(UINT32 classnum, const ArgReader* params, GpuSubdevice *pSubdev);
    bool IsGrCE(UINT32 ceType);
    UINT32 GetEngineOffset(UINT32 engineBase);
    void SetUnspecifiedCE();
    bool CheckAndSetGrCE(TraceSubChannelList tsgSubChList);
    void CalcEngineId();
    void SanityCheckSharedChannel();

    TraceTsg* m_pTraceTsg;
    bool m_IsGpuManagedChannel;
    bool m_Preemptive;
    bool m_VprMode;

    UINT32 m_GpFifoEntries;
    UINT32 m_MaxCountedGpFifoEntries;
    UINT64 m_PushbufferSize;
    UINT32 m_EngineId;

    Trace3DTest *m_Test;
    const ArgReader* params;
    GpuTrace *m_Trace;

    bool m_IsDynamic; // create channel during running trace

    // A flag to identify whether the channel is valid in parse stage
    // It's invalid after FREE_CHANNEL trace command is parsed.
    bool m_IsValidInParsing;

    LWGpuChannel::SCGType m_SCGType;
    UINT32 m_Pbdma;
    LwRm::Handle m_hVASpace;
    shared_ptr<VaSpace> m_pVaSpace;
    shared_ptr<SubCtx> m_pSubCtx;
};

#endif // INCLUDED_TRACECHAN_H

