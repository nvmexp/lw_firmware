/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_clktest.cpp
//! \brief Test for clock settings and clock features.(Silent running).
//!
//! Although Silent running has many features, this test comprises of many
//! important feature test but not all. Test for PerfMon can be found in
//! rmt_perfmon.cpp and minimum bandwith Req test in rmt_bwreqtest.cpp.
//! This test is divided into two parts: (Supported Hardware only as Perf table
//! is avaliable on hardware not on Sim  Elw and this test has major part of
//! tests for Perf table). All the tests can be ran all at once or individually.
//! For individual exelwtion use the .JS file named rmtclktest.js.
//! For individual exelwtion usage use this CMD: mods rmtclktest.js -usage.
//! 1. Clock domains based Set/Get:Test gets clock freqs information for all
//!    the supported domains and tries to set freq in each domain and verifies
//!    those after setting by getting the current information.
//!    Here lwrrently some bugs found and some clock domains settings are
//!    unsupported so test code skips over such known domains.
//! 2. Perf Table based: This test is sub-divided into following,
//!    A. Basic Set/Get for by getting level information and setting and
//!       verifying for each level. This also does the forced mode level
//!       set/reset.
//!    C. Stress test the changing of perf levels by randomly changing the
//!       levels and looping over to stress test the clock changes.
//!       This is in spirit of smashclk tool doing similar test.
//!    D. Perf Level fallback due to Screen Saver: introduces Screen saver
//!       while on max perf level and verfies the fallback by checking level
//!       change and actual clocks.
//!    E. StressTest: Verifying the SW path from client side for stress test.
//!       This is the similar stress test used by Active clocking shmoo but
//!       different as it falls back to original clk freqs where it started.
//!       and does this through RM Control call and not automatic as in shoom.
//!    F. Perf Level fallback due to RC errors: introduces RC errors and
//!       verfies the fallback by checking level change and actual clocks.
//!    G. Active clocking Enable-Disable: verifies by checking level change and
//!       actual clock changes. Also verfies the level and clock changes after
//!       RC error during Active Clocking.

#include <cmath>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/display.h"
#include "core/include/channel.h"
#include "gpu/include/notifier.h"
#include "core/include/platform.h"
#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "class/cl0073.h"
#include "ctrl/ctrl0073.h"
#include "class/cl9097.h"
#include "class/cl9097sw.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/utility.h"
#include "gpu/perf/perfsub.h"
#include "random.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"
#include "gpu/include/notifier.h"
#include "gpu/perf/perfutil.h"

#define LWCTRL_DEVICE_HANDLE                (0xbb008000)
#define LWCTRL_SUBDEVICE_HANDLE             (0xbb208000)
#define RAND_NUM_START                      (0x00000000)
#define RAND_NUM_END                        (0x00007fff)
#define ILWALID_DATA                        (0xffffffff)
#define DMAPUSHER_METHOD                    (0x000000f8)
#define VIDMEM_SIZE                         (0x1000)

//
// Refer: Bug 271474 and Bug 273966, So changed the value to 20 MHz from 5 MHz
//
#define OVERCLOCK_VALUE                     (20*1000)
#define WAIT_FOR_ACTIVE_CLK                 (10000000)

static UINT32 hDevice = LWCTRL_DEVICE_HANDLE;
static UINT32 hSubDevice = LWCTRL_SUBDEVICE_HANDLE;

class ClockTest: public RmTest
{
public:
    ClockTest();
    virtual ~ClockTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(All, bool);    //!< Grab JS variable for Default behavior
    SETGET_PROP(BasicTest, bool);    //!<Function to grab JS variable BasicTest
    SETGET_PROP(RunPerfTable, bool);    //!< Grab JS variable RunPerfTable

    SETGET_PROP(RunMode, UINT32);    //!< To grab JS variable RunMode
    SETGET_PROP(PmState, UINT32);    //!< To grab JS variable PmState
    SETGET_PROP(Random, bool);    //!< Function to grab JS variable Random
    SETGET_PROP(LoopVar, UINT32);    //!< Function to grab JS variable LoopVar

private:
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util
    GpuSubdevice *m_pSubdevice;
    //  0 = Default Run all
    #define DEFAULT_MODE             (0x00000000)
    #define ACTIVECLK_MODE           (0x00000001)
    #define STRESSTEST_MODE          (0x00000003)
    #define RCFALLBACK_MODE          (0x00000004)
    #define STRESSPERFLEVEL_MODE     (0x00000005)
    bool     m_All;
    bool     m_BasicTest;
    bool     m_RunPerfTable;

    UINT32 m_RunMode;    //!< Current mode of operation during Run() command
    UINT32 m_PmState;    //!< PM state
    UINT32 m_HighLevel;  //!< Current used in Stress Perf level
    bool m_Random; //!< Current used in Stress Perf level
    UINT32 m_LoopVar; //!< Current used in Stress Perf level
    Random m_RandomCtx;
    FLOAT64 m_TimeoutMs;
    void *m_pMapVidAddr; //!< For Mapping video memory

    LwRm::Handle m_hDisplay;
    LwRm::Handle m_hClient;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hSubDev;

    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicDevice;
    LwRm::Handle m_hBasicSubDev;

    LwRm::Handle m_vidMemHandle;//!< Handle for video memory

    //! CLK Domains variables
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS m_getDomainsParams;
    LW2080_CTRL_CLK_GET_INFO_PARAMS m_getInfoParams;
    LW2080_CTRL_CLK_SET_INFO_PARAMS m_setInfoParams;
    UINT32 m_numClkInfos;
    LW2080_CTRL_CLK_INFO *m_pClkInfos;
    LW2080_CTRL_CLK_INFO *m_pSetClkInfos;

    //! Perf Table variables
    LW2080_CTRL_PERF_GET_CLK_INFO *m_pGetPerfClkInfo;
    LW2080_CTRL_PERF_SET_CLK_INFO *m_pPerfSetClkInfos;
    LW2080_CTRL_CLK_INFO *m_restorGetInfo;

    //! PStates variables
    Perf* m_pPerf;
    vector<UINT32> m_availablePStates;

    Notifier    m_Notifier;
    //
    //! Setup and Test functions
    //
    RC SetupBasicTest();
    RC SetupPerfTest();
    RC BasicGetSetClockTest();
    RC PerfTableTest();

    //
    //!Functions involve in PMU Memory Sequencer
    //
    RC AllocAndFillVidMem();
    RC VerifyVidMem();

    //
    //! Perf Test: Sub-Test functions
    //
    RC SimpleGetSetLevelTest();

    RC StressPerfChange(UINT32 uDestPState);

    RC StressTest(UINT32 m_hClient,
                  UINT32 hDev,
                  UINT32 hSubDev,
                  UINT32 numClkInfos,
                  LW2080_CTRL_PERF_SET_CLK_INFO *pSetClkInfos,
                  LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
                  LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo);

    RC RcErrFallback(UINT32 m_hClient,
                     UINT32 hDev,
                     UINT32 hSubDev,
                     UINT32 perfmode,
                     UINT32 noClkDomains,
                     LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                       *getLevelInfoParams);

    RC ActiveClkingTest(UINT32 m_hClient,
                        UINT32 hDev,
                        UINT32 hSubDev,
                        LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                          *getLevelInfoParams,
                        LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS
                          tableInfoParams
                       );

    RC ActiveClkRcErrFallback(UINT32 m_hClient,
                              UINT32 hDev,
                              UINT32 hSubDev,
                              UINT32 noClkDomains,
                              LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                                *getLevelInfoParams
                             );

    //
    //! Perf Test: Test Helper functions
    //
    RC ClearModsPerfthenBoost(UINT32 m_hClient,
                              UINT32 hDev,
                              UINT32 hSubDev,
                              UINT32 perfmode,
                              UINT32 *levelAftBoost);

    RC ResetPerfBoost(UINT32 m_hClient,
                      UINT32 hDev,
                      UINT32 hSubDev);

