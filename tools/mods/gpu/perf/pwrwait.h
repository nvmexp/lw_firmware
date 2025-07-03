/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2009,2014-2018,2020-2021 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PWRWAIT_H
#define INCLUDED_PWRWAIT_H

#ifndef INCLUDED_PMU_H
#include "gpu/perf/pmusub.h"
#endif

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

#ifndef INCLUDED_PERFSUB_H
#include "gpu/perf/perfsub.h"
#endif

#ifndef INCLUDED_XP_H
#include "core/include/xp.h"
#endif

#include "gpu/include/dispmgr.h"
#include "gpu/include/rppgimpl.h"
#include "gpu/include/tpcpgimpl.h"

class Display;
class GCxImpl;
class LpwrCtrlImpl;
//--------------------------------------------------------------------
//! \brief Provides virtual interface for power functionality
//!
class PwrWait
{
public:
    enum Action
    {
        DEFAULT_ACT,
        PG_ON,          // ELPG Power Gate On  [so power down engine]
        PG_OFF,         // ELPG Power Gate off [so power up engine]
    };

    virtual ~PwrWait() {}
    virtual RC Initialize() = 0;
    virtual RC Teardown() = 0;
    virtual RC Wait(Action LwrWait, FLOAT64 TimeoutMs) = 0;
};

//--------------------------------------------------------------------
//! \brief Provides an interface for waiting for an engine to become
//!        power gated via ELPG
//!
class ElpgWait : public PwrWait
{
public:

    ElpgWait(GpuSubdevice* pSubdev, PMU::PgTypes Type);
    ElpgWait(GpuSubdevice* pSubdev, PMU::PgTypes Type, bool Verbose);

    virtual ~ElpgWait();

    RC Initialize();
    RC Wait(PwrWait::Action LwrWait, FLOAT64 TimeoutMs);
    RC Teardown();

    struct PgStats
    {
        PgStats():MinMs(~0),MaxMs(0),TotalMs(0),Count(0){};
        void Update(UINT64 StartMs,UINT64 StopMs);
        UINT64      MinMs;    //
        UINT64      MaxMs;    //
        UINT64      TotalMs;  //
        UINT32      Count;    // number of time this event was timed
    };
    void GetPgStats(PgStats *pPgOn, PgStats * pPgOff);

    struct WaitInfo
    {
        INT32            ThreadId;
        ModsEvent        *pPgOnEvent;    // power gate on event
        ModsEvent        *pPgOffEvent;   // power gate off event
        ModsEvent        *pMsgEvent;    // PMU message from RM
        UINT32           RefCount;
        GpuSubdevice     *pSubdev;
        PMU::PgTypes     Type;
        bool             Verbose;
        PgStats          PgOnStats;
        PgStats          PgOffStats;
    };
protected:
    RC WaitPgOn(FLOAT64 TimeoutMs);
    RC WaitPgOff(FLOAT64 TimeoutMs);

private:
    //! this stores all the waits for participating GPUs. Each engine on each
    //! GPU would have an unique Key and WaitInfo instance.
    static map<UINT32, WaitInfo> s_WaitingList;

    static UINT32 GetKey(GpuSubdevice* pSubdev, PMU::PgTypes Type);

    static bool PollElpgPwrUp(void * Args);
    static bool PollElpgPwrDown(void * Args);

    struct PollElpgArgs
    {
        PMU          *pPmu;
        UINT32        pgId;
        PMU::PgCount *pOld;
    };
    RC GetLwrrentPgCount(UINT32 pgId, PMU::PgCount *pNewCnt);

    GpuSubdevice     *m_pSubdev;
    PMU              *m_pPmu;
    PMU::PgTypes     m_Type;
    bool             m_Initialized;
    WaitInfo         *m_pWaitInfo;
    bool             m_Verbose;
    UINT32           m_PollIntervalMs;
    PMU::PgCount     m_PmuPgCounts;
};

//--------------------------------------------------------------------
//! \brief Provides an interface for waiting for the GPU to go into
//!        deep idle
//!

class DeepIdleWait : public ElpgWait
{
public:
    DeepIdleWait(GpuSubdevice *pSubdev,
                 Display      *pDisplay,
                 Surface2D    *pDisplayedImage,
                 FLOAT64       ElpgTimeoutMs,
                 FLOAT64       DeepIdleTimeoutMs,
                 bool          bShowStats,
                 UINT32         OverrideDIPState);

