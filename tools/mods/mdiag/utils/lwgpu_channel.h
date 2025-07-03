/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWGPU_CHANNEL_H
#define INCLUDED_LWGPU_CHANNEL_H

#include <stdio.h>
#include <vector>
#include <list>
#include <map>
#include "core/include/rc.h"
#include "surfutil.h"

#ifndef INCLUDED_CHANNEL_H
#include "core/include/channel.h"
#endif

class LWGpuResource;
class Semaphore;
class BuffInfo;
class RandomStream;
class Channel;
struct _limiter_table;
class LWGpuTsg;
class SemaphoreOperations;

class VaSpace;
// subctx support
class SubCtx;
class SmcEngine;

class AddrRange
{
public:
    AddrRange(string name, UINT32 base, UINT32 end) {
        m_count = 0;
        m_name = name;
        m_base = base;
        if ( end == base ) m_end = base + 1;
        else m_end = end;
    }
    ~AddrRange() { }

    bool InRange(UINT32 addr) {
        return ((addr >= m_base ) && (addr < m_end));
    }
    void IncrCount() {
        m_count++;
    }
    string GetName() {
        return m_name;
    }
    int GetCount() {
        return m_count;
    }

private:
    string m_name;
    int m_count;
    UINT32 m_base;
    UINT32 m_end;
};

// Types of methods
enum MethodType
{
    NON_INCREMENTING = 0,
    INCREMENTING = 1,
    INCREMENTING_ONCE = 3,
    INCREMENTING_IMM = 4
};

class LWGpuChannel {
public:
    LWGpuChannel(LWGpuResource *lwgpu, LwRm* pLwRm, SmcEngine* pSmcEngine);
    virtual ~LWGpuChannel();

    UINT32 ChannelHandle() const;
    UINT32 ChannelNum() const;
    UINT32 GetPushBufferSize();

    // more general way of feeding methods to a chip (takes care of PIO vs. push buffer)
    int MethodWrite(int subchannel, UINT32 method, UINT32 data);
    int MethodWriteN(int subchannel,
                     UINT32 method_start,
                     unsigned count, const UINT32 *datap,
                     MethodType mode = INCREMENTING);
    RC MethodWriteRC(int subchannel, UINT32 method, UINT32 data);
    RC MethodWriteN_RC(int subchannel,
                       UINT32 method_start,
                       unsigned count, const UINT32 *datap,
                       MethodType mode = INCREMENTING);

    RC WriteSetSubdevice(UINT32 data);

    // Ugh, a getter.  However, it does make the subchannel/channel wrappers for
    // channel less lwmbersome.
    Channel * GetModsChannel();
    void SetModsChannel(Channel * pChannel) { m_pCh = pChannel; }
    void SetChannelHandle(UINT32 channelHandle) { m_hCh = channelHandle; }

    RC ScheduleChannel(bool enable);
    int SetObject(int subch, UINT32 handle);
    RC SetObjectRC(int subch, UINT32 handle);
    RC SetSemaphoreOffsetRC(UINT64 offset);
    void SetSemaphoreReleaseFlags(UINT32 flags);
    UINT32 GetSemaphoreReleaseFlags();
    RC SetSemaphorePayloadSize(Channel::SemaphorePayloadSize size);
    Channel::SemaphorePayloadSize GetSemaphorePayloadSize();
    bool Has64bitSemaphores();
    RC SemaphoreAcquireRC(UINT64 data);
    RC SemaphoreAcquireGERC(UINT64 data);
    RC SemaphoreReleaseRC(UINT64 data);
    RC SemaphoreReductionRC(Channel::Reduction reduction, Channel::ReductionType, UINT64 data);

    RC NonStallInterruptRC();
    RC BeginInsertedMethodsRC();
    RC EndInsertedMethodsRC();
    void CancelInsertedMethods();
    UINT32 GetInsertNestingLevel() const { return m_InsertNestingLevel; }
    int MethodFlush();
    RC MethodFlushRC();

    void SetName(const string & name) { m_Name = name; }
    string GetName() const { return m_Name; }