    RC DumpActiveLevel(UINT32 m_hClient,
                       UINT32 hSubDev,
                       LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS
                        *getLwrrLevelInfoParams);

};

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
ClockTest::ClockTest()
{
    SetName("ClockTest");
    m_RunMode      = 0;
    m_hDisplay     = 0;
    m_All          = true; // Default run all
    m_BasicTest    = false;
    m_RunPerfTable = false;
    m_Random       = true;
    m_LoopVar      = 10;
    m_vidMemHandle = 0;
    m_pClkInfos = NULL;
    m_pSetClkInfos = NULL;
    m_pGetPerfClkInfo = NULL;
    m_pPerfSetClkInfos = NULL;
    m_restorGetInfo = NULL;
    m_pMapVidAddr = NULL;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ClockTest::~ClockTest()
{
}

//! \brief IsTestSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string ClockTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        // debug
        Printf(Tee::PriHigh,
            "%d:ClockTest is supported only on HW \n",__LINE__);
        Printf(Tee::PriHigh,"%d:Returns from non HW platform\n",
               __LINE__);
        return "Supported only on HW";
    }
    else
    {
        // debug
        Printf(Tee::PriHigh,
        "%d:This ClockTest is supported only on Pre-Fermi and non-iGT21A chips \n",
        __LINE__);
        Printf(Tee::PriHigh,
        "we do have fermi dedicated test as design/arch changed\n");
        return "This ClockTest is supported only on Pre-Fermi and non-iGT21A chips";
    }
}

//! \brief Setup Function.
//!
//! For basic test and Perf table test this setup function allocates the
//! required memory. Calls the sub functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa SetupBasicTest
//! \sa SetupPerfTest
//! \sa Run
//-----------------------------------------------------------------------------
RC ClockTest::Setup()
{
    RC rc;
    LwRmPtr    pLwRm;

    m_pSubdevice   = GetBoundGpuSubdevice();
    m_hBasicDevice = pLwRm->GetDeviceHandle(GetBoundGpuDevice());
    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();
    m_pPerf        = m_pSubdevice->GetPerf();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // Get all programmable clock domains.
    m_getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_PROGRAMMABALE_ONLY;

    // get clk domains
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_CLK_GET_DOMAINS,
                            &m_getDomainsParams, sizeof (m_getDomainsParams)));

    Printf(Tee::PriLow, " Dumping Programmable Domains\n");
    m_ClkUtil.DumpDomains(m_getDomainsParams.clkDomains);

    // Get the number of programmable domains this GPU supports
    m_numClkInfos = 0;

    for (LwU32 iLoopVar = 0; iLoopVar < 32; iLoopVar++)
    {
        if (BIT(iLoopVar) & m_getDomainsParams.clkDomains)
           m_numClkInfos++;
    }

    if (m_BasicTest || m_RunPerfTable)
    {
        m_All = false;
    }

    // Placed in this way to take care of case where m_All is true
    if (m_All || m_BasicTest)
    {
        CHECK_RC(SetupBasicTest());
    }
    if (m_All || m_RunPerfTable)
    {
        CHECK_RC(SetupPerfTest());
    }

    return rc;
}

//! \brief Run Function.
//!
//! Run function is important function for the test where-in it calls the sub
//! tests. Lwrrently to Set/Get the current clock settings the basic test
//! is called. For performance table related information and settings are done
//! by Perf Table test. As MODS is used for auto testing so also forcibly
//! places the Perf level to highest and locks the clk freq chnages;But this
//! clock related test needs the flexibility to change the Perf levels and clk
//! freqs so the test uses helper functions to Disable/Enable the MODS
//! restriction, and each test exelwtion called in Run() makes it a point to
//! go back to the MODS default behabvior.
//!
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicGetSetClockTest
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC ClockTest::Run()
{
    RC rc;

    //Allocating and populating Video Heap to keep track of its status
    // at the time of clk level change
    CHECK_RC(AllocAndFillVidMem());

    // Placed in this way to take care of case where m_All is true
    if (m_All || m_BasicTest)
    {
        CHECK_RC(BasicGetSetClockTest());
    }

    //
    // GT200 doesn't support adaptive clking due to bug 439993
    // and on G84GL card changing mclk causes screen corrupution(bug 489623)
    //
    if(!GetBoundGpuSubdevice()->HasBug(439993) && !GetBoundGpuSubdevice()->HasBug(489623))
    {
        if (m_All || m_RunPerfTable)
        {
            CHECK_RC(PerfTableTest());
        }
    }
    else
    {
        Printf(Tee::PriHigh,
            " The Perftest will not run on gt200/g84 due to Bug 439993/489623\n");
    }
    return rc;
}

