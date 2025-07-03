/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2017,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @Brief: This is an RM test for testing the complete PG Engine.
 *         Lwrrently only MSCG testing is enabled in the Test.
 *         Rest of the features in PG-Engine will be followed up soon.
 *
 */

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "core/include/memcheck.h"
#include "core/include/platform.h"
#include "core/utility/errloggr.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tasker.h"
#include "rmpmucmdif.h"
#include "gpu/perf/pmusub.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "core/include/utility.h"
#include "lwRmApi.h"
#include "core/include/xp.h"
#include "lwmisc.h"
#include "core/include/lwrm.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/utility/sec2rtossub.h"
#include "gpu/tests/rm/memsys/rmt_acrverify.h"
#include "rmlsfm.h"

#include "class/clb6b9.h"                            // for MAXWELL_SEC2
#include "ctrl/ctrlb6b9.h"                           // for MAXWELL SEC2
#include "class/cl902d.h"                            // for FERMI_TWOD_A
#include "class/cla06fsubch.h"                       // for LWA06F_SUBCHANNEL_2D
#include "class/cl0070.h"                            // LW01_MEMORY_VIRTUAL
#include "class/cl95a1.h"                            // LW95A1_TSEC
#include "pascal/gp102/dev_graphics_nobundle.h"      // for LW_PGRAPH_PRI_SCC_DEBUG reg-access.
#include "pascal/gp102/dev_fifo.h"

// This global variable holds the timeout period for the test.
FLOAT64 g_TimeOutMs  = 0;

// This global variable holds the waiting time that will serve as the idle period for allowing MSCG to enter and exit
FLOAT64 g_WaitTimeMs = 0;

// The GpuSubdevice refers to the current GPU the test is targeted to, and thus is a pointer to the GPU.
static GpuSubdevice    *subDev;

// This enumeration will hold which wake up method to be used by the test. Will be available in future versions.
enum ENGINE_HANDLES
{
    ENGINE_WAKEUP_HANDLE_GR,
    ENGINE_WAKEUP_HANDLE_SEC2,
    ENGINE_WAKEUP_HANDLE_NONE
};

// This enumeration holds the various features that are there in the PG-Engine pyramid.
// Lwrrently only MSCG is supported in the test.
enum FEATURE_ID
{
    FEATURE_GR_ELPG,
    FEATURE_MSCG,
    FEATURE_DIOS,
    FEATURE_NONE
};

// This enum holds the various wake-up methods available for the GPU. The PB-Activity corresponds to the method based
// wake-up, while REG_ACCESS corresponds to the register access based wakeup for the MS engine.
enum WAKE_METHOD
{
    GR_PB_ACTIVITY   = 0,
    GR_REG_ACCESS    = 1,
    SEC2_REG_ACCESS  = 2, // Send a command to SEC2 RTOS to read an MSCG blacklisted register
    SEC2_PB_ACTIVITY = 3, // Cause activity in a SEC2 channel pushbuffer
};

// The test controller class.
class LpwrTest : public RmTest
{
public:
    LpwrTest();
    virtual ~LpwrTest();

    virtual string         IsTestSupported();

    virtual RC             Setup();
    virtual RC             Run();
    virtual RC             Cleanup();

    SETGET_PROP(LoopVar, UINT32);
    SETGET_PROP(bIsMscgSelected, bool);
    SETGET_PROP(wakeupMethod, UINT32);

private:
//
// Private Member Functions.
//
    // Tests the PG-Engine for various Feature IDs. The colwersion from Feature ID to engine ID and then the process is
    // implicit and hardcoded in the function.
    RC                     PgEngTester(unsigned int featureId);

    // Parses the RM Control Command. The control command used in the test for stat collection as well as engine init
    // state parsing is: lw2080CtrlCmdMcQueryPowerGatingParameter
    RC                     RmCtrlCmdParser(LW2080_CTRL_POWERGATING_PARAMETER *pPGparams,  bool set);