    DeepIdleWait(GpuSubdevice *pSubdev,
                 Display      *pDisplay,
                 Surface2D    *pDisplayedImage,
                 FLOAT64       ElpgTimeoutMs,
                 FLOAT64       DeepIdleTimeoutMs,
                 bool          bShowStats);

    ~DeepIdleWait();

    RC Initialize();

    RC Wait(PwrWait::Action LwrWait, FLOAT64 TimeoutMs);

    RC Teardown();

private:
    RC StartWait(UINT32 *pWidth, UINT32 *pHeight,
                 UINT32 *pDepth, UINT32 *pRefresh, ModsEvent **ppPmuEvent,
                 LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStartStats);
    RC DoWait(ModsEvent *pPmuEvent);
    RC EndWait(UINT32 Width, UINT32 Height,
               UINT32 Depth, UINT32 Refresh, ModsEvent *pPmuEvent,
               LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStartStats,
               RC waitRc);
    RC ElpgMessageWait(ModsEvent *pPmuEvent, FLOAT64 *pTimeoutMs);

    // TBD: Better method is to inherit DeepIdleWait from ElpgWait or use a
    // finite state machine belwase we have to have PG_ON_DONE at beginning
    // for DeepIdle state to be achieved.
    // Idle GrEngine -> ElpgSleep -> fallback to high pstate value (Deepidle Psate) -> Switch on DeepIdle wait
    // Lwrrently the only thing that is stoping us is that the current ElpgWait
    // blocks on PG_ON/CTX_REST events. It cannot 'reset' deepIdle state when an
    // ENG_RST/CTX_REST event oclwrs.
    RC QueryPgCount(const vector<PMU::PgCount> &OldPgCounts,
                    UINT64  *pPrevTime,
                    FLOAT64 *pTimeoutMs);
    RC CpuElpgMessageWait(ModsEvent *pPmuEvent, FLOAT64 *pTimeoutMs);

    void UpdateState();

    //! This enumeration describes the current state of the deep idle
    //! state machine
    enum DeepIdleState
    {
        WAIT_POWERGATE,
        WAIT_DEEPIDLE,
        DONE
    };
    GpuSubdevice *m_pSubdev;                //!< Subdevice to wait on
    Perf         *m_pPerf;                  //!< Perf object
    Display      *m_pDisplay;               //!< Display in use
    Surface2D    *m_pDisplayedImage;        //!< Displayed image
    FLOAT64       m_ElpgTimeoutMs;
    FLOAT64       m_DeepIdleTimeoutMs;
    FLOAT64       m_ElpgWaitTimeRemainingMs;//!< Time remaining to gate all
                                            //!< engines
    FLOAT64       m_DeepIdleTimeRemainingMs;//!< Time remaining to enter deep
                                            //!< idle
    FLOAT64      *m_pTimeoutMs;             //!< Current timeout
    DeepIdleState m_State;                  //!< Deep idle state machine state
    map<UINT32, bool>  m_PgEngState;        //!< Current PG state of each
                                            //!< engine
    bool m_OverridePState;                  //!< Indicates whether the user has
    UINT32 m_DIPState;                      //!< default DIPState
    UINT32 m_PreDIPState;                      //!< Restore point for PState
    Tee::Priority m_bStatsPri;              //!< Before/after statistics print
                                            //!< priority
    bool          m_bInitialized;           //!< true if Initialize has
                                            //!< succeeded
};

//--------------------------------------------------------------------
//! \brief Provides an to toggle Elpg in the background
//!
class ElpgBackgroundToggle
{
public:
        ElpgBackgroundToggle(GpuSubdevice* pSubdev,
                             PMU::PgTypes  Type,
                             FLOAT64       TimeoutMs,
                             FLOAT64       ElpgGatedTimeMs);
        ~ElpgBackgroundToggle();
        RC Start();
        RC Stop();
        RC SetPaused(bool bPause);
private:
        static void ToggleElpg(void *pThis);
        static bool PollPaused(void *pThis);
        void Wakeup();

        GpuSubdevice *m_pSubdev;       //!< GpuSubdevice for the toggle
        PMU::PgTypes m_Type;           //!< Engine being toggled
        FLOAT64      m_TimeoutMs;      //!< Elpg Timeout in Ms
        FLOAT64      m_GatedTimeMs;    //!< Time ELPG will remain gated