//! \brief Cleanup Function.
//!
//! Cleanup all the allocated variables in the setup. Cleanup can be called
//! even in errorneous condition, in such case all the allocations should be
//! freed, this function takes care of such freeing up.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClockTest::Cleanup()
{
    LwRmPtr pLwRm;
    pLwRm->Free(m_hDisplay);
    pLwRm->UnmapMemory(m_vidMemHandle, m_pMapVidAddr, 0, GetBoundGpuSubdevice());
    pLwRm->Free(m_vidMemHandle);
    delete []m_pClkInfos;
    delete []m_pSetClkInfos;
    m_Notifier.Free();
    delete m_pPerfSetClkInfos;
    delete m_pGetPerfClkInfo;
    delete []m_restorGetInfo;

    return OK;
 }

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief SetupBasicTest Function.for basic test setup
//!
//! For basic test this setup function allocates the required memory.
//! To allocate memory for basic test it needs clock domains
//! information so the setup function also gets and dumps the clock domains
//! information.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClockTest::SetupBasicTest()
{
    RC rc;
    LwRmPtr pLwRm;

    m_pClkInfos = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    if(m_pClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
            "%d: Set Clock Info allocation failed,\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    memset((void *)m_pClkInfos,
           0,
           m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO));
    m_pSetClkInfos = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    if(m_pSetClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
               "%d: Set Clock Info allocation failed\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }
    memset((void *)m_pSetClkInfos,
           0,
           m_numClkInfos * sizeof(LW2080_CTRL_CLK_INFO));

    m_restorGetInfo = new
        LW2080_CTRL_CLK_INFO[m_numClkInfos * sizeof (LW2080_CTRL_CLK_INFO)];

    if(m_restorGetInfo == NULL)
    {
        Printf(Tee::PriHigh,
          "%d: Restore info buffer allocation failure\n",__LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }
    memset((void *)m_restorGetInfo,
           0,
           m_numClkInfos * sizeof(LW2080_CTRL_CLK_INFO));

    CHECK_RC(pLwRm->Alloc(m_hBasicDevice,
                          &m_hDisplay,
                          LW04_DISPLAY_COMMON,
                          NULL));

    return rc;
}

//! \brief SetupPerfTest Function.for Perf test setup
//!
//! For Perf test this setup function allocates the required memory.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC ClockTest::SetupPerfTest()
{
    RC rc;
    INT32 status = 0;

    m_hClient = 0;
    status = (UINT32)LwRmAllocRoot((LwU32*)&m_hClient);
    rc = RmApiStatusToModsRC(status);
    CHECK_RC(rc);

    m_hDevice = hDevice;
    m_hSubDev = hSubDevice;

    return rc;
}

//! \brief Test gets current clocks settings and sets & check for new value.
//!
//! The test basically uses the domains information that setup in the setup()
//! and gets the current clock frequencies. Then the test tries to set
//! Clk domains frequency to some values above (5MHz) what it has got. And gets
//! the clock information again to see new values got set or not. The test
//! resets back to original before exiting.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ClockTest::BasicGetSetClockTest()
{
    RC rc;
    RC retRc;
    UINT32 iLoopVar, tempVal;
    LwRmPtr pLwRm;
    const UINT32 incrVal = 50000;
    bool bSetVals = false;
    bool bSetResVals = false;
    LW0073_CTRL_SYSTEM_GET_ACTIVE_PARAMS dispActiveParams = {0};
    LW0073_CTRL_SYSTEM_GET_NUM_HEADS_PARAMS numHeadsParams = {0};
    UINT32 head;
    bool bIsPclkChangeAllowed = false;

    Printf(Tee::PriLow, " In BasicGetSetClockTest\n");

    // get number of heads
    numHeadsParams.subDeviceInstance = 0;
    numHeadsParams.flags = 0;
    CHECK_RC(pLwRm->Control(m_hDisplay,
                         LW0073_CTRL_CMD_SYSTEM_GET_NUM_HEADS,
                         &numHeadsParams, sizeof (numHeadsParams)));

    vector<UINT32> arrayHeads(numHeadsParams.numHeads, 0);

    // get active displays and place the display mask of active display
    for (head = 0; head < numHeadsParams.numHeads; head++)
    {
        arrayHeads[head] = 0;

        // hw active (vbios or vlcd enabled)
        dispActiveParams.subDeviceInstance = 0;
        dispActiveParams.head = head;
        dispActiveParams.flags = 0;

        CHECK_RC(pLwRm->Control(m_hDisplay,
                                LW0073_CTRL_CMD_SYSTEM_GET_ACTIVE,
                                &dispActiveParams, sizeof (dispActiveParams)));

        if (dispActiveParams.displayId)
        {
            arrayHeads[head] = 1;
        }
    }

    //
    // fill in clock info state, we must only use programmableDomains
    // since we are trying to set the clocks
    //
    tempVal = 0;
    for (iLoopVar = 0; iLoopVar < 32; iLoopVar++)
    {
        if (((1 << iLoopVar) & m_getDomainsParams.clkDomains) == 0)
            continue;

        m_pClkInfos[tempVal++].clkDomain = (1 << iLoopVar);
    }

    // hook up clk info table
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    // get clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_CLK_GET_INFO,
                            &m_getInfoParams,
                            sizeof (m_getInfoParams)));

    Printf(Tee::PriLow,"\n");
    Printf(Tee::PriLow,
         "######### SET COMMAND #########\n");
    Printf(Tee::PriLow, "Set the frequency\n");

    //
    // Refer Bug 321377: For G80+ cards the direct change to PCLK not allowed
    // so making sure that it should not set the flag to do so in this test
    //
    if (!GetBoundGpuSubdevice()->HasBug(321377))
    {
        bIsPclkChangeAllowed = true;
    }

    //
    // Skip the PCLK if the head is not active, like if PCLK1 and headB is
    // not active then skip.
    //
    for (iLoopVar = 0; iLoopVar < m_numClkInfos; iLoopVar++)
    {
        bSetVals = false;
        bSetResVals = false;
       if((m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_XCLK)||
          (m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_HOSTCLK)||
          (m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_DISPCLK)||
          (m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_MSDCLK)||
          ((m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_PCLK0 )&&
           ( (arrayHeads[0] != 1) || (bIsPclkChangeAllowed == false) )
          )                                                                  ||
          ((m_pClkInfos[iLoopVar].clkDomain == LW2080_CTRL_CLK_DOMAIN_PCLK1) &&
           ( (arrayHeads[1] != 1) || (bIsPclkChangeAllowed == false) )
          )
         )
        {
            continue;
        }
        else
        {
            m_pSetClkInfos[iLoopVar].clkDomain =
                m_pClkInfos[iLoopVar].clkDomain;
            m_pSetClkInfos[iLoopVar].clkSource =
                m_pClkInfos[iLoopVar].clkSource;
            m_pSetClkInfos[iLoopVar].flags = 0;
            m_pSetClkInfos[iLoopVar].actualFreq =
                m_pClkInfos[iLoopVar].actualFreq;
            //
            // Setting some value which is 50000 above the target
            // frequency we got as output
            //
            m_pSetClkInfos[iLoopVar].targetFreq =
                m_pClkInfos[iLoopVar].targetFreq + incrVal;

            memcpy(&m_restorGetInfo[iLoopVar],
                   &m_pClkInfos[iLoopVar],
                   sizeof(LW2080_CTRL_CLK_INFO)
                   );

            m_setInfoParams.clkInfoListSize = 1;
            // hook up clk info table
            m_setInfoParams.flags =
                LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
            m_setInfoParams.clkInfoList =
                (LwP64) &m_pSetClkInfos[iLoopVar];

            //
            // Refer Bug 271474 and bug 228587, OK if the PCLK setting
            // returns unsupported
            //

            // Set clocks
            rc = pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_CLK_SET_INFO,
                                &m_setInfoParams,
                                sizeof (m_setInfoParams));

            if (rc == RC::LWRM_NOT_SUPPORTED)
            {
                CHECK_RC_CLEANUP(rc);
            }

            // hook up clk info table
            m_getInfoParams.flags = 0;
            m_getInfoParams.clkInfoListSize = 1;
            m_getInfoParams.clkInfoList = (LwP64) &m_pClkInfos[iLoopVar];

            // get New clocks
            CHECK_RC_CLEANUP(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_CLK_GET_INFO,
                                    &m_getInfoParams,
                                    sizeof (m_getInfoParams)));

            Printf(Tee::PriLow, "Dump the new frequency\n");

            // dump out contents
            Printf(Tee::PriLow,
                   "%d: Setting for Clk Domain 0x%x\n",
                   __LINE__,(UINT32)m_pClkInfos[iLoopVar].clkDomain);

            if(m_pClkInfos[iLoopVar].targetFreq !=
               m_pSetClkInfos[iLoopVar].targetFreq )
            {
                Printf(Tee::PriHigh,
                       "%d: Failure :setting new target freq\n",
                       __LINE__);
                rc = RC::LWRM_ERROR;
                goto Cleanup;
            }
            else
            {
                Printf(Tee::PriLow,
                       "Setting new target freq Success  %d\n",
                       __LINE__);
            }

             bSetVals = true;
            //
            // Restore Clk values
            //
            memcpy(&m_pSetClkInfos[iLoopVar],
                   &m_restorGetInfo[iLoopVar],
                   sizeof(LW2080_CTRL_CLK_INFO)
                   );

            // hook up clk info table
            m_setInfoParams.flags =
                LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
            m_setInfoParams.clkInfoList =
                (LwP64) &m_pSetClkInfos[iLoopVar];

            // Set clocks
            CHECK_RC_CLEANUP(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_CLK_SET_INFO,
                                    &m_setInfoParams,
                                    sizeof (m_setInfoParams)));

            // hook up clk info table
            m_getInfoParams.flags = 0;
            m_getInfoParams.clkInfoListSize = 1;
            m_getInfoParams.clkInfoList = (LwP64) &m_pClkInfos[iLoopVar];

            // get New clocks
            CHECK_RC_CLEANUP(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_CLK_GET_INFO,
                                    &m_getInfoParams,
                                    sizeof (m_getInfoParams)));
            Printf(Tee::PriLow, "Dump the new frequency\n");

            // dump out contents
            Printf(Tee::PriLow,
                   "%d:Restoring for Clk Domain 0x%x \n",
                   __LINE__,(UINT32)m_pClkInfos[iLoopVar].clkDomain);

            if(m_pClkInfos[iLoopVar].targetFreq !=
               m_pSetClkInfos[iLoopVar].targetFreq )
            {
                Printf(Tee::PriHigh,
                       "%d: Fail :Restoring new target freq\n",
                       __LINE__);
                rc = RC::LWRM_ERROR;
                goto Cleanup;
            }
            else
            {
                Printf(Tee::PriLow,
                       "Restoring new target freq Success\n");
            }

             bSetResVals = true;
             bSetVals = false;
        }
    }

    Printf(Tee::PriLow, "\n");
    Printf(Tee::PriLow, "\n");

Cleanup:
    retRc = rc;

    //
    // Restoring all clock domains
    //
    if ( (!bSetResVals) && (bSetVals) )
    {
         //
         // Restore Clk values
         //
        memcpy(&m_pSetClkInfos[iLoopVar],
               &m_restorGetInfo[iLoopVar],
               sizeof(LW2080_CTRL_CLK_INFO)
               );

        // hook up clk info table
        m_setInfoParams.flags =
            LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
        m_setInfoParams.clkInfoList =
            (LwP64) &m_pSetClkInfos[iLoopVar];

        // Set clocks
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_CLK_SET_INFO,
                                &m_setInfoParams,
                                sizeof (m_setInfoParams)));

        // hook up clk info table
        m_getInfoParams.flags = 0;
        m_getInfoParams.clkInfoListSize = 1;
        m_getInfoParams.clkInfoList = (LwP64) &m_pClkInfos[iLoopVar];

        // get New clocks
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_CLK_GET_INFO,
                                &m_getInfoParams,
                                sizeof (m_getInfoParams)));
        Printf(Tee::PriLow, "Dump the new LWCLK freq  %d\n",
                        __LINE__);
        // dump out contents
         if(m_pClkInfos[iLoopVar].targetFreq !=
             m_pSetClkInfos[iLoopVar].targetFreq)
         {
            Printf(Tee::PriLow,
                "Failure :setting new target freq  %d\n",
                        __LINE__);
         }
         else
         {
            Printf(Tee::PriLow,
                "Setting new target frequency Success  %d\n",
                        __LINE__);
         }
    }

    Printf(Tee::PriLow, " BasicGetSetClockTest Done  %d\n",
                        __LINE__);

    return retRc;
}