    // Sends the GR traffic for a specific wake method.
    // The mechanism for Video traffic generation is not implemented lwrrently.
    // Will be implemented in future versions
    RC                     sendGrTraffic(unsigned int wakeMethod);
    RC                     sendVidTraffic(unsigned int wakeMethod);
    RC                     sendSec2Traffic(unsigned int wakeMethod);

    // Setup the Mechanism for GR traffic Method sending.
    // The setup mechanism for the Video Engine is not implemented. Will be
    // implemented in future versions.
    RC                     setupGrTraffic();
    RC                     setupVidTraffic();
    RC                     checkSec2RtosBootMode();
    RC                     setupSec2Channel();

    RC                     wakeupFeature(unsigned int wakeupMethod);

// Private Data Members.
    // Holds the number of loops for running the test.
    LwU32                  m_LoopVar;

    LwU32                  m_wakeupMethod;

    Sec2Rtos               *m_pSec2Rtos;
    // Store if MSCG is selected from the test environment
    bool                   m_bIsMscgSelected;

    // Check if the MSCG is supported on the chip.
    bool                   m_bIsMscgSupported;
    unsigned int           m_Platform;

    // Setup the channels and notifiers for sending GR method.
    Channel                *m_pChannel[ENGINE_WAKEUP_HANDLE_NONE];
    LwRm::Handle           m_hChannel[ENGINE_WAKEUP_HANDLE_NONE];
    LwRm::Handle           m_hObjLpwr[ENGINE_WAKEUP_HANDLE_NONE];
    Notifier               m_LpwrNotifier[ENGINE_WAKEUP_HANDLE_NONE];

    // SEC2 channel data
    UINT64       m_gpuAddr;
    UINT32 *     m_cpuAddr;

    LwRm::Handle m_hObj;

    LwRm::Handle m_hSemMem;
    LwRm::Handle m_hVA;
};

//!
//! @brief: The constructor for the LpwrTest class.
//!         The responsibility for the constructor is to assign a name to the test, which will be used to access the
//!         test from the RM Test infra.
//!
//!
//------------------------------------------------------------------------------
LpwrTest::LpwrTest()
{
    SetName("LpwrTest");
    m_bIsMscgSelected  = false;

    m_bIsMscgSupported = true;
    m_Platform         = Platform::Hardware;
    m_LoopVar          = 1;
    m_wakeupMethod     = GR_PB_ACTIVITY;

    for(int i = 0; i < ENGINE_WAKEUP_HANDLE_NONE; i++)
    {
        m_pChannel[i] = NULL;
    }

    //Get the Handle of the GPU on which the feature is to be tested.
    subDev = GetBoundGpuSubdevice();
}

//! @brief: The destructor for the LpwrTest is empty, as all the de-allocations are already done in the cleanup()
//!
//!
//!
//!
//------------------------------------------------------------------------------
LpwrTest::~LpwrTest()
{

}