        UINT32               m_StartPGCount;  //!< Starting power gate count
                                              //
        unique_ptr<ElpgWait> m_ElpgWait;       //!< ElpgWait to wait for ELPG
                                              //
        Tasker::ThreadID     m_ThreadID;      //!< Thread ID for elpg toggling
        bool                 m_bStopToggling; //!< Flag to stop toggling
        bool                 m_bPendingPause; //!< The pending pause state
        bool                 m_bPaused;       //!< The current pause state
        RC                   m_ToggleRc;      //!< Return value from toggle
                                              //!< thread
        bool                 m_bStarted;      //!< true if toggling has started
};

//--------------------------------------------------------------------
//! \brief Interface to wait on ELPG on CheetAh
//!
class TegraElpgWait: public PwrWait
{
public:
                   TegraElpgWait(GpuSubdevice* pSubdev, INT32 pri)
                   : m_pSubdev(pSubdev), m_Pri(pri) { }
    virtual        ~TegraElpgWait();
    virtual RC     Initialize();
    virtual RC     Teardown();
    virtual RC     Wait(Action LwrWait, FLOAT64 TimeoutMs);
    static  UINT64 ReadTransitions();

private:
    GpuSubdevice* m_pSubdev;
    INT32         m_Pri;
};

//--------------------------------------------------------------------------------------------------
// Core GCx helper class to support GC5/GC6 entry/exit cycles.
class GCxPwrWait : public PwrWait
{
public:
    enum WakeupEvent
    {
        GC6_GPU     = (1 << 0),   // ask the EC to generate a gpu event
        GC6_I2C     = (1 << 1),   // read the therm sensor via I2C
        GC6_TIMER   = (1 << 2),   // program the SCI timer to wakeup the gpu
        RTD3_CPU    = (1 << 7),   // RTD3 CPU event
        GC5_PEX     = (1 << 17),  // Do a PEX transaction, ie Register access
        GC5_I2C     = (1 << 18),  // Do an I2C access on the GPU side
    };

    virtual RC Teardown(){ return OK;}
    GCxPwrWait(GpuSubdevice *  pSubdev,
            LwRm *          pBoundRmClient,
            Display *       pDisplay,
            DisplayMgr::TestContext *pTestContext,
            UINT32          forcedPState,
            bool            bShowStats,
            UINT32          wakeupEvent,
            FLOAT64         timeoutMs):
        m_pGCx(nullptr),
        m_pSubdev(pSubdev),
        m_bInitialized(0),
        m_pBoundRm(pBoundRmClient),
        m_bShowStats(bShowStats),
        m_pDisp(pDisplay),
        m_pTestContext(pTestContext),
        m_ForcedPState(forcedPState),
        m_WakeupEvent(wakeupEvent),
        m_TimeoutMs(timeoutMs),
        m_EnterDelayUsec(0){}

    //----------------------------------------------------------------------------------------------
    // Perform any pre-entry setup
    RC DoEnterWait
    (
        UINT32 *pW, //display width
        UINT32 *pH, //display height
        UINT32 *pD, //display depth
        UINT32 *pR  //display refresh rate
    );

    //----------------------------------------------------------------------------------------------
    // Perform any post-entry cleanup
    RC DoExitWait
    (
        UINT32 width,
        UINT32 height,
        UINT32 depth,
        UINT32 refresh
    );

    void SetGCxResidentTime(UINT32 delayUs) { m_EnterDelayUsec = delayUs; }

    struct WakeupStats
    {
        unsigned int minUs;
        unsigned int maxUs;
        unsigned int avgUs;
        unsigned int exits;
        unsigned int aborts;
        unsigned int miscompares;
        unsigned int diEntries;  // used only for DI_OS & DI_SSC
        unsigned int diExits;    // used only for DI_OS & DI_SSC
        unsigned int diAttempts; // used only for DI_OS & DI_SSC
        unsigned long expectedEntryTimeUs;
        unsigned long actualEntryTimeUs;
        WakeupStats():minUs(~0),maxUs(0),avgUs(0),exits(0),aborts(0),miscompares(0),
        diEntries(0),diExits(0),diAttempts(0),expectedEntryTimeUs(0L),
        actualEntryTimeUs(0L) {}
    };