//! \brief Test Perf table and many silent running feature verification.
//!
//! The test does the Performance table related tests by calling subtests one
//! by one. The test is done only for single GPU setup and don't support SLI or
//! multi-GPUs but facilitated with multi-GPU elwiornmenta and need
//! modification to include for loop for multi-GPU.
//!
//! \return OK if the tests passed, specific RC if failed.
//! \sa Run
//! \sa StressTest
//! \sa RcErrFallback
//------------------------------------------------------------------------------
RC ClockTest::PerfTableTest()
{
    RC rc;
    RC retCode;
    LwRmPtr    pLwRm;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW2080_ALLOC_PARAMETERS lw2080Params = {0};
    LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS tableInfoParams = {0};
    LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS getLevelInfoParams = {0};
    LW2080_CTRL_PERF_SET_FORCE_PSTATE_EX_PARAMS setForcePstateExParams = {0};
    LW2080_CTRL_PERF_GET_CLK_INFO *pGetClkInfos = NULL;
    LW2080_CTRL_PERF_SET_CLK_INFO *pSetClkInfos = NULL;
    UINT32 hDev, hSubDev, numClkInfos;
    INT32 status = 0;
    UINT32 i,j,k;
    UINT32 initialPstate = Perf::ILWALID_PSTATE;
    bool random;

    Printf(Tee::PriLow, " **** PerfTableTest STARTS **** \n");
    if (!m_hClient)
    {
        return (RC::DID_NOT_INITIALIZE_RM);
    }

    // get attached gpus ids
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &gpuAttachedIdsParams, sizeof (gpuAttachedIdsParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "%d:GET_IDS failed: 0x%x \n", __LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    // for each attached gpu...
    gpuIdInfoParams.szName = (LwP64)NULL;

    // First GPU from the attached GPUs
    i = 0;

    // get gpu instance info
    gpuIdInfoParams.gpuId = gpuAttachedIdsParams.gpuIds[i];
    status = LwRmControl(m_hClient, m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                         &gpuIdInfoParams, sizeof (gpuIdInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "%d: GET_ID_INFO failed: 0x%x, \n",__LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    // allocate the gpu's device.
    lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
    lw0080Params.hClientShare = 0;
    hDev = m_hDevice + gpuIdInfoParams.deviceInstance;
    status = LwRmAlloc(m_hClient, m_hClient, hDev,
                       LW01_DEVICE_0,
                       &lw0080Params);
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d: PerfTableTest: Dev Alloc failed!\n",
                        __LINE__);
        return RmApiStatusToModsRC(status);
    }

    // allocate subdevice
    hSubDev = m_hSubDev + gpuIdInfoParams.subDeviceInstance;
    lw2080Params.subDeviceId = gpuIdInfoParams.subDeviceInstance;
    status = LwRmAlloc(m_hClient, hDev, hSubDev,
                       LW20_SUBDEVICE_0,
                       &lw2080Params);
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d: LwRmAlloc(SubDevice) failed: status 0x%x\n",
                __LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    CHECK_RC_CLEANUP(m_ClkUtil.dumpTableInfo(m_hClient,
                                             hSubDev,
                                             &tableInfoParams));

    m_HighLevel = (tableInfoParams.numLevels -1 );

    CHECK_RC_CLEANUP(m_pPerf->GetAvailablePStates(&m_availablePStates));
    CHECK_RC_CLEANUP(m_pPerf->PrintAllPStateInfo());

    // No point in perf test if only one PState is available
    if (m_availablePStates.size() <= 1)
    {
        Printf(Tee::PriHigh,"%d: At ClockTest::PerfTableTest()\n", __LINE__);

        if (m_availablePStates.size() == 0)
        {
            Printf(Tee::PriHigh,"No PStates defined\n");
            return rc;
        }
        if (m_availablePStates.size() == 1)
        {
            Printf(Tee::PriHigh,"There is only one pstate defined - ");
            Printf(Tee::PriHigh,"NO Point doing PERF TABLE TEST SO SKIPPING\n");
            // This is OK case so return rc
            return rc;
        }
    }

    // malloc space for get clk domain info table
    numClkInfos = tableInfoParams.numPerfClkDomains;
    MASSERT(numClkInfos > 0);
    pGetClkInfos = new
        LW2080_CTRL_PERF_GET_CLK_INFO
         [numClkInfos * sizeof (LW2080_CTRL_PERF_GET_CLK_INFO)];
    if (pGetClkInfos == NULL)
    {
        rc = RC::BUFFER_ALLOCATION_ERROR;
        goto Cleanup;
    }
    memset((void *)pGetClkInfos,
           0,
           numClkInfos * sizeof (LW2080_CTRL_PERF_GET_CLK_INFO)
          );

    // malloc space for set clk domain info table
    pSetClkInfos = new
        LW2080_CTRL_PERF_SET_CLK_INFO
         [numClkInfos * sizeof (LW2080_CTRL_PERF_SET_CLK_INFO)];

    if (pSetClkInfos == NULL)
    {
        rc = RC::BUFFER_ALLOCATION_ERROR;
        goto Cleanup;
    }
    memset((void *)pSetClkInfos,
           0,
           numClkInfos * sizeof (LW2080_CTRL_PERF_SET_CLK_INFO)
          );

    // fill in clock info state
    k = 0;
    for (j = 0; j < 32; j++)
    {
        if (((1 << j) & tableInfoParams.perfClkDomains) == 0)
            continue;

        pGetClkInfos[k++].domain = (1 << j);
    }

    // hook up GET clk info table
    getLevelInfoParams.flags = 0;
    getLevelInfoParams.perfGetClkInfoListSize = numClkInfos;
    getLevelInfoParams.perfGetClkInfoList = (LwP64)pGetClkInfos;

    // Cache the current pstate
    CHECK_RC(m_pPerf->GetLwrrentPState(&initialPstate));

    // Clear the forced p-state mode
    CHECK_RC(m_pPerf->ClearForcedPState());

    // Execute SimpleGetSetLevelTest
    if (m_RunMode == DEFAULT_MODE)
    {
        Printf(Tee::PriLow, " \n");
        Printf(Tee::PriLow,
               "**** PerfTableTest: SimpleGetSetLevelTest STARTS ****\n");
        CHECK_RC_CLEANUP(SimpleGetSetLevelTest());
        Printf(Tee::PriLow,
               "****  PerfTableTest: SimpleGetSetLevelTest ENDS ****\n");
    }

    // Execute StressPerfChange either default or Individual
    if ( (m_RunMode == DEFAULT_MODE) ||
         (m_RunMode == STRESSPERFLEVEL_MODE)
       )
    {
        UINT32 index = 0;
        random = m_Random;
        Printf(Tee::PriLow, " \n");
        Printf(Tee::PriLow,
            "**** PerfTableTest: StressPerfChange STARTS **** \n");

        for (UINT32 iLoopVar = 0; iLoopVar< m_LoopVar; iLoopVar++)
        {
            CHECK_RC_CLEANUP(StressPerfChange(m_availablePStates[index]));
            if (!random)
            {
                index++;
                index %= m_availablePStates.size();
            }
            else
            {
                index = rand() % m_availablePStates.size();
            }
        }
        Printf(Tee::PriLow,"**** PerfTableTest: StressPerfChange ENDS ****\n");
    }

    //
    // Refer Bug 636598: The test is failing with high degree of intermittence,
    // needs to be disabled till a fix is found.
    //

    // Execute StressTest either default or Individual
    if ( (m_RunMode == DEFAULT_MODE) ||
         (m_RunMode == STRESSTEST_MODE)
       )
    {
        Printf(Tee::PriLow, " \n");
        Printf(Tee::PriLow, "**** PerfTableTest: StressTest STARTS **** \n");
        CHECK_RC_CLEANUP(StressTest(m_hClient,
                            hDev,
                            hSubDev,
                            numClkInfos,
                            pSetClkInfos,
                            &tableInfoParams,
                            &getLevelInfoParams));
        Printf(Tee::PriLow, "**** PerfTableTest: StressTest ENDS **** \n");
    }
    // Execute RcErrFallback either default or Individual
    if ( m_RunMode == RCFALLBACK_MODE )
    {
        Printf(Tee::PriLow, " \n");
        Printf(Tee::PriLow, "**** PerfTableTest: RcErrFallback STARTS ****\n");
        // Check for fallbacks due to RC error
        CHECK_RC_CLEANUP(RcErrFallback(m_hClient,
                               hDev,
                               hSubDev,
                               LW_CFGEX_PERF_MODE_3D_BOOST,
                               tableInfoParams.numPerfClkDomains,
                               &getLevelInfoParams));
        Printf(Tee::PriLow, "**** PerfTableTest: RcErrFallback ENDS ****\n");
    }
    // Execute ActiveClkingTest either default or Individual
    if ( m_RunMode == ACTIVECLK_MODE )
    {
        Printf(Tee::PriLow, " \n");
        Printf(Tee::PriLow, "**** PerfTableTest: ActiveClkingTest STARTS ****\n");
        CHECK_RC_CLEANUP(ActiveClkingTest(m_hClient,
                                          hDev,
                                          hSubDev,
                                          &getLevelInfoParams,
                                          tableInfoParams));
        Printf(Tee::PriLow, "**** PerfTableTest: ActiveClkingTest ENDS ****\n");
    }

Cleanup:

    // Restore Forced PState
    if (initialPstate != Perf::ILWALID_PSTATE)
    {
        setForcePstateExParams.forcePstate = m_pPerf->PStateNumTo2080Bit(initialPstate);
        setForcePstateExParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;
        setForcePstateExParams.flags = 0;
        setForcePstateExParams.flags = FLD_SET_DRF(2080, _CTRL_PERF_SET_FORCE_PSTATE_EX_FLAGS,
                _PRIORITY, _MODS, setForcePstateExParams.flags);

        pLwRm->Control(m_hBasicSubDev,
                       LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE_EX,
                       &setForcePstateExParams,
                       sizeof(setForcePstateExParams));
    }

    retCode = rc;

    // free clk info table
    delete []pGetClkInfos;
    delete []pSetClkInfos;

    // free handles
    LwRmFree(m_hClient, hDev, hSubDev);
    LwRmFree(m_hClient, m_hClient, hDev);
    LwRmFree(m_hClient, m_hClient, m_hClient);

    Printf(Tee::PriLow, "\n");
    Printf(Tee::PriLow, " **** PerfTableTest ENDS ****\n");
    return retCode;
}

//! \brief SimpleGetSetLevelTest Function.
//!
//! Get/Set test for Perf table Level.
//!
//! \return OK if the tests passed, specific RC if failed
//! \sa Run
//! \sa PerfTableTest
//------------------------------------------------------------------------------
RC ClockTest::SimpleGetSetLevelTest()
{
    RC rc;
    UINT32 clkDomainMask;

    Printf(Tee::PriHigh,"Starting SimpleGetSetLevelTest...\n");

    for (LwU32 i = 0; i < m_availablePStates.size(); ++i)
    {
        vector<LW2080_CTRL_PERF_CLK_DOM_INFO> vClkInfos;
        vector<LW2080_CTRL_PERF_VOLT_DOM_INFO> vVoltInfos;
        LW2080_CTRL_PERF_CLK_DOM_INFO clkInfo;

        Printf(Tee::PriHigh, "Forcing to PState P%d:\n", m_availablePStates[i]);

        // Force to this PState
        CHECK_RC(m_pPerf->SuggestPState(m_availablePStates[i],
                          LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR));

       // Get the current P-State clk information
        clkDomainMask = m_pPerf->GetPStateClkDomainMask();

        for (LwU32 j = 0; j < 32; ++j)
        {
            UINT32 bitMask = BIT(j);
            Gpu::ClkDomain ModsDomain = PerfUtil::ClkCtrl2080BitToGpuClkDomain(bitMask);
            if (!m_pSubdevice->HasDomain(ModsDomain))
            {
                Printf(Tee::PriNormal, "RM doesnt not allow programming of %s",
                       PerfUtil::ClkDomainToStr(ModsDomain));
                continue;
            }
            if (clkDomainMask & bitMask)
            {
               UINT64 clkFreqHz;
               CHECK_RC(m_pPerf->GetClockAtPState(m_availablePStates[i],
                                         PerfUtil::ClkCtrl2080BitToGpuClkDomain(bitMask),
                                         &clkFreqHz));
               Printf(Tee::PriHigh, "Clk Domain: 0x%x, Frequency : %llu KHZ\n",
                      bitMask, clkFreqHz/1000);

               // Overclock this clock unless its HostClk
               if (LW2080_CTRL_CLK_DOMAIN_HOSTCLK != bitMask)
               {
                   clkInfo.domain = bitMask;
                   clkInfo.freq   = UNSIGNED_CAST(LwU32, (clkFreqHz/1000) + OVERCLOCK_VALUE);
                   vClkInfos.push_back(clkInfo);
                   Printf(Tee::PriHigh, "Overclocking to %d KHZ\n", clkInfo.freq);
               }
            }
        }
        Printf(Tee::PriHigh, "Overriding default pstate table for P%d with overclocked values\n",
               m_availablePStates[i]);

        // Try overclocking this p-state
        CHECK_RC_MSG(m_pPerf->SetPStateTable(m_availablePStates[i], vClkInfos, vVoltInfos),
               "Overriding PState Table failed");

        // Change to this overclocked pstate
        Printf(Tee::PriHigh, "Switching to overridden pstate P%d\n", m_availablePStates[i]);
        CHECK_RC_MSG(m_pPerf->SuggestPState(m_availablePStates[i],
            LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR),"Switching to overridden pstate failed");

        //
        // After setting new clock, check whether the memory information is
        // consistent.
        //
        CHECK_RC(VerifyVidMem());
        Printf(Tee::PriHigh, "Successfully switched to overridden pstate P%d\n",
               m_availablePStates[i]);

        // Restore the original p-state table
        Printf(Tee::PriHigh, "Restoring default pstate table for P%d\n", m_availablePStates[i]);
        CHECK_RC(m_pPerf->RestorePStateTable());

        //
        // After restoring clock, check whether the memory information is
        // consistent.
        //
        CHECK_RC(VerifyVidMem());
        Printf(Tee::PriHigh, "Successfully restored default pstate settings for P%d\n",
               m_availablePStates[i]);
    }

    Printf(Tee::PriLow,
            " **** PerfTableTest: OVERCLOCKING LEVELS ENDS ****\n");
    return rc;
}

//! \brief StressPerfChange Function.
//!
//! Tests for N number of times the pstate changes and that too randomly
//! between them, after each change it also verifies for actual register
//! level clock changes.
//!
//! \sa Run
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC ClockTest::StressPerfChange(UINT32 uDestPState)
{
    RC rc;
    UINT32 lwrPState = 0;
    UINT32 clkDomainMask;

    // Get current pstate
    m_pPerf->GetLwrrentPState(&lwrPState);
    Printf(Tee::PriHigh, "\nLwrrent PState P%d, Mask 0x%08x\n",
           lwrPState, m_pPerf->PStateNumTo2080Bit(lwrPState));

    //
    // Before setting pstate, check whether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

    // Change to the target pstate
    Printf(Tee::PriHigh, "Switching to PState P%d, Mask 0x%08x\n",
        uDestPState, m_pPerf->PStateNumTo2080Bit(uDestPState));

    CHECK_RC_MSG(m_pPerf->SuggestPState(uDestPState,
        LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR), "PState Switch failed" );

    // Get current pstate
    m_pPerf->GetLwrrentPState(&lwrPState);
    Printf(Tee::PriHigh, "Current PState P%d, Mask 0x%08x\n",
           lwrPState, m_pPerf->PStateNumTo2080Bit(lwrPState));

    if (lwrPState != uDestPState)
    {
        Printf(Tee::PriHigh,
                   "%d: %s: Fail to change the pstate\n",
                   __LINE__, __FUNCTION__);
        return RC::LWRM_ERROR;
    }

    // Print the new clks info
    clkDomainMask = m_pPerf->GetPStateClkDomainMask();

    for (LwU32 j = 0; j < 32; ++j)
    {
        UINT32 bitMask = BIT(j);
        if (clkDomainMask & bitMask)
        {
           UINT64 clkFreqHz;
           m_pPerf->GetClockAtPState(lwrPState,
                                     PerfUtil::ClkCtrl2080BitToGpuClkDomain(bitMask),
                                     &clkFreqHz);
           Printf(Tee::PriHigh, "Clk Domain: 0x%x, Frequency : %llu KHZ\n",
                  bitMask, clkFreqHz/1000);
        }
    }

    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());
    return rc;
}

