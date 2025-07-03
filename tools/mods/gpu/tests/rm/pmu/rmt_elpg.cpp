/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2017,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_elpg.cpp
//! \brief Test For Elpg code path
//!
//! Under this we do the directed as well as random testing of ELPG Code path.
//! For directed testing we created threads for each supported ELPG engine:
//! 1) GrElpgThread
//! 2) VidElpgThread
//! 3) VicElpgThread (retired)
//! 4) MsElpgThread
//! For Random testing along with above threads we took more threads :
//! 5) GrPbActivity
//! 6) VidPbActivity
//! 7) VicPbActivity (retireD)
//!
//! Soul OF Directed Testing :-
//! WHEN ---> testType = MethodOp OR testType = RegisterOp
//! In this section we try to elwoke ELPG_OFF while ELPG_ON is in progress.
//!
//! if (testType == MethodOp)--> try to elwoke ELPG OFF through PB activity
//!                              for engine.
//!
//! if (testType == RegisterOp)-->Try to elwoke ELPG_OFF through register
//!                               access.
//!
//! Soul OF Random Testing :-
//! WHEN ---> testType = Random
//! Spawn all the four threads mentioned above, among that,
//!
//! GrElpgThread ---|
//!                 |
//! VidElpgThread---|----- >Will try to elwoke ELPG routine for relevant engine.
//!                 |
//! VicElpgThread---|
//!
//! GrPbActivity----|
//!                 |
//! VidPbActivity---|----- >Will try to perform some activity on engine.
//!                 |
//! VicPbActivity---|
//!
//! MsBarBlockerTest: Checks MSCG working register access
//!

#include "gpu/tests/rmtest.h"
#include "core/include/platform.h"
#include "core/include/jscript.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tasker.h"
#include "rmpmucmdif.h"
#include "gpu/perf/pmusub.h"
#include "class/cl502d.h"
#include "class/cl85b6.h"
#include "class/cl0070.h" // LW01_MEMORY_VIRTUAL
#include "class/cl826f.h"
#include "class/cl906f.h"
#include "class/cl902d.h"
#include "class/cl007d.h"
#include "class/cla06fsubch.h"
#include "class/cla097.h"
#include "class/cl95b1.h"
#include "class/cla06fsubch.h"
#include "class/cla06f.h"
#include "class/clc0b7.h" // LWC0B7_VIDEO_ENCODER
#include "class/clb0b5.h" // MAXWELL_DMA_COPY_A
#include "class/cl0005.h" // LW0005_ALLOC_PARAMETERS
#include "class/cl947d.h" // Pixel Depth
#include "kepler/gk104/dev_fifo.h"
#include "kepler/gk104/dev_graphics_nobundle.h"
#include "kepler/gk104/dev_msppp_pri.h"
#include "kepler/gk104/dev_therm.h"
#include "kepler/gk104/dev_lw_xve.h"
#include "kepler/gk104/dev_lw_xp.h"
#include "disp/v02_01/dev_disp.h"
#include "kepler/gk104/dev_bus.h"
#include "kepler/gk104/dev_pwr_pri.h"
#include "kepler/gk104/dev_top.h"
#include "kepler/gk104/dev_timer.h"
#include "kepler/gk104/dev_perf.h"
#include "kepler/gk104/dev_pbdma.h"
#include "filteredgk104regs.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/utility.h"
#include "lwRmApi.h"
#include "core/include/xp.h"
#include "lwmisc.h"
//#include "gpu/utility/bglogger.h"
#include "core/include/display.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/display/evo_dp.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "core/include/memcheck.h"
using namespace DTIUtils;

#define GR_ENGINE                      RM_PMU_LPWR_CTRL_ID_GR_PG
#define VID_ENGINE                     RM_PMU_LPWR_CTRL_ID_GR_PASSIVE
#define MS_ENGINE                      RM_PMU_LPWR_CTRL_ID_MS_MSCG
#define TOTAL_ENGINES                  RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE

#define ACQUIRE_SEMAPHORE              0
#define RELEASE_SEMAPHORE              1

#define SEC_TO_NSEC(sec)               (sec * 1000000000)

//
// Macro to check for error return from threads
//
#define CHECK_THSTATUS( retValue, rcVariable, errorMessage )                  \
rcVariable = retValue;                                                        \
if (rcVariable)                                                               \
{                                                                             \
    Printf( Tee::PriHigh, errorMessage );                                     \
    goto DONE;                                                                \
}

static void *g_pMuHandle = NULL;
static void *g_pMuVidPbHandle = NULL;
static void *g_pMuVicPbHandle = NULL;
static FLOAT64 g_TimeoutMs = 0;
static UINT32 g_semValue = 0;
static UINT32 g_nThCompletionCount = 0;
static UINT32 g_waitTimeMs = 0;

static bool g_bIsKepler         = false;
static bool g_bIsMaxwell        = false;
static bool g_bIsMaxwellNoVideo = false;

// GC5 test specific static variables.
static bool g_bGC5GpuReady     = false;
static bool g_bGC5CycleAborted = false;

static GpuSubdevice    *subDev;

enum
{
    GR_ELPG_CHANNEL_CONTEXT   = 0x0,
    GR_PB_CHANNEL_CONTEXT,
    VID_ELPG_CHANNEL_CONTEXT,
    VID_PB_CHANNEL_CONTEXT,
    MS_ELPG_CHANNEL_CONTEXT,
    MS_PB_CHANNEL_CONTEXT,
    TOTAL_CHANNEL_CONTEXT,
};

enum
{
    GR_SEMAPHORE_CONTEXT     = 0x0,
    VID_SEMAPHORE_CONTEXT,
    VID_PB_SEMAPHORE_CONTEXT,
    VIC_SEMAPHORE_CONTEXT,
    VIC_PB_SEMAPHORE_CONTEXT,
    MS_SEMAPHORE_CONTEXT,
    MS_PB_SEMAPHORE_CONTEXT,
    TOTAL_SEMAPHORE_CONTEXT,
};

enum TestType
{
    MethodOp,
    RegisterOp,
    Random,
    GC5
};

enum SubTestType
{
    Disallow,
    XVEBlocker,
    NumSubTest,
    GC5WaitForMSCG
};

enum GC5TestExitCondition
{
    SwTimer,
    HwTimer
};

struct THREAD_INFO
{
    RC           tRc;
    Channel      *pCh;
    UINT32       *thCpuAddr;
    UINT64       thGpuAddr;
    UINT32       *thCpuAddrPb;
    UINT64       thGpuAddrPb;
    TestType     testType;
    SubTestType  subTestType;
    Notifier     *pThNotifier;
    LwRm::Handle hThClass;
    LwRm::Handle hThSwClass;
    LwRm::Handle hThSemVA;
    LwRm::Handle hThSemVAPb;
    GpuSubdevice *gpuSubdev;
};

// structure for storing the idle and post power thresholds for the engines
struct OrigThresh
{
    UINT32 idleThresh;
    UINT32 ppuThresh;
    bool   threshChanged;
}OrigThresh[RM_PMU_LPWR_CTRL_ID_ILWALID_ENGINE];

static void GrPbActivity(void *p);                     // Pb Activity thread for GR
static void GrElpgThread(void *p);                     // Elpg Sequence thread for GR
static void VidPbActivity(void *p);                    // Pb Activity thread for Video
static void VidElpgThread(void *p);                    // Elpg Sequence thread for Video
static void MsElpgThread(void *p);                     // Elpg Sequence thread for MS
static void MsBarBlockerTest(void *p);                 // Elpg Sequence thread for MS stress test
static RC GrMethods(void *p);                          // Helper Function for GR PB activity
static RC VidMethods(void *p);                         // Helper Function for Vid PB activity
static RC SemaphoreOperation(void *p, bool bRelease);  // Helper Function For Semaphore opertaions
static RC PowerDownMs(void *p);                        // Helper function for MS testing

static RC QueryPowerGatingParameter(UINT32 param,
                                    UINT32 paramExt,
                                    UINT32 *paramVal,
                                    bool set);

static RC SetGpuPowerOnOff(UINT32 action,
                           bool   bUseHwTimer,
                           UINT32 timeToWakeUs,
                           bool   bAutoSetupHwTimer);
static RC WaitForGC6PlusIslandLoad();
static RC SchedulePtimerAlarm(UINT64 timeNs);
static RC CancelPendingPtimerAlarms();

//! Memory storing notifer responses from RM that contain the information
//! regarding gpu_rdy
Surface2D       m_GpuRdyNotifier;
LwNotification *m_pGpuRdyNotifier;

//! Callback information for ptimer alarm
LWOS10_EVENT_KERNEL_CALLBACK_EX tmrCallback;
LW0005_ALLOC_PARAMETERS         allocParams;

class ElpgTest: public RmTest
{
public:
    ElpgTest();
    virtual ~ElpgTest();
    virtual string     IsTestSupported();
    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();

    SETGET_PROP(LoopVar, UINT32);           //!< Grab JS variable LoopVar
    SETGET_PROP(NoGr,  bool);               //!< Grab JS variable NoGr
    SETGET_PROP(NoVideo, bool);             //!< Grab JS variable NoVideo
    SETGET_PROP(NoMs, bool);                //!< Grab JS variable NoMS
    SETGET_PROP(IdleThresh, UINT32);        //!< Grab JS variable IdleThresh
    SETGET_PROP(PpuThresh, UINT32);         //!< Grab JS variable PpuThresh
    SETGET_PROP(EnableBarBlocker, bool);    //!< Grab JS variable EnableBarBlocker
    SETGET_PROP(GC5TestDisplay, bool);      //!< Grab JS variable GC5TestDisplay
    SETGET_PROP(EnableGC5, bool);           //!< Grab JS variable EnableGC5
    SETGET_PROP(EnableDIOS, bool);           //!< Grab JS variable EnableDIOS
    SETGET_PROP(GC5WaitForMSCG, bool);      //!< Grab JS variable GC5WaitForMSCG
    SETGET_PROP(GC5UseHwTimer, bool);       //!< Grab JS variable GC5UseHwTimer
    SETGET_PROP(GC5WakeAfterUs, UINT32);    //!< Grab JS variable GC5WakeAfterUs
    SETGET_PROP(UseSmallRaster, bool);      //!< Grab JS variable UseSmallRaster
    SETGET_PROP(GC5Loops, UINT32);          //!< Grab JS variable GC5Loops
    SETGET_PROP(GC5ModeFRL, bool);          //!< Grab JS variable GC5ModeFRL
    SETGET_PROP(GC5FrameRate, UINT32);      //!< Grab JS variable GC5FrameRate

private:
    RC RunDIOSTest();
    RC RunGC5Test();
    RC SetupGr(UINT32 className);
    RC SetupVid(UINT32 className);
    RC SetupVic(UINT32 className);
    RC SetupMs(UINT32 className);
    RC SetGpuRdyEventNotifierMemory();
    RC SetGpuRdyEventNotification(UINT32 index, UINT32 action);

    RC AllocSemMemory(UINT32 channelContext, UINT32 memContext);

    void InitializeThreads(UINT32 channelContext, UINT32 engine);
    UINT32           m_NumElpgEngs;    // number of Elpg engines