    enum GCxMode
    {
        GCxModeDisabled,
        GCxModeGC5,
        GCxModeGC6,
        GCxModeRTD3
    };

protected:
    GCxImpl      *  m_pGCx;        // Implementor to execute the GC5/GC6 cycles.
    GpuSubdevice *  m_pSubdev;
    bool            m_bInitialized;
    LwRm         *  m_pBoundRm;
    bool            m_bShowStats;
    Display      *  m_pDisp;
    DisplayMgr::TestContext *m_pTestContext;
    UINT32          m_ForcedPState;
    UINT32          m_WakeupEvent;
    FLOAT64         m_TimeoutMs;
    UINT32          m_EnterDelayUsec;
};

//--------------------------------------------------------------------------------------------------
// Power wait class that implements GC5 entry/exit cycles.
class GC5PwrWait : public GCxPwrWait
{
public:

    GC5PwrWait(GpuSubdevice *  pSubdev,
            LwRm *          pBoundRmClient, // The bound RM client for this test
            Display *       pDisplay,       // The current display that needs to be disabled
            DisplayMgr::TestContext *pTestContext,
            UINT32          forcedPState,   // Force this pstate prior to waiting
            bool            bShowStats,     // If true show statistics
            UINT32          wakeupEvent,    // Type of event to generate a wakeup
            FLOAT64         timeoutMs)      // Timeout for RM control calls
    :GCxPwrWait(pSubdev,
        pBoundRmClient,
        pDisplay,
        pTestContext,
        forcedPState,
        bShowStats,
        wakeupEvent,
        timeoutMs), m_Stats() {}

    void       GetStats(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats);
    virtual RC Initialize();
    virtual RC Wait(Action LwrWait, FLOAT64 TimeoutMs);

private:
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS m_Stats;

};

//--------------------------------------------------------------------------------------------------
// Power wait class that implements GC6 entry/exit cycles.
class GC6PwrWait : public GCxPwrWait
{
public:
    GC6PwrWait(GpuSubdevice *  pSubdev,
            LwRm *          pBoundRmClient,
            Display *       pDisplay,
            DisplayMgr::TestContext *pTestContext,
            UINT32          forcedPState,
            bool            bShowStats,
            UINT32          wakeupEvent,
            FLOAT64         timeoutMs)
    :GCxPwrWait(pSubdev,
       pBoundRmClient,
       pDisplay,
       pTestContext,
       forcedPState,
       bShowStats,
       wakeupEvent,
       timeoutMs) {}
    void       GetStats(Xp::JTMgr::CycleStats * pStats);
    virtual RC Initialize();
    virtual RC Wait(Action LwrWait, FLOAT64 TimeoutMs);

private:
    Xp::JTMgr::CycleStats m_Stats;
};

class GCxBubble
{
public:
    GCxBubble(GpuSubdevice *  pSubdev)
        : m_pSubdev(pSubdev)
        , m_pBoundRm(nullptr)
        , m_bShowStats(false)
        , m_pDisp(nullptr)
        , m_pTestContext(nullptr)
        , m_ForcedPState(8)
        , m_TimeoutMs(1000.0)
        , m_Mode(0)
        {}
    ~GCxBubble();

    bool IsGCxSupported(UINT32 mode, UINT32 forcePstate);
    void SetGCxParams(
            LwRm *          pBoundRmClient,
            Display *       pDisplay,
            DisplayMgr::TestContext *pTestContext,
            UINT32          forcedPState,
            bool            bShowStats,
            FLOAT64         timeoutMs,
            UINT32          mode);
    RC Setup();
    RC ActivateBubble(UINT32 pwrWaitDelayMs);
    RC Cleanup();
    void PrintStats();
    void SetPciCfgRestoreDelayMs(UINT32 delayMs);

private:
    GpuSubdevice *  m_pSubdev;
    LwRm         *  m_pBoundRm;
    bool            m_bShowStats;
    Display      *  m_pDisp;
    DisplayMgr::TestContext *m_pTestContext;
    UINT32          m_ForcedPState;
    FLOAT64         m_TimeoutMs;
    UINT32          m_Mode;
    GCxPwrWait::WakeupStats m_WakeupStats;
    PStateOwner     m_PStateOwner;
    UINT32          m_PciCfgRestoreDelayMs;