//! \brief StressTest: Client Stress test path check
//!
//! This gives back the stress test result by setting to maximum perf level
//! and for desktop level
//!
//! \return OK if the if everything fine, specific RC if failed
//!            This doesn't fail if stress test fails, just informs
//------------------------------------------------------------------------------
RC ClockTest::StressTest(UINT32 m_hClient,
                  UINT32 hDev,
                  UINT32 hSubDev,
                  UINT32 numClkInfos,
                  LW2080_CTRL_PERF_SET_CLK_INFO *pSetClkInfos,
                  LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS *pTableInfo,
                  LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS *pLevelInfo)
{
    MYPERFINFO perfInfo;
    LW2080_CTRL_PERF_TEST_LEVEL_PARAMS testLevel = {0};
    INT32 status = 0;
    RC rc;
    UINT32 perfmode = 0;

    Printf(Tee::PriLow,"Stress test STARTS\n");
    status = m_ClkUtil.getLevels(m_hClient, hSubDev,
                       pTableInfo, pLevelInfo, &perfInfo);
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"%d:TEST_LEVEL failed: 0x%x\n",
               __LINE__,status);
        return RC::LWRM_ERROR;;
    }

    testLevel.flags  = 0;
    testLevel.result = 0;
    testLevel.level  = m_HighLevel;

    testLevel.perfSetClkInfoListSize = numClkInfos;
    testLevel.perfSetClkInfoList = (LwP64)pSetClkInfos;

    pSetClkInfos[0].domain = LW2080_CTRL_CLK_DOMAIN_MCLK;
    pSetClkInfos[0].targetFreq = perfInfo.mclk_maxperf + (OVERCLOCK_VALUE);

    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_TEST_LEVEL,
                         &testLevel, sizeof (testLevel));
    //
    // Return OK if the stress test is not supported:
    // Refer Bug 255840 and Bug 271474
    //
    if (status == LW_ERR_NOT_SUPPORTED)
    {
        Printf(Tee::PriHigh,
            "%d:StressTest not supported : 0x%x\n",
            __LINE__,status);
        return rc;
    }
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"%d:TEST_LEVEL failed: 0x%x\n",
                __LINE__,status);
        return RC::LWRM_ERROR;;
    }

    Printf(Tee::PriLow,"result: %d\n", (INT32)(testLevel.result));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

    Printf(Tee::PriLow,"Testing overclocked desktop level...\n");

    testLevel.flags  = 0;
    testLevel.result = 0;
    testLevel.level  = 0;

    pSetClkInfos[0].domain = LW2080_CTRL_CLK_DOMAIN_MCLK;
    pSetClkInfos[0].targetFreq = perfInfo.mclk_desktop + (OVERCLOCK_VALUE);

    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_TEST_LEVEL,
                         &testLevel, sizeof (testLevel));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"%d:TEST_LEVEL failed: 0x%x\n",
               __LINE__,status);
        return RC::LWRM_ERROR;;
    }

    Printf(Tee::PriLow,"  result: %d\n", (INT32)(testLevel.result));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

    //
    // Reset to MODS original i.e. forced higher perf mode
    // Flag is false as we want to force change not to reset
    //
    if(m_HighLevel == 2)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_ALWAYS;
    }
    if(m_HighLevel == 1)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_LP_ALWAYS;
    }
    CHECK_RC(m_ClkUtil.ChangeModeForced(m_hClient,
                              hDev,
                              hSubDev,
                              perfmode,
                              false));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

    return rc;
}