    UINT32           m_LoopVar;
    bool             m_NoGr;
    bool             m_NoVideo;
    bool             m_NoMs;
    UINT32           m_IdleThresh;
    UINT32           m_PpuThresh;
    bool             m_EnableBarBlocker;
    bool             m_GC5TestDisplay;
    bool             m_EnableGC5;
    bool             m_EnableDIOS;
    bool             m_GC5WaitForMSCG;
    bool             m_GC5UseHwTimer;
    UINT32           m_GC5WakeAfterUs;
    bool             m_UseSmallRaster;
    UINT32           m_GC5Loops;
    bool             m_GC5ModeFRL;
    UINT32           m_GC5FrameRate;
    PMU             *m_pPmu;

    Channel         *m_pCh[TOTAL_CHANNEL_CONTEXT];
    LwRm::Handle     m_hCh[TOTAL_CHANNEL_CONTEXT];
    THREAD_INFO     *m_psThInfo[TOTAL_CHANNEL_CONTEXT];

    UINT64           m_gpuAddr[TOTAL_SEMAPHORE_CONTEXT];
    UINT32 *         m_cpuAddr[TOTAL_SEMAPHORE_CONTEXT];
    LwRm::Handle     m_hVA[TOTAL_SEMAPHORE_CONTEXT];
    LwRm::Handle     m_hSemMem[TOTAL_SEMAPHORE_CONTEXT];

    LwRm::Handle     m_hObjElpg[TOTAL_ENGINES];
    LwRm::Handle     m_hObjPb[TOTAL_ENGINES];
    LwRm::Handle     m_hObjElpgSw[TOTAL_ENGINES];
    LwRm::Handle     m_hObjPbSw[TOTAL_ENGINES];

    Notifier         m_ElpgNotifier[TOTAL_ENGINES];
    Notifier         m_PbNotifier[TOTAL_ENGINES];

    string           m_protocol;
    Display          *pDisplay;

};

//! \brief ElpgTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ElpgTest::ElpgTest()
{
    SetName("ElpgTest");

    m_NoGr              = false;
    m_NoVideo           = false;
    m_NoMs              = false;
    m_EnableBarBlocker  = false;
    m_GC5TestDisplay    = false;
    m_UseSmallRaster    = false;
    m_EnableDIOS        = false;
    m_EnableGC5         = false;
    m_GC5WaitForMSCG    = false;
    m_GC5UseHwTimer     = false;
    m_GC5WakeAfterUs    = 5000000; // arbitrary default value
    m_GC5Loops          = 2;       // Default value
    m_GC5ModeFRL        = false;
    m_GC5FrameRate      = 60;      // Default frame rate
    m_protocol          = "DP_A,DP_B"; // GC5 only supported on DP

    memset(&OrigThresh, 0, sizeof(OrigThresh));

    // Initialize variables
    m_NumElpgEngs = TOTAL_ENGINES;

    for (int i = 0; i < TOTAL_CHANNEL_CONTEXT; i++)
    {
        m_pCh[i] = NULL;
        m_hCh[i] = 0;
        m_psThInfo[i] = NULL;
    }

    for (int i = 0; i < TOTAL_SEMAPHORE_CONTEXT; i++)
    {
        m_hVA[i] = 0;
        m_hSemMem[i] = 0;
        m_gpuAddr[i] = 0;
        m_cpuAddr[i] = NULL;
    }

    for (int i = 0; i < TOTAL_ENGINES; i++)
    {
        m_hObjElpg[i]    = 0;
        m_hObjPb[i]      = 0;
        m_hObjElpgSw[i]  = 0;
        m_hObjPbSw[i]    = 0;
    }
    subDev = GetBoundGpuSubdevice();
}

//! \brief ElpgTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ElpgTest::~ElpgTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string ElpgTest::IsTestSupported()
{
    RC           rc;
    UINT32       aelpgEnable;

    UINT32       powerGatingParameterEnabled;

    g_bIsKepler  = (LwRmPtr()->IsClassSupported(KEPLER_CHANNEL_GPFIFO_A, GetBoundGpuDevice()) &&
                    LwRmPtr()->IsClassSupported(LW95B1_VIDEO_MSVLD, GetBoundGpuDevice()));

    g_bIsMaxwell = (LwRmPtr()->IsClassSupported(MAXWELL_DMA_COPY_A, GetBoundGpuDevice()));
    g_bIsMaxwellNoVideo = g_bIsMaxwell && (!LwRmPtr()->IsClassSupported(LWC0B7_VIDEO_ENCODER, GetBoundGpuDevice()));

    if (g_bIsKepler || g_bIsMaxwell || g_bIsMaxwellNoVideo)
    {
        rc = QueryPowerGatingParameter
                 (LW2080_CTRL_MC_POWERGATING_PARAMETER_NUM_POWERGATEABLE_ENGINES,
                  0, &m_NumElpgEngs, false);
        if (rc == OK)
        {
            // BUG : 607411, Check to see if aelpg is enabled
            rc = QueryPowerGatingParameter
                     (LW2080_CTRL_MC_POWERGATING_PARAMETER_AELPG_ENABLED,
                      GR_ENGINE, &aelpgEnable, false);
            if (rc == OK)
            {
                if (aelpgEnable)
                {
                    return "aelpg is enabled";
                }
            }
        }
        else
        {
            Printf(Tee::PriHigh," NUM_POWERGATEABLE_ENGINES not supported\n");
            return "NUM_POWERGATEABLE_ENGINES not supported";
        }
    }

    for(UINT32 eng = 0; eng < m_NumElpgEngs; eng++)
    {
        switch(eng)
        {
            case GR_ENGINE:
                if (!m_NoGr)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                       GR_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled";
                    }
                    else
                    {
                        m_NoGr = !(powerGatingParameterEnabled);
                    }
                }
                break;

            case VID_ENGINE:
                if (!m_NoVideo)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                    VID_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled";
                    }
                    else
                    {
                        m_NoVideo = !(powerGatingParameterEnabled);
                    }
                }
                break;

            case MS_ENGINE:
                if (!m_NoMs)
                {
                    rc = QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_ENABLED,
                                    MS_ENGINE, &powerGatingParameterEnabled, false);
                    if (rc == RC::LWRM_NOT_SUPPORTED)
                    {
                        return "Elpg Is Not Enabled";
                    }
                    else
                    {
                        m_NoMs = !(powerGatingParameterEnabled);
                    }
                }
                break;
        }
    }

    if (m_NoGr && m_NoVideo && m_NoMs)
    {
        return "No engine supports ELPG, returning false";
    }

    if (g_bIsMaxwell)
    {
        if (m_EnableGC5 && m_GC5TestDisplay)
        {
            return "GC5+Display testing is not supported";
        }
        return RUN_RMTEST_TRUE;
    }

    return "Supported on GT21X, IGT21X, Fermi+";

}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsSupported
//-----------------------------------------------------------------------------
RC ElpgTest::Setup()
{
    RC           rc;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    g_TimeoutMs = GetTestConfiguration()->TimeoutMs();
    if(Platform::GetSimulationMode() == Platform::Fmodel)
    {
        // increase the time out on fmodel and base
        // it as a multiple of TimeoutMs
        g_waitTimeMs = (UINT32)g_TimeoutMs * 100;
    }
    else
    {
        g_waitTimeMs = (UINT32)g_TimeoutMs;
    }
    m_TestConfig.SetAllowMultipleChannels(true);

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    // Allocate channel for Gr engine
    if (!m_NoGr)
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[GR_ELPG_CHANNEL_CONTEXT], &m_hCh[GR_ELPG_CHANNEL_CONTEXT]));
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[GR_PB_CHANNEL_CONTEXT], &m_hCh[GR_PB_CHANNEL_CONTEXT]));
    }

    // Allocate channel for video engine
    if (!m_NoVideo)
    {
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[VID_ELPG_CHANNEL_CONTEXT], &m_hCh[VID_ELPG_CHANNEL_CONTEXT]));
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[VID_PB_CHANNEL_CONTEXT], &m_hCh[VID_PB_CHANNEL_CONTEXT]));
    }

    if (!m_NoMs)
    {
        // Allocate channel for MS
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[MS_ELPG_CHANNEL_CONTEXT], &m_hCh[MS_ELPG_CHANNEL_CONTEXT]));
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh[MS_PB_CHANNEL_CONTEXT], &m_hCh[MS_PB_CHANNEL_CONTEXT]));
    }

    // setup specific to GK104
    if (g_bIsKepler)
    {
        CHECK_RC(SetupGr(FERMI_TWOD_A));
        CHECK_RC(SetupMs(LW95B1_VIDEO_MSVLD));
    }
    //setup specific to GM10x
    else if (g_bIsMaxwell)
    {
        CHECK_RC(SetupGr(FERMI_TWOD_A));
        if (!g_bIsMaxwellNoVideo)
        {
            CHECK_RC(SetupMs(LWC0B7_VIDEO_ENCODER));
        }
    }

    if (m_EnableDIOS)
    {
        // Get PMU class instance
        rc = (GetBoundGpuSubdevice()->GetPmu(&m_pPmu));
        if (OK != rc)
        {
            Printf(Tee::PriHigh,"PMU not supported\n");
            CHECK_RC(rc);
        }
    }

    // BUG : 515433, Change idleThreshold and postPowerUpThreshold for fmodel
    if(Platform::GetSimulationMode() == Platform::Fmodel)
    {
        UINT32 engId;

        for(engId = 0; engId < TOTAL_ENGINES; engId++)
        {
            switch(engId)
            {
                case GR_ENGINE:
                    if(!m_NoGr)
                    {
                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        GR_ENGINE, &OrigThresh[GR_ENGINE].idleThresh , false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        GR_ENGINE, &m_IdleThresh, true));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        GR_ENGINE, &OrigThresh[GR_ENGINE].ppuThresh, false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        GR_ENGINE, &m_PpuThresh, true));
                        OrigThresh[GR_ENGINE].threshChanged = true;
                    }
                    break;
                case VID_ENGINE:
                    if(!m_NoVideo)
                    {
                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        VID_ENGINE,&OrigThresh[VID_ENGINE].idleThresh, false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        VID_ENGINE,&m_IdleThresh, true));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        VID_ENGINE,&OrigThresh[VID_ENGINE].ppuThresh, false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        VID_ENGINE, &m_PpuThresh, true));
                        OrigThresh[VID_ENGINE].threshChanged = true;
                     }
                    break;
                case MS_ENGINE:
                    if(!m_NoMs)
                    {
                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        MS_ENGINE,&OrigThresh[MS_ENGINE].idleThresh, false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                        MS_ENGINE,&m_IdleThresh, true));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        MS_ENGINE,&OrigThresh[MS_ENGINE].ppuThresh, false));

                        CHECK_RC(QueryPowerGatingParameter
                        (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                        MS_ENGINE, &m_PpuThresh, true));
                        OrigThresh[MS_ENGINE].threshChanged = true;
                     }
                    break;
                default:
                    MASSERT(false);
                    break;
            } //end switch engInd

        } // end for
     }

    return rc;
}

//! \brief GPU ready handler
//!
//! Ignore the Args struct because mods events don't support data with them.
//! Data is obtained from separate surface.
//-----------------------------------------------------------------------------
static void s_GpuReadyNotifyHandler(void* Args)
{
    UINT32 eventType =
        MEM_RD16(&m_pGpuRdyNotifier[LW2080_NOTIFIERS_GC5_GPU_READY].info16);
    Printf(Tee::PriHigh,"ElpgTest: Received EventType: %d\n", eventType);

    switch (eventType)
    {
        case LW2080_GC5_EXIT_COMPLETE:
            Printf(Tee::PriHigh,"ElpgTest: Received GC5 Exit complete notification\n");
            break;

        case LW2080_GC5_ENTRY_ABORTED:
            Printf(Tee::PriHigh,"ElpgTest: Received GC5 Entry Abort notification\n");
            g_bGC5CycleAborted = true;
            break;

        default:
            Printf(Tee::PriHigh,"ElpgTest: Unknown gpu_rdy notification sent from RM\n");
            g_bGC5CycleAborted = true; // Unknown gpu ready type should fail the test.
            break;
    }

    g_bGC5GpuReady = true;
}