//! @brief: Checks the support status of the feature on the chip.
//!
//!  1. checks the family of GPU (kepler, maxwell, pascal)
//!  2. checks the platform for current testing (Fmodel, silicon, amodel, RTL)
//!  3. checks the support status of the feature (MSCG supported)
//!
//!   Returns: if the test can be run on the system or not.
//------------------------------------------------------------------------------
string LpwrTest::IsTestSupported()
{
    RC rc;
    LW2080_CTRL_POWERGATING_PARAMETER PgParam = {0};

    Printf(Tee::PriHigh,"***************************** In IsTestSupported() ***************************\n");

    // Detect if the GPU is from a supported family.
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if(chipName >= Gpu::GM107)
        Printf(Tee::PriHigh, "Gpu is supported.\n");
    else
        return "Unsupported GPU type.\n";

    // Detect if SEC2 is supported on the system.
    if(m_wakeupMethod == SEC2_REG_ACCESS)
    {
        const RegHal &regs  = GetBoundGpuSubdevice()->Regs();
        UINT32  regVal      = regs.Read32(MODS_PSEC_SCP_CTL_STAT);
        LwBool  bDebugBoard = !regs.TestField(regVal, MODS_PSEC_SCP_CTL_STAT_DEBUG_MODE_DISABLED);

        if(!bDebugBoard)
        {
            return "\nSEC2 RTOS : Not supported in prod Mode.";
        }
        if(chipName >= Gpu::GP102)
        {
            Printf(Tee::PriHigh, "\n SEC2 test is supported.\n");
        }
    }

    // Get the current Platform for the simulation (HW/FMODEL/RTL). Other Platforms are not supported.
    m_Platform = Platform::GetSimulationMode();
    if(m_Platform == Platform::Fmodel)
        Printf(Tee::PriHigh, "The Platform is FModel.\n");
    else if(m_Platform == Platform::Hardware)
        Printf(Tee::PriHigh, "The Platform is Hardware.\n");
    else if(m_Platform == Platform::RTL)
        Printf(Tee::PriHigh, "The Platform is RTL.\n");
    else
        return "Unknown/Unsupported Platform found.\n";

    // Check if the MSCG is enabled on the GPU.
    PgParam.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_SUPPORTED;
    PgParam.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
    RmCtrlCmdParser(&PgParam, false);
    m_bIsMscgSupported &= !!PgParam.parameterValue;

    if(m_bIsMscgSelected == false)
    {
        return "No Test has been selected\n";
    }
    if(m_bIsMscgSupported == false)
    {
        return "MSCG not enabled on the GPU.\n";
    }

    m_bIsMscgSelected &= m_bIsMscgSupported;

    // TODO - write RM-Ctrl Call for checking the support mask status of the features here. This for addition of
    //        feature in the future to the test.
    return RUN_RMTEST_TRUE;
}

//! @brief: Setups the test environment for the feature.
//!
//!  1. Get timeout duration for the test, and waiting time based on the platform being tested.
//!  2. Update the number of loops as obtained from the test arguments.
//!  3. Setup the GR engine for sending methods to it.
//!
//!   Returns: OK, if the setup was a success.
//------------------------------------------------------------------------------

RC LpwrTest::Setup()
{
    RC rc;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    Printf(Tee::PriHigh,"***************************** In Setup() ***************************\n");

    // The Test will wait for g_WaitTimeMs time for allowing the feature to allow the gating of the feature
    g_TimeOutMs = GetTestConfiguration()->TimeoutMs();

    switch(m_Platform)
    {
        case Platform::Fmodel:
            // increase the time out on fmodel and base
            // it as a multiple of TimeoutMs
            g_WaitTimeMs = g_TimeOutMs*10;
            break;
        case Platform::Hardware:
            g_WaitTimeMs = g_TimeOutMs;
            break;
        default:
            // increasing the time out on RTL. This time-out period is not tested yet.
            g_WaitTimeMs = g_TimeOutMs*100;
            break;
    };

    if(m_LoopVar == 0)
    {
        Printf(Tee::PriHigh, "\n0 loops have been selected !!!");
        Printf(Tee::PriHigh, "\nSetting the number of loops to 1");
        m_LoopVar = 1;
    }
    else
    {
        Printf(Tee::PriHigh, "\nThe number of loops selected is : %u\n", m_LoopVar);
    }

    switch(m_wakeupMethod)
    {
        case GR_PB_ACTIVITY:
        {
            Printf(Tee::PriHigh, "The GR - Method Access mechanism is used for the test.\n");
            // Setting up the GR engine for PB Activity.
            rc = setupGrTraffic();
            break;
        }
        case GR_REG_ACCESS:
        {
            Printf(Tee::PriHigh, "The GR - Register Access mechanism is used for the test.\n");
            rc = OK;
            break;
        }
        case SEC2_REG_ACCESS:
        {
            Printf(Tee::PriHigh, "The SEC2 - Register Access mechanism is used for the test.\n");
            rc = checkSec2RtosBootMode();
            break;
        }
        case SEC2_PB_ACTIVITY:
        {
            Printf(Tee::PriHigh, "The SEC2 - Method Access mechanism is used for the test.\n");
            CHECK_RC(checkSec2RtosBootMode());
            CHECK_RC(setupSec2Channel());

            //
            // Write a known pattern to the semaphore location to initialize it
            // and don't keep writing it over and over again for every loop of
            // the test to avoid FB accesses.
            //
            MEM_WR32(m_cpuAddr, 0xdeaddead);

            break;
        }
        default:
        {
            Printf(Tee::PriHigh, "Error: Unknown Wakeup method is selected!");
            return RC::SOFTWARE_ERROR;
            break;
        }
    }

    Printf(Tee::PriHigh, "The Time-Out period is :%f\n",g_WaitTimeMs);

    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);
    return rc;
}