//! \brief Test for fallback due to RC Errors
//!
//! Tests if the current lvele fallsback due to RC errors, also checks whether
//! clocks falled down to the new set level
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ClockTest::RcErrFallback(UINT32 m_hClient,
                            UINT32 hDev,
                            UINT32 hSubDev,
                            UINT32 perfmode,
                            UINT32 noClkDomains,
                            LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                              *getLevelInfoParams)
{
    RC rc, errorRc;
    RC retCode;
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};
    UINT32 levelBfrRC;//, status;

    Printf(Tee::PriLow,"RcErrFallback test STARTS \n");

    CHECK_RC_CLEANUP(ClearModsPerfthenBoost(m_hClient,
                                            hDev,
                                            hSubDev,
                                            perfmode,
                                            &levelBfrRC)
                    );

    //
    // Introduce Parse error and see whether the level falls back to
    // 2D/one level below
    //

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC_CLEANUP(m_Notifier.Allocate(m_pCh,1,&m_TestConfig));
    CHECK_RC_CLEANUP(m_pCh->Write(0, DMAPUSHER_METHOD, ILWALID_DATA));
    CHECK_RC_CLEANUP(m_Notifier.Instantiate(0));
    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);
    CHECK_RC_CLEANUP(m_pCh->Write(0,LW9097_NOTIFY,
                          LW9097_NOTIFY_TYPE_WRITE_ONLY));
    CHECK_RC_CLEANUP(m_pCh->Write(0, LW9097_NO_OPERATION, 0));
    CHECK_RC_CLEANUP(m_pCh->Flush());
    CHECK_RC_CLEANUP(m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, m_TimeoutMs));
    errorRc = m_pCh->CheckError();
    if (errorRc != RC::RM_RCH_FIFO_ERROR_PARSE_ERR)
    {
        Printf(Tee::PriHigh,
               "%d:RobustChannelTest:fifo error notifier incorrect\n",
               __LINE__);
        rc = RC::UNEXPECTED_ROBUST_CHANNEL_ERR;

        //
        // If unexpected RC error then return that error to MODS
        // go to cleanup to free the allocated resources
        //
        goto Cleanup;
    }

    // get and dump active level
    CHECK_RC_CLEANUP(DumpActiveLevel(m_hClient,
                                     hSubDev,
                                     &getLwrrLevelInfoParams));

    Printf(Tee::PriLow,"Current level: %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));
    Printf(Tee::PriLow,"LEVEL after RC errors is %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    if ( (levelBfrRC - 1) != getLwrrLevelInfoParams.lwrrLevel)
    {
        Printf(Tee::PriHigh,
            "%d:The Current level didn't fall by 1 after RC errors\n",
            __LINE__);
        rc =  RC::LWRM_ERROR;
        goto Cleanup;
    }

    // Additinally verfiy that it also set the clocks to the lowest level
    getLevelInfoParams->level = getLwrrLevelInfoParams.lwrrLevel;

    CHECK_RC(m_ClkUtil.LwrrentLevelClks(m_hClient,
                     hSubDev,
                     noClkDomains,
                     getLevelInfoParams,
                     false));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

Cleanup:
    retCode = rc;
    // Next few steps to see whether we recovered from Unknown Method error
    m_TestConfig.FreeChannel(m_pCh);

    //
    // Reset to MODS original i.e. forced higher perf mode
    // Flag is false as we want to force change not to reset.
    // Done by following two methods
    //
    CHECK_RC(ResetPerfBoost(m_hClient,
                            hDev,
                            hSubDev)
            );
    if(m_HighLevel == 2)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_ALWAYS;
    }
    if(m_HighLevel == 1)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_LP_ALWAYS;
    }
    CHECK_RC(m_ClkUtil.ChangeModeForced(m_hClient,
                              hDev,
                              hSubDev,
                              perfmode,
                              false));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());
    return retCode;
}