//! \brief Ptimer Alarm Handler for GC5 test at given frame rate.
//!
//-----------------------------------------------------------------------------
static void s_PtimerAlarmHandler(void *pArg, void *pData, LwU32 hEvent, LwU32 data, LwU32 status)
{
    Printf(Tee::PriHigh,"ElpgTest: Ptimer alarm fired!\n");
}

//! \brief Set up event notifications
RC ElpgTest::SetGpuRdyEventNotification(UINT32 index, UINT32 action)
{
    LW2080_CTRL_EVENT_SET_NOTIFICATION_PARAMS setEventParams;
    RC                                        rc;
    LwRmPtr                                   pLwRm;

    setEventParams.event = index;
    setEventParams.action = action;

    CHECK_RC(pLwRm->ControlBySubdevice(subDev,
                                       LW2080_CTRL_CMD_EVENT_SET_NOTIFICATION,
                                       &setEventParams, sizeof(setEventParams)));
    return rc;
}

//! \brief Set up gpu_rdy event notifier memory
RC ElpgTest::SetGpuRdyEventNotifierMemory()
{
    RC           rc;
    UINT32       eventMemSize = sizeof(LwNotification) * LW2080_NOTIFIERS_MAXCOUNT;
    LwRmPtr      pLwRm;
    LwRm::Handle RmHandle = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    LW2080_CTRL_EVENT_SET_MEMORY_NOTIFIES_PARAMS memNotifiesParams = { 0 };

    // Setup the surface for the Event queues
    eventMemSize = ALIGN_UP(eventMemSize,
                            ColorUtils::PixelBytes(ColorUtils::VOID32));
    m_GpuRdyNotifier.SetWidth(eventMemSize /
                            ColorUtils::PixelBytes(ColorUtils::VOID32));
    m_GpuRdyNotifier.SetPitch(eventMemSize);
    m_GpuRdyNotifier.SetHeight(1);
    m_GpuRdyNotifier.SetColorFormat(ColorUtils::VOID32);
    m_GpuRdyNotifier.SetLocation(Memory::Coherent);
    m_GpuRdyNotifier.SetAddressModel(Memory::Paging);
    m_GpuRdyNotifier.SetKernelMapping(true);
    CHECK_RC(m_GpuRdyNotifier.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_GpuRdyNotifier.Fill(0,
                         GetBoundGpuSubdevice()->GetSubdeviceInst()));
    CHECK_RC(m_GpuRdyNotifier.Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));

    m_pGpuRdyNotifier = (LwNotification *)m_GpuRdyNotifier.GetAddress();

    memNotifiesParams.hMemory = m_GpuRdyNotifier.GetMemHandle();

    // set event notifer memory
    CHECK_RC(pLwRm->Control(RmHandle,
                            LW2080_CTRL_CMD_EVENT_SET_MEMORY_NOTIFIES,
                            &memNotifiesParams, sizeof(memNotifiesParams)));
    return rc;
}