//! @brief: run the actual test.
//!
//! 1. if feature selected is MSCG, then run the test for MSCG.
//!
//!
//------------------------------------------------------------------------------
RC LpwrTest::Run()
{
    RC rc = OK;

    Printf(Tee::PriHigh,"*************************************** In Run() ************************************\n");

    // if MSCG is being tested and is supported on the machine, run the test.
    if(m_bIsMscgSelected)
    {
        rc = PgEngTester(FEATURE_MSCG);
    }
    return rc;
}

//! @brief: Cleanup the test environment.
//!
//!  1. clean all the channels that were allocated for the GR engine.
//!  2. clean all the notifiers that were allocated for the GR engine.
//!
//!
//------------------------------------------------------------------------------
RC LpwrTest::Cleanup()
{
    m_bIsMscgSelected = false;
    m_bIsMscgSupported = false;

    Printf(Tee::PriHigh, "*********************************** In Cleanup() ********************************");

    if (m_wakeupMethod == SEC2_PB_ACTIVITY)
    {
        LwRmPtr pLwRm;
        RC      rc;

       pLwRm->UnmapMemory(m_hSemMem, m_cpuAddr, 0, GetBoundGpuSubdevice());
       pLwRm->UnmapMemoryDma(m_hVA, m_hSemMem,
                             LW04_MAP_MEMORY_FLAGS_NONE, m_gpuAddr, GetBoundGpuDevice());

       pLwRm->Free(m_hVA);
       pLwRm->Free(m_hSemMem);
       pLwRm->Free(m_hObj);

       CHECK_RC(m_TestConfig.FreeChannel(m_pCh));
    }

    // Free up all the channels that were allocated on the machine for sending the GR Methods.
    for(int i = 0; i < ENGINE_WAKEUP_HANDLE_NONE; i++)
    {
        m_TestConfig.FreeChannel(m_pChannel[i]);
        m_pChannel[i] = NULL;

        m_LpwrNotifier[i].Free();
    }
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//! @brief: The implementation for testing the feature.
//!
//! Strategy is:
//!       a. collect stats from GPU - S1.
//!       b. wait for sometime.
//!       c. wakeup the GR engine
//!       d. collect stats from GPU - S2.
//!       e. if (S2-S1) > 0, test is pass, else fail.
//!
//! 1. check for the feature to test.
//! 2. test for both kind of wakeup methods - Register access and Method activity.
//! 3. loop the test for N number of times, as given in test args.
//!
//!---------------------------------------------------------------------------------------
RC LpwrTest::PgEngTester(unsigned int featureId)
{
    RC rc;
    static LwU32 origGatingCount = 0;
    LwU32 newGatingCount  = 0;
    LwU32 diffGatingCount;
    bool b_isTestPassed   = true;
    LwU64 retrieved       = 0;
    LwU64 initialTimeMs   = 0;
    LwU64 finalTimeMs     = 0;
    LwU32 engineState     = 0;
    LW2080_CTRL_POWERGATING_PARAMETER PgParams;

    if(featureId == FEATURE_MSCG)
    {
        if(m_wakeupMethod == GR_PB_ACTIVITY)
        {
            Printf(Tee::PriHigh, "\n**********The GR - PB Activity method is being used for waking the MS Engine.********");
        }
        if(m_wakeupMethod == GR_REG_ACCESS)
        {
            Printf(Tee::PriHigh, "\n******The GR - Register Access method is being used for waking the MS Engine.********");
        }
        if(m_wakeupMethod == SEC2_REG_ACCESS)
        {
            Printf(Tee::PriHigh, "\n******The SEC2 - Register Access method is being used for waking the MS Engine.******");
        }
        for (LwU32 i = 0; i < m_LoopVar; i++)
        {
          newGatingCount  = 0;
          Printf(Tee::PriHigh,"\n\n*****************************Loop Number : %d**********************************\n",i+1);

          // Waiting for Sleep time : g_WaitTimeMs for entering into an MSCG cycle.
          Printf(Tee::PriHigh, "\nGoing to sleep.\n");
          //Tasker::Sleep(g_WaitTimeMs);
          initialTimeMs  =  Utility::GetSystemTimeInSeconds()*1000;
          do
          {
              Tasker::Sleep(1000);
              finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
              Printf(Tee::PriHigh,"Parsing engine state.\n");
              PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE;
              PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
              RmCtrlCmdParser(&PgParams, false);
              engineState = PgParams.parameterValue;
              if (finalTimeMs-initialTimeMs > g_WaitTimeMs)
              {
                  Printf(Tee::PriHigh, "Timed-out trying to power down the MS engine\n");
                  break;
              }
              if(engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_ON)
                  Printf(Tee::PriHigh, "The MS engine is still not down.");
          }while(engineState != LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF);

          if(engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
              Printf(Tee::PriHigh, "Engine state is powered off.\n");

          // Sending Wake call to the engine.
          if(engineState == LW2080_CTRL_MC_POWERGATING_PARAMETER_PWR_STATE_ENGINE_PWR_OFF)
          {
              Printf(Tee::PriHigh,"Waking up the engine.\n");
              wakeupFeature(m_wakeupMethod);
          }
          else
          {
              Printf(Tee::PriHigh, "The bubble of idleness was not able to bring down MS engine. Skipping GR wakeup.\n");
          }

          /*
           *    Stats collected after coming out of sleep.
           *
           */

          Printf(Tee::PriHigh,"\n\nStat collected after coming out of sleep : \n");

          // Parse the idle threshold for the engine in micro-second.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_IDLE_THRESHOLD_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The Idle threshold for the MS engine is : %llu \n", retrieved);

          // Parse Post-power-up threshold for the engine in micro-second.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_POST_POWERUP_THRESHOLD_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The PPU threshold for the MS engine is : %llu \n", retrieved);

          // Parse initialization status of the MSCG.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INITIALIZED;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The initialization status of MSCG is : %llu \n", retrieved);

          // Parse gating count.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_GATINGCOUNT;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The Gating Count for MSCG is : %llu \n", retrieved);
          newGatingCount = UNSIGNED_CAST(LwU32, retrieved);

          // Parse deny count.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_DENYCOUNT;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The deny Count for MSCG is : %llu \n", retrieved);

          // Parse InGating count.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGCOUNT;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The Ingating Count for MSCG is : %llu \n", retrieved);

          // Parse UnGating count.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGCOUNT;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The Ungating Count for MSCG is : %llu \n", retrieved);

          // Parse InGating Time in US.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_INGATINGTIME_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The ingating time for MSCG is : %llu \n", retrieved);

          // Parse UnGating Time in US.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_UNGATINGTIME_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The ungating time for MSCG is : %llu \n", retrieved);

          // Parse Avg Entry Time in US.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_ENTRYTIME_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The average entry time for MSCG is : %llu \n", retrieved);

          // Parse Avg Exit Time in US.
          PgParams.parameter         = LW2080_CTRL_MC_POWERGATING_PARAMETER_AVG_EXITTIME_US;
          PgParams.parameterExtended = LW2080_CTRL_MC_POWERGATING_ENGINE_ID_MS;
          RmCtrlCmdParser(&PgParams, false);
          retrieved = PgParams.parameterValue;
          Printf(Tee::PriHigh, "The average exit time for MSCG is : %llu \n", retrieved);

          diffGatingCount = newGatingCount - origGatingCount;
          origGatingCount = newGatingCount;
          Printf(Tee::PriHigh,"\n\nThe difference in gating Counts is : %d", diffGatingCount);
          if(diffGatingCount > 0)
          {
              b_isTestPassed = true;
              Printf(Tee::PriHigh,"\n\nThe test has passed for %d loop.\n",i+1);
          }
          else
          {
              b_isTestPassed = false;
              break;
          }
        }
    }
    if(b_isTestPassed == true)
    {
        Printf(Tee::PriHigh, "\nTest Passed.\n");
        rc = OK;
    }
    else
    {
        Printf(Tee::PriHigh, "\nTest Failed.\n");
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

//! @brief : Wake-up LPWR feature using different Engines.
//!
//! The function implements the switching logic for sending wake-up call to various engines.
//! lwrrently supported engines for waking up are GR and SEC2.
//!
//!---------------------------------------------------------------------------------------
RC LpwrTest::wakeupFeature(unsigned int wakeMethod)
{
    RC rc;
    switch(wakeMethod)
    {
        case GR_REG_ACCESS :
        case GR_PB_ACTIVITY:
           rc = sendGrTraffic(wakeMethod);
           break;
        case SEC2_REG_ACCESS:
        case SEC2_PB_ACTIVITY:
           rc = sendSec2Traffic(wakeMethod);
           break;
        default:
           Printf(Tee::PriHigh, "Unknown/un-implemented wakeup type...\n");
           rc = RC::SOFTWARE_ERROR;
           break;
    }
    return rc;
}

// setup the test for sending methods to GR engine.
RC LpwrTest::setupGrTraffic()
{
    RC rc;
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pChannel[ENGINE_WAKEUP_HANDLE_GR], &m_hChannel[ENGINE_WAKEUP_HANDLE_GR]));
    CHECK_RC(LwRmPtr()->Alloc(m_hChannel[ENGINE_WAKEUP_HANDLE_GR], (m_hObjLpwr+ENGINE_WAKEUP_HANDLE_GR), FERMI_TWOD_A, NULL));
    CHECK_RC(m_LpwrNotifier[ENGINE_WAKEUP_HANDLE_GR].Allocate(m_pChannel[ENGINE_WAKEUP_HANDLE_GR], 1, &m_TestConfig));
    return OK;
}

//
// Make sure SEC2 RTOS is booted in the expected mode.
//
RC LpwrTest::checkSec2RtosBootMode()
{
    RC rc;
    AcrVerify lsSec2State;

    CHECK_RC(lsSec2State.BindTestDevice(GetBoundTestDevice()));

    rc = GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos);
    if(OK != rc)
    {
        Printf(Tee::PriHigh, "SEC2 RTOS not supported.\n");
        return rc;
    }

    // Waiting for SEC2 to bootstrap.
    rc = m_pSec2Rtos->WaitSEC2Ready();
    if(OK != rc)
    {
        Printf(Tee::PriHigh, "Ucode OS for SEC2 is not ready. Skipping test.\n");
        // fail the test, we need to figure out why SEC2 bootstrap failed.
        CHECK_RC(RC::LWRM_STATUS_ERROR_NOT_READY);
        return rc;
    }

    if(lsSec2State.IsTestSupported() == RUN_RMTEST_TRUE)
    {
        lsSec2State.Setup();
        // Verify is SEC2 is booted in LS mode.
        rc = lsSec2State.VerifyFalconStatus(LSF_FALCON_ID_SEC2, LW_TRUE);
        if(OK != rc)
        {
            Printf(Tee::PriHigh, "SEC2 failed to boot in expected Mode.\n");
            return rc;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Falcon test is not supported on the config.\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriHigh, "The SEC2 engine has been setup successfully.\n");
    return rc;
}

//! @brief : Setup a SEC2 channel to send methods to, for wakup from MSCG later.
RC LpwrTest::setupSec2Channel()
{
    const UINT32 memSize = 0x1000;
    LwRmPtr      pLwRm;
    RC           rc;

    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));

    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObj, LW95A1_TSEC, NULL));

    CHECK_RC(pLwRm->AllocSystemMemory(&m_hSemMem,
             DRF_DEF(OS02, _FLAGS, _PHYSICALITY, _NONCONTIGUOUS) |
             DRF_DEF(OS02, _FLAGS, _LOCATION, _PCI) |
             DRF_DEF(OS02, _FLAGS, _COHERENCY, _UNCACHED),
             memSize, GetBoundGpuDevice()));

    LW_MEMORY_VIRTUAL_ALLOCATION_PARAMS vmparams = { 0 };
    CHECK_RC(pLwRm->Alloc(pLwRm->GetDeviceHandle(GetBoundGpuDevice()),
                          &m_hVA, LW01_MEMORY_VIRTUAL, &vmparams));

    CHECK_RC(pLwRm->MapMemoryDma(m_hVA, m_hSemMem, 0, memSize,
                                 LW04_MAP_MEMORY_FLAGS_NONE, &m_gpuAddr, GetBoundGpuDevice()));
    CHECK_RC(pLwRm->MapMemory(m_hSemMem, 0, memSize, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));
    return rc;
}