    void ReleasePState();
    std::unique_ptr<GCxPwrWait> m_PwrWait;
};

class RppgWait : public PwrWait
{
public:
    RppgWait(GpuSubdevice* pSubdev);
    virtual ~RppgWait();

    virtual RC Initialize();
    virtual RC Teardown();
    virtual RC Wait(PwrWait::Action LwrWait, FLOAT64 TimeoutMs);

    RC Setup(UINT32 rppgMask, Display* pDisp, Surface2D* pDispImage);
    RC GetAllSupported
    (
        bool* grSupported,
        bool* msSupported,
        bool* diSupported
    ) const;

    RC GetAllTpcSupported
    (
        bool* tpcPgSupported,
        bool* adaptiveTpcPgSupported
    ) const;

    RC GetAllTpcEnabled
    (
        bool* pgEnabled,
        bool* adaptivePgEnabled
    ) const;

    void PrintStats(Tee::Priority pri) const;

    struct PollEntryCountArgs
    {
        RppgImpl* grRppgImpl;
        RppgImpl* diRppgImpl;
        UINT32 grEntryCount;
        UINT32 diEntryCount;
    };

private:
    GpuSubdevice* m_pSubdev         = nullptr;
    UINT32        m_ForceRppgMask   = 0;
    UINT32        m_GrEntryCount    = 0;
    UINT32        m_DiEntryCount    = 0;

    Display*      m_pDisplay        = nullptr;
    Surface2D*    m_pDisplayImage   = nullptr;

    bool          m_GrSupported     = false;
    bool          m_MsSupported     = false;
    bool          m_DiSupported     = false;

    bool          m_TpcPgSupported          = false;
    bool          m_AdaptiveTpcPgSupported  = false;
    bool          m_TpcPgEnabled            = false;
    bool          m_AdaptiveTpcPgEnabled    = false;
    UINT32        m_TpcPgIdleThresholdUs    = 0;

    unique_ptr<RppgImpl> m_GrRppgImpl;
    unique_ptr<RppgImpl> m_DiRppgImpl;
    unique_ptr<TpcPgImpl> m_TpcPgImpl;
};

class RppgBubble
{
public:
    RppgBubble(GpuSubdevice* pSubdev);

    bool IsSupported() const;
    RC PrintIsSupported(Tee::Priority pri) const;
    RC Setup(UINT32 rppgMask, Display* pDisp, Surface2D* pDispImage);
    RC ActivateBubble(FLOAT64 timeoutMs) const;
    void PrintStats(Tee::Priority pri) const;

private:
    unique_ptr<RppgWait> m_RppgWait;
};


class TpcPgWait : public PwrWait
{
public:
    TpcPgWait(GpuSubdevice* pSubdev);
    virtual ~TpcPgWait();

    virtual RC Initialize();
    virtual RC Teardown();
    virtual RC Wait(PwrWait::Action LwrWait, FLOAT64 TimeoutMs);

    RC Setup(Display* pDisp, Surface2D* pDispImage);
    RC GetAllSupported(bool* tpcSupported, bool* adaptiveTpcSupported) const;
    RC GetAllEnabled(bool* tpcPgEnabled, bool* adaptiveTpcPgEnabled) const;
    UINT32 GetSleepUs() const;
    void PrintStats(Tee::Priority pri) const;

    struct PollEntryCountArgs
    {
        TpcPgImpl* tpcPgImpl;
        UINT32 tpcEntryCount;
        UINT32 pgThresholdMs;
    };

private:
    GpuSubdevice* m_pSubdev             = nullptr;
    UINT32        m_EntryCount          = 0;
    bool          m_PgEnabled           = 0;
    bool          m_AdaptivePgEnabled   = 0;
    UINT32        m_PgIdleThresholdUs   = 0;
    TpcPgImpl::AdaptivePgIdleThresholds  m_AdaptivePgThresholds = {0, 0, 0}; //$

    Display*      m_pDisplay               = nullptr;
    Surface2D*    m_pDisplayImage          = nullptr;

    bool          m_PgSupported            = false;
    bool          m_AdaptivePgSupported    = false;
    UINT32        m_SleepUs                = 100;
    unique_ptr<TpcPgImpl> m_TpcPgImpl;
};