    RC AllocSurfaceRC(Memory::Protect Protect,
                         UINT64 Bytes,
                         _DMA_TARGET Target,
                         _PAGE_LAYOUT Layout,
                         UINT32 *Handle,
                         uintptr_t *Buf,
                         UINT32 Attr = 0,
                         UINT64 *GpuOffset = 0,
                         UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    // Make a context DMA usable within this channel
    RC BindContextDma(UINT32 Handle);

    // objects are also created within a channel

    // creates an object of class 'obj_class' - returns handle on success, 0 on failure
    UINT32 CreateObject(UINT32 Class, void *Params = NULL, UINT32 *retClass = NULL);

    // nukes an object by handle
    void DestroyObject(UINT32 handle);

    // various ways of waiting until work is done
    // all return 1 on success, 0 on failure

    // wait until get == put
    int WaitForDmaPush();
    RC WaitForDmaPushRC();

    // wait for graphics engine to be idle (or in other channel)
    int WaitGrIdle();
    RC WaitGrIdleRC();

    // wait until no pending work for this channel exists
    int WaitForIdle();
    RC WaitForIdleRC();

    // see if any fifo or graphics errors happened
    int CheckForErrors();
    RC CheckForErrorsRC();

    // for channel switching
    int BeginChannelSwitch();
    int EndChannelSwitch();

    RC GetGpPut(UINT32 *GpPutPtr);
    RC GetGpGet(UINT32 *GpGetPtr);
    RC FinishOpenGpFifoEntry(UINT64* pEnd);
    RC SetExternalGPPutAdvanceMode(bool enable);

    virtual unsigned int GetSemaphoreAcquireTimeout(void) const;
    virtual void SetSemaphoreAcquireTimeout(unsigned int acq_to);

    enum ChannelType
    {
        PIO_CHANNEL = 0,
        DMA_CHANNEL = 1,
        GPFIFO_CHANNEL = 2,
    };

    // Create the channel
    void ParseChannelArgs(const ArgReader *params);
    RC AllocChannel(UINT32 engineId, UINT32 HwClass = 0);
    static RC AllocErrorNotifierRC(LWGpuResource *lwgpu,
                                   const ArgReader *params,
                                   Surface2D *pErrorNotifier, LwRm *pLwRm);

    // push buffer creation, nuking
    int DestroyPushBuffer();

    enum UpdateType {
        AUTOMATIC,
        MANUAL
    };

    void SetUpdateType(UpdateType t);

    // Accessor functions to be able to know where the next instruction will be placed
    int GetLwrrentDput();
    void SetLwrrentDput( int new_lwrrent_dput);
    int SetDput(int new_dput);
    UINT32 GetMinDGet(int ch_num, int subch);

    void initCtxSw();
    void SetCtxSwPoints(const vector<int>* pPoints);
    void SetCtxSwPercents(const vector<UINT32>* pPercents);
    void SetCtxSwGran(int gran);
    void SetCtxSwNum(int num);
    void SetCtxSwNumReplay(int num);
    void SetLimiterFileName(const char *);
    void SetCtxSwSeeds(int seed0, int seed1, int seed2);
    int  GetMethodCount() { return methodCount; }
    void SetMethodCount(int n);
    int  GetMethodWriteCount() { return methodWriteCount; }
    void SetMethodWriteCount(int n);
    void SetCtxSwMethods(vector<string>*);
    bool isCtxSwEnabled();
    void setCtxSwDisabled();
    bool GetEnabled() const;
    void SetEnabled(bool enabled);
    void StopSendingMethod();
    void incrCtxSwCnt(int n);
    static UINT32 GetMethodAddr(UINT32 methodStart, UINT32 index, MethodType mode);
    static const UINT32* GetMethodData(const UINT32 *pData, UINT32 index,
                                       MethodType mode, UINT32 count, UINT32* pDataSize);
    bool hasCtxSwEvent(int subchannel, UINT32 methodStart, UINT32 index, UINT32 count,
                       const UINT32 *pData, MethodType mode);
    void SetCtxSwSelf();
    void SetCtxSwBash(bool val);
    void SetCtxSwReset(int val);
    void SetCtxSwLog(int val);
    void SetCtxSwSerial();
    void SetCtxSwSingleStep();
    void SetCtxSwDebug(int val);
    void SetCtxSwType(int val);
    void SetCtxSwResetMask(int val);
    void SetCtxSwResetOnly(int val);
    void SetCtxSwWFIOnly(bool val);
    void SetCtxSwStopPull(bool val);
    void SetCtxSwTimeSlice(int val);
    int GetNumMethodsSent();
    bool SetSuspendCtxSw(bool val);

    void SetTimeSlice(UINT32 slice) { m_TimesliceOverride = true; m_Timeslice = slice; }

    void SetGpFifoEntries(UINT32 val);
    UINT32 GetGpFifoEntries() const { return m_GpFifoEntries; }
    void SetPushbufferSize(UINT64 val);

    void EnableLogging( bool value );

    LWGpuResource* GetLWGpu() const { return lwgpu; };

    struct PushBufferListener
    {
        virtual ~PushBufferListener() {}

        virtual void notify_MethodWrite(LWGpuChannel*, int subch, UINT32 method, MethodType mode) {}
        virtual void notify_Flush(LWGpuChannel*) {}
    };

    void add_listener(PushBufferListener*);
    void remove_listener(PushBufferListener*);

    FLOAT64 GetDefaultTimeoutMs();
    UpdateType GetUpdateType();

    void CtxSwEnable(bool enable) {m_CtxSwEnabled = enable;}
    void GetBuffInfo(BuffInfo* inf, const string& name);

    void SetIsGpuManaged(bool b) { m_IsGpuManaged = b; }
    bool GetIsGpuManaged() const { return m_IsGpuManaged; }

    void SetPreemptive(bool b) { m_Preemptive = b; }
    bool GetPreemptive() const { return m_Preemptive; }

    void SetVprMode(bool b) { m_VprMode = b; }
    bool GetVprMode() const { return m_VprMode; }

    MdiagSurf* GetPreemptCtxBuf() { return &m_PreemptCtxBuf; }
    void SetSkedRefBuf(MdiagSurf *surf) { m_SkedRefBuf = surf; }
    void SetHostRefBuf(MdiagSurf *surf) { m_HostRefBuf = surf; }

    void SetLWGpuTsg(LWGpuTsg* pLWGpuTsg);
    LWGpuTsg* GetLWGpuTsg() { return m_pLWGpuTsg; }

    void SetCtxswPointsBase(UINT32 offset) { ctxswPointsBase = offset; }

    RC GetSubChannelNumClass(UINT32 Handle, UINT32* pRtnSubchNum,
                             UINT32* pRtnSubchClass) const;
    RC GetSubChannelHandle(UINT32 Class, UINT32* pRtnHandle) const;
    void SetTraceSubchNum(UINT32 classId, UINT32 traceSubchNum);
    RC GetSubChannelClass(UINT32 subChannelNum, UINT32* pClassNum);

    UINT32 GetChannelClass() const { return m_HwChannelClass; }

    bool HasGrEngineObjectCreated(bool inTsgScope) const;
    bool HasVideoEngineObjectCreated(bool inTsgScope) const;
    bool HasCopyEngineObjectCreated(bool inTsgScope) const;

    enum AllocChIdMode
    {
        ALLOC_ID_MODE_DEFAULT = 0,
        ALLOC_ID_MODE_GROWUP,
        ALLOC_ID_MODE_GROWDOWN,
        ALLOC_ID_MODE_RANDOM,
        ALLOC_ID_MODE_RM,
        ALLOC_ID_MODS_LAST = ALLOC_ID_MODE_RM
    };

    enum SCGType
    {
        GRAPHICS_COMPUTE0 = 0,
        COMPUTE1,
        SCG_TYPE_END = COMPUTE1
    };

    enum SmpcCtxswMode : UINT32
    {
        SMPC_CTXSW_MODE_NO_CTXSW = 0,
        SMPC_CTXSW_MODE_CTXSW = 1
    };

    RC SetSCGType(SCGType type);
    RC SetCEPrefetch(bool enable);
    void SetPbdma(UINT32 pbdma);
    UINT32 GetPbdma() const { return m_Pbdma; }
    LWGpuResource * GetGpuResource() { return lwgpu; }
    void SetVASpace(const shared_ptr<VaSpace> &pVaSpace);
    const shared_ptr<VaSpace>& GetVASpace() { return m_pVaSpace; }
    LwRm::Handle GetVASpaceHandle() { return m_hVASpace; }
    void SetSubCtx(const shared_ptr<SubCtx> &pSubCtx) { m_pSubCtx = pSubCtx; }
    const shared_ptr<SubCtx>& GetSubCtx() const { return m_pSubCtx; }

    void* GetMutex() const { return m_Mutex; }

    UINT32 GetChannelInitMethodCount() const { return m_ChannelInitMethodCount; }
    void SetChannelInitMethodCount(UINT32 n) { m_ChannelInitMethodCount = n; }

    RC SetupCtxswPm();
    RC SetupCtxswSmpc(SmpcCtxswMode smpcCtxswMode);

    RC GetEngineTypeByHandle(LwRm::Handle objHandle, UINT32* pRtnEngineType) const;
    LwRm* GetLwRmPtr() { return m_pLwRm; };
    SmcEngine* GetSmcEngine() { return m_pSmcEngine; };
    UINT32 GetEngineId() { return m_pCh->GetEngineId(); };

    RC InsertSemMethodsForNonblockingPoll();
    bool CheckSemaphorePayload(UINT32 payloadValue);
    UINT32 GetPayloadValue() const { return m_PayloadForNonblockingPoll; }

    void BeginTestMethods();
    void EndTestMethods();
    bool IsSendingTestMethods() const { return m_SendingTestMethods > 0; }
    UINT32 GetVerifFlags2(UINT32 engineId, UINT32 channelClass);
    void SetAllocChIdMode(const ArgReader *params);
    bool GetUseBar1Doorbell() { return m_UseBar1Doorbell; } 
    static void *GetLwEncObjCreateParam(UINT32 engineOffset, LW_MSENC_ALLOCATION_PARAMETERS *objParam);
    static void *GetLwDecObjCreateParam(UINT32 engineOffset, LW_BSP_ALLOCATION_PARAMETERS *objParam);
    static void* GetLwJpgObjCreateParam(UINT32 engineOffset, LW_LWJPG_ALLOCATION_PARAMETERS *objParam);

private:

    UINT32 GetWriteCount(int subchannel, UINT32 methodStart, UINT32 count,
                         const UINT32 *pData, MethodType mode,
                         bool *pCtxSwitchAfterWrite);

    // for channel switching
    RC SaveChannelState();
    RC RestoreChannelState();
    RC CtxSwResetMask();
    RC CtxSwLog();
    RC CtxSwBash();
    RC CtxSwSerial();
    RC CtxSwSingleStep();
    RC CtxSwDebug(int num);
    RC CtxSwSelf();
    RC CtxSwZlwllDefault();
    RC CtxSwVprMode();
    bool ParseFile
    (
        int* pNum,
        vector<LwS32>* pOutLimiters,
        vector<string>* pOutNames,
        vector<char*>* pOutNamePointers,
        _limiter_table* pLimiterTable
    ) const;
    RC CtxSwType(int);

    void ParseChannelInstLocArgs(const ArgReader *params);

    bool HasSetCtxswPmDone() const; // whether PmCtxsw has been set
    bool HasSetCtxswSmpcDone() const; // whether SmpcCtxsw has been set

    list<PushBufferListener*> m_push_buffer_listeners;

    Channel *m_pCh;
    UINT32 m_hCh;
    MdiagSurf m_Pushbuf;
    MdiagSurf m_GpFifo;
    MdiagSurf m_Err;
    UINT32 m_GpFifoEntries;
    UINT32 m_KickoffThreshold;
    UpdateType m_UpdateType;
    bool m_AutoGpEntryEnable;
    UINT32 m_AutoGpEntryThreshold;
    bool m_UseBar1Doorbell;
    Channel::MinMaxSeed m_RandomAutoFlushThreshold;
    Channel::MinMaxSeed m_RandomAutoFlushThresholdGpFifoEntries;
    Channel::MinMaxSeed m_RandomAutoGpEntryThreshold;
    Channel::MinMaxSeed m_RandomPauseBeforeGPPutWrite;
    Channel::MinMaxSeed m_RandomPauseAfterGPPutWrite;
    string m_PushbufPteKindName;
    string m_GpFifoPteKindName;

    string m_Name;

    LWGpuResource *lwgpu;
    UINT32 ch_num;
    int methodCount;    // the total number of methods this test will send
    int methodWriteCount;    // the total number of method writes this test will send
    UINT32 ctxswPointsBase; // base offset of ctxsw point list
    vector<int> ctxswPoints;   // a sorted, increasing list of the method counts when a context switch will be forced
    vector<UINT32> ctxswPercents; // a list of percentage of testlength.
                                  // it will be colwerted into methodcounts and merged to ctxswPoints.
    vector<AddrRange> ctxswMethods; // list of methods to ctxsw after
    int ctxswIndex;     // the next ctxswPoint we are waiting for
    int ctxswNum;       // number of context switches to do for this test
    int ctxswNumReplay; // number of SpillReplayOnly
    string LimiterFileName;
    int ctxswGran;      // granularity of context switches between MethodWrites (-1 = no switching)
    int ctxswCount;     // number of MethodWrites in between context switches
    int ctxswCountPrev; // Previous place where we did a context switch
    int ctxswPending;   // Number of context switches we skipped due to atoms
    int ctxswSelf;      // set to force a context save and restore of this channel
    bool ctxswBash;     // bash state before restore
    int ctxswReset;     // do a partial gfx reset before doing the restore part of a context switch
    int ctxswLog;
    int ctxswSerial;
    int ctxswSingleStep;
    int ctxswDebug;
    int ctxswType;
    int ctxswResetOnly; // do just the reset at ctxsw points
    bool ctxswWFIOnly;  // just do the WaitForIdel at ctxsw points
    int ctxswResetMask[10];     // array of reset masks
    bool ctxswStopPull; // stop the puller at ctxsw event point
    int ctxswTimeSlice;
    unique_ptr<RandomStream> pRandomStream; // used by ctxswitching code in MethodWrite
    bool ctxswSuspended;

    UINT32 m_IdleMethodCount;
    UINT32 m_IdleMethodLwrrent;
    UINT32 m_SleepBetweenMethods;

    bool m_InstMemLocOverride;
    bool m_RAMFCMemLocOverride;
    bool m_HashTableSizeOverride;
    bool m_TimesliceOverride;
    bool m_EngTimesliceOverride;
    bool m_PbTimesliceOverride;
    bool m_TimescaleOverride;
    _DMA_TARGET m_InstMemLoc;
    _DMA_TARGET m_RAMFCMemLoc;
    UINT32 m_HashTableSize;
    UINT32 m_Timeslice;
    UINT32 m_Timescale;
    UINT32 m_EngTimeslice;
    UINT32 m_PbTimeslice;
    bool m_Timeout;
    unsigned int semAcquireTimeout;

    FLOAT64 m_TimeoutMs;
    bool    m_CtxSwEnabled;
    bool    m_NcohSnoop;
    bool    m_Enabled;        //!< When false, m_pCh points at a null channel
    Channel *m_pNullChannel;  //!< Used when m_Enabled is false
    UINT32  m_InsertNestingLevel; //!< Number of nested BeginInsertedMethods()

    bool m_UseLegacyWaitIdleWithRiskyIntermittentDeadlocking;
    UINT32 m_HwCrcCheckMode;
    bool      m_Preemptive;
    bool      m_VprMode;
    bool      m_IsGpuManaged;
    MdiagSurf m_PreemptCtxBuf;
    MdiagSurf* m_SkedRefBuf;
    MdiagSurf* m_HostRefBuf;
    LWGpuTsg* m_pLWGpuTsg;

    // marking these two values are set by CHANNEL command in trace_3d header
    bool m_GpFifoEntriesFromTrace;
    bool m_PushbufferSizeFromTrace;

    // classId of the channel. default: GF100_CHANNEL_GPFIFO
    UINT32 m_HwChannelClass;

    void CheckScheduleStatus();
    bool m_Scheduled;
    bool m_MethodWritten;

    bool UseFixedSubChannelNum(LwRm::Handle objHandle) const;
    // alloc a subchannel number
    RC AllocSubChannelNum(UINT32 classId, LwRm::Handle objHandle, UINT32* pRtnSubchNum);
    // availale subch number in this channel
    vector<UINT32> m_AvailableSubchNum;
    // map<subchHandle, pair<subchNum, class> >
    map<UINT32, pair<UINT32, UINT32> > m_ObjHandle2SubchNumClassMap;
    // map <classId, traceSpecifiedSubchNum>
    map<UINT32, UINT32> m_TraceSpecifiedSubchNum;
    // subchannel number base
    UINT32 m_SubchNumBase;

    // Channel ID selection mode of RM
    AllocChIdMode m_AllocChIdMode;

    SCGType m_SCGType;
    bool m_CEPrefetchEnable;
    UINT32 m_Pbdma;
    LwRm::Handle m_hVASpace;
    shared_ptr<VaSpace> m_pVaSpace;
    shared_ptr<SubCtx> m_pSubCtx;

    // A mutex used to prevent timing issues when multiple threads send methods
    // and wait-for-idle on the same channel.
    void *m_Mutex;

    // A mutex used to protect WaitIdle() to prevent semaphore(surf2d object)
    // initialization and GpPut update issue.
    void *m_WaitIdleMutex;

    UINT32 m_ChannelInitMethodCount;

    bool m_SetCtxswPmModeDone; // a bool flag to tag whether PmCtxsw Mode is set
    bool m_SetCtxswSmpcModeDone; // a bool flag to tag whether SmpcCtxsw Mode is set

    LwRm* m_pLwRm;
    SmcEngine *m_pSmcEngine;

    unique_ptr<MdiagSurf> m_pSemaphoreForNonblockingPoll;
    UINT32              m_PayloadForNonblockingPoll;
    enum { NON_BLOCKING_SEM_SIZE = 16 };

    UINT32 m_SendingTestMethods = 0;
    UINT32 m_EngineId;
};

//--------------------------------------------------------------------
//! \brief class to represent creation parameter of ce object
//!
//! This class hides the detail about how to generate ce creation
//! parameter buffer for different ce type
//!
//! CEType: There are CE_INSTANCE_MAX copy engines defined whose
//!         instance number is between 0 and CE_INSTANCE_MAX -1;
//!
//!         CE_INSTANCE_MAX is the max instance number;
//!
//!         GRAPHIC_CE is a copy engine betwenn CE0 and CE_MAX
//!         which can co-exist with grapchi engine in same channel.
//!
//!         UNSPECIFIED_CE means cetype is not specified, and
//!         CEObjCreateParam need to select one wisely
class CEObjCreateParam
{
public:
    enum CEType
    {
        CE0 = 0,   // ce instance num0
        CE1 = 1,   // ce instance num1
        CE2 = 2,   // ce instance num2
        CE3 = 3,   // ce instance num3
        CE4 = 4,   // ce instance num4
        CE5 = 5,   // ce instance num5
        CE6 = 6,   // ce instance num6
        CE7 = 7,   // ce instance num7
        CE8 = 8,   // ce instance num8
        CE9 = 9,   // ce instance num9
        LAST_CE = CE9,
        CE_INSTANCE_MAX = LW2080_ENGINE_TYPE_COPY_SIZE, // max instance num
        GRAPHIC_CE,     // ce name
        UNSPECIFIED_CE, // ce name
        END_CE
    };

    CEObjCreateParam(UINT32 type, UINT32 classId);
    ~CEObjCreateParam();

    RC GetObjCreateParam(LWGpuChannel* pLwGpuCh, void** ppRtnObjCreateParams, UINT32* pCeInstance = nullptr);

    static const char* GetCeName(UINT32 ceType);
    static UINT32 GetCeTypeByEngine(UINT32 ceEngineType);
    static UINT32 GetEngineTypeByCeType(UINT32 ceType);

private:
    UINT32 m_CeType;
    UINT32 m_ClassId;
    vector<UINT08> m_ClassParam;
};

#endif