// send the GR traffic based on the wake-up method.
RC LpwrTest::sendGrTraffic(LwU32 wakeMethod)
{
    RC rc;
    UINT32 regRead = 0;

    if(wakeMethod == GR_PB_ACTIVITY)
    {
        Printf(Tee::PriHigh, "Now waking GR engine using method transfer...\n");
        if(LwRmPtr()->IsClassSupported(FERMI_TWOD_A, GetBoundGpuDevice()))
        {
            m_pChannel[ENGINE_WAKEUP_HANDLE_GR]->SetObject(LWA06F_SUBCHANNEL_2D, m_hObjLpwr[ENGINE_WAKEUP_HANDLE_GR]);
            rc = (m_LpwrNotifier+ENGINE_WAKEUP_HANDLE_GR)->Instantiate(LWA06F_SUBCHANNEL_2D);
            if(rc!=OK)
            {
                Printf(Tee::PriHigh, "\nGR Methods failed to Instantiate..\n");
                return rc;
            }
            (m_LpwrNotifier+ENGINE_WAKEUP_HANDLE_GR)->Clear(0);
            m_pChannel[ENGINE_WAKEUP_HANDLE_GR]->Write(LWA06F_SUBCHANNEL_2D, LW902D_NOTIFY,
                                           LW902D_NOTIFY_TYPE_WRITE_ONLY);

            m_pChannel[ENGINE_WAKEUP_HANDLE_GR]->Write(LWA06F_SUBCHANNEL_2D, LW902D_NO_OPERATION, 0);
        }
        else
        {
            Printf(Tee::PriHigh, "\nFERMI_TWOD_A class is not supported. Hence, cannot send GR traffic.\n");
            return RC::LWRM_NOT_SUPPORTED;
        }
        m_pChannel[ENGINE_WAKEUP_HANDLE_GR]->Flush();

        rc = (m_LpwrNotifier+ENGINE_WAKEUP_HANDLE_GR)->Wait(0, g_WaitTimeMs);

        if(rc!=OK)
        {
            Printf(Tee::PriHigh, "\nFailed on Notification of GR engine.\n");
            return rc;
        }
        Printf(Tee::PriHigh, "\nSent Traffic to GR engine successfully.\n");
    }

    else if(wakeMethod == GR_REG_ACCESS)
    {
        Printf(Tee::PriHigh,"Now waking GR using Register access...\n");
        regRead = subDev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
        regRead &= 0xFFFFFFFF;
        Printf(Tee::PriHigh, "\nFollowing data was read from the Register: %d\n", regRead);
        Printf(Tee::PriHigh, "Wating for wakeup to commit.\n");
        Tasker::Sleep(100);
    }
    return OK;
}