//! \brief ActiveClkingTest: Test for Active clcoking (AC) Enable/Disable
//!
//! Triggers the active clcoking, verifies whether AC enabled or not.
//! Introduces RC error, verfies the fallback, disabled the AC and verifies
//! the disabling. For RC it calls the private function ActiveClkRcErrFallback.
//!
//! \return OK if the tests passed, specific RC if failed
//! \sa Run
//! \sa PerfTableTest
//! \sa ActiveClkRcErrFallback
//------------------------------------------------------------------------------
RC ClockTest::ActiveClkingTest(UINT32 m_hClient,
                               UINT32 hDev,
                               UINT32 hSubDev,
                               LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                                 *getLevelInfoParams,
                               LW2080_CTRL_PERF_GET_TABLE_INFO_PARAMS
                                 tableInfoParams
                              )
{
    RC rc;
    INT32 status = 0;
    LW2080_CTRL_PERF_SET_ACTIVE_CLOCKING_PARAMS setActiveClocking = {0};
    UINT32 noOfLevelsBfrAC = 0;
    UINT32 noOfLevelsAc = 0;
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};

    noOfLevelsBfrAC = tableInfoParams.numLevels;
    // enable active clocking
    Printf(Tee::PriLow,"Enabling active clocking...\n");
    setActiveClocking.flags =
        DRF_DEF(2080, _CTRL_PERF_SET_ACTIVE_CLOCKING, _FLAGS, _ENABLE);
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_SET_ACTIVE_CLOCKING,
                         &setActiveClocking, sizeof (setActiveClocking));
    //
    // Return OK if the Active clocking test is not supported:
    // Refer Bug 274260 and Bug 271474
    //
    if (status == LW_ERR_NOT_SUPPORTED)
    {
        Printf(Tee::PriHigh,
            "%d:ActiveClkingTest not supported : 0x%x\n",
            __LINE__,status);
        return rc;
    }
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,"%d:SET_ACTIVE_CLOCKING failed: 0x%x\n",
               __LINE__,status);
        return (RmApiStatusToModsRC(status));
    }

    Printf(Tee::PriLow,"  active clocking enabled\n");
    // re-get perf table header and dump it's contents
    CHECK_RC(m_ClkUtil.dumpTableInfo(m_hClient, hSubDev, &tableInfoParams));

    Printf(Tee::PriLow,"Current number of levels is %d \n",
           (INT32)(tableInfoParams.numLevels));

    if(tableInfoParams.numLevels != (noOfLevelsBfrAC + 1))
    {
        Printf(Tee::PriHigh,
            "%d:Number of level didn't changed\n", __LINE__);
        Printf(Tee::PriHigh,
            "%d:Current number of levels is %d\n",
               __LINE__,(INT32)(tableInfoParams.numLevels));
        return RC::LWRM_ERROR;
    }
    noOfLevelsAc = tableInfoParams.numLevels;

    // get and dump current perf levels
    status = m_ClkUtil.dumpLevels(m_hClient, hSubDev,
                        &tableInfoParams, getLevelInfoParams);
    if ((INT32)status != OK)
    {
        Printf(Tee::PriHigh,
               "%d:Dump Level info failed: 0x%x \n",
                __LINE__,status);
        return (RmApiStatusToModsRC(status));
    }
    Printf(Tee::PriLow,"Wait for Active clocking to become active\n");
    // >5 Sec
    Utility::Delay(WAIT_FOR_ACTIVE_CLK/*microseconds*/);

    // get and dump active level
    CHECK_RC(DumpActiveLevel(m_hClient, hSubDev, &getLwrrLevelInfoParams));

    // Additinally verfiy that it also set the clocks to the current level
    getLevelInfoParams->level = getLwrrLevelInfoParams.lwrrLevel;

    // Check current clk levels
    CHECK_RC(m_ClkUtil.LwrrentLevelClks(m_hClient,
                                        hSubDev,
                                        tableInfoParams.numPerfClkDomains,
                                        getLevelInfoParams,
                                        true));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

    //
    // STARTS:RC error fallbacks,
    // after we enable active clocking
    //
    CHECK_RC(ActiveClkRcErrFallback(m_hClient,
                                    hDev,
                                    hSubDev,
                                    tableInfoParams.numPerfClkDomains,
                                    getLevelInfoParams
                                   ));

    //
    // ENDS:RC error fallbacks,
    // after we enable active clocking
    //

    // disable active clocking
    Printf(Tee::PriLow,"Disabling active clocking...\n");
    setActiveClocking.flags =
        DRF_DEF(2080, _CTRL_PERF_SET_ACTIVE_CLOCKING, _FLAGS, _DISABLE);

    status = LwRmControl(m_hClient,
                         hSubDev,
                         LW2080_CTRL_CMD_PERF_SET_ACTIVE_CLOCKING,
                         &setActiveClocking,
                         sizeof (setActiveClocking));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
            "%d:SET_ACTIVE_CLOCKING failed: 0x%x \n",
                 __LINE__,status);
        return RC::LWRM_ERROR;
    }

    Printf(Tee::PriLow,"  active clocking disabled\n");
    Printf(Tee::PriLow,"Wait for Active clocking to disble\n");
    // >5 Sec
    Utility::Delay(WAIT_FOR_ACTIVE_CLK/*microseconds*/);

    // re-get perf table header and dump it's contents after AC disabled
    CHECK_RC(m_ClkUtil.dumpTableInfo(m_hClient, hSubDev, &tableInfoParams));
    Printf(Tee::PriLow,"Number of Levels are: %d\n",
           (INT32)(tableInfoParams.numLevels));
    if(tableInfoParams.numLevels == noOfLevelsAc)
    {
        Printf(Tee::PriHigh,
            "%d:Number of level didn't changed\n", __LINE__);
        Printf(Tee::PriHigh,"Current number of levels is %d \n",
               (INT32)(tableInfoParams.numLevels));
        return RC::LWRM_ERROR;
    }
    //
    // After disabling Active clocking, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());
    return rc;
}

//! \brief ActiveClkRcErrFallback: Test for fallback due to RC Errors
//!
//! Checks for the RC fallback in case of active clocking.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC ClockTest::ActiveClkRcErrFallback(UINT32 m_hClient,
                                     UINT32 hDev,
                                     UINT32 hSubDev,
                                     UINT32 noClkDomains,
                                     LW2080_CTRL_PERF_GET_LEVEL_INFO_PARAMS
                                      *getLevelInfoParams
                                     )
{
    RC errorRc, rc;
    RC retCode;
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};
    UINT32 levelBfrRC;//, status;
    UINT32 levelAcRcFall;
    UINT32 perfmode = 0;

    // get and dump active level
    CHECK_RC(DumpActiveLevel(m_hClient, hSubDev, &getLwrrLevelInfoParams));

    Printf(Tee::PriLow,"Current level after active clocking is ON: %d\n",
            (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    levelBfrRC = getLwrrLevelInfoParams.lwrrLevel;

    //
    // Introduce Parse error and see whether the level falls back to
    // 2D/one level below
    //
    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC_CLEANUP(m_Notifier.Allocate(m_pCh,1,&m_TestConfig));
    CHECK_RC_CLEANUP(m_pCh->Write(0, DMAPUSHER_METHOD, ILWALID_DATA));
    CHECK_RC_CLEANUP(m_Notifier.Instantiate(0));
    m_Notifier.Clear(LW9097_NOTIFIERS_NOTIFY);
    CHECK_RC_CLEANUP(m_pCh->Write(0,LW9097_NOTIFY,
                          LW9097_NOTIFY_TYPE_WRITE_ONLY));
    CHECK_RC_CLEANUP(m_pCh->Write(0, LW9097_NO_OPERATION, 0));
    CHECK_RC_CLEANUP(m_pCh->Flush());
    CHECK_RC_CLEANUP(m_Notifier.Wait(LW9097_NOTIFIERS_NOTIFY, m_TimeoutMs));
    errorRc = m_pCh->CheckError();
    if (errorRc != RC::RM_RCH_FIFO_ERROR_PARSE_ERR)
    {
        Printf(Tee::PriHigh,
               "%d:RobustChannelTest:fifo error notifier incorrect\n",
               __LINE__);
        rc = RC::UNEXPECTED_ROBUST_CHANNEL_ERR;

        //
        // If unexpected RC error then return that error to MODS
        // go to cleanup to free the allocated resources
        //
        goto Cleanup;
    }

    // get and dump active level
    CHECK_RC_CLEANUP(DumpActiveLevel(m_hClient,
                                     hSubDev,
                                     &getLwrrLevelInfoParams));

    Printf(Tee::PriLow,"Current level: %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    Printf(Tee::PriLow,"LEVEL after RC errors is %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel));

    if ( getLwrrLevelInfoParams.lwrrLevel == levelBfrRC)
    {
        Printf(Tee::PriHigh,
            "%d:The Current level didn't fall by 1 after RC errors\n",
             __LINE__);
        rc = RC::LWRM_ERROR;
        goto Cleanup;
    }

    // Additinally verfiy that it also set the clocks to the lowest level
    getLevelInfoParams->level = getLwrrLevelInfoParams.lwrrLevel;

    CHECK_RC_CLEANUP(m_ClkUtil.LwrrentLevelClks(m_hClient,
                                                hSubDev,
                                                noClkDomains,
                                                getLevelInfoParams,
                                                false));
    //
    // After setting clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());

Cleanup:
    retCode = rc;
    // Next few steps to see whether we recovered from Unknown Method error
    m_TestConfig.FreeChannel(m_pCh);

    //
    // This is because we get the level 2 (levels starting at 0
    // so level 2 is highest performance)
    // as suspended due to RC errors affecting the level 2 to suspend mode
    //
    CHECK_RC(ClearModsPerfthenBoost(m_hClient,
                                hDev,
                                hSubDev,
                                LW_CFGEX_PERF_MODE_3D_BOOST,
                                &levelAcRcFall)
                                );

    // Reset to MODS original Done by following two sub-functions

    //
    // Reset to LW_CFGEX_PERF_MODE_NONE so that Client request decreases
    // In current case it will decrease to zero
    //

    CHECK_RC(ResetPerfBoost(m_hClient,
                            hDev,
                            hSubDev)
             );

    //
    // Forced higher perf mode
    // Flag is false as we want to force change not to reset
    //
    if(m_HighLevel == 2)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_ALWAYS;
    }
    if(m_HighLevel == 1)
    {
        perfmode = LW_CFGEX_PERF_MODE_3D_LP_ALWAYS;
    }
    CHECK_RC(m_ClkUtil.ChangeModeForced(m_hClient,
                              hDev,
                              hSubDev,
                              perfmode,
                              false));
    //
    // After setting new clock, check wether the memory information is
    // consistent.
    //
    CHECK_RC(VerifyVidMem());
    return retCode;
}