class TpcPgBubble
{
public:
    TpcPgBubble(GpuSubdevice* pSubdev);

    bool IsSupported() const;
    RC PrintIsSupported(Tee::Priority pri) const;
    RC Setup(Display* pDisp, Surface2D* pDispImage);
    RC ActivateBubble(FLOAT64 timeoutMs) const;
    void PrintStats(Tee::Priority pri) const;

private:
    unique_ptr<TpcPgWait> m_TpcPgWait;
};

class LowPowerPgWait : public PwrWait
{
public:
    LowPowerPgWait(GpuSubdevice* pSubdev);
    virtual ~LowPowerPgWait();

    RC Initialize() override;
    RC Teardown() override;
    RC Wait(PwrWait::Action lwrWait, FLOAT64 timeoutMs) override;

    RC Setup(Display* pDisp, DisplayMgr::TestContext *pTestContext);
    bool GetSupported() const;
    RC GetEnabled(bool *pgEnabled) const;
    FLOAT64 GetSleepUs() const;
    void PrintStats(Tee::Priority pri) const;
    bool IsSupported() const { return m_PgSupported; }
    bool IsEnabled() const { return m_PgEnabled; }

    enum PgFeature
    {
        GR_RPG,
        GPC_RG,
        MSCG,
        MSRPG,
        MSRPPG,
        EI,
        PgDisable = 0xFF
    };

    struct IdPair
    {
        IdPair(UINT32 f = PgDisable, UINT32 s = PgDisable) :
            featureId(f), subfeatureId(s) {}
        UINT32 featureId;
        UINT32 subfeatureId;
    };

    using PgFeatureMap = std::map<PgFeature, IdPair>;
    static PgFeatureMap s_PgFeatureMap;
    static PgFeatureMap FillFeaturesIds();

    struct PollEntryCountArgs
    {
        LpwrCtrlImpl *pgImpl;
        UINT32 pgEntryCount;
        FLOAT64 pgThresholdMs;
        PgFeature feature;
    };

    void SetFeatureId(UINT32 feature);
    char* GetFeatureName() const;
    RC SwitchMode(Tee::Priority pri);
    RC GetEntryCount(UINT32 *pEntryCount) const;
    void SetDelayAfterBubble(FLOAT64 delayMs) { m_DelayAfterBubbleMs = delayMs; }

private:
    GpuSubdevice* m_pSubdev = nullptr;
    UINT32 m_EntryCount = 0;
    bool m_PgEnabled = false;
    bool m_PgSupported = false;
    Display* m_pDisplay = nullptr;
    DisplayMgr::TestContext *m_pTestContext = nullptr;
    FLOAT64 m_SleepUs = 100.0;
    std::unique_ptr<LpwrCtrlImpl> m_PgImpl;
    PgFeature m_Feature = PgFeature::PgDisable;
    UINT32 m_FeatureId = PgFeature::PgDisable;
    UINT32 m_SubFeatureId = PgFeature::PgDisable;
    vector<UINT32> m_Modes;
    UINT32 m_LwrrentMode = 0;
    bool m_DisableDisplay = false;
    FLOAT64 m_DelayAfterBubbleMs = 0;

    RC SetLowPowerMinThreshold() const;
    RC CacheEntryCount();
    RC PerformFeatureBasedSetup();
    void PrintLowPowerStats() const;
    bool IsEISupported() const;
};

// Generic PG bubble capable of engaging features like
// GPC-RG/GR-RPG/MSCG
class LowPowerPgBubble
{
public:
    LowPowerPgBubble(GpuSubdevice* pSubdev);

    bool IsSupported(UINT32 pgFeature) const;
    RC PrintIsSupported(Tee::Priority pri) const;
    RC Setup(Display* pDisp, DisplayMgr::TestContext *pTestContext);
    RC Cleanup();
    RC ActivateBubble(FLOAT64 timeoutMs) const;
    void PrintStats(Tee::Priority pri) const;
    void SetFeatureId(UINT32 feature) const;
    RC SwitchMode(Tee::Priority pri);
    RC GetEntryCount(UINT32 *pEntryCount) const;
    void SetDelayAfterBubble(FLOAT64 delayMs) const;

private:
    std::unique_ptr<LowPowerPgWait> m_LowPowerPgWait;
};
#endif