RC LpwrTest::sendSec2Traffic(LwU32 wakeupMethod)
{
    RC                    rc = OK;
    RM_FLCN_CMD_SEC2      sec2Cmd;
    RM_FLCN_MSG_SEC2      sec2Msg;
    UINT32                seqNum;
    UINT32                ret;

    if (wakeupMethod == SEC2_PB_ACTIVITY)
    {
        PollArguments args;
        static UINT32 loop = 0;
        UINT32 semaphoreVal = (0x12341234 + loop++); // Update semaphore release value in every loop

        m_pCh->SetObject(0, m_hObj);

        Printf(Tee::PriHigh, "Sending methods to SEC2 channel.\n");
        // Send methods to release semaphore with a different value
        m_pCh->Write(0, LW95A1_SEMAPHORE_A,
            DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(m_gpuAddr >> 32LL)));
        m_pCh->Write(0, LW95A1_SEMAPHORE_B, (UINT32)(m_gpuAddr & 0xffffffffLL));
        m_pCh->Write(0, LW95A1_SEMAPHORE_C,
            DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, semaphoreVal));
        m_pCh->Write(0, LW95A1_SEMAPHORE_D,
            DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) |
            DRF_DEF(95A1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE));
        m_pCh->Flush();

        // Poll for semaphore value to change
        Printf(Tee::PriHigh, "Polling on semaphore release by SEC2.\n");
        args.value  = semaphoreVal;
        args.pAddr  = m_cpuAddr;
        CHECK_RC(PollVal(&args, g_TimeOutMs));
        Printf(Tee::PriHigh, "Succeeded semaphore release\n");
    }
    else if (wakeupMethod == SEC2_REG_ACCESS)
    {
        memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
        memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

        // Send an empty command to SEC2. NULL command does nothing but make SEC2 return corresponding message.
        sec2Cmd.hdr.unitId           = RM_SEC2_UNIT_TEST;
        sec2Cmd.hdr.size             = RM_SEC2_CMD_SIZE(TEST, RD_BLACKLISTED_REG);
        sec2Cmd.cmd.sec2Test.cmdType = RM_SEC2_CMD_TYPE(TEST, RD_BLACKLISTED_REG);

        Printf(Tee::PriHigh, "Submitting a command to SEC2 to read a clock gated register.\n");

        // submit the command.
        rc = m_pSec2Rtos->Command(&sec2Cmd, &sec2Msg, &seqNum);
        if(OK != rc)
        {
            Printf(Tee::PriHigh, "Error with the register read request.\n");
            return rc;
        }

        ret = sec2Msg.msg.sec2Test.rdBlacklistedReg.val;
        Printf(Tee::PriHigh, "\tValue Returned by blacklisted reg access = 0x%x\n",ret);
    }
    else
    {
        Printf(Tee::PriHigh, "Unknown SEC2 wakeup request recieved.\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

// file the RM Control for stat collection as well as the engine state parsing.
RC LpwrTest::RmCtrlCmdParser(LW2080_CTRL_POWERGATING_PARAMETER *pPGparams,  bool set)
{
    RC           rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_MC_QUERY_POWERGATING_PARAMETER_PARAMS  QueryParams = {0};

    if(!set)
    {
        QueryParams.listSize = 1;
        QueryParams.parameterList = (LwP64)pPGparams;

        // Send the RM control command.
        rc = pLwRm->ControlBySubdevice(subDev,
                                       LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER,
                                       &QueryParams,
                                       sizeof(QueryParams)
                                      );
        if (OK != rc)
        {
        Printf(Tee::PriHigh, "\n %s: LW2080_CTRL_CMD_MC_QUERY_POWERGATING_PARAMETER Query failed with rc=%d\n", __FUNCTION__,(UINT32)rc);
        }
    }
    return rc;
}

//! TODORMT - Private member functions here

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(LpwrTest, RmTest,
    "RM Test for verifying the PG-ENG features.");
CLASS_PROP_READWRITE(LpwrTest, LoopVar,             LwU32, "Loops for all Functional Tests.");
CLASS_PROP_READWRITE(LpwrTest, bIsMscgSelected,     bool,  "Test the MSCG feature for running.");
CLASS_PROP_READWRITE(LpwrTest, wakeupMethod,        LwU32, "Select the Wakeup Method to be used.");