//! \brief ClearModsPerfthenBoost: Helper function.
//!
//! This helper function used to Clear the MODS Forced perf and then
//! does the boost as per the perfmode input arg.Helper used at many
//! places to clear out forced and honor 1st perfmode change.
//!
//! \return OK if the passed, specific RC if failed
//! \sa ResetPerfBoost
RC ClockTest::ClearModsPerfthenBoost(UINT32 m_hClient,
                                    UINT32 hDev,
                                    UINT32 hSubDev,
                                    UINT32 perfmode,
                                    UINT32 *levelAftBoost)
{
    RC rc;
    LW2080_CTRL_PERF_BOOST_PARAMS perfBoostParams = {0};
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};
    INT32 status = 0;

    // STARTS: Clear the force mode if any previously exists
    perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR;
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_BOOST,
                         &perfBoostParams,
                         sizeof(perfBoostParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d:SET MODE failed for mode %d with status 0x%x\n",
               (INT32)(perfBoostParams.flags), __LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    // get and dump lwrrently active level
    Printf(Tee::PriLow,"Dumping current perf level...\n");
    getLwrrLevelInfoParams.lwrrLevel = ILWALID_DATA;
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                         &getLwrrLevelInfoParams,
                         sizeof (getLwrrLevelInfoParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
              "%d:CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x\n",
                __LINE__,status);
        return RmApiStatusToModsRC(status);
    }
    Printf(Tee::PriLow,
           "Current level: %d, After Clearing the forced mode  %d\n",
           (INT32)(getLwrrLevelInfoParams.lwrrLevel), __LINE__);

    // ENDS: Clear the force mode if any previously exists

    // START: SET to the requested 3D Level
    perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_BOOST_TO_MAX;
    perfBoostParams.duration = LW2080_CTRL_PERF_BOOST_DURATION_INFINITE;
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_BOOST,
                         &perfBoostParams,
                         sizeof(perfBoostParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d:SET MODE failed for mode %d with status 0x%x\n",
               (INT32)(perfBoostParams.flags), __LINE__,status);
        return RmApiStatusToModsRC(status);
    }
    // get and dump active level
    CHECK_RC(DumpActiveLevel(m_hClient, hSubDev, &getLwrrLevelInfoParams));

    *levelAftBoost = getLwrrLevelInfoParams.lwrrLevel;

    // END: SET to the requested 3D Level
    return rc;
}

//! \brief ResetPerfBoost: Helper function.
//!
//! This helper function used to Reset the perf boost done by calling
//! ClearModsPerfthenBoost
//!
//! \return OK if the passed, specific RC if failed
//! \sa ClearModsPerfthenBoost
RC ClockTest::ResetPerfBoost(UINT32 m_hClient,
                             UINT32 hDev,
                             UINT32 hSubDev)
{
    RC rc;
    LW2080_CTRL_PERF_BOOST_PARAMS perfBoostParams = {0};
    INT32 status = 0;
    LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS getLwrrLevelInfoParams = {0};

    //
    // reset the counters by, placing LW_CFGEX_PERF_MODE_NONE
    //
    perfBoostParams.flags = LW2080_CTRL_PERF_BOOST_FLAGS_CMD_CLEAR;

    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_BOOST,
                         &perfBoostParams,
                         sizeof(perfBoostParams));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d:SET_LEVEL_INFO failed: 0x%x \n", __LINE__,status);
        return RmApiStatusToModsRC(status);
    }

    // get and dump active level
    CHECK_RC(DumpActiveLevel(m_hClient, hSubDev, &getLwrrLevelInfoParams));

    //
    // END: the counters by
    // placing LW_CFGEX_PERF_MODE_NONE
    //
    return rc;
}

//! \brief DumpActiveLevel: Helper function.
//!
//! This helper function used at multiple places to get and dump the current
//! Perf level.
//!
//! \return OK if the passed, specific RC if failed
RC ClockTest::DumpActiveLevel(UINT32 m_hClient,
                              UINT32 hSubDev,
                              LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS
                                *getLwrrLevelInfoParams)
{
    RC rc;
    INT32 status = 0;
    Printf(Tee::PriLow,"Dumping current perf level...\n");
    getLwrrLevelInfoParams->lwrrLevel = ILWALID_DATA;
    status = LwRmControl(m_hClient, hSubDev,
                         LW2080_CTRL_CMD_PERF_GET_LWRRENT_LEVEL,
                         getLwrrLevelInfoParams,
                       sizeof (LW2080_CTRL_PERF_GET_LWRRENT_LEVEL_PARAMS));
    if (status != LW_OK)
    {
        Printf(Tee::PriHigh,
               "%d:CMD_PERF_GET_LWRRENT_LEVEL failed: 0x%x \n",
               __LINE__,status);
        return RmApiStatusToModsRC(status);
    }
    Printf(Tee::PriLow," current level is: %d\n",
           (INT32)(getLwrrLevelInfoParams->lwrrLevel));
    return  rc;
}

//! \brief AllocAndFillVidem
//!
//!This function is used to allocate video memory of page size of RM and fill
//!it up with some data.
//!
//! \return OK if passed.
RC ClockTest::AllocAndFillVidMem()
{
    RC rc;
    UINT64 heapOffset = 0;
    UINT08 *pVidMapMemIterator,uiMemData=0;
    UINT32 uiLocalVar;
    UINT32 uiNoOfInts;
    LwRmPtr pLwRm;
    //
    // Allocate the video heap of RM page size
    //
    CHECK_RC(pLwRm->VidHeapAllocSize(LWOS32_TYPE_IMAGE,
                                     LWOS32_ATTR_NONE,
                                     LWOS32_ATTR2_NONE,
                                     VIDMEM_SIZE,
                                     &m_vidMemHandle,
                                     &heapOffset,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     GetBoundGpuDevice()));
    //
    // Map the allocated video heap memory
    //
    CHECK_RC(pLwRm->MapMemory(m_vidMemHandle,
                              0,
                              VIDMEM_SIZE,
                              &m_pMapVidAddr,
                              0,
                              GetBoundGpuSubdevice()));
    //
    // Fill some known values to the allocated heap
    //
    uiNoOfInts = VIDMEM_SIZE/sizeof(UINT08);
    pVidMapMemIterator = (UINT08 *)m_pMapVidAddr;
    for(uiLocalVar = 0; uiLocalVar < uiNoOfInts; uiLocalVar++)
    {
        MEM_WR08(pVidMapMemIterator, uiMemData++);
        pVidMapMemIterator++;
    }
    return rc;
}

//! \brief VerifyVidMem:
//!
//! This function verify the contents of Video memory which was populated by
//! function AllocAndFillVidMem.
//!
//! \return OK if passed
RC ClockTest::VerifyVidMem()
{
    RC rc;
    UINT32 uiLocalVar,uiNoOfInts;
    UINT08 *pVidMapMemIterator,uiMemData=0;

    uiNoOfInts = VIDMEM_SIZE / sizeof(UINT08);
    //
    // Read values from the Video memory's mapped address, verify
    //
    pVidMapMemIterator = (UINT08 *)m_pMapVidAddr;
    for (uiLocalVar = 0; uiLocalVar < uiNoOfInts; uiLocalVar++)
    {
        if (MEM_RD08(pVidMapMemIterator) != uiMemData++)
        {
            Printf(Tee::PriHigh,
                  "%d:VerifyVidMem:VidMem incorrect after CLK change",
                  __LINE__);
            return RC::LWRM_ERROR;
        }
        pVidMapMemIterator++;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ClockTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ClockTest, RmTest, "ClockTest RM test.");
CLASS_PROP_READWRITE(ClockTest, All, bool,
    "Execute All tests: Default All.User needs to specify");
CLASS_PROP_READWRITE(ClockTest, BasicTest, bool,
    "Change the exelwtion Basic Set/Get test: 0=Default.");
CLASS_PROP_READWRITE(ClockTest, RunPerfTable, bool,
    "Change the Perf table test: 0=Default.");
CLASS_PROP_READWRITE(ClockTest, RunMode, UINT32,
    "Change the exelwtion of the Run() test: 0=Default.");
CLASS_PROP_READWRITE(ClockTest, PmState, UINT32,
    "PowerMizer state value.");
CLASS_PROP_READWRITE(ClockTest, Random, bool,
    "Check if Random is set or not.");
CLASS_PROP_READWRITE(ClockTest, LoopVar, UINT32,
    "Check if Random is set or not.");
