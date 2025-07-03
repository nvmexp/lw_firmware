/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/display.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/lpwrctrlimpl.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pwrwait.h"
#include "pmu/pmuifpg.h"
#include "cheetah/include/tegra_gpu_file.h"
#include "t12x/t124/dev_pwr_pri.h"

//*****************************************************************************
// ElpgWait class: allows GPU to trigger ELPG
//*****************************************************************************

// static map
map<UINT32, ElpgWait::WaitInfo> ElpgWait::s_WaitingList;

static void CpuElpgWaitThread(void* pArgs);

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ElpgWait::ElpgWait(GpuSubdevice* pSubdev, PMU::PgTypes Type)
 : m_pSubdev(pSubdev),
   m_pPmu(NULL),
   m_Type(Type),
   m_Initialized(false),
   m_pWaitInfo(NULL),
   m_Verbose(false),
   m_PollIntervalMs(0)
{
}

ElpgWait::ElpgWait(GpuSubdevice* pSubdev, PMU::PgTypes Type, bool Verbose)
 : m_pSubdev(pSubdev),
   m_pPmu(NULL),
   m_Type(Type),
   m_Initialized(false),
   m_pWaitInfo(NULL),
   m_Verbose(Verbose),
   m_PollIntervalMs(0)
{
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
ElpgWait::~ElpgWait()
{
    if (m_Initialized)
    {
        Teardown();
    }
}

//-----------------------------------------------------------------------------
//! \brief Initialize the ElpgWait by kicking off a new Thread and allocating a
//!        new Wait event if there isn't already one (tracked by refcount).
//!        This function make use of s_WaitingList to determine whether create
//!        a new WaitInfo object.  If one exists the refcount is incremented
//!
//! \return OK if successful, not OK otherwise
//!
RC ElpgWait::Initialize()
{
    RC rc;
    CHECK_RC(m_pSubdev->GetPmu(&m_pPmu));
    if (m_pPmu->IsPmuElpgEnabled())
    {
        CHECK_RC(m_pPmu->GetLwrrentPgCount(m_Type, &m_PmuPgCounts));
        m_Initialized = true;
        return rc;
    }

    UINT32 Key = GetKey(m_pSubdev, m_Type);
    map<UINT32, WaitInfo>::iterator pWaitInfo = s_WaitingList.find(Key);

    // Create a new Thread/Event if current type of PMU wait does not exist on
    // the GPU
    if (pWaitInfo == s_WaitingList.end())
    {
        WaitInfo NewIdle = {0};
        s_WaitingList[Key] = NewIdle;
        m_pWaitInfo        = &(s_WaitingList[Key]);
        m_pWaitInfo->RefCount  = 1;
        m_pWaitInfo->pSubdev   = m_pSubdev;
        m_pWaitInfo->Type      = m_Type;
        m_pWaitInfo->Verbose   = m_Verbose;

        bool EventAllocFailed = false;
        m_pWaitInfo->pPgOnEvent = Tasker::AllocEvent("ElpgWait PgOn event");
        m_pWaitInfo->pPgOffEvent = Tasker::AllocEvent("ElpgWait Pg Off event");
        EventAllocFailed = (m_pWaitInfo->pPgOnEvent == NULL) ||
                           (m_pWaitInfo->pPgOffEvent == NULL);
        m_pWaitInfo->pMsgEvent = m_pPmu->AllocEvent(PMU::EVENT_UNIT_ID,
                                                    false,
                                                    RM_PMU_UNIT_LPWR);
        if (EventAllocFailed || (m_pWaitInfo->pMsgEvent == NULL))
        {
            Tasker::FreeEvent(m_pWaitInfo->pPgOnEvent);
            Tasker::FreeEvent(m_pWaitInfo->pPgOffEvent);
            Tasker::FreeEvent(m_pWaitInfo->pMsgEvent);
            return RC::CANNOT_ALLOCATE_MEMORY;
        }
        m_pWaitInfo->ThreadId = Tasker::CreateThread(CpuElpgWaitThread,
                                                     m_pWaitInfo,
                                                     0,
                                                     "Wait Thread - CPU Elpg\n");
    }
    else
    {
        pWaitInfo->second.RefCount++;
        m_pWaitInfo = &(pWaitInfo->second);
    }

    m_Initialized = true;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform the actual wait for the type of ELPG to occur
//!
//! \return OK if successful, not OK otherwise
//!
RC ElpgWait::Wait(PwrWait::Action WhatToWait, FLOAT64 TimeoutMs)
{
    if (!m_Initialized)
    {
        Printf(Tee::PriNormal, "ElpgWait not initialzed properly\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    switch (WhatToWait)
    {
        case PG_ON:
            return WaitPgOn(TimeoutMs);
        case PG_OFF:
            return WaitPgOff(TimeoutMs);
        default:
            return RC::BAD_PARAMETER;
    }
}
//------------------------------------------------------------------------------
bool ElpgWait::PollElpgPwrUp(void * Args)
{
    RC rc;
    MASSERT(Args);
    PollElpgArgs *args = static_cast<PollElpgArgs *>(Args);

    PMU::PgCount lwr;
    rc = args->pPmu->GetLwrrentPgCount(args->pgId, &lwr);

    bool bDone = (OK != rc) ||
                 ((args->pOld->pwrOn != lwr.pwrOn) &&
                  !args->pPmu->IsPowerGated(static_cast<PMU::PgTypes>(args->pgId)));
    if (bDone && (OK == rc))
        *args->pOld = lwr;
    return bDone;
}
//------------------------------------------------------------------------------
bool ElpgWait::PollElpgPwrDown(void * Args)
{
    RC rc;
    MASSERT(Args);
    PollElpgArgs *args = static_cast<PollElpgArgs *>(Args);

    PMU::PgCount lwr;
    rc = args->pPmu->GetLwrrentPgCount(args->pgId, &lwr);

    bool bDone = (OK != rc) ||
                 ((args->pOld->pwrOff != lwr.pwrOff) &&
                  args->pPmu->IsPowerGated(static_cast<PMU::PgTypes>(args->pgId)));
    if (bDone && (OK == rc))
        *args->pOld = lwr;
    return bDone;
}

//-----------------------------------------------------------------------------
void ElpgWait::GetPgStats(PgStats *pPgOn, PgStats * pPgOff)
{
    if(m_pWaitInfo)
    {
        *pPgOn = m_pWaitInfo->PgOnStats;
        *pPgOff  = m_pWaitInfo->PgOffStats;
    }
}
//-----------------------------------------------------------------------------
RC ElpgWait::WaitPgOn(FLOAT64 TimeoutMs)
{
    RC rc;
    if((m_Type != PMU::ELPG_GRAPHICS_ENGINE) &&
       (m_Type != PMU::ELPG_VIDEO_ENGINE) &&
       (m_Type != PMU::ELPG_VIC_ENGINE) &&
       (m_Type != PMU::PWR_RAIL))
        return RC::SOFTWARE_ERROR;

    if (!m_pPmu->IsPmuElpgEnabled())
    {
        Tasker::ResetEvent(m_pWaitInfo->pPgOnEvent);
    }

    if (m_pPmu->IsPowerGated(m_Type))
    {
        Tasker::Yield();
        return OK;
    }

    Printf(Tee::PriLow, "block until power gates\n");
    const UINT64 startMs = Xp::GetWallTimeMS();

    if (m_pPmu->IsPmuElpgEnabled())
    {
        PollElpgArgs pollArgs;
        pollArgs.pPmu      = m_pPmu;
        pollArgs.pgId      = m_Type;
        pollArgs.pOld      = &m_PmuPgCounts;
        CHECK_RC(POLLWRAP(PollElpgPwrDown, &pollArgs, TimeoutMs));
    }
    else
    {
        CHECK_RC(Tasker::WaitOnEvent(m_pWaitInfo->pPgOnEvent, TimeoutMs));
        // Collect statistics on this event
        m_pWaitInfo->PgOnStats.Update(startMs,Xp::GetWallTimeMS());
    }

    return rc;
}
//-----------------------------------------------------------------------------
RC ElpgWait::WaitPgOff(FLOAT64 TimeoutMs)
{
    RC rc;

    if((m_Type != PMU::ELPG_GRAPHICS_ENGINE) &&
       (m_Type != PMU::ELPG_VIDEO_ENGINE) &&
       (m_Type != PMU::ELPG_VIC_ENGINE) &&
       (m_Type != PMU::PWR_RAIL))
        return RC::SOFTWARE_ERROR;

    if (!m_pPmu->IsPmuElpgEnabled())
    {
        Tasker::ResetEvent(m_pWaitInfo->pPgOffEvent);
    }

    if (!m_pPmu->IsPowerGated(m_Type))
    {
        Tasker::Yield();
        return OK;
    }

    const UINT64 startMs = Xp::GetWallTimeMS();
    if (m_pPmu->IsPmuElpgEnabled())
    {
        PollElpgArgs pollArgs;
        pollArgs.pPmu      = m_pPmu;
        pollArgs.pgId      = m_Type;
        pollArgs.pOld      = &m_PmuPgCounts;
        CHECK_RC(POLLWRAP(PollElpgPwrUp,
                          &pollArgs,
                          TimeoutMs));
    }
    else
    {
        CHECK_RC(Tasker::WaitOnEvent(m_pWaitInfo->pPgOffEvent, TimeoutMs));
        // Collect statistics on this event
        m_pWaitInfo->PgOffStats.Update(startMs,Xp::GetWallTimeMS());
    }

    return rc;
}
//-----------------------------------------------------------------------------
// Update the current power gate stats.
void ElpgWait::PgStats::Update(const UINT64 StartMs, const UINT64 StopMs)
{
    // Collect statistics on this event
    const UINT64 delta = StopMs - StartMs;
    Count++;
    TotalMs += delta;

    MinMs = MIN(delta,MinMs);
    MaxMs = MAX(delta,MaxMs);
}
//-----------------------------------------------------------------------------
//! \brief Decrement the ref count and destroy the wait if it is non-zero
//!
//! \return OK if successful, not OK otherwise
//!
RC ElpgWait::Teardown()
{
    RC rc;
    if (!m_Initialized)
    {
        // do nothing
        return OK;
    }

    if (!m_pPmu->IsPmuElpgEnabled())
    {
        m_pWaitInfo->RefCount--;
        if (m_pWaitInfo->RefCount == 0)
        {
            PMU *pPmu;
            m_pWaitInfo->pSubdev->GetPmu(&pPmu);
            // allow the wait thread to finish
            Tasker::SetEvent(m_pWaitInfo->pMsgEvent);
            CHECK_RC(Tasker::Join(m_pWaitInfo->ThreadId));
            Tasker::FreeEvent(m_pWaitInfo->pPgOnEvent);
            m_pWaitInfo->pPgOnEvent = NULL;
            Tasker::FreeEvent(m_pWaitInfo->pPgOffEvent);
            m_pWaitInfo->pPgOffEvent = NULL;
            pPmu->FreeEvent(m_pWaitInfo->pMsgEvent);
            m_pWaitInfo->pMsgEvent = NULL;
            // remove the current one from map
            s_WaitingList.erase(GetKey(m_pSubdev, m_Type));
        }
    }

    m_Initialized = false;
    m_pWaitInfo   = NULL;
    m_pSubdev     = NULL;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Static function to translate a GpuSubdevice/Engine pair into a
//!        a key used by the waiting list
//!
//! \param pSubdev : Pointer to the subdevice
//! \param Type    : Type of wait
//!
//! \return Key used for indexing into the waiting list
//!
UINT32 ElpgWait::GetKey(GpuSubdevice* pSubdev, PMU::PgTypes Type)
{
    UINT32 GpuMask = 1 << pSubdev->GetGpuInst();

    // we only allow up to 16 GPUs
    MASSERT(!(GpuMask & 0xFFFF0000));
    UINT32 Key = ((Type << 16)|(GpuMask & 0xFFFF));
    return Key;
}

//-----------------------------------------------------------------------------
//! \brief Thread that waits for elpg events and updates the state.  A
//!        ModsEvent is signaled when the desired power gating state has been
//!        reached
//!
//! \param pArgs : Void pointer to the ElpgWait class instance doing the waiting
//!
static void CpuElpgWaitThread(void* pArgs)
{
    ElpgWait::WaitInfo *pWaitInfo = (ElpgWait::WaitInfo*)pArgs;
    PMU *pPmu;
    pWaitInfo->pSubdev->GetPmu(&pPmu);

    RC rc;
    while (pWaitInfo->RefCount)
    {
        // wait for messages from RM
        if (OK != (rc = Tasker::WaitOnEvent(pWaitInfo->pMsgEvent, Tasker::NO_TIMEOUT)))
        {
            Printf(Tee::PriNormal, "ElpgWaitThread exiting - due to rc = %s\n",
                   rc.Message());
            return;
        }
        Tasker::ResetEvent(pWaitInfo->pMsgEvent);

        vector<PMU::Message> ElpgMessages;
        if (OK != (rc = pPmu->GetMessages(pWaitInfo->pMsgEvent, &ElpgMessages)))
        {
            Printf(Tee::PriNormal, "PMU::GetMessages failed in PMU wait thread\n");
            return;
        }
    }
}

//*****************************************************************************
// DeepIdleWait class: allows GPU to trigger DeepIdle
//*****************************************************************************

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
DeepIdleWait::DeepIdleWait
(
    GpuSubdevice *pSubdev,
    Display      *pDisplay,
    Surface2D    *pDisplayedImage,
    FLOAT64       ElpgTimeoutMs,
    FLOAT64       DeepIdleTimeoutMs,
    bool          bShowStats
)
:  ElpgWait(pSubdev,
        PMU::ELPG_GRAPHICS_ENGINE,
        bShowStats),
    m_pSubdev(pSubdev),
    m_pDisplay(pDisplay),
    m_pDisplayedImage(pDisplayedImage),
    m_ElpgTimeoutMs(ElpgTimeoutMs),
    m_DeepIdleTimeoutMs(DeepIdleTimeoutMs),
    m_ElpgWaitTimeRemainingMs(0.0),
    m_DeepIdleTimeRemainingMs(0.0),
    m_pTimeoutMs(nullptr),
    m_State(WAIT_POWERGATE),
    m_OverridePState(false),
    m_DIPState(0),
    m_bInitialized(false)
{
    m_bStatsPri = bShowStats ? Tee::PriNormal : Tee::PriLow;
    m_pPerf =     pSubdev->GetPerf();
    m_pPerf->GetLwrrentPState(&m_PreDIPState);
}

DeepIdleWait::DeepIdleWait
(
    GpuSubdevice *pSubdev,
    Display      *pDisplay,
    Surface2D    *pDisplayedImage,
    FLOAT64       ElpgTimeoutMs,
    FLOAT64       DeepIdleTimeoutMs,
    bool          bShowStats,
    UINT32        DIPState
)
:  ElpgWait(pSubdev,
        PMU::ELPG_GRAPHICS_ENGINE,
        bShowStats),
    m_pSubdev(pSubdev),
    m_pDisplay(pDisplay),
    m_pDisplayedImage(pDisplayedImage),
    m_ElpgTimeoutMs(ElpgTimeoutMs),
    m_DeepIdleTimeoutMs(DeepIdleTimeoutMs),
    m_ElpgWaitTimeRemainingMs(0.0),
    m_DeepIdleTimeRemainingMs(0.0),
    m_pTimeoutMs(nullptr),
    m_State(WAIT_POWERGATE),
    m_OverridePState(true),
    m_DIPState(DIPState),
    m_bInitialized(false)
{
    m_bStatsPri = bShowStats ? Tee::PriNormal : Tee::PriLow;
    m_pPerf =     m_pSubdev->GetPerf();
    m_pPerf->GetLwrrentPState(&m_PreDIPState);
}

//-----------------------------------------------------------------------------
//! \brief Destructor
//!
DeepIdleWait::~DeepIdleWait()
{
    Teardown();
}

//-----------------------------------------------------------------------------
//! \brief Initialize the DeepIdleWait object
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::Initialize()
{
    PMU  *pPmu;
    RC    rc;
    //New Elpg logic has moved to PMU

    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(1);
    CHECK_RC(ElpgWait::Initialize());

    CHECK_RC(m_pSubdev->GetPmu(&pPmu));

    //Find num of power gatable engines first
     pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES;
    pgParams[0].parameterExtended = 0;
    CHECK_RC(pPmu->GetPowerGatingParameters(&pgParams));
    pgParams.resize(pgParams[0].parameterValue);

    for (UINT32 engidx = 0; engidx < pgParams.size(); engidx++)
    {
        pgParams[engidx].parameter =
            LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED;
        pgParams[engidx].parameterExtended = engidx;
    }
    CHECK_RC(pPmu->GetPowerGatingParameters(&pgParams));
    for (UINT32 i = 0; i < pgParams.size(); i++)
    {
        if (0 != pgParams[i].parameterValue)
        {
            Printf(Tee::PriDebug, "Engine: %u is power gatable\n", i);
            m_PgEngState.insert(pair<UINT32,bool>(i,true));
        }
    }
    m_bInitialized = true;
    return rc;
}
//-----------------------------------------------------------------------------
//! \brief Perform the actual DeepIdleWait.  This function is guaranteed to
//!        wait for at least DeepIdleTimeoutMs (since it is not possible to
//!        check for deep idle without waking up from deep idle).
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::Wait(PwrWait::Action Action, FLOAT64 TimeoutMs)
{
    StickyRC firstRc;
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 refresh;
    ModsEvent *pPmuEvent;
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS startStats = { 0 };

    if (!m_bInitialized)
    {
        Printf(Tee::PriNormal, "DeepIdleWait not initialzed properly\n");
        return RC::WAS_NOT_INITIALIZED;
    }
    firstRc = StartWait(&width, &height, &depth, &refresh, &pPmuEvent,
            &startStats);
    if (OK == firstRc)
    {
        firstRc = DoWait(pPmuEvent);
    }
    firstRc = EndWait(width, height, depth, refresh, pPmuEvent,
            &startStats, firstRc);

    return firstRc;
}

//-----------------------------------------------------------------------------
//! \brief Teardown the DeepIdleWait object
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::Teardown()
{
    RC rc;
    UINT32 lwrPState;
    m_pPerf->SetUseHardPStateLocks(true);
    CHECK_RC(m_pPerf->GetLwrrentPState(&lwrPState));
    if (lwrPState != m_PreDIPState)
    {
        CHECK_RC(m_pPerf->ForcePState(m_PreDIPState,
                    LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));
    }
    m_PgEngState.clear();
    m_bInitialized = false;
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to start the wait.  Sets the PState and disables
//!        the display so that deep idle can actually occur
//!
//! \param pWidth     : Returned width of the current display
//! \param pHeight    : Returned height of the current display
//! \param pDepth     : Returned depth of the current display
//! \param pRefresh   : Returned refresh rate of the current display
//! \param ppPmuEvent : Returned pointer to PMU event for checking
//!                     ELPG messages
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::StartWait
(
    UINT32 *pWidth,
    UINT32 *pHeight,
    UINT32 *pDepth,
    UINT32 *pRefresh,
    ModsEvent **ppPmuEvent,
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStartStats
)
{
    PMU  *pPmu;
    RC    rc;

    *pWidth = 0;
    *pHeight = 0;
    *pDepth = 0;
    *pRefresh = 0;
    *ppPmuEvent = NULL;

    CHECK_RC(m_pSubdev->GetPmu(&pPmu));

    if( m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        CHECK_RC(m_pDisplay->GetMode(m_pDisplay->Selected(),
                                     pWidth, pHeight, pDepth, pRefresh));
    }
    if (!m_OverridePState)
    {
        CHECK_RC(m_pPerf->GetDeepIdlePState(&m_DIPState));

    }

    Printf(Tee::PriDebug, "%s: Deepidle Pstate %u\n",
            __FUNCTION__, m_DIPState);

    CHECK_RC(pPmu->GetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS,
                                         pStartStats));
    if( m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        CHECK_RC(m_pDisplay->Disable());
    }
    *ppPmuEvent = pPmu->AllocEvent(PMU::EVENT_UNIT_ID,
                                   false,
                                   RM_PMU_UNIT_LPWR);
    m_State = WAIT_POWERGATE;

    m_ElpgWaitTimeRemainingMs = m_ElpgTimeoutMs;
    m_DeepIdleTimeRemainingMs = m_DeepIdleTimeoutMs;

    Printf(m_bStatsPri,
           "Deep Idle Starting Stats :Attempts (%d), Entries (%d), Exits(%d)\n",
           (UINT32)pStartStats->attempts, (UINT32)pStartStats->entries,
           (UINT32)pStartStats->exits);
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to actually wait for deep idle to occur
//!
//! \param pPmuEvent : Pointer to PMU event for checking ELPG messages
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::DoWait(ModsEvent *pPmuEvent)
{
    RC rc;
    PMU* pPmu;
    CHECK_RC(m_pSubdev->GetPmu(&pPmu));
    map<UINT32, bool>::iterator it = m_PgEngState.begin();

    for ( ; it != m_PgEngState.end(); it++)
    {
        it->second = (it->second || pPmu->IsPowerGated((PMU::PgTypes)(it->first)));
    }
    vector <PMU::PgCount> OrgPgCount(m_PgEngState.size()); //NumPwrGatable Engines
    if (pPmu->IsPmuElpgEnabled())
    {
        UINT32 i = 0;
        for ( it = m_PgEngState.begin() ; it != m_PgEngState.end(); it++)
        {
            rc = pPmu->GetLwrrentPgCount(it->first, &(OrgPgCount[i]));
            Printf(Tee::PriDebug,
                    "OrgPgCount Pgid[%d]: PwrOff = %d, PwrOn = %d\n",
                    it->first, OrgPgCount[i].pwrOff, OrgPgCount[i].pwrOn);
            i++;
        }
    }

    m_pPerf->SetUseHardPStateLocks(true);
    Printf(Tee::PriDebug, "%s: Deepidle Pstate %u\n", __FUNCTION__, m_DIPState);
    CHECK_RC(m_pPerf->ForcePState(m_DIPState,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));

    UINT64 LwrTime = Platform::GetTimeMS(); //Debug Help
    while (m_State != DONE)
    {
        UpdateState();
        if(pPmu->IsPmuElpgEnabled())
        {
            rc = QueryPgCount(OrgPgCount, &LwrTime, m_pTimeoutMs);
        }
        else
        {
            rc = CpuElpgMessageWait(pPmuEvent, m_pTimeoutMs);
        }
        switch (m_State)
        {
            case WAIT_POWERGATE:
                CHECK_RC(rc);
                break;
            case WAIT_DEEPIDLE:
                if (rc == RC::TIMEOUT_ERROR)
                {
                    m_State = DONE;
                    rc.Clear();
                }
                else
                {
                    CHECK_RC(rc);
                }
                break;
            default:
                break;
        }
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to end the wait.  Restores the PState and display
//!
//! \param Width     : Width of the display to restore
//! \param Height    : Height of the display to restore
//! \param Depth     : Depth of the display to restore
//! \param Refresh   : Refresh rate of the display to restore
//! \param pPmuEvent : Pointer to PMU event to free
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::EndWait
(
    UINT32 Width,
    UINT32 Height,
    UINT32 Depth,
    UINT32 Refresh,
    ModsEvent *pPmuEvent,
    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStartStats,
    RC     waitRc
)
{
    StickyRC firstRc;
    PMU  *pPmu;
    firstRc = m_pSubdev->GetPmu(&pPmu);

    if ((Width != 0) && (Height != 0) && (Depth != 0) && (Refresh != 0) &&
        m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        firstRc = m_pDisplay->SetMode(Width, Height, Depth, Refresh);
        if (m_pDisplayedImage)
        {
            firstRc = m_pDisplay->SetImage(m_pDisplayedImage);
        }
    }

    if(m_pSubdev->HasBug(1042805))
    {
        //Set Pstate 0 before reading DeepIdle Statistics to get pmu counters updated
        //quickly. This is a workaround. Before removing please contact RM team
        firstRc = m_pPerf->ForcePState(0,
                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR);
    }

    if (waitRc == OK)
    {
        bool bFailDeepIdle;

        LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS endStats = { 0 };
        RC getStatsRc;
        firstRc = getStatsRc = pPmu->GetDeepIdleStatistics(
                PMU::DEEP_IDLE_NO_HEADS, &endStats);
        bFailDeepIdle =
            ((getStatsRc == OK) && (endStats.entries <= pStartStats->entries));
        Printf(bFailDeepIdle ? Tee::PriNormal : m_bStatsPri,
               "Deep Idle Ending Stats   : Attempts (%d), "
               "Entries (%d), Exits(%d)\n",
               (UINT32)endStats.attempts, (UINT32)endStats.entries,
               (UINT32)endStats.exits);
        if (bFailDeepIdle)
        {
            Printf(Tee::PriError, "Failed to enter Deep Idle within %fms\n",
                   m_DeepIdleTimeoutMs);
            firstRc = RC::TIMEOUT_ERROR;
        }
    }

    if (NULL != pPmuEvent)
    {
        pPmu->FreeEvent(pPmuEvent);
    }
    return firstRc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to monitor power gate counter changes and
//         update the current deepIdle state machine
RC DeepIdleWait::QueryPgCount(const vector<PMU::PgCount> &OldPgCounts,
                              UINT64  *pPrevTime,
                              FLOAT64 *pTimeoutMs)
{
    RC rc;
    UINT64 LwrTime = Platform::GetTimeMS();
    UINT64 elapsedTime = LwrTime - *pPrevTime;
    if (elapsedTime > *pTimeoutMs)
    {
        *pTimeoutMs = 0;
        //This is not an error. The RC status is used just to record the event.
        Printf(Tee::PriDebug, "\t%s: DeepIdle Timedout; Now end Polling\n",
                __FUNCTION__);
        return RC::TIMEOUT_ERROR;
    }
    else
    {
        *pPrevTime  = LwrTime;
        *pTimeoutMs = *pTimeoutMs - elapsedTime;
    }

    PMU *pPmu;
    CHECK_RC(m_pSubdev->GetPmu(&pPmu));

    UINT32 PgCount = 0;
    map<UINT32, bool>::iterator it = m_PgEngState.begin();

    //Wait for PMU elpg
    CHECK_RC(WaitPgOn(*pTimeoutMs));

    for ( ; it != m_PgEngState.end(); it++)
    {
        UINT32 engId = it->first;
        PMU::PgCount NewPgCount;
        rc = pPmu->GetLwrrentPgCount(it->first, &NewPgCount);
        if (rc != OK)
            continue;
        Printf(Tee::PriLow,
               "DeepIdleWait Eng: %d, PwrOff=%d, Old=%d\t"
               "PwrOn=%d, Old=%d\n",
               engId, NewPgCount.pwrOff, OldPgCounts[engId].pwrOff,
               NewPgCount.pwrOn, OldPgCounts[engId].pwrOn);
        PgCount++;
        it->second = true;
    }

    UINT32 LwrrPstate = Perf::ILWALID_PSTATE;
    CHECK_RC(m_pPerf->GetLwrrentPState(&LwrrPstate));
    m_pPerf->SetUseHardPStateLocks(true);
    //Forcing any pstate causes RM to try and arm DeepIdle
    CHECK_RC(m_pPerf->ForcePState(LwrrPstate,
                                LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));

    // all power gated? Stop the polling for PgCount, and just sleep until timeout
    if (PgCount == m_PgEngState.size())
    {
        *pTimeoutMs = m_DeepIdleTimeRemainingMs;
        Printf(Tee::PriDebug, "sleep for %fms\n", *pTimeoutMs);
        Tasker::Sleep(m_DeepIdleTimeRemainingMs);
    }
    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Private function to wait for ELPG messages and update the current
//!        state
//!
//! \param pPmuEvent  : ELPG PMU event to wait for
//! \param pTimeoutMs : Pointer to the current timeout is Ms (updated after the
//!                     wait is complete)
//!
//! \return OK if successful, not OK otherwise
//!
RC DeepIdleWait::CpuElpgMessageWait(ModsEvent *pPmuEvent, FLOAT64 *pTimeoutMs)
{
    RC rc;
    UINT64 startTime = Platform::GetTimeMS();
    UINT64 elapsedTime;
    PMU *pPmu;

    CHECK_RC(m_pSubdev->GetPmu(&pPmu));
    if (*pTimeoutMs == 0)
    {
        return RC::TIMEOUT_ERROR;
    }
    // wait for messages from RM
    rc = Tasker::WaitOnEvent(pPmuEvent, *pTimeoutMs);
    if (rc == RC::TIMEOUT_ERROR)
    {
        *pTimeoutMs = 0;
        return rc;
    }
    elapsedTime = (Platform::GetTimeMS() - startTime);
    if (elapsedTime > *pTimeoutMs)
    {
        *pTimeoutMs = 0;
    }
    else
    {
        *pTimeoutMs = *pTimeoutMs - elapsedTime;
    }

    Tasker::ResetEvent(pPmuEvent);
    CHECK_RC(rc);

    vector<PMU::Message> ElpgMessages;
    CHECK_RC(pPmu->GetMessages(pPmuEvent, &ElpgMessages));

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Private function to update the deep idle state machine
//!
void DeepIdleWait::UpdateState()
{
    m_State = WAIT_DEEPIDLE;
    m_pTimeoutMs = &m_DeepIdleTimeRemainingMs;
    map<UINT32, bool>::iterator it = m_PgEngState.begin();
    for ( ; it != m_PgEngState.end(); it++)
    {
        if ( (*it).second == false)
        {
            m_State = WAIT_POWERGATE;
            m_pTimeoutMs = &m_ElpgWaitTimeRemainingMs;
        }
    }
}

//-----------------------------------------------------------------------------
//! \brief Constructor
//!
ElpgBackgroundToggle::ElpgBackgroundToggle(GpuSubdevice* pSubdev,
                                           PMU::PgTypes  Type,
                                           FLOAT64       TimeoutMs,
                                           FLOAT64       ElpgGatedTimeMs)
:   m_pSubdev(pSubdev),
    m_Type(Type),
    m_TimeoutMs(TimeoutMs),
    m_GatedTimeMs(ElpgGatedTimeMs),
    m_StartPGCount(0),
    m_ThreadID(Tasker::NULL_THREAD),
    m_bStopToggling(false),
    m_bPendingPause(false),
    m_bPaused(false),
    m_bStarted(false)
{ }

ElpgBackgroundToggle::~ElpgBackgroundToggle()
{
    Stop();
}
//-----------------------------------------------------------------------------
//! \brief Start toggling ELPG in the background
//!
//! \return OK if successfully started, not OK otherwise
RC ElpgBackgroundToggle::Start()
{
    RC rc;

    if ((m_Type != PMU::ELPG_GRAPHICS_ENGINE) &&
        (m_Type != PMU::ELPG_VIDEO_ENGINE) &&
        (m_Type != PMU::ELPG_VIC_ENGINE))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (m_ElpgWait.get() == NULL)
    {
        m_ElpgWait.reset(new ElpgWait(m_pSubdev, m_Type));
    }

    if (m_ElpgWait.get() == NULL)
    {
        return RC::CANNOT_ALLOCATE_MEMORY;
    }

    CHECK_RC(m_ElpgWait->Initialize());

    PMU *pPmu;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(1);
    CHECK_RC(m_pSubdev->GetPmu(&pPmu));

    pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
    pgParams[0].parameterExtended = m_Type;

    // Get the starting power gate count for the test
    CHECK_RC(pPmu->GetPowerGatingParameters(&pgParams));
    m_StartPGCount = pgParams[0].parameterValue;

    if (m_ThreadID == Tasker::NULL_THREAD)
    {
        m_bStopToggling = false;
        m_bPaused = false;
        m_bPendingPause = false;
        m_ToggleRc = OK;
        m_ThreadID = Tasker::CreateThread(ToggleElpg, this,
                                          Tasker::MIN_STACK_SIZE,
                                          "ElpgBackgroundToggle::ToggleElpgTask");
    }

    m_bStarted = true;

    return OK;
}

//-----------------------------------------------------------------------------
//! \brief Stop toggling ELPG in the background
//!
//! \return OK if successfully stopped, not OK otherwise
RC ElpgBackgroundToggle::Stop()
{
    StickyRC firstRc;

    if (!m_bStarted)
        return OK;

    if ((m_Type != PMU::ELPG_GRAPHICS_ENGINE) &&
        (m_Type != PMU::ELPG_VIDEO_ENGINE) &&
        (m_Type != PMU::ELPG_VIC_ENGINE))
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_bStarted = false;

    if (m_ThreadID != Tasker::NULL_THREAD &&
        m_ThreadID != Tasker::LwrrentThread())
    {
        m_bStopToggling = true;
        firstRc = Tasker::Join(m_ThreadID);
        m_ThreadID = Tasker::NULL_THREAD;
    }

    if (m_ElpgWait.get() != NULL)
    {
        firstRc = m_ElpgWait->Teardown();
    }

    PMU *pPmu;
    vector<LW2080_CTRL_POWERGATING_PARAMETER> pgParams(1);
    RC getPmuRc;
    firstRc = getPmuRc = m_pSubdev->GetPmu(&pPmu);

    if (getPmuRc == OK)
    {
        pgParams[0].parameter = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
        pgParams[0].parameterExtended = m_Type;

        // Get the ending power gate count for the test
        firstRc = pPmu->GetPowerGatingParameters(&pgParams);

        Printf(Tee::PriNormal, "ElpgBackgroundToggle : %d power gating events oclwrred\n",
               (UINT32)(pgParams[0].parameterValue - m_StartPGCount));
    }

    firstRc = m_ToggleRc;  // if OK, replace with m_ToggleRc
    return firstRc;
}

//-----------------------------------------------------------------------------
//! \brief Pause/unpause ELPG toggling.  When pausing, wait for toggle thread
//!        to actually pause before returning
//!
//! \param bPause : true to pause toggling, false to resume toggling
//!
//! \return OK if successfully paused, not OK otherwise
RC ElpgBackgroundToggle::SetPaused(bool bPause)
{
    if (bPause == m_bPendingPause)
        return OK;

    if (m_bPaused == bPause)
    {
        m_bPendingPause = bPause;
        return OK;
    }

    m_bPendingPause = bPause;
    if (!bPause)
    {
        m_bPaused = bPause;
        return OK;
    }
    return POLLWRAP(PollPaused, this, m_TimeoutMs);
}

//-----------------------------------------------------------------------------
//! \brief Static function which actually toggles ELPG
//!
//! \param pThis : Pointer to ElpgBackgroundToggle class
/* static */ void ElpgBackgroundToggle::ToggleElpg(void *pThis)
{
    ElpgBackgroundToggle *pToggle = (ElpgBackgroundToggle *)pThis;

    while (!pToggle->m_bStopToggling && (pToggle->m_ToggleRc == OK))
    {
        if (!pToggle->m_bPaused)
        {
            pToggle->Wakeup();
            pToggle->m_ToggleRc = pToggle->m_ElpgWait->Wait(PwrWait::PG_ON,
                                                            pToggle->m_TimeoutMs);
        }
        Tasker::Sleep(pToggle->m_GatedTimeMs);
        if (pToggle->m_bPendingPause && !pToggle->m_bPaused)
        {
            pToggle->m_ToggleRc = OK;
            pToggle->m_bPaused  = true;
        }

    }
}

//-----------------------------------------------------------------------------
//! \brief Static polling function to wait for an actual pause to occur
//!
//! \param pThis : Pointer to ElpgBackgroundToggle class
/* static */ bool ElpgBackgroundToggle::PollPaused(void *pThis)
{
    ElpgBackgroundToggle *pToggle = (ElpgBackgroundToggle *)pThis;
    return pToggle->m_bPaused;
}

//-----------------------------------------------------------------------------
//! \brief Cause the underlying engine to wakeup
//!
void ElpgBackgroundToggle::Wakeup()
{
    switch (m_Type)
    {
        case PMU::ELPG_GRAPHICS_ENGINE:
            m_pSubdev->GetEngineStatus(GpuSubdevice::GR_ENGINE);
            break;
        case PMU::ELPG_VIDEO_ENGINE:
            // use a different read - this one is banned by RM
            m_pSubdev->GetEngineStatus(GpuSubdevice::MSPDEC_ENGINE);
            break;
        case PMU::ELPG_VIC_ENGINE:
        default:
            Printf(Tee::PriWarn,
                   "ElpgBackgroundToggle::Wakeup : Unable to wakeup engine %d\n",
                   m_Type);
            break;
    }
}

//-----------------------------------------------------------------------------
TegraElpgWait::~TegraElpgWait()
{
}

//-----------------------------------------------------------------------------
RC TegraElpgWait::Initialize()
{
    return OK;
}

//-----------------------------------------------------------------------------
RC TegraElpgWait::Teardown()
{
    return OK;
}

//-----------------------------------------------------------------------------
RC TegraElpgWait::Wait(Action action, FLOAT64 timeoutMs)
{
    Tasker::DetachThread detach;
    const UINT64 endTime = Xp::GetWallTimeMS() + static_cast<UINT64>(timeoutMs);
    UINT32 oldPgStat       = 0xDEADBEEF;
    UINT32 oldEngineStatus = 0xDEADBEEF;
    do
    {
        // TODO Move this to GpuSubdevice
        const UINT32 pgStat       = m_pSubdev->RegRd32PGSafe(LW_PPWR_PMU_PG_STAT(0));
        const UINT32 engineStatus = DRF_VAL(_PPWR, _PMU_PG_STAT, _EST, pgStat);

        if (action == PG_OFF)
        {
            if (engineStatus == LW_PPWR_PMU_PG_STAT_EST_IDLE ||
                engineStatus == LW_PPWR_PMU_PG_STAT_EST_ACTIVE)
            {
                return OK;
            }
        }
        else
        {
            if (engineStatus == LW_PPWR_PMU_PG_STAT_EST_PWROFF)
            {
                return OK;
            }
        }

        if (pgStat != oldPgStat || engineStatus != oldEngineStatus)
        {
            Printf(m_Pri, "PG status = 0x%x .. Engine status = %u\n",
                   pgStat, engineStatus);
            oldPgStat       = pgStat;
            oldEngineStatus = engineStatus;
        }

        Tasker::Sleep(1);
    }
    while (Xp::GetWallTimeMS() < endTime);
    return RC::TIMEOUT_ERROR;
}

//-----------------------------------------------------------------------------
UINT64 TegraElpgWait::ReadTransitions()
{
    const string gpuNode = CheetAh::GetDebugGpuSysFsFile("elpg_transitions");
    if (gpuNode.empty())
    {
        return 0;
    }

    UINT64 value = 0;
    Xp::InteractiveFileRead(gpuNode.c_str(), &value);
    return value;
}

//-----------------------------------------------------------------------------
//! \brief Perform the necessary setup before any GCx cycle.
//!
//! \return OK if successful, not OK otherwise
//!
RC GCxPwrWait::DoEnterWait
(
    UINT32 *pW, //display width
    UINT32 *pH, //display height
    UINT32 *pD, //display depth
    UINT32 *pR  //display refresh rate
)
{
    RC rc;
    if (m_pDisp->GetDisplayClassFamily() == Display::EVO)
    {
        // Display must be disabled for GC5(DI-OS or DI-SSC), or GC6
        CHECK_RC(m_pDisp->GetMode(m_pDisp->Selected(), pW, pH, pD, pR));
        m_pDisp->Disable();
    }
    else if (m_pDisp->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        CHECK_RC(m_pDisp->GetDisplayMgr()->DeactivateComposition());
    }

    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform the necessary cleanup after any GCx cycle.
//!
//! \return OK if successful, not OK otherwise
//!
RC GCxPwrWait::DoExitWait
(
    UINT32 width,
    UINT32 height,
    UINT32 depth,
    UINT32 refresh
)
{
    RC rc;

    Surface2D *pDispImage = m_pTestContext->GetSurface2D();
    if ((m_pDisp->GetDisplayClassFamily() == Display::EVO) &&
        (width != 0) && (height != 0) && (depth != 0) && (refresh != 0))
    {
        CHECK_RC(m_pDisp->SetMode(width,height,depth,refresh));
        if (pDispImage)
        {
            CHECK_RC(m_pDisp->SetImage(pDispImage));
        }
    }
    else if (m_pDisp->GetDisplayClassFamily() == Display::LWDISPLAY)
    {
        CHECK_RC(m_pDisp->GetDisplayMgr()->ActivateComposition());
        if (pDispImage)
        {
            CHECK_RC(m_pTestContext->DisplayImage(pDispImage, DisplayMgr::DONT_WAIT_FOR_UPDATE));
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform the necessary initialization prior to any GC5 cycle.
//!
//! \return OK if successful, not OK otherwise
//!
RC GC5PwrWait::Initialize()
{
    RC rc;
    m_pGCx = m_pSubdev->GetGCxImpl();
    if(m_pGCx)
    {
        m_pGCx->SetBoundRmClient(m_pBoundRm);
        m_pGCx->SetVerbosePrintPriority(m_bShowStats ? Tee::PriNormal : Tee::PriLow);
        m_pGCx->SetGc5Mode(static_cast<GCxImpl::gc5Mode>(GCxImpl::gc5DI_OS));
        CHECK_RC(m_pGCx->InitGc5());
    }
    else
        return RC::UNSUPPORTED_FUNCTION;

    memset(&m_Stats, 0, sizeof(m_Stats));
    m_bInitialized = true;
    return rc;
}

//-----------------------------------------------------------------------------
void GC5PwrWait::GetStats(LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS *pStats)
{
    memcpy(pStats, &m_Stats,sizeof(m_Stats));
}

//-----------------------------------------------------------------------------
//! \brief Perform the actual GC5 entry.  This function is guaranteed to
//!        wait for at least m_EntryDelayMs (since it is not possible to
//!        check for GC5 entry without waking up the GPU).
//!
//! \return OK if successful, not OK otherwise
//!
RC GC5PwrWait::Wait(Action LwrWait, FLOAT64 TimeoutMs)
{
    RC rc;
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 refresh;
    UINT32 lwrPState;
    Perf::PerfPoint perfPt;
    const bool doRestorePstate = true;

    if (!m_bInitialized)
    {
        Printf(Tee::PriNormal, "GC5PwrWait not initialzed properly\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    m_pSubdev->GetPerf()->GetLwrrentPerfPoint(&perfPt);
    lwrPState = (m_ForcedPState == Perf::ILWALID_PSTATE) ? perfPt.PStateNum : m_ForcedPState;

    // Do common setup before entry
    CHECK_RC(DoEnterWait(&width, &height, &depth, &refresh));

    // Enter GC5 and wait for m_EnterDelayUsec
    CHECK_RC(m_pGCx->DoEnterGc5(lwrPState,m_WakeupEvent,m_EnterDelayUsec));
    CHECK_RC(m_pGCx->DoExitGc5(0,doRestorePstate));

    // Do common cleanup after exit
    CHECK_RC(DoExitWait(width, height, depth, refresh));

    CHECK_RC(m_pGCx->GetDeepIdleStats(&m_Stats));
    if (m_bShowStats)
    {
        Printf(Tee::PriNormal,"GC5 Stats: entries:%04d exits:%04d attepmts:%04d time:%08d\n",
            m_Stats.entries,m_Stats.exits,m_Stats.attempts,m_Stats.time);
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Perform the necessary initialization prior to any GC6 cycle.
//!
//! \return OK if successful, not OK otherwise
//!
RC GC6PwrWait::Initialize()
{
    MASSERT(m_pSubdev && m_pBoundRm);
    RC rc;
    m_pGCx = m_pSubdev->GetGCxImpl();
    if(m_pGCx)
    {
        m_pGCx->SetBoundRmClient(m_pBoundRm);
        m_pGCx->SetVerbosePrintPriority(m_bShowStats ? Tee::PriNormal : Tee::PriLow);
        m_pGCx->SetSkipSwFuseCheck(true);
        CHECK_RC(m_pGCx->InitGc6(m_TimeoutMs));
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_bInitialized = true;
    return rc;
}

//-----------------------------------------------------------------------------
void GC6PwrWait::GetStats(Xp::JTMgr::CycleStats * pStats)
{
    *pStats = m_Stats;
}

//-----------------------------------------------------------------------------
//! \brief Perform the actual GC6 entry.  This function is guaranteed to
//!        wait for at least m_EnterDelayUsec(since it is not possible to
//!        check for a GCx entry without waking up the GPU).
//!
//! \return OK if successful, not OK otherwise
//!
RC GC6PwrWait::Wait(Action LwrWait, FLOAT64 TimeoutMs)
{
    RC rc;
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    UINT32 refresh;
    const bool doRestorePstate = true;

    if (!m_bInitialized)
    {
        Printf(Tee::PriNormal, "GC6PwrWait not initialzed properly\n");
        return RC::WAS_NOT_INITIALIZED;
    }

    // Do common setup before entry
    CHECK_RC(DoEnterWait(&width, &height, &depth, &refresh));

    if (m_WakeupEvent == GCxPwrWait::RTD3_CPU)
    {
        m_pGCx->SetUseRTD3(true);
    }

    // Enter GC6 and wait m_EnterDelayUsec
    CHECK_RC(m_pGCx->DoEnterGc6(m_WakeupEvent, m_EnterDelayUsec));
    CHECK_RC(m_pGCx->DoExitGc6(0, doRestorePstate));

    // Do common cleanup after exit
    CHECK_RC(DoExitWait(width, height, depth, refresh));

    m_pGCx->GetGc6CycleStats(&m_Stats);

    if (m_bShowStats)
    {
        Tee::Priority pri = Tee::PriNormal;
        Printf(pri, "GC6 cycle stats\n");
        if (m_pGCx->IsGc6SMBusSupported())
        {
            Printf(pri, "SmbEC EntryMs:%4lld  LinkDnMs:%4lld PostDlyUsec:%9d EntryStatus:%d ECPwrStatus:0x%x \n",
                m_Stats.entryTimeMs, m_Stats.enterLinkDnMs,  m_EnterDelayUsec,
                m_Stats.entryStatus, m_Stats.entryPwrStatus);

            Printf(pri, "SmbEC ExitMs :%4lld  LinkUpMs:%4lld PostDlyUsec:%9d ExitStatus :%d ECPwrStatus:0x%x wakeupEvent:%d(%s)\n",
                m_Stats.exitTimeMs, m_Stats.exitLinkUpMs, 0,
                m_Stats.exitStatus, m_Stats.exitPwrStatus,
                m_Stats.exitWakeupEvent, m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        }
        else
        {
            const char* acpiMethod = m_pGCx->IsGc6JTACPISupported() ? "JT ACPI" : "Native ACPI";
            Printf(pri, "%s EntryMs:%4lld PostDlyUsec:%9d \n",
                acpiMethod, m_Stats.entryTimeMs, m_EnterDelayUsec);
            Printf(pri, "%s ExitMs :%4lld PostDlyUsec:%9d wakeupEvent:%d(%s)\n",
                acpiMethod, m_Stats.exitTimeMs, 0, m_WakeupEvent,
                m_pGCx->WakeupEventToString(m_WakeupEvent).c_str());
        }
    }
    return rc;
}

GCxBubble::~GCxBubble()
{
    Cleanup();
}

bool GCxBubble::IsGCxSupported(UINT32 mode, UINT32 forcePstate)
{
    Perf* pPerf = m_pSubdev->GetPerf();

    // Cannot run GCx with pstates disabled
    if (!pPerf->HasPStates())
        return false;

    // Use case here is that we want to test out this feature when running in pstate 0
    // which isn't supported. So we will want to force the pstate before GCx entry.
    if (forcePstate == Perf::ILWALID_PSTATE)
    {
        Perf::PerfPoint perfPt;
        pPerf->GetLwrrentPerfPoint(&perfPt);
        forcePstate = perfPt.PStateNum;
    }
    GCxImpl * pGCx = m_pSubdev->GetGCxImpl();
    if(pGCx)
    {
        // GC5/GC6 is only support some pstates
        if ((mode == GCxPwrWait::GCxModeGC5) && !pGCx->IsGc5Supported(forcePstate))
        {
            return false;
        }
        else if (mode == GCxPwrWait::GCxModeRTD3)
        {
            if (!pGCx->IsRTD3Supported(forcePstate, false))
            {
                return false;
            }

            if (m_pSubdev->HasBug(2363749) &&
                pGCx->GetGc6FsmMode() != LW_EC_INFO_FW_CFG_FSM_MODE_RTD3)
            {
                Printf(Tee::PriError, "RTD3 bubble test cannot be run due to bug 2363749"
                    "Change EC mode to RTD3\n");
                return false;
            }
        }
        else if ((mode == GCxPwrWait::GCxModeGC6) && !pGCx->IsGc6Supported(forcePstate))
        {
            return false;
        }
    }
    else
    {
        return false;
    }
    return true;
}

RC GCxBubble::Setup()
{
    RC rc;

    if (m_Mode == GCxPwrWait::GCxModeGC5)
    {
        m_PwrWait = make_unique<GC5PwrWait>(
                        m_pSubdev,
                        m_pBoundRm,
                        m_pDisp,
                        m_pTestContext,
                        m_ForcedPState,   //default is don't force a pstate switch
                        m_bShowStats,
                        GCxPwrWait::GC5_PEX,
                        m_TimeoutMs);

        CHECK_RC(m_PwrWait->Initialize());
    }
    else if (m_Mode == GCxPwrWait::GCxModeGC6 ||
            m_Mode == GCxPwrWait::GCxModeRTD3)
    {
        UINT32 wakeUpEvent = m_Mode == GCxPwrWait::GCxModeRTD3 ?
            GCxPwrWait::RTD3_CPU : GCxPwrWait::GC6_GPU;
        m_PwrWait = make_unique<GC6PwrWait>(
                        m_pSubdev,
                        m_pBoundRm,
                        m_pDisp,
                        m_pTestContext,
                        m_ForcedPState,   //default is don't force a pstate switch
                        m_bShowStats,
                        wakeUpEvent,
                        m_TimeoutMs);

        CHECK_RC(m_PwrWait->Initialize());
    }
    else
    {
        Printf(Tee::PriError, "Unsupported GCx mode\n");
        return RC::UNSUPPORTED_SYSTEM_CONFIG;
    }

    return rc;
}

RC GCxBubble::Cleanup()
{
    RC rc;
    if (m_PwrWait.get())
    {
        m_PwrWait.reset();
        rc = m_pSubdev->GetGCxImpl()->Shutdown();
    }
    return rc;
}

void GCxBubble::SetGCxParams
(
    LwRm *          pBoundRmClient,
    Display *       pDisplay,
    DisplayMgr::TestContext *pTestContext,
    UINT32          forcedPState,
    bool            bShowStats,
    FLOAT64         timeoutMs,
    UINT32          mode
)
{
    MASSERT(pBoundRmClient);
    MASSERT(pDisplay);
    MASSERT(pTestContext);

    m_pBoundRm = pBoundRmClient;
    m_pDisp = pDisplay;
    m_pTestContext = pTestContext;
    m_ForcedPState = forcedPState;
    m_bShowStats = bShowStats;
    m_TimeoutMs = timeoutMs;
    m_Mode = mode;
}

RC GCxBubble::ActivateBubble(UINT32 pwrWaitDelayMs)
{
    StickyRC rc;
    if (!m_pSubdev->GetGCxImpl() || (m_Mode == GCxPwrWait::GCxModeDisabled))
    {
        return OK;
    }

    // GCx bubbles may change the current pstate, if so we need to claim the
    // pstate to prevent another thread from changing it behind our backs.
    CHECK_RC(m_PStateOwner.ClaimPStates(m_pSubdev));
    Utility::CleanupOnReturn<GCxBubble> releasePState(this,
            &GCxBubble::ReleasePState);

    m_PwrWait->SetGCxResidentTime(pwrWaitDelayMs * 1000);
    if(m_Mode == GCxPwrWait::GCxModeGC5)
    {
        rc = m_PwrWait->Wait(PwrWait::PG_ON, 0);
        if (rc != OK)
        {
            Printf(Tee::PriNormal, "Failed to enter/exit GC5 cycle\n");
        }
        LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS diStats = {0};
        static_cast<GC5PwrWait*>(m_PwrWait.get())->GetStats(&diStats);
        m_WakeupStats.diExits      += diStats.exits;
        m_WakeupStats.diEntries    += diStats.entries;
        m_WakeupStats.diAttempts   += diStats.attempts;
        m_WakeupStats.actualEntryTimeUs     += diStats.time;
        if(diStats.attempts > diStats.entries)
            m_WakeupStats.aborts++;
    }
    else if (m_Mode == GCxPwrWait::GCxModeGC6 ||
            m_Mode == GCxPwrWait::GCxModeRTD3)
    {
        rc = m_PwrWait->Wait(PwrWait::PG_ON, 0);
        if (rc != OK)
        {
            Printf(Tee::PriNormal, "Failed to enter/exit GC6 cycle\n");
        }
        Xp::JTMgr::CycleStats jtStats;
        static_cast<GC6PwrWait*>(m_PwrWait.get())->GetStats(&jtStats);

        m_WakeupStats.diExits      += (jtStats.exitStatus == OK) ? 1 : 0;
        m_WakeupStats.diEntries    += (jtStats.entryStatus == OK) ? 1: 0;
    }
    return rc;
}

void GCxBubble::PrintStats()
{
    if (m_bShowStats)
    {
        Printf(Tee::PriNormal,"GCx Cycle Stats:(entries:%d exits:%d aborts:%d)\n",
                m_WakeupStats.diEntries,
                m_WakeupStats.diExits,
                m_WakeupStats.aborts);
    }
}

void GCxBubble::ReleasePState()
{
    m_PStateOwner.ReleasePStates();
}

void GCxBubble::SetPciCfgRestoreDelayMs(UINT32 delayMs)
{
    m_pSubdev->GetGCxImpl()->SetPciConfigSpaceRestoreDelay(delayMs);
}
//************************************************************************************************
// annonymous namespace to support RppgWait & TpcPgWait implementations
namespace
{
    bool PollEntryCountFunction(void* args)
    {
        RppgWait::PollEntryCountArgs* pArgs = static_cast<RppgWait::PollEntryCountArgs*>(args);

        bool grEntryInc = true;
        UINT32 grEntryCount = 0;
        if (pArgs->grRppgImpl != nullptr)
        {
            grEntryInc = false;
            pArgs->grRppgImpl->GetEntryCount(&grEntryCount);
            if (grEntryCount > pArgs->grEntryCount)
                grEntryInc = true;
        }

        bool diEntryInc = true;
        UINT32 diEntryCount = 0;
        if (pArgs->diRppgImpl != nullptr)
        {
            diEntryInc = false;
            pArgs->diRppgImpl->GetEntryCount(&diEntryCount);
            if (diEntryCount > pArgs->diEntryCount)
                diEntryInc = true;
        }

        bool done = grEntryInc && diEntryInc;

        // Give the GPU a chance to engage RPPG. Sleep value hard coded for now.
        // TODO: Get Sleep value from RM
        if (!done)
        {
            Tasker::Sleep(200);
        }
        else
        {
            pArgs->grEntryCount = grEntryCount;
            pArgs->diEntryCount = diEntryCount;
        }

        return done;
    }

    bool PollTpcPgEntryCountFunction(void* args)
    {
        TpcPgWait::PollEntryCountArgs* pArgs = static_cast<TpcPgWait::PollEntryCountArgs*>(args);

        bool tpcEntryInc = true;
        UINT32 tpcEntryCount = 0;
        if (pArgs->tpcPgImpl != nullptr)
        {
            tpcEntryInc = false;
            pArgs->tpcPgImpl->GetEntryCount(&tpcEntryCount);
            if (tpcEntryCount > pArgs->tpcEntryCount)
                tpcEntryInc = true;
        }

        // Give the GPU a chance to engage TPC-PG.
        if (!tpcEntryInc)
        {
            Tasker::Sleep(pArgs->pgThresholdMs);
        }
        else
        {
            pArgs->tpcEntryCount = tpcEntryCount;
        }

        return tpcEntryInc;
    }


    struct RestoreDisplayArgs
    {
        Display* pDisp;
        Surface2D* pDispImage;
        UINT32 width, height, depth, refresh;
    };
    RC RestoreDisplay(RestoreDisplayArgs* args)
    {
        RC rc;

        if ((args->pDisp->GetDisplayClassFamily() != Display::NULLDISP) &&
            (args->width != 0) && (args->height != 0) &&
            (args->depth != 0) && (args->refresh != 0))
        {
            CHECK_RC(args->pDisp->SetMode(args->width, args->height,
                                          args->depth, args->refresh));
            if (args->pDispImage)
            {
                CHECK_RC(args->pDisp->SetImage(args->pDispImage));
            }
        }

        return rc;
    }

    bool PollPgIdleEntryCountFunction(void* args)
    {
        LowPowerPgWait::PollEntryCountArgs* pArgs =
            static_cast<LowPowerPgWait::PollEntryCountArgs*>(args);

        bool pgEntryInc = false;
        bool isFeatureEngaged = false;
        UINT32 pgEntryCount = 0;

        UINT32 featureId = LowPowerPgWait::s_PgFeatureMap[pArgs->feature].featureId;
        UINT32 subFeatureId = LowPowerPgWait::s_PgFeatureMap[pArgs->feature].subfeatureId;
        if (pArgs->pgImpl != nullptr)
        {
            pArgs->pgImpl->GetFeatureEngageStatus(featureId, &isFeatureEngaged);
            if (subFeatureId != LowPowerPgWait::PgFeature::PgDisable)
            {
                bool isSubFeatureEnabled = false;
                UINT32 mask = 0;

                pArgs->pgImpl->GetRppgEntryCount(subFeatureId, &pgEntryCount);
                pArgs->pgImpl->GetSubFeatureEnableMask(featureId, &mask);

                if ((pArgs->feature == LowPowerPgWait::MSRPPG &&
                        (mask & BIT(RM_PMU_LPWR_CTRL_FEATURE_ID_MS_RPPG))) ||
                    (pArgs->feature == LowPowerPgWait::MSRPG &&
                        (mask & BIT(RM_PMU_LPWR_CTRL_FEATURE_ID_MS_RPG))))
                {
                    isSubFeatureEnabled = true;
                }
                isFeatureEngaged = isFeatureEngaged && isSubFeatureEnabled;
            }
            else
            {
                pArgs->pgImpl->GetLpwrCtrlEntryCount(featureId, &pgEntryCount);
            }

            if (pgEntryCount > pArgs->pgEntryCount || isFeatureEngaged)
            {
                pgEntryInc = true;
            }
        }

        // Give the GPU a chance to engage pg idle feature
        if (!pgEntryInc)
        {
            Tasker::Sleep(pArgs->pgThresholdMs);
        }
        else
        {
            pArgs->pgEntryCount = pgEntryCount;
        }

        return pgEntryInc;
    }
}

//************************************************************************************************
// RppgWait implementation
RppgWait::RppgWait(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
{}

RppgWait::~RppgWait()
{
    Teardown();
}

RC RppgWait::Setup(UINT32 rppgMask, Display* pDisp, Surface2D* pDispImage)
{
    RC rc;

    m_ForceRppgMask = rppgMask;
    m_pDisplay      = pDisp;
    m_pDisplayImage = pDispImage;

    CHECK_RC(GetAllSupported(&m_GrSupported, &m_MsSupported, &m_DiSupported));
    CHECK_RC(GetAllTpcSupported(&m_TpcPgSupported, &m_AdaptiveTpcPgSupported));
    CHECK_RC(GetAllTpcEnabled(&m_TpcPgEnabled, &m_AdaptiveTpcPgEnabled));

    return (OK);
}

RC RppgWait::Initialize()
{
    RC rc;

    m_GrRppgImpl.reset(nullptr);
    m_DiRppgImpl.reset(nullptr);
    m_TpcPgImpl.reset(new TpcPgImpl(m_pSubdev));

    if (!m_ForceRppgMask || (m_ForceRppgMask & (1 << RppgImpl::GRRPPG)))
    {
        if (m_GrSupported)
        {
            m_GrRppgImpl.reset(new RppgImpl(m_pSubdev));
            m_GrRppgImpl->SetType(RppgImpl::GRRPPG);
        }
        else
        {
            if (m_ForceRppgMask & (1 << RppgImpl::GRRPPG))
            {
                Printf(Tee::PriError, "GR-RPPG not supported\n");
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
            else
            {
                Printf(Tee::PriWarn, "GR-RPPG not supported\n");
            }
        }
    }

    if ((!m_ForceRppgMask || (m_ForceRppgMask & (1 << RppgImpl::MSRPPG)))
        && m_GrRppgImpl.get() == nullptr)
    {
        // If GR-RPPG is unsupported or unrequested, fall back on MS-RPPG
        if (m_MsSupported)
        {
            m_GrRppgImpl.reset(new RppgImpl(m_pSubdev));
            m_GrRppgImpl->SetType(RppgImpl::MSRPPG);
        }
        else
        {
            if (m_ForceRppgMask & (1 << RppgImpl::MSRPPG))
            {
                Printf(Tee::PriError, "MS-RPPG not supported\n");
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
            else
            {
                Printf(Tee::PriWarn, "MS-RPPG not supported\n");
            }
        }
    }

    if (!m_ForceRppgMask || (m_ForceRppgMask & (1 << RppgImpl::DIRPPG)))
    {
        if (m_DiSupported)
        {
            m_DiRppgImpl.reset(new RppgImpl(m_pSubdev));
            m_DiRppgImpl->SetType(RppgImpl::DIRPPG);
        }
        else
        {
            if (m_ForceRppgMask & (1 << RppgImpl::DIRPPG))
            {
                Printf(Tee::PriError, "DI-RPPG not supported\n");
                return RC::UNSUPPORTED_HARDWARE_FEATURE;
            }
            else
            {
                Printf(Tee::PriWarn, "DI-RPPG not supported\n");
            }
        }
    }

    if (m_GrRppgImpl.get() != nullptr)
    {
        if (m_TpcPgSupported)
        {
            CHECK_RC(m_TpcPgImpl->GetPgIdleThresholdUs(&m_TpcPgIdleThresholdUs));
            if (m_AdaptiveTpcPgSupported && m_AdaptiveTpcPgEnabled)
            {
                CHECK_RC(m_TpcPgImpl->SetAdaptivePgEnabled(false));
                TpcPgImpl::AdaptivePgIdleThresholds  PgThresholds = {0, 0, 0}; //$
                CHECK_RC(m_TpcPgImpl->GetAdaptivePgIdleThresholdUs(&PgThresholds));
                // set the TPC power gating threshold to min.
                MASSERT(PgThresholds.min != 0);
                CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(PgThresholds.min));
            }
            else
            {
                CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(100));
            }
        }
        CHECK_RC(m_GrRppgImpl->GetEntryCount(&m_GrEntryCount));
    }
    if (m_DiRppgImpl.get() != nullptr)
    {
        CHECK_RC(m_DiRppgImpl->GetEntryCount(&m_DiEntryCount));
    }

    return rc;
}

RC RppgWait::Teardown()
{
    RC rc;
    if (m_TpcPgImpl.get() != nullptr)
    {
        if (m_AdaptiveTpcPgSupported && m_AdaptiveTpcPgEnabled)
        {   // restore original value
            CHECK_RC(m_TpcPgImpl->SetAdaptivePgEnabled(true));
        }
        if (m_TpcPgSupported && m_TpcPgEnabled)
        {   // restore original value
            CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(m_TpcPgIdleThresholdUs));
        }
    }

    return (OK);
}

RC RppgWait::Wait(PwrWait::Action action, FLOAT64 TimeoutMs)
{
    RC rc;

    PStateOwner pstateOwner;
    CHECK_RC(pstateOwner.ClaimPStates(m_pSubdev));
    Utility::CleanupOnReturn<PStateOwner> releasePState(&pstateOwner,
            &PStateOwner::ReleasePStates);

    UINT32 width = 0, height = 0, depth = 0, refresh = 0;
    if (m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        // Display must be disabled for GC5(DI-OS or DI-SSC), or GC6
        CHECK_RC(m_pDisplay->GetMode(m_pDisplay->Selected(),
                                     &width, &height, &depth, &refresh));
        m_pDisplay->Disable();
    }
    RestoreDisplayArgs restoreDisp = {m_pDisplay, m_pDisplayImage,
                                      width, height, depth, refresh};
    Utility::CleanupOnReturnArg<void, RC, RestoreDisplayArgs*>
        RestoreDisplayCleanup(&RestoreDisplay, &restoreDisp);

    PollEntryCountArgs args = {m_GrRppgImpl.get(), m_DiRppgImpl.get(),
                               m_GrEntryCount, m_DiEntryCount};
    CHECK_RC(POLLWRAP_HW(&PollEntryCountFunction, &args, TimeoutMs));

    m_GrEntryCount = args.grEntryCount;
    m_DiEntryCount = args.diEntryCount;

    RestoreDisplayCleanup.Cancel();
    CHECK_RC(RestoreDisplay(&restoreDisp));

    return rc;
}

void RppgWait::PrintStats(Tee::Priority pri) const
{
    if (m_GrRppgImpl.get() != nullptr)
    {
        if (m_GrRppgImpl->GetType() == RppgImpl::GRRPPG)
            Printf(pri, "GR-RPPG Entry Count = %d\n", m_GrEntryCount);
        else
            Printf(pri, "MS-RPPG Entry Count = %d\n", m_GrEntryCount);
    }
    if (m_DiRppgImpl.get() != nullptr)
    {
        Printf(pri, "DI-RPPG Entry Count = %d\n", m_DiEntryCount);
    }
}

RC RppgWait::GetAllSupported(bool* grSupported, bool* msSupported, bool* diSupported) const
{
    MASSERT(grSupported);
    MASSERT(msSupported);
    MASSERT(diSupported);
    RC rc;

    *grSupported = false;
    *msSupported = false;
    *diSupported = false;
    unique_ptr<RppgImpl> rppgImpl(new RppgImpl(m_pSubdev));

    // RPPG(test 303) is replaced by MsRppg(test 334) in Ampere
    if (m_pSubdev->IsSOC() || m_pSubdev->GetParentDevice()->GetFamily() > GpuDevice::Turing)
    {
        return RC::OK; // nothing supported
    }

    Perf* pPerf = m_pSubdev->GetPerf();

    // RPPG is not supported with pstates disabled
    if (!pPerf->HasPStates())
        return OK;

    Perf::PerfPoint lwrrPerfPoint;
    CHECK_RC(pPerf->GetLwrrentPerfPoint(&lwrrPerfPoint));
    const UINT32 lwrrPstate = lwrrPerfPoint.PStateNum;

    rppgImpl->SetType(RppgImpl::GRRPPG);
    CHECK_RC(rppgImpl->GetSupported(lwrrPstate, grSupported));

    rppgImpl->SetType(RppgImpl::MSRPPG);
    CHECK_RC(rppgImpl->GetSupported(lwrrPstate, msSupported));

    rppgImpl->SetType(RppgImpl::DIRPPG);
    CHECK_RC(rppgImpl->GetSupported(lwrrPstate, diSupported));

    return OK;
}

RC RppgWait::GetAllTpcSupported
(
    bool* tpcPgSupported,
    bool* adaptiveTpcPgSupported
) const
{
    MASSERT(tpcPgSupported);
    MASSERT(adaptiveTpcPgSupported);
    RC rc;

    *tpcPgSupported = false;
    *adaptiveTpcPgSupported = false;
    unique_ptr<TpcPgImpl> tpcPgImpl(new TpcPgImpl(m_pSubdev));

    if (m_pSubdev->IsSOC())
        return OK; // nothing supported

    CHECK_RC(tpcPgImpl->GetPgSupported(tpcPgSupported));
    CHECK_RC(tpcPgImpl->GetAdaptivePgSupported(adaptiveTpcPgSupported));

    return OK;
}

RC RppgWait::GetAllTpcEnabled
(
    bool* pgEnabled,
    bool* adaptivePgEnabled
) const
{
    MASSERT(pgEnabled);
    MASSERT(adaptivePgEnabled);
    RC rc;

    *pgEnabled = false;
    *adaptivePgEnabled = false;
    unique_ptr<TpcPgImpl> tpcPgImpl(new TpcPgImpl(m_pSubdev));

    if (m_pSubdev->IsSOC())
        return OK; // nothing supported

    if (m_TpcPgSupported)
    {
        CHECK_RC(tpcPgImpl->GetPgEnabled(pgEnabled));
    }
    if (m_AdaptiveTpcPgSupported)
    {
        CHECK_RC(tpcPgImpl->GetAdaptivePgEnabled(adaptivePgEnabled));
    }

    return OK;
}

//************************************************************************************************
// RppgBubble implementation
RppgBubble::RppgBubble(GpuSubdevice* pSubdev)
{
    m_RppgWait.reset(new RppgWait(pSubdev));
}

bool RppgBubble::IsSupported() const
{
    bool grSupported = false;
    bool msSupported = false;
    bool diSupported = false;

    if (OK != m_RppgWait->GetAllSupported(&grSupported, &msSupported, &diSupported))
        return false;

    if (!grSupported && !msSupported && !diSupported)
        return false;

    return true;
}

RC RppgBubble::PrintIsSupported(Tee::Priority pri) const
{
    RC rc;

    bool grSupported = false;
    bool msSupported = false;
    bool diSupported = false;
    CHECK_RC(m_RppgWait->GetAllSupported(&grSupported, &msSupported, &diSupported));

    Printf(pri, "GR-RPPG supported = %s\n", grSupported ? "true" : "false");
    Printf(pri, "MS-RPPG supported = %s\n", msSupported ? "true" : "false");
    Printf(pri, "DI-RPPG supported = %s\n", diSupported ? "true" : "false");

    return rc;
}

RC RppgBubble::Setup(UINT32 rppgMask, Display* pDisp, Surface2D* pDispImage)
{
    RC rc;

    CHECK_RC(m_RppgWait->Setup(rppgMask, pDisp, pDispImage));
    CHECK_RC(m_RppgWait->Initialize());

    return rc;
}

RC RppgBubble::ActivateBubble(FLOAT64 timeoutMs) const
{
    RC rc;

    CHECK_RC(m_RppgWait->Wait(PwrWait::PG_ON, timeoutMs));

    return rc;
}

void RppgBubble::PrintStats(Tee::Priority pri) const
{
    m_RppgWait->PrintStats(pri);
}



//************************************************************************************************
// TpcPgWait implementation
TpcPgWait::TpcPgWait(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
{}

TpcPgWait::~TpcPgWait()
{
    Teardown();
}

RC TpcPgWait::GetAllSupported
(
    bool* tpcPgSupported,
    bool* adaptiveTpcPgSupported
) const
{
    MASSERT(tpcPgSupported);
    MASSERT(adaptiveTpcPgSupported);
    RC rc;

    *tpcPgSupported = false;
    *adaptiveTpcPgSupported = false;
    unique_ptr<TpcPgImpl> tpcPgImpl(new TpcPgImpl(m_pSubdev));

    if (m_pSubdev->IsSOC())
        return OK; // nothing supported

    CHECK_RC(tpcPgImpl->GetPgSupported(tpcPgSupported));
    CHECK_RC(tpcPgImpl->GetAdaptivePgSupported(adaptiveTpcPgSupported));

    return OK;
}

RC TpcPgWait::GetAllEnabled
(
    bool* pgEnabled,
    bool* adaptivePgEnabled
) const
{
    MASSERT(pgEnabled);
    MASSERT(adaptivePgEnabled);
    RC rc;

    *pgEnabled = false;
    *adaptivePgEnabled = false;
    unique_ptr<TpcPgImpl> tpcPgImpl(new TpcPgImpl(m_pSubdev));

    if (m_pSubdev->IsSOC())
        return OK; // nothing supported
    if (m_PgSupported)
    {
        CHECK_RC(tpcPgImpl->GetPgEnabled(pgEnabled));
    }
    if (m_AdaptivePgSupported)
    {
        CHECK_RC(tpcPgImpl->GetAdaptivePgEnabled(adaptivePgEnabled));
    }

    return OK;
}

RC TpcPgWait::Setup(Display* pDisp, Surface2D* pDispImage)
{
    RC rc;

    m_pDisplay       = pDisp;
    m_pDisplayImage  = pDispImage;

    CHECK_RC(GetAllSupported(&m_PgSupported, &m_AdaptivePgSupported));
    CHECK_RC(GetAllEnabled(&m_PgEnabled, &m_AdaptivePgEnabled));
    return (OK);
}

RC TpcPgWait::Initialize()
{
    RC rc;

    m_TpcPgImpl.reset(nullptr);

    if (m_PgSupported)
    {
        m_TpcPgImpl.reset(new TpcPgImpl(m_pSubdev));
        // save off original threshold value
        CHECK_RC(m_TpcPgImpl->GetPgIdleThresholdUs(&m_PgIdleThresholdUs));
        if (m_AdaptivePgSupported && m_AdaptivePgEnabled)
        {
            // Note: This code has not been verified. I have not found a board with
            // Adaptive TPC-PG enabled, However I did observe that tring to read the
            // idle threshold values with Adaptive TPC-PG disabled returns all zeros.
            //
            // Disable adaptive power gating
            CHECK_RC(m_TpcPgImpl->SetAdaptivePgEnabled(false));
            CHECK_RC(m_TpcPgImpl->GetAdaptivePgIdleThresholdUs(&m_AdaptivePgThresholds));
            // set the TPC power gating threshold to min.
            MASSERT(m_AdaptivePgThresholds.min != 0);
            //We need time for the GPU to enter into the sleep mode.
            m_SleepUs = min(m_AdaptivePgThresholds.min * 5, m_AdaptivePgThresholds.max);
            m_SleepUs += 100000; // add 100ms for transition time.

            CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(m_AdaptivePgThresholds.min));
        }
        else
        {
            CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(100));
            m_SleepUs = 500 + 100000;
        }

        CHECK_RC(m_TpcPgImpl->GetEntryCount(&m_EntryCount));
    }
    else
    {
        Printf(Tee::PriError, "TPC-PG not supported\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

RC TpcPgWait::Teardown()
{
    RC rc;
    if (m_PgEnabled)
    {
        CHECK_RC(m_TpcPgImpl->SetPgIdleThresholdUs(m_PgIdleThresholdUs));
        if (m_AdaptivePgEnabled)
        {
            CHECK_RC(m_TpcPgImpl->SetAdaptivePgEnabled(true));
        }
    }
    return (OK);
}

RC TpcPgWait::Wait(PwrWait::Action action, FLOAT64 TimeoutMs)
{
    RC rc;

    PStateOwner pstateOwner;
    CHECK_RC(pstateOwner.ClaimPStates(m_pSubdev));
    Utility::CleanupOnReturn<PStateOwner> releasePState(&pstateOwner,
            &PStateOwner::ReleasePStates);

    UINT32 width = 0, height = 0, depth = 0, refresh = 0;
    if (m_pDisplay->GetDisplayClassFamily() != Display::NULLDISP)
    {
        // Display must be disabled for GC5(DI-OS or DI-SSC), or GC6
        CHECK_RC(m_pDisplay->GetMode(m_pDisplay->Selected(),
                                     &width, &height, &depth, &refresh));
        m_pDisplay->Disable();
    }
    RestoreDisplayArgs restoreDisp =
        {m_pDisplay, m_pDisplayImage, width, height, depth, refresh}; //$
    Utility::CleanupOnReturnArg<void, RC, RestoreDisplayArgs*>
        RestoreDisplayCleanup(&RestoreDisplay, &restoreDisp);

    PollEntryCountArgs args = {m_TpcPgImpl.get(), m_EntryCount, m_SleepUs/1000}; //$
    CHECK_RC(POLLWRAP_HW(&PollTpcPgEntryCountFunction, &args, TimeoutMs));

    m_EntryCount = args.tpcEntryCount;

    RestoreDisplayCleanup.Cancel();
    CHECK_RC(RestoreDisplay(&restoreDisp));

    return rc;
}

void TpcPgWait::PrintStats(Tee::Priority pri) const
{
    if (m_TpcPgImpl.get() != nullptr)
    {
        Printf(pri, "TPC-PG Entry Count = %d\n", m_EntryCount);
    }
}

UINT32 TpcPgWait::GetSleepUs() const
{
    return m_SleepUs;
}

//************************************************************************************************
// TpcPgBubble implementation
TpcPgBubble::TpcPgBubble(GpuSubdevice* pSubdev)
{
    m_TpcPgWait.reset(new TpcPgWait(pSubdev));
}

bool TpcPgBubble::IsSupported() const
{
    bool tpcSupported = false;
    bool adaptiveTpcSupported = false;
    if (OK != m_TpcPgWait->GetAllSupported(&tpcSupported, &adaptiveTpcSupported))
        return false;

    if (!tpcSupported)
        return false;

    return true;
}

RC TpcPgBubble::PrintIsSupported(Tee::Priority pri) const
{
    RC rc;

    bool tpcSupported = false;
    bool tpcEnabled = false;
    bool adaptiveTpcSupported = false;
    bool adaptiveTpcEnabled = false;
    CHECK_RC(m_TpcPgWait->GetAllSupported(&tpcSupported, &adaptiveTpcSupported));
    CHECK_RC(m_TpcPgWait->GetAllEnabled(&tpcEnabled, &adaptiveTpcEnabled));

    Printf(pri, "TPC-PG supported = %s enabled = %s\n",
           tpcSupported ? "true" : "false",
           tpcEnabled ? "true" : "false");
    Printf(pri, "TPC-APG supported = %s enabled = %s\n",
           adaptiveTpcSupported ? "true" : "false",
           adaptiveTpcEnabled ? "true" : "false");

    return rc;
}

RC TpcPgBubble::Setup(Display* pDisp, Surface2D* pDispImage)
{
    RC rc;

    CHECK_RC(m_TpcPgWait->Setup(pDisp, pDispImage));
    CHECK_RC(m_TpcPgWait->Initialize());

    return rc;
}

RC TpcPgBubble::ActivateBubble(FLOAT64 timeoutMs) const
{
    RC rc;
    FLOAT64 sleepMs = static_cast<FLOAT64>(m_TpcPgWait->GetSleepUs()/1000);
    timeoutMs = max(sleepMs, timeoutMs);
    CHECK_RC(m_TpcPgWait->Wait(PwrWait::PG_ON, timeoutMs));

    return rc;
}

void TpcPgBubble::PrintStats(Tee::Priority pri) const
{
    m_TpcPgWait->PrintStats(pri);
}

LowPowerPgWait::PgFeatureMap LowPowerPgWait::s_PgFeatureMap = LowPowerPgWait::FillFeaturesIds();

LowPowerPgWait::PgFeatureMap LowPowerPgWait::FillFeaturesIds()
{
    PgFeatureMap pgFeatureMap;
    pgFeatureMap[GR_RPG] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_GRAPHICS};
    pgFeatureMap[GPC_RG] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_GR_RG};
    pgFeatureMap[MSCG] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS};
    pgFeatureMap[MSRPG] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS,
                    LW2080_CTRL_LPWR_SUB_FEATURE_ID_RPPG_MS_COUPLED};
    pgFeatureMap[MSRPPG] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_MS,
                    LW2080_CTRL_LPWR_SUB_FEATURE_ID_RPPG_MS_COUPLED};
    pgFeatureMap[EI] = {LW2080_CTRL_LPWR_SUB_FEATURE_ID_EI};
    return pgFeatureMap;
}

// LowPowerPgWait implementation
LowPowerPgWait::LowPowerPgWait(GpuSubdevice* pSubdev)
: m_pSubdev(pSubdev)
{
}

LowPowerPgWait::~LowPowerPgWait()
{
}

void LowPowerPgWait::SetFeatureId(UINT32 feature)
{
    m_Feature = static_cast<PgFeature>(feature);

    m_FeatureId = s_PgFeatureMap[m_Feature].featureId;

    if (feature == PgFeature::MSRPG || feature == PgFeature::MSRPPG)
    {
        m_SubFeatureId = s_PgFeatureMap[m_Feature].subfeatureId;
    }
}

bool LowPowerPgWait::GetSupported() const
{
    StickyRC rc;

    if (m_pSubdev->IsSOC())
    {
        return false;
    }

    bool isSupported = false;

    std::unique_ptr<LpwrCtrlImpl> pgImpl = make_unique<LpwrCtrlImpl>(m_pSubdev);

    if (pgImpl->GetLpwrCtrlSupported(m_FeatureId, &isSupported) != RC::OK || !isSupported)
    {
        return false;
    }

    if (m_Feature == MSRPG || m_Feature == MSRPPG)
    {
        if (pgImpl->GetRppgCtrlSupport(m_SubFeatureId, &isSupported) != RC::OK || !isSupported)
        {
            return false;
        }

        UINT32 mask = 0;
        UINT32 reqSubFeature = (m_Feature == MSRPG) ?
            RM_PMU_LPWR_CTRL_FEATURE_ID_MS_RPG : RM_PMU_LPWR_CTRL_FEATURE_ID_MS_RPPG;

        rc = pgImpl->GetSubFeatureEnableMask(m_FeatureId, &mask);

        if (rc != RC::OK || !(mask & BIT(reqSubFeature)))
        {
            return false;
        }
    }

    Perf::PerfPoint lwrPerfPoint;
    rc = m_pSubdev->GetPerf()->GetLwrrentPerfPoint(&lwrPerfPoint);

    UINT32 pstateMask = 0;
    rc = pgImpl->GetLpwrPstateMask(m_FeatureId, &pstateMask);

    if (rc != RC::OK || !(BIT(lwrPerfPoint.PStateNum) & pstateMask))
    {
        return false;
    }

    if (m_Feature == MSRPPG)
    {
        pstateMask = 0;
        rc = pgImpl->GetRppgPstateMask(m_SubFeatureId, &pstateMask);
        if (rc != RC::OK || !(BIT(lwrPerfPoint.PStateNum) & pstateMask))
        {
            return false;
        }
    }
    else if (m_Feature == MSRPG && lwrPerfPoint.PStateNum != 8)
    {
        return false;
    }

    return true;
}

RC LowPowerPgWait::GetEnabled(bool* pgEnabled) const
{
    MASSERT(pgEnabled);
    RC rc;

    if (m_pSubdev->IsSOC())
    {
        return RC::OK;
    }

    *pgEnabled = false;

    std::unique_ptr<LpwrCtrlImpl> pgImpl = make_unique<LpwrCtrlImpl>(m_pSubdev);

    if (m_PgSupported)
    {
        CHECK_RC(pgImpl->GetLpwrCtrlEnabled(m_FeatureId, pgEnabled));
    }

    return rc;
}

RC LowPowerPgWait::Setup(Display* pDisp, DisplayMgr::TestContext *pTestContext)
{
    RC rc;

    m_pDisplay = pDisp;
    m_pTestContext = pTestContext;

    if (m_FeatureId == LowPowerPgWait::PgDisable)
    {
        Printf(Tee::PriError, "PG feature is not set\n");
        return RC::SOFTWARE_ERROR;
    }

    m_PgSupported = GetSupported();
    CHECK_RC(GetEnabled(&m_PgEnabled));

    return rc;
}

bool LowPowerPgWait::IsEISupported() const
{
    bool isSupported = false;

    RC rc = m_PgImpl->GetLpwrCtrlSupported(s_PgFeatureMap[EI].featureId, &isSupported);

    return rc == RC::OK ? isSupported : false;
}

RC LowPowerPgWait::SetLowPowerMinThreshold() const
{
    RC rc;

    // TODO: RM will provide feature support mask after which
    // we will set min threshold of all supported features
    if (IsEISupported())
    {
        CHECK_RC(m_PgImpl->SetLpwrMinThreshold(s_PgFeatureMap[EI].featureId));
    }

    CHECK_RC(m_PgImpl->SetLpwrMinThreshold(m_FeatureId));

    return rc;
}

RC LowPowerPgWait::PerformFeatureBasedSetup()
{
    RC rc;

    if (m_Feature == MSRPPG)
    {
        if (IsEISupported())
        {
            CHECK_RC(m_PgImpl->SetLpwrCtrlEnabled(s_PgFeatureMap[EI].featureId, 0));
        }

        CHECK_RC(m_PgImpl->SetSubFeatureEnableMask(m_FeatureId, RM_PMU_LPWR_MS_RPG_DISABLE_MASK));
    }

    if (m_Feature == MSRPG)
    {
        CHECK_RC(m_PgImpl->SetSubFeatureEnableMask(m_FeatureId, RM_PMU_LPWR_MS_RPPG_DISABLE_MASK));
    }

    if (m_Feature == PgFeature::MSCG)
    {
        UINT32 lowPowerModes = 0;
        CHECK_RC(m_PgImpl->GetLpwrModes(&lowPowerModes));
        INT32 index;
        while ((index = Utility::BitScanForward(lowPowerModes)) != -1)
        {
            m_Modes.push_back(index);
            lowPowerModes ^= 1 << index;
        }
    }

    // Some features like MSCG or features who depend on it require
    // IMP to be enabled if the display is active, otherwise,
    // display needs to be disabled to make feature engage
    if (m_Feature == MSCG || m_Feature == MSRPG || m_Feature == MSRPPG)
    {
        m_DisableDisplay = !m_pDisplay->IsIMPEnabled();
    }

    return rc;
}

RC LowPowerPgWait::Initialize()
{
    RC rc;

    m_PgImpl.reset(nullptr);

    if (m_PgSupported)
    {
        m_PgImpl.reset(new LpwrCtrlImpl(m_pSubdev));

        CHECK_RC(PerformFeatureBasedSetup());

        CHECK_RC(SetLowPowerMinThreshold());

        CHECK_RC(CacheEntryCount());
    }
    else
    {
        Printf(Tee::PriError, "%s not supported\n", GetFeatureName());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    return rc;
}

RC LowPowerPgWait::Teardown()
{
    StickyRC rc;

    if (m_PgSupported)
    {
        rc = m_PgImpl->ResetLpwrMinThreshold(m_FeatureId);

        if (m_Modes.size() > 1)
        {
            rc = m_PgImpl->ResetLpwrMode();
        }

        if (m_Feature == MSRPPG)
        {
            if (IsEISupported())
            {
                rc = m_PgImpl->SetLpwrCtrlEnabled(s_PgFeatureMap[EI].featureId, 1);
            }
        }

        if (m_Feature == MSRPG || m_Feature == MSRPPG)
        {
            rc = m_PgImpl->SetSubFeatureEnableMask(
                m_FeatureId, std::numeric_limits<UINT32>::max());
        }
    }

    m_PgImpl.reset(nullptr);

    return rc;
}

RC LowPowerPgWait::Wait(PwrWait::Action action, FLOAT64 timeoutMs)
{
    RC rc;

    PStateOwner pstateOwner;
    CHECK_RC(pstateOwner.ClaimPStates(m_pSubdev));

    bool displayOff = (m_DisableDisplay &&
        m_pDisplay->GetDisplayClassFamily() == Display::LWDISPLAY); 

    if (displayOff)
    {
        CHECK_RC(m_pDisplay->GetDisplayMgr()->DeactivateComposition());
    }

    DEFER
    {
        if (displayOff)
        {
            rc = m_pDisplay->GetDisplayMgr()->ActivateComposition();
            Surface2D *pDisplayImage = m_pTestContext ?
                m_pTestContext->GetSurface2D() : nullptr;

            if (pDisplayImage)
            {
                rc = m_pTestContext->DisplayImage(
                    pDisplayImage, DisplayMgr::DONT_WAIT_FOR_UPDATE);
            }
        }
    };

    PollEntryCountArgs args = { m_PgImpl.get(),
        m_EntryCount, GetSleepUs() / 1000, m_Feature };
    rc = POLLWRAP_HW(&PollPgIdleEntryCountFunction, &args, timeoutMs);

    if (rc == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriError, "Activate bubble timeout on %s entry!\n", GetFeatureName());
        PrintLowPowerStats();
    }

    if (m_DelayAfterBubbleMs)
    {
        Tasker::Sleep(m_DelayAfterBubbleMs);
    }

    m_EntryCount = args.pgEntryCount;

    return rc;
}

RC LowPowerPgWait::SwitchMode(Tee::Priority pri)
{
    RC rc;

    if (m_Modes.size() > 1)
    {
        CHECK_RC(m_PgImpl->SetLpwrMode(m_Modes[m_LwrrentMode]));
        Printf(pri, "Running Low power mode 0x%x\n", m_Modes[m_LwrrentMode]);

        m_LwrrentMode = (m_LwrrentMode + 1) % m_Modes.size();
    }

    return rc;
}

void LowPowerPgWait::PrintStats(Tee::Priority pri) const
{
    if (m_PgImpl.get() != nullptr)
    {
        Printf(pri, "%s Entry Count = %u\n", GetFeatureName(), m_EntryCount);
    }
}

RC LowPowerPgWait::GetEntryCount(UINT32 *pEntryCount) const
{
    RC rc;

    if (m_Feature == MSRPPG || m_Feature == MSRPG)
    {
        CHECK_RC(m_PgImpl->GetRppgEntryCount(m_SubFeatureId, pEntryCount));
    }
    else
    {
        CHECK_RC(m_PgImpl->GetLpwrCtrlEntryCount(m_FeatureId, pEntryCount));
    }

    return rc;
}

void LowPowerPgWait::PrintLowPowerStats() const
{
    bool isFeatureEngaged = false;
    UINT32 enableMask = 0;
    UINT32 abortMask = 0;
    UINT32 supportMask = 0;
    UINT32 featureEntryCount = 0;
    UINT32 subFeatureEntryCount = 0;
    UINT32 abortCount = 0;
    vector<UINT32> idleMask(3, 0);
    vector<UINT32> reasonMask(4, 0);

    MASSERT(m_PgImpl);
    m_PgImpl->GetLpwrCtrlEntryCount(m_FeatureId, &featureEntryCount);
    m_PgImpl->GetAbortCount(m_FeatureId, &abortCount);
    m_PgImpl->GetSubFeatureEnableMask(m_FeatureId, &enableMask);
    m_PgImpl->GetSubfeatureSupportMask(m_FeatureId, &supportMask);
    m_PgImpl->GetAbortReasonMask(m_FeatureId, &abortMask);
    m_PgImpl->GetFeatureEngageStatus(m_FeatureId, &isFeatureEngaged);
    m_PgImpl->GetDisableReasonMask(m_FeatureId, &reasonMask);
    m_PgImpl->GetLpwrCtrlIdleMask(m_FeatureId, &idleMask);

    Printf(Tee::PriNormal, "PG bubble stats:\n");
    if (m_Feature == MSRPPG || m_Feature == MSRPG)
    {
        m_PgImpl->GetRppgEntryCount(m_SubFeatureId, &subFeatureEntryCount);
        Printf(Tee::PriNormal,
            "\t%s entry count: %u\n", GetFeatureName(), subFeatureEntryCount);
    }

    Printf(Tee::PriNormal, "\tFeature entry count: %u\n", featureEntryCount);
    Printf(Tee::PriNormal, "\tFeature abort count: %u\n", abortCount);
    Printf(Tee::PriNormal, "\tFeature abort mask: 0x%x\n", abortMask);
    Printf(Tee::PriNormal, "\tFeature engage status: %u\n", isFeatureEngaged);
    Printf(Tee::PriNormal, "\tSubfeature support mask: 0x%x\n", supportMask);
    Printf(Tee::PriNormal, "\tSubfeature enabled mask: 0x%x\n", enableMask);
    Printf(Tee::PriNormal, "\tIdle Mask 0: 0x%x\n", idleMask[0]);
    Printf(Tee::PriNormal, "\tIdle Mask 1: 0x%x\n", idleMask[1]);
    Printf(Tee::PriNormal, "\tIdle Mask 2: 0x%x\n", idleMask[2]);
    Printf(Tee::PriNormal, "\tSW Disable Mask RM: 0x%x\n", reasonMask[0]);
    Printf(Tee::PriNormal, "\tHW Disable Mask RM: 0x%x\n", reasonMask[1]);
    Printf(Tee::PriNormal, "\tSW Disable Mask PMU: 0x%x\n", reasonMask[2]);
    Printf(Tee::PriNormal, "\tHW Disable Mask PMU: 0x%x\n", reasonMask[3]);
}

RC LowPowerPgWait::CacheEntryCount()
{
    return GetEntryCount(&m_EntryCount);
}

FLOAT64 LowPowerPgWait::GetSleepUs() const
{
    return m_SleepUs;
}

char* LowPowerPgWait::GetFeatureName() const
{
    switch (m_Feature)
    {
        case GPC_RG:
            return "GPC-RG";
        case GR_RPG:
            return "GR-RPG";
        case MSCG:
            return "MSCG";
        case MSRPG:
            return "MS-RPG";
        case MSRPPG:
            return "MS-RPPG";
        case EI:
            return "EI";
        default:
            MASSERT(!"Invalid feature\n");
            return "Invalid feature";
    }
}

//************************************************************************************************
// LowPowerPgBubble implementation
LowPowerPgBubble::LowPowerPgBubble(GpuSubdevice* pSubdev)
{
    m_LowPowerPgWait = make_unique<LowPowerPgWait>(pSubdev);
}

bool LowPowerPgBubble::IsSupported(UINT32 pgFeature) const
{
    if (m_LowPowerPgWait.get() == nullptr)
    {
        return false;
    }

    m_LowPowerPgWait->SetFeatureId(pgFeature);

    return m_LowPowerPgWait->GetSupported();
}

void LowPowerPgBubble::SetFeatureId(UINT32 feature) const
{
    if (m_LowPowerPgWait.get() != nullptr)
    {
        m_LowPowerPgWait->SetFeatureId(feature);
    }
}

RC LowPowerPgBubble::PrintIsSupported(Tee::Priority pri) const
{
    Printf(pri, "%s supported = %s enabled = %s\n",
           m_LowPowerPgWait->GetFeatureName(),
           m_LowPowerPgWait->IsSupported() ? "true" : "false",
           m_LowPowerPgWait->IsEnabled() ? "true" : "false");

    return RC::OK;
}

RC LowPowerPgBubble::Setup(Display* pDisp, DisplayMgr::TestContext *pTestContext)
{
    RC rc;

    CHECK_RC(m_LowPowerPgWait->Setup(pDisp, pTestContext));
    CHECK_RC(m_LowPowerPgWait->Initialize());

    return rc;
}

RC LowPowerPgBubble::Cleanup()
{
    return m_LowPowerPgWait->Teardown();
}

RC LowPowerPgBubble::ActivateBubble(FLOAT64 timeoutMs) const
{
    RC rc;

    FLOAT64 sleepMs = m_LowPowerPgWait->GetSleepUs() / 1000;
    timeoutMs = max(sleepMs, timeoutMs);
    CHECK_RC(m_LowPowerPgWait->Wait(PwrWait::PG_ON, timeoutMs));

    return rc;
}

void LowPowerPgBubble::PrintStats(Tee::Priority pri) const
{
    m_LowPowerPgWait->PrintStats(pri);
}

RC LowPowerPgBubble::SwitchMode(Tee::Priority pri)
{
    return m_LowPowerPgWait->SwitchMode(pri);
}

RC LowPowerPgBubble::GetEntryCount(UINT32 *pEntryCount) const
{
    return m_LowPowerPgWait->GetEntryCount(pEntryCount);
}

void LowPowerPgBubble::SetDelayAfterBubble(FLOAT64 delayMs) const
{
    m_LowPowerPgWait->SetDelayAfterBubble(delayMs);
}