//! \brief DIOS test
RC ElpgTest::RunDIOSTest()
{
    RC      rc;
    UINT32  sleepTimems = 1000; // default value for non-fmodel
    Printf(Tee::PriHigh,"%s: -------- DIOS Test -------------\n", GetName().c_str());

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        //
        // Fmodel is slow and we need to be idle for a much longer time in order
        // to enter GC5 DI-OS.
        //
        sleepTimems = 100000;

        //
        // Also schedule a ptimer alarm since fmodel doesn't wake up on register
        // reads.
        //

        Printf(Tee::PriHigh,"%s: On fmodel, schedule the ptimer alarm at some "
                            "point in the future. When the PMU looks at the "
                            "scheduled ptimer alarm, it will program the wakeup "
                            "timer as a backup wake option. PEX based wakeups "
                            "can take arbitrarily long (in lwclk) on fmodel\n",
               GetName().c_str());
        SchedulePtimerAlarm((UINT64)(1000000000/120));
    }

    // Sleep for a while, so we have a chance to enter GC5
    Printf(Tee::PriHigh,"%s: Creating a bubble of idleness to allow "
                        "the GPU to enter DI-OS.\n", GetName().c_str());
    Tasker::Sleep((FLOAT64)sleepTimems);

    // Send some activity over PEX bus
    Printf(Tee::PriHigh,"%s: Trying to trigger a wake up.\n", GetName().c_str());
    subDev->RegRd32(LW_PTIMER_TIME_0);

    // Collect the deepidle statstics
    Printf(Tee::PriHigh, "%s: Querying DI-OS stats\n", GetName().c_str());

    LW2080_CTRL_GPU_READ_DEEP_IDLE_STATISTICS_PARAMS endStats = { 0 };
    CHECK_RC(m_pPmu->GetDeepIdleStatistics(PMU::DEEP_IDLE_NO_HEADS,
                                            &endStats));

    Printf(Tee::PriHigh,
            "%s: Deep Idle Stats - Attempts (%d) Entries (%d), Exits(%d)\n",
            GetName().c_str(),
            (UINT32)endStats.attempts, (UINT32)endStats.entries,
            (UINT32)endStats.exits);

    Printf(Tee::PriHigh,"%s: -------- DIOS Test Complete -------------\n", GetName().c_str());

    if (endStats.entries == 0)
    {
        Printf(Tee::PriHigh,"%s: No successful entries. This will fail the test.\n", GetName().c_str());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC ElpgTest::RunGC5Test()
{
    RC                  rc;

    UINT32               loop = 0;
    UINT32               thNumber;
    DisplayIDs           supported;
    DisplayIDs           Detected;
    DisplayIDs           displaysToFake;
    DisplayIDs           workingSet;
    AcrVerify            acrVerify;

    CHECK_RC(acrVerify.BindTestDevice(GetBoundTestDevice()));

    if (!m_NoMs)
    {
        Printf(Tee::PriHigh,"%s: -------- GC5 Test -------------\n", GetName().c_str());

        // Setup the surface for the 2080 Event queues
        CHECK_RC(SetGpuRdyEventNotifierMemory());

        GpuSubdevice::NotifierMemAction memAction = GpuSubdevice::NOTIFIER_MEMORY_DISABLED;
        // Hook up Gpu ready event handler
        CHECK_RC(subDev->HookResmanEvent(LW2080_NOTIFIERS_GC5_GPU_READY,
                                         s_GpuReadyNotifyHandler,
                                         this,
                                         LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT,
                                         memAction));

        Printf(Tee::PriHigh,"%s: setting up gpu ready notification parameters\n", GetName().c_str());

        CHECK_RC(SetGpuRdyEventNotification(LW2080_NOTIFIERS_GC5_GPU_READY, LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_DISABLE));
        CHECK_RC(SetGpuRdyEventNotification(LW2080_NOTIFIERS_GC5_GPU_READY, LW2080_CTRL_EVENT_SET_NOTIFICATION_ACTION_REPEAT));

        while (loop++ < m_GC5Loops)
        {
            Printf(Tee::PriHigh,"%s: Starting Loop %d for GC5 Testing\n", GetName().c_str(), loop);
            thNumber = 1;

            if (loop == 1)
            {
                // Wait for RAM_VALID (SCI/BSI to load) on the first loop
                CHECK_RC(WaitForGC6PlusIslandLoad());

                // Activate FRL mode on the first loop
                CHECK_RC(SetGpuPowerOnOff(LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ACTIVATE,
                                          false, // don't care
                                          0,     // don't care
                                          false  // don't care
                                          ));
            }

            if (m_GC5ModeFRL)
            {
                // Schedule PTIMER_ALARM interrupt after 1/FRL.
                CHECK_RC(SchedulePtimerAlarm((UINT64)(1000000000/m_GC5FrameRate)));
            }

            m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->testType = GC5;
            if(m_GC5WaitForMSCG && !m_GC5ModeFRL)
            {
                //
                // Only run the MSCG thread in non-FRL mode. We can't schedule a ptimer alarm
                // and then execute code for an undeterministic period of time in FRL mode.
                //
                m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->subTestType = GC5WaitForMSCG;
            }
            else
            {
                m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->subTestType = Disallow;
            }

            Tasker::CreateThread(MsElpgThread,(void *)m_psThInfo[MS_ELPG_CHANNEL_CONTEXT],0,NULL);

            while (thNumber > g_nThCompletionCount)
            {
                // Wait for all the threads to complete their exelwtion
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->tRc);

            // Enter GC5
            if (m_GC5UseHwTimer)
            {
                CHECK_RC(SetGpuPowerOnOff(LW2080_CTRL_GPU_POWER_ON_OFF_GC5_ENTER,
                                          true,
                                          m_GC5WakeAfterUs,
                                          (m_GC5ModeFRL ? true: false)));
            }
            else
            {
                Printf(Tee::PriHigh,"%s: GC5 Test doesn't support exit by sw timer/mehtod op yet\n",
                       GetName().c_str());
                return RC::SOFTWARE_ERROR;
            }

            // Wait for GC5 GPU ready
            do
            {
                Tasker::Sleep(100);
            } while (g_bGC5GpuReady == false);

            if (m_GC5ModeFRL)
            {
                // Cancel pending ptimer alarms
                CHECK_RC(CancelPendingPtimerAlarms());
            }

            // Ready yourself for the next loop.
            g_bGC5GpuReady = false;

            g_nThCompletionCount = 0;

            // Verify ACR and LS falcon setups
            if (acrVerify.IsTestSupported() == RUN_RMTEST_TRUE)
            {
                Printf(Tee::PriHigh,"%s: Verifying ACR Feature\n", GetName().c_str());
                CHECK_RC(acrVerify.Setup());
                // Run Verifies ACM setup and status of each Falcon.
                rc = acrVerify.Run();
            }

            Printf(Tee::PriHigh,"%s: Loop %d completed for GC5 Testing\n", GetName().c_str(), loop);
        }

        CHECK_RC(SetGpuPowerOnOff(LW2080_CTRL_GPU_POWER_ON_OFF_GC5_DEACTIVATE,
                                  false, // don't care
                                  0,     // don't care
                                  false  // don't care
                                  ));

        Printf(Tee::PriHigh,"%s: -------- GC5 Test Complete -------------\n", GetName().c_str());

        if (g_bGC5CycleAborted)
        {
            Printf(Tee::PriHigh,"%s: One or more GC5 cycles aborted. This is not expected and will fail the test.\n",
                   GetName().c_str());
            rc = RC::SOFTWARE_ERROR;
        }

        return rc;
    }
    return rc;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC ElpgTest::Run()
{
    RC             rc;
    UINT32         thNumber;
    UINT32         loop = 0;
    g_semValue = 0x123;
    g_nThCompletionCount = 0;

    g_pMuHandle = Tasker::AllocMutex("ElpgTest::g_pMuHandle",
                                     Tasker::mtxUnchecked);
    g_pMuVidPbHandle = Tasker::AllocMutex("ElpgTest::g_MuVidPbHandle",
                                          Tasker::mtxUnchecked);
    g_pMuVicPbHandle = Tasker::AllocMutex("ElpgTest::g_pMuVicPbHandle",
                                          Tasker::mtxUnchecked);

    // Initialize thread Arguments
    InitializeThreads(GR_ELPG_CHANNEL_CONTEXT, GR_ENGINE);
    InitializeThreads(GR_PB_CHANNEL_CONTEXT, GR_ENGINE);
    InitializeThreads(VID_ELPG_CHANNEL_CONTEXT, VID_ENGINE);
    InitializeThreads(VID_PB_CHANNEL_CONTEXT, VID_ENGINE);
    InitializeThreads(MS_ELPG_CHANNEL_CONTEXT, MS_ENGINE);
    InitializeThreads(MS_PB_CHANNEL_CONTEXT, MS_ENGINE);

    // Test the DIOS sequence
    if (m_EnableDIOS)
    {
        return RunDIOSTest();
    }

    // Test the GC5 sequence
    if (m_EnableGC5)
    {
        return RunGC5Test();
    }

    thNumber = 1;

    Printf(Tee::PriHigh,"-------- Directed Test Start -------------\n");
    // This loop will handle the case of ELPG OFF While ELPG on is in Process
    while (loop++ < m_LoopVar)
    {
         if (!m_NoVideo)
        {
            Tasker::CreateThread(VidElpgThread,(void *)m_psThInfo[VID_ELPG_CHANNEL_CONTEXT],0,NULL);

            while (thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[VID_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;
        }
        if (!m_NoGr)
        {
            Tasker::CreateThread(GrElpgThread,(void *)m_psThInfo[GR_ELPG_CHANNEL_CONTEXT],0,NULL);
            while (thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[GR_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;
        }

        Printf(Tee::PriHigh,"LOOP %d Completed For Method Operation Testing\n", loop);
    }

    // This loop will handle the case of ELPG_OFF because of register access
    // To some extent it will also validate the CTX Save/Restore mecahnism.

    m_psThInfo[VID_ELPG_CHANNEL_CONTEXT]->testType = RegisterOp;
    m_psThInfo[GR_ELPG_CHANNEL_CONTEXT]->testType  = RegisterOp;

    loop = 0;

    while (loop++ < m_LoopVar)
    {
        if (!m_NoVideo)
        {
            Tasker::CreateThread(VidElpgThread,(void *)m_psThInfo[VID_ELPG_CHANNEL_CONTEXT],0,NULL);
            while (thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[VID_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;
        }
        if (!m_NoGr)
        {
            Tasker::CreateThread(GrElpgThread,(void *)m_psThInfo[GR_ELPG_CHANNEL_CONTEXT],0,NULL);
            while(thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[GR_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;
        }

        Printf(Tee::PriHigh,"LOOP %d Completed For Register Operation Testing\n", loop);
    }
    Printf(Tee::PriHigh,"-------- Directed Test Complete -------------\n");

    Printf(Tee::PriHigh,"-------- Random Testing Start -------------\n");

    // Below procedure is to Go for Random ELPG test
    m_psThInfo[VID_ELPG_CHANNEL_CONTEXT]->testType = Random;
    m_psThInfo[VID_PB_CHANNEL_CONTEXT]->testType = Random;
    m_psThInfo[GR_ELPG_CHANNEL_CONTEXT]->testType = Random;
    m_psThInfo[GR_PB_CHANNEL_CONTEXT]->testType = Random;

    loop = 0;

    thNumber = ((m_NoGr || m_NoVideo)? 2 : 4);

    while (loop++ < m_LoopVar)
    {

        g_semValue++;
        if (!m_NoGr)
        {
            Tasker::CreateThread(GrElpgThread,(void *)m_psThInfo[GR_ELPG_CHANNEL_CONTEXT],0,NULL);
            Tasker::CreateThread(GrPbActivity,(void *)m_psThInfo[GR_PB_CHANNEL_CONTEXT],0,NULL);
        }
        if (!m_NoVideo)
        {
            Tasker::CreateThread(VidElpgThread,(void *)m_psThInfo[VID_ELPG_CHANNEL_CONTEXT],0,NULL);
            Tasker::CreateThread(VidPbActivity,(void *)m_psThInfo[VID_PB_CHANNEL_CONTEXT],0,NULL);
        }

        while (thNumber > g_nThCompletionCount)
        {
            //Wait for all the threads to complete their exelwtion
            Tasker::Sleep(100);
        }
        CHECK_RC(m_psThInfo[VID_ELPG_CHANNEL_CONTEXT]->tRc);
        CHECK_RC(m_psThInfo[VID_PB_CHANNEL_CONTEXT]->tRc);
        CHECK_RC(m_psThInfo[GR_ELPG_CHANNEL_CONTEXT]->tRc);
        CHECK_RC(m_psThInfo[GR_PB_CHANNEL_CONTEXT]->tRc);

        g_nThCompletionCount = 0;

        Printf(Tee::PriHigh,"LOOP %d Completed For Random Testing\n", loop);
    }

    if (!m_NoMs)
    {
        thNumber = 1;
        Printf(Tee::PriHigh,"-------- Directed Test for MS with MethodOp-------------\n");
        m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->testType = MethodOp;
        m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->subTestType = XVEBlocker;

        Tasker::CreateThread(MsElpgThread,(void *)m_psThInfo[MS_ELPG_CHANNEL_CONTEXT],0,NULL);

        while (thNumber > g_nThCompletionCount)
        {
            //
            //Wait for all the threads to complete their exelwtion
            //
            Tasker::Sleep(100);
        }
        CHECK_RC(m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->tRc);
        g_nThCompletionCount = 0;
        Printf(Tee::PriHigh,"-------- Directed Test for MS with RegisterOp -------------\n");
        m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->testType = RegisterOp;
        m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->subTestType = Disallow;
        loop = 0;
        while (loop < NumSubTest)
        {
            Tasker::CreateThread(MsElpgThread,(void *)m_psThInfo[MS_ELPG_CHANNEL_CONTEXT],0,NULL);

            while (thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(100);
            }
            CHECK_RC(m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;
            loop++;
            m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->subTestType = (SubTestType) loop;
         }

        Printf(Tee::PriHigh,"-------- Directed Test for MS Complete-------------\n");

        if(m_EnableBarBlocker)
        {
            Printf(Tee::PriHigh,"-------- Barblocker Test for MS Complete-------------\n");

            Tasker::CreateThread(MsBarBlockerTest,(void *)m_psThInfo[MS_ELPG_CHANNEL_CONTEXT],0,NULL);

            while (thNumber > g_nThCompletionCount)
            {
                //
                //Wait for all the threads to complete their exelwtion
                //
                Tasker::Sleep(10);
            }
            CHECK_RC(m_psThInfo[MS_ELPG_CHANNEL_CONTEXT]->tRc);
            g_nThCompletionCount = 0;

            Printf(Tee::PriHigh,"-------- Barblocker Test for MS Complete-------------\n");
        }
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ElpgTest::Cleanup()
{
    RC          rc;
    LwRmPtr     pLwRm;

    Tasker::FreeMutex(g_pMuHandle);
    Tasker::FreeMutex(g_pMuVidPbHandle);
    Tasker::FreeMutex(g_pMuVicPbHandle);

    for (int i = 0; i < TOTAL_ENGINES; i++)
    {
        m_ElpgNotifier[i].Free();
        m_PbNotifier[i].Free();

        if (m_hObjElpg[i])
        {
            pLwRm->Free(m_hObjElpg[i]);
            m_hObjElpg[i] = 0;
        }
        if (m_hObjPb[i])
        {
            pLwRm->Free(m_hObjPb[i]);
            m_hObjPb[i]  = 0;
        }
        if (m_hObjElpgSw[i])
        {
            pLwRm->Free(m_hObjElpgSw[i]);
            m_hObjElpgSw[i] = 0;
        }
        if (m_hObjPbSw[i])
        {
            pLwRm->Free(m_hObjPbSw[i]);
            m_hObjPbSw[i] = 0;
        }
    }

    for (int i = 0; i < TOTAL_SEMAPHORE_CONTEXT; i++)
    {
        if (m_cpuAddr[i])
        {
            pLwRm->UnmapMemory(m_hSemMem[i], m_cpuAddr[i], 0, GetBoundGpuSubdevice());
        }
        if (m_gpuAddr[i])
        {
            pLwRm->UnmapMemoryDma(m_hVA[i], m_hSemMem[i],
                                LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr[i], GetBoundGpuDevice());
            m_gpuAddr[i] = 0;
        }
        if (m_hVA[i])
        {
            pLwRm->Free(m_hVA[i]);
            m_hVA[i] = 0;
        }
        if (m_hSemMem[i])
        {
            pLwRm->Free(m_hSemMem[i]);
            m_hSemMem[i] = 0;
        }
    }

    for (int i = 0; i < TOTAL_CHANNEL_CONTEXT; i++)
    {
        if (m_pCh[i])
        {
            m_TestConfig.FreeChannel(m_pCh[i]);
            m_pCh[i] = NULL;
        }
        if (m_psThInfo[i])
        {
            delete m_psThInfo[i];
            m_psThInfo[i] = NULL;
        }
        m_hCh[i] = 0;
    }

    // BUG: 515433 , resetting the values of idle and postPowerUp thresholds for fmodel
    if(Platform::GetSimulationMode() == Platform::Fmodel)
    {
        if(OrigThresh[GR_ENGINE].threshChanged == true)
        {
            CHECK_RC(QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                    GR_ENGINE, &OrigThresh[GR_ENGINE].idleThresh, true));
            CHECK_RC(QueryPowerGatingParameter
                    (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                    GR_ENGINE, &OrigThresh[GR_ENGINE].ppuThresh, true));
        }
        if(OrigThresh[VID_ENGINE].threshChanged == true)
        {
            CHECK_RC(QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                    VID_ENGINE, &OrigThresh[VID_ENGINE].idleThresh,true));
            CHECK_RC(QueryPowerGatingParameter
                    (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                    VID_ENGINE, &OrigThresh[VID_ENGINE].ppuThresh,true));
        }
        if(OrigThresh[MS_ENGINE].threshChanged == true)
        {
            CHECK_RC(QueryPowerGatingParameter(LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD,
                    MS_ENGINE, &OrigThresh[MS_ENGINE].idleThresh,true));
            CHECK_RC(QueryPowerGatingParameter
                    (LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD,
                    MS_ENGINE, &OrigThresh[MS_ENGINE].ppuThresh,true));
        }
    }

   return OK;
}

//! \brief Query for a parameter if set=false, else Set the passed parameter
//!
//! \sa IsSupported, Setup, CleanUp
//----------------------------------------------------------------------------
static RC QueryPowerGatingParameter(UINT32 param, UINT32 paramExt, UINT32 *paramVal,  bool set)
{
    RC           rc;
    LwRmPtr      pLwRm;

    LW2080_CTRL_POWERGATING_PARAMETER powerGatingParam = {0};
    LW2080_CTRL_MC_SET_POWERGATING_PARAMETER_PARAMS setPowerGatingParam = {0};

    powerGatingParam.parameterExtended = paramExt;
    powerGatingParam.parameter = param;

    if(!set)
    {
        LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS queryPowerGatingParam = {0};

        queryPowerGatingParam.listSize = 1;
        queryPowerGatingParam.parameterList = (LwP64)&powerGatingParam;

        rc = pLwRm->ControlBySubdevice(subDev,
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &queryPowerGatingParam,
                                       sizeof(queryPowerGatingParam));
        if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgTest::QueryPowerGatingParameter : Query not done\n");
            return rc;
        }
        else
        {
            *paramVal = powerGatingParam.parameterValue;
        }
    }
    else
    {

        powerGatingParam.parameterExtended = paramExt;
        powerGatingParam.parameter = param;
        powerGatingParam.parameterValue = *paramVal;
        setPowerGatingParam.listSize = 1;
        setPowerGatingParam.parameterList = (LwP64)&powerGatingParam;
        rc = pLwRm->ControlBySubdevice(subDev,
                                           LW2080_CTRL_CMD_MC_SET_POWERGATING_PARAMETER,
                                           &setPowerGatingParam,
                                           sizeof(setPowerGatingParam));
         if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            Printf(Tee::PriHigh,"ElpgTest::QueryPowerGatingParameter : Set not done\n");
        }
    }
    return rc;
}

//!
//!
//! \brief Set GPU Power On/Off
//!
//! \return RC
static RC SetGpuPowerOnOff(UINT32 action, bool bUseHwTimer, UINT32 timeToWakeUs, bool bAutoSetupHwTimer)
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GPU_POWER_ON_OFF_PARAMS powerParams = {0};

    powerParams.action = action;
    powerParams.params.bUseHwTimer = bUseHwTimer;
    powerParams.params.bAutoSetupHwTimer = bAutoSetupHwTimer;

    if (bUseHwTimer)
    {
        powerParams.params.timeToWakeUs = timeToWakeUs;
    }

    rc = pLwRm->ControlBySubdevice(subDev,
                                   LW2080_CTRL_CMD_GPU_POWER_ON_OFF,
                                   &powerParams,
                                   sizeof(powerParams));
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriHigh,"ElpgTest::SetGpuPowerOnOff : Error exelwting control call\n");
    }
    return rc;
}

//!
//!
//! \brief Schedule a Ptimer Alarm to fire after timeNs nanoseconds.
//!
//! \return RC
static RC SchedulePtimerAlarm(UINT64 timeNs)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_CMD_TIMER_SCHEDULE_PARAMS scheduleParams;
    LwRm::Handle hEvent;
    Printf(Tee::PriHigh,"ElpgTest::SchedulePtimerAlarm : Scheduling Ptimer alarm for GC5-FRL mode. Time = %llu\n", timeNs);

    // Set a callback to fire immediately
    memset(&tmrCallback, 0, sizeof(tmrCallback));
    tmrCallback.func = s_PtimerAlarmHandler;
    tmrCallback.arg = NULL;

    memset(&allocParams, 0, sizeof(allocParams));
    allocParams.hParentClient = pLwRm->GetClientHandle();
    allocParams.hClass        = LW01_EVENT_KERNEL_CALLBACK_EX;
    allocParams.notifyIndex   = LW2080_NOTIFIERS_TIMER;
    allocParams.data          = LW_PTR_TO_LwP64(&tmrCallback);

    CHECK_RC(pLwRm->Alloc(pLwRm->GetSubdeviceHandle(subDev),
                          &hEvent,
                          LW01_EVENT_KERNEL_CALLBACK_EX,
                          &allocParams));

    memset(&scheduleParams, 0, sizeof(scheduleParams));
    scheduleParams.time_nsec = timeNs;
    scheduleParams.flags = DRF_NUM(2080, _CTRL_TIMER_SCHEDULE_FLAGS, _TIME, LW2080_CTRL_TIMER_SCHEDULE_FLAGS_TIME_REL);

    rc = pLwRm->ControlBySubdevice(subDev,
                                   LW2080_CTRL_CMD_TIMER_SCHEDULE,
                                   &scheduleParams,
                                   sizeof(scheduleParams));

    return rc;
}

//!
//!
//! \brief Cancel all pending ptimer alarms.
//!
//! \return RC
static RC CancelPendingPtimerAlarms()
{
    RC rc;
    LwRmPtr pLwRm;

    rc = pLwRm->ControlBySubdevice(subDev, LW2080_CTRL_CMD_TIMER_CANCEL, NULL, 0);

    return rc;
}

//!
//!
//! \brief Wait for SCI/BSI to load.
//!
//! \return RC
static RC WaitForGC6PlusIslandLoad()
{
    RC           rc;
    LwRmPtr      pLwRm;
    LW2080_CTRL_GC6PLUS_IS_ISLAND_LOADED_PARAMS params;

    Printf(Tee::PriHigh,"ElpgTest::WaitForGC6PlusIslandLoad : Waiting for SCI/BSI to be loaded\n");

    while (1)
    {
        memset(&params, 0, sizeof(params));

        rc = pLwRm->ControlBySubdevice(subDev,
                                       LW2080_CTRL_CMD_GC6PLUS_IS_ISLAND_LOADED,
                                       &params,
                                       sizeof(params));

        if (params.bIsIslandLoaded != LW_FALSE)
            break;

        Tasker::Sleep(1000);
    }

    Printf(Tee::PriHigh,"ElpgTest::WaitForGC6PlusIslandLoad : Island is loaded. Free to ARM GC5 now.\n");
    return rc;
}

//! \brief Setup For GR eninge state before running the test.
//!
//! \sa IsSupported
//----------------------------------------------------------------------------
RC ElpgTest::SetupGr(UINT32 className)
{
    RC           rc;
    LwRmPtr      pLwRm;

    if (m_NoGr)
    {
        return OK;
    }

    // Allocate 2D Class Object
    CHECK_RC(pLwRm->Alloc(m_hCh[GR_ELPG_CHANNEL_CONTEXT], (m_hObjElpg + GR_ENGINE), className, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[GR_PB_CHANNEL_CONTEXT], (m_hObjPb + GR_ENGINE), className, NULL));

    // Allocate SW class
    CHECK_RC(pLwRm->Alloc(m_hCh[GR_ELPG_CHANNEL_CONTEXT], (m_hObjElpgSw + GR_ENGINE), LW04_SOFTWARE_TEST, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[GR_PB_CHANNEL_CONTEXT], (m_hObjPbSw + GR_ENGINE), LW04_SOFTWARE_TEST, NULL));

    // Allocate Notifier
    CHECK_RC(m_ElpgNotifier[GR_ENGINE].Allocate(m_pCh[GR_ELPG_CHANNEL_CONTEXT], 1, &m_TestConfig));
    CHECK_RC(m_PbNotifier[GR_ENGINE].Allocate(m_pCh[GR_PB_CHANNEL_CONTEXT], 1, &m_TestConfig));

    // Allocate Semaphore Memory
    CHECK_RC(AllocSemMemory(GR_ELPG_CHANNEL_CONTEXT, GR_SEMAPHORE_CONTEXT));

    return rc;
}

//! \brief Setup For Vid engine state before running the test.
//!
//! \sa IsSupported
//----------------------------------------------------------------------------
RC ElpgTest::SetupVid(UINT32 className)
{
    RC           rc;
    LwRmPtr      pLwRm;

    if (m_NoVideo)
    {
        return OK;
    }

    // Allocate MSVLD Class Object
    CHECK_RC(pLwRm->Alloc(m_hCh[VID_ELPG_CHANNEL_CONTEXT], (m_hObjElpg + VID_ENGINE), className, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[VID_PB_CHANNEL_CONTEXT], (m_hObjPb + VID_ENGINE), className, NULL));

    // Allocate SW class
    CHECK_RC(pLwRm->Alloc(m_hCh[VID_ELPG_CHANNEL_CONTEXT], (m_hObjElpgSw + VID_ENGINE), LW04_SOFTWARE_TEST, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[VID_PB_CHANNEL_CONTEXT], (m_hObjPbSw + VID_ENGINE), LW04_SOFTWARE_TEST, NULL));

    // Allocate Notifier
    CHECK_RC(m_ElpgNotifier[VID_ENGINE].Allocate(m_pCh[VID_ELPG_CHANNEL_CONTEXT], 1, &m_TestConfig));
    CHECK_RC(m_PbNotifier[VID_ENGINE].Allocate(m_pCh[VID_PB_CHANNEL_CONTEXT], 1, &m_TestConfig));

    // Allocate System memory for Semaphore handling
    CHECK_RC(AllocSemMemory(VID_ELPG_CHANNEL_CONTEXT, VID_SEMAPHORE_CONTEXT));

    // Allocate System memory for Semaphore handling For PB activity on vid
    CHECK_RC(AllocSemMemory(VID_ELPG_CHANNEL_CONTEXT, VID_PB_SEMAPHORE_CONTEXT));

    return rc;
}

//! \brief Setup For MS engine state before running the test.
//!
//! \sa IsSupported
//----------------------------------------------------------------------------
RC ElpgTest::SetupMs(UINT32 className)
{
    RC           rc;
    LwRmPtr      pLwRm;

    if (m_NoMs)
    {
        return OK;
    }

    // Allocate MSVLD Class Object
    CHECK_RC(pLwRm->Alloc(m_hCh[MS_ELPG_CHANNEL_CONTEXT], (m_hObjElpg + MS_ENGINE), className, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[MS_PB_CHANNEL_CONTEXT], (m_hObjPb + MS_ENGINE), className, NULL));

    // Allocate SW class
    CHECK_RC(pLwRm->Alloc(m_hCh[MS_ELPG_CHANNEL_CONTEXT], (m_hObjElpgSw + MS_ENGINE), LW04_SOFTWARE_TEST, NULL));
    CHECK_RC(pLwRm->Alloc(m_hCh[MS_PB_CHANNEL_CONTEXT], (m_hObjPbSw + MS_ENGINE), LW04_SOFTWARE_TEST, NULL));

    // Allocate Notifier
    CHECK_RC(m_ElpgNotifier[MS_ENGINE].Allocate(m_pCh[MS_ELPG_CHANNEL_CONTEXT], 1, &m_TestConfig));
    CHECK_RC(m_PbNotifier[MS_ENGINE].Allocate(m_pCh[MS_PB_CHANNEL_CONTEXT], 1, &m_TestConfig));

    // Allocate System memory for Semaphore handling
    CHECK_RC(AllocSemMemory(MS_ELPG_CHANNEL_CONTEXT, MS_SEMAPHORE_CONTEXT));

    // Allocate System memory for Semaphore handling For PB activity on vid
    CHECK_RC(AllocSemMemory(MS_ELPG_CHANNEL_CONTEXT, MS_PB_SEMAPHORE_CONTEXT));

    return rc;
}

//! \brief Allocate Memory for Semaphore operations.
//!
//! \sa Setup
//----------------------------------------------------------------------------
RC ElpgTest::AllocSemMemory(UINT32 channelContext, UINT32 memContext)
{
    RC            rc;
    LwRmPtr       pLwRm;
    const UINT32  memSize = 0x1000;

    // Allocate System memory for Semaphore handling
    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem[memContext],
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _CACHED),
             memSize, GetBoundGpuDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA[memContext], LW01_MEMORY_VIRTUAL, &vmparams));

    //Get Gpu virtual address
    CHECK_RC(pLwRm->MapMemoryDma(m_hVA[memContext], m_hSemMem[memContext], 0, memSize,
                                 DRF_DEF(OS46, _FLAGS, _CACHE_SNOOP, _ENABLE),
                                 &m_gpuAddr[memContext], GetBoundGpuDevice()));
    //Get Cpu Virtual Address
    CHECK_RC(pLwRm->MapMemory(m_hSemMem[memContext], 0, memSize, (void **)&m_cpuAddr[memContext], 0, GetBoundGpuSubdevice()));

    return rc;
}

//! \brief Initialize Thread context before ilwoking them
//!
//! \sa Run
//----------------------------------------------------------------------------
void ElpgTest::InitializeThreads(UINT32 channelContext, UINT32 engine)
{

    // Initialize common info in Thread
    m_psThInfo[channelContext]                 =  new THREAD_INFO;
    m_psThInfo[channelContext]->pCh            =  m_pCh[channelContext];
    m_psThInfo[channelContext]->gpuSubdev      =  subDev;
    m_psThInfo[channelContext]->testType       =  MethodOp;
    m_psThInfo[channelContext]->tRc            =  0;

    // Initialize Engine specific info
    if (engine == GR_ENGINE)
    {
        m_psThInfo[channelContext]->thCpuAddr  =  m_cpuAddr[GR_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thGpuAddr  =  m_gpuAddr[GR_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->hThSemVA   =  m_hVA[GR_SEMAPHORE_CONTEXT];
    }
    else if (engine == VID_ENGINE)
    {
        m_psThInfo[channelContext]->thCpuAddr   =  m_cpuAddr[VID_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thGpuAddr   =  m_gpuAddr[VID_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->hThSemVA    =  m_hVA[VID_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->hThSemVAPb  =  m_hVA[VID_PB_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thCpuAddrPb =  m_cpuAddr[VID_PB_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thGpuAddrPb =  m_gpuAddr[VID_PB_SEMAPHORE_CONTEXT];
    }
    else if (engine == MS_ENGINE)
    {
        m_psThInfo[channelContext]->thCpuAddr  =  m_cpuAddr[MS_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thGpuAddr  =  m_gpuAddr[MS_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->hThSemVA   =  m_hVA[MS_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->hThSemVAPb =  m_hVA[MS_PB_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thCpuAddrPb =  m_cpuAddr[MS_PB_SEMAPHORE_CONTEXT];
        m_psThInfo[channelContext]->thGpuAddrPb =  m_gpuAddr[MS_PB_SEMAPHORE_CONTEXT];
    }

    // Initilaze Thread functionality specific info
    if ((channelContext == GR_ELPG_CHANNEL_CONTEXT) ||
        (channelContext == VID_ELPG_CHANNEL_CONTEXT) ||
        (channelContext == MS_ELPG_CHANNEL_CONTEXT))
    {
        m_psThInfo[channelContext]->hThSwClass     =  m_hObjElpgSw[engine];
        m_psThInfo[channelContext]->hThClass       =  m_hObjElpg[engine];
        m_psThInfo[channelContext]->pThNotifier =  (m_ElpgNotifier + engine);
    }
    else if ((channelContext == GR_PB_CHANNEL_CONTEXT) ||
             (channelContext == VID_PB_CHANNEL_CONTEXT) ||
             (channelContext == MS_PB_CHANNEL_CONTEXT) )
    {
        m_psThInfo[channelContext]->hThSwClass     =  m_hObjPbSw[engine];
        m_psThInfo[channelContext]->hThClass       =  m_hObjPb[engine];
        m_psThInfo[channelContext]->pThNotifier = (m_PbNotifier + engine);
    }

    return;
}

//-----------------------------------------------------------------------------
// Global Function
//-----------------------------------------------------------------------------
//! \brief GrElpgThread function. To ilwoke ELPG ON and OFF sequence
//!
//! This function is designed to perform ELPG ON/OFF sequence for GR engine.
//!
//! \return void
//-----------------------------------------------------------------------------

static void GrElpgThread(void *p)
{
    THREAD_INFO   *Args;
    GpuSubdevice  *SubDev;
    UINT32         engineState;
    UINT32         gateCount, newGateCount = 0;
    UINT32         regRead = 0;
    RC             rc;
    UINT32         startTimeMs, lwrTimeMs;

    Args   = (THREAD_INFO *)p;
    SubDev = Args->gpuSubdev;

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                    GR_ENGINE, &gateCount , false);
    CHECK_THSTATUS(rc, Args->tRc, "GrElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                  "GATING_COUNT failed\n");

    Printf(Tee::PriHigh,"----------GR ELPG LOOP STARTED----------\n");

    // Wakeup the engine through Priv Register operation , if specified
    if (Args->testType == RegisterOp)
    {
        regRead = SubDev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
        regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
    }
    else
    {
        // Do some Pb Activity to Wake up the Engine if not so
        CHECK_THSTATUS(GrMethods(p), Args->tRc, "PbActivity :: Gr Methods exelwtion failed\n");
    }

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                    GR_ENGINE, &engineState , false);
    CHECK_THSTATUS(rc, Args->tRc, "GrElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                   "PWR_STATE_ENGINE failed\n");

    if (engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
    {
        Printf(Tee::PriHigh,"GrElpgThread:: GR Enigne is/was not Power ON\n");
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
             // In case of Random Testing release semaphore
            if (Args->testType == Random)
                CHECK_THSTATUS(SemaphoreOperation(p, RELEASE_SEMAPHORE), Args->tRc,
                               " GrElpgThread :: Wait For Notifiaction fail For SW class\n");
            goto DONE;
        }
        Printf(Tee::PriHigh,"GrElpgThread:: On HW Platform, GR might gets Power Off after PB activity, as it switches fast \n");
    }

    startTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
    do
    {
        Tasker::Sleep(100);
        lwrTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
        if (lwrTimeMs - startTimeMs > g_waitTimeMs)
        {
            rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                            GR_ENGINE, &newGateCount , false);
            CHECK_THSTATUS(rc, Args->tRc, "GrElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                           "GATING_COUNT failed\n");

            Printf(Tee::PriHigh,"---GrElpgThread :: Check if ELPG_ON happend at least once, otherwise TIMEOUT---\n");

           if (!(newGateCount > gateCount))
            {
                Printf(Tee::PriHigh,"---------GrElpgThread :: Wait For Priv Interface disable Timeout---------\n");
                Args->tRc = RC::TIMEOUT_ERROR;
                goto DONE;
            }
            break;
        }

        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                        GR_ENGINE, &engineState , false);
        CHECK_THSTATUS(rc, Args->tRc, "GrElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                       "PWR_STATE_ENGINE failed\n");
    } while (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

    // In case of Random Testing
    if (Args->testType == Random)
    {
        // Release the semaphore inorder to execute Parallel GrPbActivity thread
        CHECK_THSTATUS(SemaphoreOperation(p, RELEASE_SEMAPHORE), Args->tRc,
                       " GrElpgThread :: Wait For Notifiaction fail For SW class\n");
    }

    // Wakeup the engine through register operation , if specified
    if (Args->testType == RegisterOp)
    {
        regRead = SubDev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
        regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
    }
    // It is valid For "MethodOp" As well as "Random" testType
    else
    {
        CHECK_THSTATUS(GrMethods(p), Args->tRc, "GrElpgThread :: GR Methods exelwtion failed\n");
    }
    Printf(Tee::PriHigh,"----------GR ELPG LOOP COMPLETED---------------\n");

DONE :
    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//-----------------------------------------------------------------------------
// Global Function
//-----------------------------------------------------------------------------

//! \brief GrPbActivity function. To perfome Pb activity on GR engine
//!
//! This function is designed to perform GR engine activity while ELPG
//! process is in sequence
//!
//! \return void
//-----------------------------------------------------------------------------
static void GrPbActivity(void *p)
{
    THREAD_INFO *Args;

    Args = (THREAD_INFO *)p;
    Printf(Tee::PriHigh,"----------GR PB LOOP STARTED---------------\n");

    CHECK_THSTATUS(SemaphoreOperation(p, ACQUIRE_SEMAPHORE),
                   Args->tRc, "GrPbActivity :: Acquiring Semaphore Fail\n");

    CHECK_THSTATUS(GrMethods(p), Args->tRc, "GrPbActivity :: Gr Methods exelwtion failed\n");

    Printf(Tee::PriHigh,"----------GR PB LOOP END---------------\n");
    Tasker::Sleep(200);

DONE :
    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//-----------------------------------------------------------------------------
// Global Function
//-----------------------------------------------------------------------------

//! \brief VidElpgThread function. To ilwoke ELPG ON and OFF sequence
//!
//! This function is designed to perform ELPG ON/OFF sequence for Vid engine.
//!
//! \return void
//-----------------------------------------------------------------------------

static void VidElpgThread(void *p)
{

    THREAD_INFO  *Args;
    GpuSubdevice *SubDev;
    UINT32        engineState;
    UINT32        gateCount, newGateCount = 0;
    UINT32        regRead = 0;
    RC            rc;
    UINT32        startTimeMs, lwrTimeMs;

    Args   = (THREAD_INFO *)p;
    SubDev = Args->gpuSubdev;

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                    VID_ENGINE, &gateCount , false);
    CHECK_THSTATUS(rc, Args->tRc, "VidElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                  "GATING_COUNT failed\n");

    Printf(Tee::PriHigh,"----------Vid ELPG LOOP STARTED---------------\n");

    // Wakeup the engine through priv register , if specified
    if (Args->testType == RegisterOp)
    {
         //Read the register so as to wake-up he Engine , if not so already
        regRead = SubDev->RegRd32(LW_PMSPPP_FALCON_DEBUGINFO);
        regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
    }
    else
    {
        // Do PB activity for Vid engine
        Tasker::MutexHolder mh(g_pMuVidPbHandle);
        CHECK_THSTATUS(VidMethods(p), Args->tRc, "VidElpgThread :: Vid PB activity Fail\n");
    }

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                    VID_ENGINE, &engineState , false);
    CHECK_THSTATUS(rc, Args->tRc, "VidElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                  "PWR_STATE_ENGINE failed\n");

    if (engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
    {
        Printf(Tee::PriHigh,"VidElpgThread:: Video Enigne is/was not Power ON\n");
        if (Platform::GetSimulationMode() != Platform::Hardware)
        {
            //In case of random testing release semaphore
            if (Args->testType == Random)
                CHECK_THSTATUS(SemaphoreOperation(p, RELEASE_SEMAPHORE), Args->tRc,
                                          "VidElpgThread :: Releasing Semaphore Fail\n");
            goto DONE;
        }
        Printf(Tee::PriHigh,"VidElpgThread:: On HW Platform, VID might gets Power Off after PB activity, as it switches fast \n");
    }

    startTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
    do
    {
        Tasker::Sleep(100);
        lwrTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
        if (lwrTimeMs - startTimeMs > g_waitTimeMs)
        {
            rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                            VID_ENGINE, &newGateCount , false);
            CHECK_THSTATUS(rc, Args->tRc, "VidElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                           "GATING_COUNT failed\n");

            Printf(Tee::PriHigh,"---VidElpgThread :: Check if ELPG_ON happend in between, otherwise TIMEOUT---\n");

            if (!(newGateCount > gateCount))
            {
                Printf(Tee::PriHigh,"---------VidElpgThread :: Wait For Priv Interface disable Timeout---------\n");
                Args->tRc = RC::TIMEOUT_ERROR;
                goto DONE;
            }
            break;
        }

        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                        GR_ENGINE, &engineState , false);
        CHECK_THSTATUS(rc, Args->tRc, "GrElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                       "PWR_STATE_ENGINE failed\n");
    } while (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

    // In case of Random Testing
    if (Args->testType == Random)
    {
        // Release the semaphore inorder to execute Parallel VidPbActivity thread
        CHECK_THSTATUS(SemaphoreOperation(p, RELEASE_SEMAPHORE), Args->tRc,
                                          "VidElpgThread :: Releasing Semaphore Fail\n");
    }
    // Wake up the engine through priv register access , if specified
    if (Args->testType == RegisterOp)
    {
        // Validate engine power on through register access
        regRead = SubDev->RegRd32(LW_PMSPPP_FALCON_DEBUGINFO);
        regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
    }
    // It is valid For "MethodOp" As well as "Random" testType
    else
    {
        Tasker::MutexHolder mh(g_pMuVidPbHandle);
        CHECK_THSTATUS(VidMethods(p), Args->tRc, "VidElpgThread :: Vid PB activity Fail after ELPG ON\n");
    }
    Printf(Tee::PriHigh,"----------VidElpgThread :: Vid ELPG LOOP COMPLETED---------------\n");

DONE :
    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//! \brief VidPbActivity function. To perfome Pb activity on Vid engine
//!
//! This function is designed to perform Vid engine activity while ELPG
//! process is in sequence
//!
//! \return void
//-----------------------------------------------------------------------------
static void VidPbActivity(void *p)
{
    THREAD_INFO *Args;

    Args = (THREAD_INFO *)p;

    Printf(Tee::PriHigh,"---------- Vid PB LOOP STARTED---------------\n");
    CHECK_THSTATUS(SemaphoreOperation(p, ACQUIRE_SEMAPHORE),
                   Args->tRc,
                   "VidPbActivity :: Acquiring Semaphore Fail\n");

    // Do PB activity on Vid Engine
    {
        Tasker::MutexHolder mh(g_pMuVidPbHandle);
        CHECK_THSTATUS(VidMethods(p), Args->tRc,
                       "VidPbActivity :: Vid PB activity Fail\n");
    }
    Printf(Tee::PriHigh,"---------- Vid PB LOOP COMPLETED---------------\n");

DONE :
    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//-----------------------------------------------------------------------------
// Global Function
//-----------------------------------------------------------------------------
//! \brief MsElpgThread function. To ilwoke ELPG ON and OFF sequence
//!
//! This function is designed to perform ELPG ON/OFF sequence for MS engine.
//!
//! \return void
//-----------------------------------------------------------------------------

static void MsElpgThread(void *p)
{
    THREAD_INFO   *Args;
    GpuSubdevice  *SubDev;
    UINT32        engineState = 0;
    UINT32        origCount, newCount = 0;
    RC            rc;
    UINT32        regRead = 0;
    UINT32        startTimeMs, lwrTimeMs;

    Args   = (THREAD_INFO *)p;
    SubDev = Args->gpuSubdev;

    // Wakeup the engine through any Priv Register operation , if specified
    if (Args->testType == RegisterOp)
    {
        if (Args->subTestType == Disallow)
        {
            regRead = SubDev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
        }
        else if (Args->subTestType == XVEBlocker)
        {
            regRead = SubDev->RegRd32(LW_PMSPPP_FALCON_DEBUGINFO);
        }
        regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
    }
    else
    {
        //
        // Do PB activity for Vid engine to trigger XVE blocker. Skip video
        // methods on an optimus only chip.
        //
        if (!g_bIsMaxwellNoVideo)
        {
            Tasker::MutexHolder mh(g_pMuVidPbHandle);
            CHECK_THSTATUS(VidMethods(p), Args->tRc, "VidElpgThread :: Vid PB activity Fail\n");
        }
    }

    if (Args->testType == GC5)
    {
        if (Args->subTestType != GC5WaitForMSCG)
        {
            Printf(Tee::PriHigh,"MsElpgThread: Not waiting for PowerDownMs\n");
            goto DONE;
        }
    }

    rc = PowerDownMs(p);

    if (rc != OK)
    {
        Printf(Tee::PriHigh,"PowerDownMs failed!\n");
        goto DONE;
    }

    Printf(Tee::PriHigh,"MsElpgThread:: MS Enigne is Powered OFF\n");
    if (Args->testType == GC5)
    {
        goto DONE;
    }

    // Wakeup the engine through register operation , if specified
    if (Args->testType == RegisterOp)
    {
        if (Args->subTestType == Disallow)
        {
            rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_DISALLOW_COUNT,
                                                    MS_ENGINE, &origCount , false);
            CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                           "DISALLOW_COUNT failed\n");

            Printf(Tee::PriHigh,"MsElpgThread:: Orig disallow count %d\n", origCount);

            regRead = SubDev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
            regRead &= 0xFFFFFFFF; // get rid of set but not used warnings
        }
        else if (Args->subTestType == XVEBlocker)
        {
            rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_XVE_BLOCKER_COUNT,
                                                    MS_ENGINE, &origCount , false);
            CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                           "XVE_BLOCKER_COUNT failed\n");
            Printf(Tee::PriHigh,"MsElpgThread:: Original XVE count %d\n", origCount);

            SubDev->RegWr32(LW_PMSPPP_FALCON_DEBUGINFO, 0xdeadbeef);
        }
    }
    else
    {
        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_XVE_BLOCKER_COUNT,
                                                MS_ENGINE, &origCount , false);
        CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                       "XVE_BLOCKER_COUNT failed\n");

        Printf(Tee::PriHigh,"MsElpgThread:: Orig XVE count %d\n", origCount);

        // Do PB activity for Vid engine
        {
            Tasker::MutexHolder mh(g_pMuVidPbHandle);
            CHECK_THSTATUS(VidMethods(p), Args->tRc, "VidElpgThread :: Vid PB activity Fail\n");
        }
    }

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                    MS_ENGINE, &engineState , false);
    CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                   "PWR_STATE_ENGINE failed\n");

    startTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
    while (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_ON)
    {
        Tasker::Sleep(100);
        lwrTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
        if ((lwrTimeMs - startTimeMs) > g_waitTimeMs)
        {
            if (Args->testType == RegisterOp)
            {
                if (Args->subTestType == Disallow)
                {
                    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_DISALLOW_COUNT,
                                                    MS_ENGINE, &newCount , false);
                    CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER"
                                   "DISALLOW_COUNT failed\n");
                    Printf(Tee::PriHigh,"MsElpgThread :: Disallow BAR0 RD 0x%08x\n", SubDev->RegRd32(0x88944));
                }
                else if (Args->subTestType == XVEBlocker)
                {
                    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_XVE_BLOCKER_COUNT,
                                                    MS_ENGINE, &newCount , false);
                    CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER"
                                   "XVE_BLOCKER_COUNT failed\n");
                    Printf(Tee::PriHigh,"MsElpgThread :: XVE BAR0 WR 0x%08x\n", SubDev->RegRd32(0x88944));
                }
            }
            else
            {
                rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PG_WAKEUP_XVE_BLOCKER_COUNT,
                                                MS_ENGINE, &newCount , false);
                CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER"
                               "XVE_BLOCKER_COUNT failed\n");
                Printf(Tee::PriHigh,"MsElpgThread :: XVE BAR1 RD 0x%08x\n", SubDev->RegRd32(0x88944));
            }

            if (!(newCount > origCount))
            {
                Printf(Tee::PriHigh,"MsElpgThread :: Error. Original %d, Current %d\n",
                       origCount, newCount);
                Printf(Tee::PriHigh,"MsElpgThread :: Timeout error in powering up.---------\n");

                Args->tRc = RC::TIMEOUT_ERROR;
                goto DONE;
            }
            break;
        }

        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                                MS_ENGINE, &engineState , false);
        CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                      "PWR_STATE_ENGINE failed\n");
    };

    Printf(Tee::PriHigh,"MsElpgThread:: MS Enigne is Powered ON\n");

DONE :
    Printf(Tee::PriHigh,"----------MS ELPG LOOP COMPLETED---------------\n");

    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//-----------------------------------------------------------------------------
// Global helper function
//-----------------------------------------------------------------------------
//! \brief PowerDownMs
//!
//! This function powers down MS Engine.
//!
//! \return RC
//-----------------------------------------------------------------------------

RC PowerDownMs(void *p)
{
    THREAD_INFO   *Args;
    UINT32        engineState = 0;
    UINT32        gateCount, newGateCount = 0;
    RC            rc = OK;
    UINT32        startTimeMs, lwrTimeMs;
    Args = (THREAD_INFO *)p;

    // No GR powergating on Maxwell
    if (!g_bIsMaxwell)
    {
        //
        // Before doing an MS ELPG operations
        // wait for GR to be powergated as it
        // is a must condition for MS ELPG.
        //
        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                        GR_ENGINE, &engineState , false);
        CHECK_THSTATUS(rc, Args->tRc, "PowerDownMs :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                       "PWR_STATE_ENGINE failed\n");

        startTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
        do
        {
            Tasker::Sleep(100);
            lwrTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
            if (lwrTimeMs - startTimeMs > g_waitTimeMs)
            {
                Printf(Tee::PriHigh, "PowerDownMs: Timed out waiting for GR to powergate.\n");
                Args->tRc = RC::TIMEOUT_ERROR;
                goto DONE;
            }
            rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                            GR_ENGINE, &engineState , false);
            CHECK_THSTATUS(rc, Args->tRc, "PowerDownMs :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                           "PWR_STATE_ENGINE failed\n");
        } while (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

        Printf(Tee::PriHigh,"----------GR PG_ON_DONE----------\n");
    }

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                    MS_ENGINE, &gateCount , false);

    CHECK_THSTATUS(rc, Args->tRc, "PowerDownMs :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                  "GATING_COUNT failed\n");

    rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                    MS_ENGINE, &engineState , false);
    CHECK_THSTATUS(rc, Args->tRc, "PowerDownMs :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                   "PWR_STATE_ENGINE failed\n");

    Printf(Tee::PriHigh,"MS Engine State %x\n", engineState);
    // If Engine status was not Power off
    if (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
    {
        startTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
        while (true)
        {
            Tasker::Sleep(100);
            lwrTimeMs = Utility::GetSystemTimeInSeconds() * 1000;
            if ((lwrTimeMs - startTimeMs) > g_waitTimeMs)
            {
                rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT,
                                                MS_ENGINE, &newGateCount , false);
                CHECK_THSTATUS(rc, Args->tRc, "PowerDownMs :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                               "GATING COUNT failed\n");

                Printf(Tee::PriHigh,"---PowerDownMs :: Check if ELPG_ON happend at least once, otherwise TIMEOUT---\n");

                if (!(newGateCount > gateCount))
                {
                    Printf(Tee::PriHigh,"---------PowerDownMs :: Timed out waiting for gating count to change---------\n");
                    Args->tRc = RC::TIMEOUT_ERROR;
                    goto DONE;
                }
                break;
            }
        }
    }
    else
    {
        Printf(Tee::PriHigh,"PowerDownMs:: MS Enigne is/was not Power ON\n");
    }
    Printf(Tee::PriHigh,"PowerDownMs:: MS Enigne is Powered OFF\n");

DONE:
    return Args->tRc;
}

//-------------------------------------------------------------------------------
// Global Function
//-------------------------------------------------------------------------------
//! \brief MsBarBlockerTest. To check whether reg accesses are woking as expected
//!
//! Accessing registers from the while list should not exit MSCG
//! Accessing the 1st blacklisted reg should wake up MSCG
//!
//! \return void
//-------------------------------------------------------------------------------

static void MsBarBlockerTest(void *p)
{
    THREAD_INFO     *Args;
    GpuSubdevice    *SubDev;
    UINT32          engineState = 0;
    RC              rc = OK;
    UINT32          regRead = 0;
    Args = (THREAD_INFO *)p;
    SubDev = Args->gpuSubdev;

    rc = PowerDownMs(p);

    if (rc != OK)
    {
        Printf(Tee::PriHigh,"PowerDownMs failed!\n");
        goto DONE;
    }

    // The engine is powered off now
    // Accessing registers from the whilte list should not power it on

    for (UINT32 reg = 0 ; reg < (sizeof(RegListAccess)/sizeof(RegListAccess[0])); reg++)
    {
        regRead = SubDev->RegRd32(RegListAccess[reg]);
        Tasker::Sleep(100);
        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                                MS_ENGINE, &engineState , false);
        CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                                      "PWR_STATE_ENGINE failed\n");

        // Quit if engine wakes up
        if (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
        {
            Printf(Tee::PriHigh,"----MsBarBlockerTest :: White list access Failed!! ----\n");
            Printf(Tee::PriHigh,"Register = %x count = %d \n", RegListAccess[reg], reg);
            Args->tRc = RC::LWRM_ERROR;
            goto DONE;
        }
    }

    Printf(Tee::PriHigh,"MS Engine State after reading whitelist = %x\n", engineState);
    Printf(Tee::PriHigh,"---------MsBarBlockerTest :: White list access Successful---------\n");

    // Access black listed registers:
    for (UINT32 reg = 0 ; reg < (sizeof(RegBlackList)/sizeof(RegBlackList[0])); reg++)
    {
        regRead = SubDev->RegRd32(RegBlackList[reg]);
        rc = QueryPowerGatingParameter (LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE,
                                        MS_ENGINE, &engineState , false);

        CHECK_THSTATUS(rc, Args->tRc, "MsElpgThread :: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER "
                   "PWR_STATE_ENGINE failed\n");
    }

    regRead &= 0xFFFFFFFF; // get rid of set but not used warnings

    Printf(Tee::PriHigh,"MS Engine State after reading blacklist = %x\n", engineState);

    // If Engine status was not Power off
    if (engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
    {
        Printf(Tee::PriHigh,"---------MsBarBlockerTest :: Black list access Successful---------\n");
    }
    else
    {
        Printf(Tee::PriHigh,"----MsBarBlockerTest :: Black list access Failed!! ----\n");
        Args->tRc = RC::LWRM_ERROR;
        goto DONE;
    }

DONE :
    Tasker::AcquireMutex(g_pMuHandle);
    g_nThCompletionCount ++;
    Tasker::ReleaseMutex(g_pMuHandle);
    return;
}

//-----------------------------------------------------------------------------
// Global Helper Function
//-----------------------------------------------------------------------------

//! \brief SemaphoreOperation function.
//!
//! This function is designed to perform Semaphore Acquire/Release operation.
//!
//! \return RC
//-----------------------------------------------------------------------------
static RC SemaphoreOperation(void *p, bool bRelease)
{
    RC rc;
    Channel *pCh;
    LwRmPtr pLwRm;
    THREAD_INFO *Args;

    Args = (THREAD_INFO *)p;
    pCh = Args->pCh;

    if (pLwRm->IsClassSupported(FERMI_TWOD_A, Args->gpuSubdev->GetParentDevice()))
    {

        pCh->Write(0, LW906F_SEMAPHOREA, (UINT32)(Args->thGpuAddr >> 32LL));
        pCh->Write(0, LW906F_SEMAPHOREB, (UINT32)(Args->thGpuAddr & 0xffffffffLL));
        pCh->Write(0, LW906F_SEMAPHOREC, g_semValue);
        if (bRelease)
        {
            pCh->Write(0, LW906F_SEMAPHORED, DRF_DEF(906F, _SEMAPHORED, _OPERATION, _RELEASE));
        }
        else
        {
            pCh->Write(0, LW906F_SEMAPHORED, DRF_DEF(906F, _SEMAPHORED, _OPERATION, _ACQUIRE) |
                                             DRF_DEF(906F, _SEMAPHORED, _ACQUIRE_SWITCH, _ENABLED));
        }
    }

    pCh->Flush();

    if (g_bIsKepler || g_bIsMaxwell || g_bIsMaxwellNoVideo)
    {
        PollArguments args;
        args.value  = g_semValue;
        args.pAddr  = Args->thCpuAddr;
        CHECK_RC(PollVal(&args, g_TimeoutMs));
    }
    else
    {
        // Waiting on subchannel
        CHECK_RC_MSG(Args->pThNotifier->Wait(0, (1000*g_TimeoutMs)),
                 "SemaphoreOperation :: For Sw class notification Not Success\n");

        if (MEM_RD32(Args->thCpuAddr) != g_semValue)
        {
            Printf(Tee::PriHigh, "SemaphoreOperation: semaphore value incorrect\n");
            return RC::DATA_MISMATCH;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Global Helper Function
//-----------------------------------------------------------------------------

//! \brief GrMethods function.
//!
//! This function is designed to perform Graphics Engine specific NOP operation.
//!
//! \return RC
//-----------------------------------------------------------------------------
static RC GrMethods(void *p)
{
    RC rc;
    Channel     *pCh;
    LwRmPtr     pLwRm;
    THREAD_INFO *Args;

    Args = (THREAD_INFO *)p;
    pCh = Args->pCh;

    if (pLwRm->IsClassSupported(LW50_TWOD, Args->gpuSubdev->GetParentDevice()))
    {
        pCh->Write(0, LW502D_SET_OBJECT,Args->hThClass);
        CHECK_RC_MSG(Args->pThNotifier->Instantiate(0, LW502D_SET_CONTEXT_DMA_NOTIFY),
                     "GrMethods :: Instantiate Fail\n");

        Args->pThNotifier->Clear(0);
        pCh->Write(0, LW502D_NOTIFY, LW502D_NOTIFY_TYPE_WRITE_ONLY);

        pCh->Write(0, LW502D_NO_OPERATION, 0);
    }
    else if(pLwRm->IsClassSupported(FERMI_TWOD_A, Args->gpuSubdev->GetParentDevice()))
    {
        pCh->SetObject(LWA06F_SUBCHANNEL_2D, Args->hThClass);
        CHECK_RC_MSG(Args->pThNotifier->Instantiate(LWA06F_SUBCHANNEL_2D),
                     "GrMethods :: Instantiate Fail\n");

        Args->pThNotifier->Clear(0);
        pCh->Write(LWA06F_SUBCHANNEL_2D, LW902D_NOTIFY, LW902D_NOTIFY_TYPE_WRITE_ONLY);

        pCh->Write(LWA06F_SUBCHANNEL_2D, LW902D_NO_OPERATION, 0);
    }

    pCh->Flush();
    // Waiting on subchannel
    CHECK_RC_MSG(Args->pThNotifier->Wait(0, g_TimeoutMs),
                 "GrMethods :: Wait for Noticication Fail\n");

    return rc;
}
//-----------------------------------------------------------------------------
// Global Helper Function
//-----------------------------------------------------------------------------

//! \brief VidMethods function.
//!
//! This function is designed to perform MSVLD Engine specific PB activity.
//!
//! \return RC
//-----------------------------------------------------------------------------
static RC VidMethods(void *p)
{
    RC rc;
    Channel      *pCh;
    LwRmPtr      pLwRm;
    THREAD_INFO  *Args;

    PollArguments args;

    Args = (THREAD_INFO *)p;
    pCh = Args->pCh;

    MEM_WR32(Args->thCpuAddrPb, 0x87654321);

    pCh->SetObject(0, Args->hThClass);

    if (pLwRm->IsClassSupported(LW95B1_VIDEO_MSVLD, Args->gpuSubdev->GetParentDevice()))
    {
        pCh->Write(0, LW95B1_SEMAPHORE_A,
                      DRF_NUM(95B1, _SEMAPHORE_A, _UPPER, (UINT32)(Args->thGpuAddrPb >> 32LL)));
        pCh->Write(0, LW95B1_SEMAPHORE_B,(UINT32)(Args->thGpuAddrPb & 0xffffffffLL));
        pCh->Write(0, LW95B1_SEMAPHORE_C, 0x12345678);
        pCh->Write(0, LW95B1_SEMAPHORE_D,
                      DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                      DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
    }
    else if (pLwRm->IsClassSupported(LWC0B7_VIDEO_ENCODER, Args->gpuSubdev->GetParentDevice()))
    {
        pCh->Write(0, LWC0B7_SEMAPHORE_A,
                      DRF_NUM(C0B7, _SEMAPHORE_A, _UPPER, (UINT32)(Args->thGpuAddrPb >> 32LL)));
        pCh->Write(0, LWC0B7_SEMAPHORE_B,(UINT32)(Args->thGpuAddrPb & 0xffffffffLL));
        pCh->Write(0, LWC0B7_SEMAPHORE_C, 0x12345678);
        pCh->Write(0, LWC0B7_SEMAPHORE_D,
                      DRF_DEF(C0B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
                      DRF_DEF(C0B7, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
    }
    pCh->Flush();
    args.value  = 0x12345678;
    args.pAddr  = Args->thCpuAddrPb;
    CHECK_RC(PollVal(&args, g_TimeoutMs));

    return rc;
}

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ElpgTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ElpgTest, RmTest,
                 "ElpgTest RM test.");
CLASS_PROP_READWRITE(ElpgTest, LoopVar, UINT32,
    "Loop Var For All Functional Blocks.");
CLASS_PROP_READWRITE(ElpgTest, IdleThresh, UINT32,
    "Set idle threshold.");
CLASS_PROP_READWRITE(ElpgTest, PpuThresh, UINT32,
    "Set post powerup threshold.");
CLASS_PROP_READWRITE(ElpgTest, NoGr, bool,
    "Flag to disable GR threads.");
CLASS_PROP_READWRITE(ElpgTest, NoVideo, bool,
    "Flags to disable Video threads.");
CLASS_PROP_READWRITE(ElpgTest, NoMs, bool,
    "Flags to disable MS threads.");
CLASS_PROP_READWRITE(ElpgTest, EnableBarBlocker, bool,
    "Flags to enable MsBarBlockerTest.");
CLASS_PROP_READWRITE(ElpgTest, GC5TestDisplay, bool,
    "Flag to enable GC5 with display.");
CLASS_PROP_READWRITE(ElpgTest, EnableDIOS, bool,
    "Flag to enable DIOS test.");
CLASS_PROP_READWRITE(ElpgTest, EnableGC5, bool,
    "Flag to enable GC5 test.");
CLASS_PROP_READWRITE(ElpgTest, GC5WaitForMSCG, bool,
    "Flag to wait for MSCG to get engaged before entering GC5");
CLASS_PROP_READWRITE(ElpgTest, GC5UseHwTimer, bool,
    "Flag to indicate that hw timer will be used for GC5 exit");
CLASS_PROP_READWRITE(ElpgTest, GC5WakeAfterUs, UINT32,
    "Number of microseconds to stay in GC5 if using hw timer");
CLASS_PROP_READWRITE(ElpgTest, UseSmallRaster, bool,
    "Flag to indicate that a small raster should be used for GC5 with display");
CLASS_PROP_READWRITE(ElpgTest, GC5Loops, UINT32,
    "Number of loops to re-enter burst before exitting in continuous mode");
CLASS_PROP_READWRITE(ElpgTest, GC5ModeFRL, bool,
    "Flag to indicate that we would do GC5 cycles at given frame rate");
CLASS_PROP_READWRITE(ElpgTest, GC5FrameRate, UINT32,
    "rame Rate in fps at which to execute GC5 cycles");
