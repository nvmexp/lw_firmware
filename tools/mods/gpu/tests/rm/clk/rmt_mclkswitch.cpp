/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2014,2016-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_mclkswitch.cpp
//!
//! \RM-Test to perform a switch test for DRAMCLK switch verification on HW.
//!
//! The test basically switches the clock frequency based on the command line
//! arguments provided by the caller.
//!
//! -freq : Switches MCLK to the requested freq as given through command line.
//!         In this case if number of frequencies is more than one caller should
//!         separate them with underscores. For example, if I want to switch to
//!         1620MHz then to 202.5MHz, then the -freq option would look something
//!         like: -freq 1620000_202500
//!         Freq should be specified in KHz.
//!
//!         An additional feature has been added that extends this basic
//!         frequency list specificiation. After each frequency specified,
//!         an optional 1-character suffix may be appended to specify which
//!         path RM should use for MCLK.
//!
//!         The supported path suffixes are as follows:
//!         "b" => Force use of BYPASS/alt path.
//!         "v" => Force use of VCO/ref path.
//!         ""  => Let RM decide which path to use (default).
//!
//!         e.g. -freq 162000b_324000v_2500000_324000b_162000v
//!
//!         This feature is more useful for debugging non-POR freq/paths so it
//!         is the responsibility of the user to know if a specific path
//!         should work with a specific frequency.  If RM decides not
//!         to use the path requested it is considered a failure from
//!         the tests perspective.
//!
//! This test also allocates some vid mem and writes some known data in that
//! before MCLK Switch. After every MCLK switch that data is read to verify
//! the sanity of FB after the MCLK switch operation.
//!
//! Info:-
//!       https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Architecture/RM_Test_Infrastructure/Lwrrent_RM_Test_Plan/SW_Clock_Testing#MCLK_Switch_Test
//!

#include "core/include/channel.h"
#include "core/utility/errloggr.h"
#include "gpu/tests/rmtest.h"
#include "gpu/tests/gputestc.h"
#include "core/include/jscript.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "gpu/utility/rmclkutil.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"

#define VIDMEM_SIZE                         (0x1000)

#define AltPathTolerance100X  200
#define RefPathTolerance100X  50

class MCLKSwitchTest: public RmTest
{
public:
    MCLKSwitchTest();
    virtual ~MCLKSwitchTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(L2Bypass, bool);    //!< Grab JS variable to bypass L2.
    RC SetSwitchFreqArray(JsArray *freqArr);
    RC GetSwitchFreqArray(JsArray *freqArr);

    enum TargetPath
    {
        TARGET_PATH_DEFAULT,
        TARGET_PATH_BYPASS,
        TARGET_PATH_VCO,
    };

private:
    RmtClockUtil m_ClkUtil;          // Instantiate Clk Util
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;
    LwRm::Handle m_vidMemHandle;     //!< Handle for video memory
    GpuSubdevice::L2Mode m_origL2Mode;
    FLOAT64 m_TimeoutMs;
    bool restoreOnExit;

    //! CLK Domains variables
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS m_getDomainsParams;
    LW2080_CTRL_CLK_GET_INFO_PARAMS m_getInfoParams;
    LW2080_CTRL_CLK_SET_INFO_PARAMS m_setInfoParams;
    LW2080_CTRL_CLK_MEASURE_FREQ_PARAMS m_clkCounterFreqParams;
    LW2080_CTRL_CLK_GET_CLOSEST_BYPASS_FREQ_PARAMS m_clkBypassInfoParams;
    LW2080_CTRL_CLK_INFO *m_pClkInfos;
    LW2080_CTRL_CLK_INFO *m_pSetClkInfos;
    UINT32 m_numClkInfos, m_targetFreq, m_presentFreq, m_restoreFreq;
    TargetPath m_targetPath;
    void *m_pMapVidAddr;            //!< For Mapping video memory

    //
    //! Setup and Test functions
    //
    RC SetupMCLKSwitchTest();
    RC UsePstateFrequencies();
    RC BypassL2();
    RC BasicMCLKSwitchTest();
    RC ProgramAndVerifyClockFreq();
    RC FillVidMem();
    RC VerifyVidMem();

    //
    // Command Line Arguments.
    //
    bool m_L2Bypass;
    vector<UINT32> m_SwitchFreqArray;
    vector<TargetPath> m_SwitchPathArray;
};

//! \brief ClockTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
MCLKSwitchTest::MCLKSwitchTest()
{
    SetName("MCLKSwitchTest");
    m_hBasicClient         = 0;
    m_hBasicSubDev         = 0;
    m_vidMemHandle         = 0;
    m_origL2Mode           = GpuSubdevice::L2_DEFAULT;
    m_TimeoutMs            = 0;
    m_getDomainsParams     = {};
    m_getInfoParams        = {};
    m_setInfoParams        = {};
    m_clkCounterFreqParams = {};
    m_clkBypassInfoParams  = {};
    m_pClkInfos            = nullptr;
    m_pSetClkInfos         = nullptr;
    m_numClkInfos          = 0;
    m_targetFreq           = 0;
    m_presentFreq          = 0;
    m_restoreFreq          = 0;
    m_targetPath           = TARGET_PATH_DEFAULT;
    m_pMapVidAddr          = nullptr;
    m_L2Bypass             = false;

    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        restoreOnExit = true;
    }
    else
    {
        restoreOnExit = false;
    }
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
MCLKSwitchTest::~MCLKSwitchTest()
{
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string MCLKSwitchTest::IsTestSupported()
{
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();

    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        //
        // RM now has support to skip the timeouts caused by this incomplete register modelling
        // on FMODEL. However this still only exercises the RM code and provides little coverage
        // of the actual switch so we are leaving it as unsupported.
        //
        // To run this test for debugging purposes (or as a sanity check before a fullchip run)
        // pass "-ignore_supported_flag" to rmtest.js.  This is much easier than hacking
        // this file to return RUN_RMTEST_TRUE, although that works too.
        //
        return "MCLK Switch Test is not supported for F-Model due to incomplete register modelling.";
    }

    if (!IsKEPLERorBetter(chipName))
    {
        return "This Clock Sanity Test is only supported for Kepler+.";
    }

    if (IsGP107orBetter(chipName))
    {
        return "Support for this test has been deprecated from GP107+ in favor of -testname UniversalClockTest -ctp mclk*";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! For basic test and Perf table test this setup function allocates the
//! required memory. Calls the sub functions to do appropriate allocations.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa SetupMCLKSwitchTest
//! \sa SetupPerfTest
//! \sa Run
//-----------------------------------------------------------------------------
RC MCLKSwitchTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(SetupMCLKSwitchTest());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicMCLKSwitchTest.
//! \return OK if the tests passed, specific RC if failed
//! \sa BasicMCLKSwitchTest
//! \sa PerfTableTest
//-----------------------------------------------------------------------------
RC MCLKSwitchTest::Run()
{
    RC rc;
    CHECK_RC(BasicMCLKSwitchTest());
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
RC MCLKSwitchTest::Cleanup()
{
    RC rc;
    LwRmPtr pLwRm;

    delete []m_pClkInfos;
    delete []m_pSetClkInfos;
    pLwRm->UnmapMemory(m_vidMemHandle, m_pMapVidAddr, 0, GetBoundGpuSubdevice());
    pLwRm->Free(m_vidMemHandle);

    if(m_L2Bypass)
    {
        // restore L2 bypass setting
        Printf(Tee::PriHigh, "MCLKSwitchTest: Restoring L2 bypass mode to what it was before this test.\n");
        CHECK_RC(GetBoundGpuSubdevice()->SetL2Mode(m_origL2Mode));
    }

    return rc;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief Get several MCLK P-State frequencies to switch to.
//!
//! This mode requires P12, P8, and P0 to be present in VBIOS.
//!
//! \return OK if sufficient P-States were found, specific RC if failed
//-----------------------------------------------------------------------------
RC MCLKSwitchTest::UsePstateFrequencies()
{
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT64 freqP12 = 0;
    UINT64 freqP8 = 0;
    UINT64 freqP0 = 0;

    if (pPerf->GetClockAtPState(12, Gpu::ClkM, &freqP12) != OK ||
        pPerf->GetClockAtPState(8, Gpu::ClkM, &freqP8) != OK ||
        pPerf->GetClockAtPState(0, Gpu::ClkM, &freqP0) != OK)
    {
        Printf(Tee::PriHigh, "MCLKSwitchTest: ERROR: Default mode requires a "
                             "P-States VBIOS with P12, P8, and P0 defined.\n");
        return RC::WRONG_BIOS;
    }

    //
    // VBIOS usually boots to P12 and RM will switch to P0 before this test starts.
    //

    // P0 => P8
    m_SwitchFreqArray.push_back(UNSIGNED_CAST(UINT32, freqP8));
    m_SwitchPathArray.push_back(TARGET_PATH_DEFAULT);

    // P8 => P0
    m_SwitchFreqArray.push_back(UNSIGNED_CAST(UINT32, freqP0));
    m_SwitchPathArray.push_back(TARGET_PATH_DEFAULT);

    // P0 => P12
    m_SwitchFreqArray.push_back(UNSIGNED_CAST(UINT32, freqP12));
    m_SwitchPathArray.push_back(TARGET_PATH_DEFAULT);

    return OK;
}

RC MCLKSwitchTest::BypassL2()
{
    RC rc;

    //
    // We need to clean/ilwalidate L2 to FB before bypassing it
    //
    Printf(Tee::PriHigh, "MCLKSwitchTest: Waiting for L2 to be flushed...\n");

    CHECK_RC(GetBoundGpuSubdevice()->IlwalidateL2Cache(GpuSubdevice::L2_ILWALIDATE_CLEAN));

    Printf(Tee::PriHigh, "MCLKSwitchTest: L2 flushed.\n");

    // bypass L2
    Printf(Tee::PriHigh, "MCLKSwitchTest: Bypassing L2.\n");
    CHECK_RC(GetBoundGpuSubdevice()->GetL2Mode(&m_origL2Mode));
    CHECK_RC(GetBoundGpuSubdevice()->SetL2Mode(GpuSubdevice::L2_DISABLED));

    return rc;
}

//! \brief SetupMCLKSwitchTest Function.for basic test setup
//!
//! For basic test this setup function allocates the required memory.
//! To allocate memory for basic test it needs clock domains
//! information so the setup function also gets and dumps the clock domains
//! information.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC MCLKSwitchTest::SetupMCLKSwitchTest()
{
    RC rc;
    LwRmPtr pLwRm;

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();

    m_numClkInfos = 0;
    m_TimeoutMs = GetTestConfiguration()->TimeoutMs();

    // Check if default P-State mode was requested.
    if (m_SwitchFreqArray.empty())
    {
        CHECK_RC(UsePstateFrequencies());
    }

    // Bypass L2 if l2_bypass flag is set
    if (m_L2Bypass)
    {
        CHECK_RC(BypassL2());
    }

    // Get all exposable clock domains.
    m_getDomainsParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_ALL;

    // get clk domains
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &m_getDomainsParams, sizeof (m_getDomainsParams)));

    // See if MCLK Domain is exposed or not
    if (LW2080_CTRL_CLK_DOMAIN_MCLK & (m_getDomainsParams.clkDomains))
    {
        m_numClkInfos = 1;
    }
    else
    {
        Printf(Tee::PriHigh, "ERROR::MCLK domain is not exposed. This test can not go any further.\n");
        rc = RC::ILWALID_ARGUMENT;
        return rc;
    }

    // Initilaize the ClkInfo Params.
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

    // Initilaize the SetClkInfo Params.
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

    return rc;
}

//! \brief Test gets current clocks state and programs it to a different path/freq.
//!
//! The test basically switches the clock frequency based on the command line
//! arguments provided by the caller.
//! -cmos : Switches MCLK to closest suitable Bypass path frequency.
//!         In case of Fermi we drive CMOS O/P only through Bypass path.
//!         Since we only support GDDR5 on fermi our bypass path frequencies
//!         are restricted between 127MHz(Memory is not stable with MCLK<127Mhz)&
//!         405 MHz(Any switch to CML path further if CMOS > 500 Mhz is glitchy).
//! -cml :  Switches MCLK to closest suitable VCO path frequency.
//!         In case of Fermi we drive CML O/P only through VCO path.
//! -freq : Switches MCLK to the requested freq as given through command line.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC MCLKSwitchTest::BasicMCLKSwitchTest()
{
    RC rc;
    LwRmPtr pLwRm;
    const char *domainName;
    const char *sourceName;
    UINT32 index = 0;
    UINT64 heapOffset = 0;

    Printf(Tee::PriHigh, "%s: Start\n", __FUNCTION__);

    m_targetFreq = 0;
    m_presentFreq = 0;
    m_targetPath = TARGET_PATH_DEFAULT;

    // fill in clock info state
    m_pClkInfos[0].clkDomain = LW2080_CTRL_CLK_DOMAIN_MCLK;

    // hook up clk info table
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    // get clocks
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    Printf(Tee::PriHigh, "%s: Present Clock State:\n", __FUNCTION__);

    domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[0].clkDomain);
    sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[0].clkSource);

    Printf(Tee::PriHigh, "Clock Domain is %s.\n", domainName);
    Printf(Tee::PriHigh, "Clock Source is %s.\n", sourceName);
    Printf(Tee::PriHigh, "Actual Frequency is  %d\n", (int)m_pClkInfos[0].actualFreq);
    Printf(Tee::PriHigh, "Target Frequency is  %d\n", (int)m_pClkInfos[0].targetFreq);
    Printf(Tee::PriHigh, "Flag Value is 0x%x\n", (UINT32)m_pClkInfos[0].flags);

    m_presentFreq = m_pClkInfos[0].actualFreq;

    // Get the Clock Counter Frequency for this clock.
    m_clkCounterFreqParams.clkDomain = m_pClkInfos[0].clkDomain;

    if (Platform::GetSimulationMode() != Platform::Fmodel)
    {
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
              LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
              &m_clkCounterFreqParams,
              sizeof (m_clkCounterFreqParams)));
        Printf(Tee::PriHigh, "Measured Clock Counter Frequency for %s Domain was %d\n", domainName, (int)m_clkCounterFreqParams.freqKHz );
    }
    Printf(Tee::PriHigh,"\n");

    // Do some verif before proceeding.

    // MCLK should not be running on default source(No Source!)
    if (m_pClkInfos[0].clkSource == LW2080_CTRL_CLK_SOURCE_DEFAULT)
    {
        Printf(Tee::PriHigh, "Invalid Clock Source\n");
        Printf(Tee::PriHigh, "ERROR: %s is running on No Source.\n",
            domainName);
        rc =  RC::LWRM_ERROR;
        return rc;
    }

    //
    // If MCLK is running on any source(even a PLL),it should
    // be on and running. Otherwise the clock Actual Frequency will be zero.
    //
    if ((m_pClkInfos[0].clkSource != LW2080_CTRL_CLK_SOURCE_DEFAULT) &&
        (m_pClkInfos[0].actualFreq == 0))
    {
        Printf(Tee::PriHigh, "Invalid Clock Source Configuration\n");
        Printf(Tee::PriHigh, "ERROR: The following clock source is not running -> %s.\n",
            sourceName);
        rc =  RC::LWRM_ERROR;
        return rc;
    }

    // MCLK should not be running on XTAL.
    if (m_pClkInfos[0].clkSource == LW2080_CTRL_CLK_SOURCE_XTAL)
    {
        Printf(Tee::PriHigh, "Invalid Clock Tree Configuration\n");
        Printf(Tee::PriHigh, "ERROR: %s is still running on XTAL.\n", domainName);
        rc =  RC::LWRM_ERROR;
        return rc;
    }

    // Store current MCLK Freq
    m_restoreFreq = m_pClkInfos[0].actualFreq;

    // General Init if the Set CLK Infos.
    m_pSetClkInfos[0].clkDomain = m_pClkInfos[0].clkDomain;
    m_pSetClkInfos[0].clkSource = m_pClkInfos[0].clkSource;
    m_pSetClkInfos[0].flags = 0;
    m_pSetClkInfos[0].actualFreq = m_pClkInfos[0].actualFreq;
    // Initialize Bypass Info params.
    m_clkBypassInfoParams.clkDomain = m_pClkInfos[0].clkDomain;

    if (restoreOnExit)
    {
        m_SwitchFreqArray.push_back(m_restoreFreq);
        m_SwitchPathArray.push_back(TARGET_PATH_DEFAULT);
    }

    //
    // Do one time allocation of Video Heap memeory.
    // Fill the memeory with some known data before switching the clocks
    // and write/read different data after every mclk switch to verify the sanity
    // of FB after the switch.
    //
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

    Printf(Tee::PriHigh, "%s: Starting the MClk switches\n", __FUNCTION__);
    for (index = 0; index < m_SwitchFreqArray.size(); ++index)
    {
        m_targetFreq = m_SwitchFreqArray[index];
        m_targetPath = m_SwitchPathArray[index];

        CHECK_RC(FillVidMem());

        CHECK_RC(ProgramAndVerifyClockFreq());
    }

    Printf(Tee::PriHigh, "%s: Done\n", __FUNCTION__);

    return rc;
}

// Helper functions for clk path test.

//! \brief ProgramAndVerifyClockFreq function.
//!
//! This function Programs and Verifies a set of clock frequencies.
//! In the function BasicMCLKSwitchTest for the clock domain
//! we prepare a set of values to be programmed in m_pSetClkInfos and
//! then call this function to program those clocks only, and then
//! verify the programmed frequency as per the target
//! Frequencies set in m_pSetClkInfos.
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC MCLKSwitchTest::ProgramAndVerifyClockFreq()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 rangePercent = 0;
    const char *domainName;
    const char *sourceName;
    bool bFinalPathBypass;

    m_pSetClkInfos[0].targetFreq = m_targetFreq;

    // Set forced path flags.
    m_pSetClkInfos[0].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
    m_pSetClkInfos[0].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                          _DISABLE, m_pSetClkInfos[0].flags);
    m_pSetClkInfos[0].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS,
                                          _DISABLE, m_pSetClkInfos[0].flags);
    switch (m_targetPath)
    {
        case TARGET_PATH_DEFAULT:
            Printf(Tee::PriHigh, "%s: Switching MCLK to targetFreq %d KHz\n",
                   __FUNCTION__, m_targetFreq);
            break;
        case TARGET_PATH_BYPASS:
            Printf(Tee::PriHigh, "%s: Forcing MCLK to use BYPASS path for targetFreq %d KHz\n",
                   __FUNCTION__, m_targetFreq);
            m_pSetClkInfos[0].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_BYPASS,
                                                  _ENABLE, m_pSetClkInfos[0].flags);
            break;
        case TARGET_PATH_VCO:
            Printf(Tee::PriHigh, "%s: Forcing MCLK to use VCO path for targetFreq %d KHz\n",
                   __FUNCTION__, m_targetFreq);
            m_pSetClkInfos[0].flags = FLD_SET_DRF(2080, _CTRL_CLK_INFO_FLAGS, _FORCE_PLL,
                                                   _ENABLE, m_pSetClkInfos[0].flags);
            break;
    }

    // hook up clk info table.
    m_setInfoParams.flags = LW2080_CTRL_CLK_SET_INFO_FLAGS_WHEN_IMMEDIATE;
    m_setInfoParams.clkInfoListSize = m_numClkInfos;
    m_setInfoParams.clkInfoList = (LwP64)m_pSetClkInfos;

    // Set new clocks frequency
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_SET_INFO,
        &m_setInfoParams,
        sizeof (m_setInfoParams)));

    // Verify the newly set clock frequencies.

    // hook up clk info table to get the back the recently set values
    m_getInfoParams.flags = 0;
    m_getInfoParams.clkInfoListSize = m_numClkInfos;
    m_getInfoParams.clkInfoList = (LwP64)m_pClkInfos;

    Printf(Tee::PriHigh, "\n");
    Printf(Tee::PriHigh, "%s: Reading the set frequency for MCLK\n", __FUNCTION__);

    // get New clock freq.
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &m_getInfoParams,
        sizeof (m_getInfoParams)));

    domainName = m_ClkUtil.GetClkDomainName(m_pClkInfos[0].clkDomain);
    sourceName = m_ClkUtil.GetClkSourceName(m_pClkInfos[0].clkSource);

    Printf(Tee::PriHigh, "%s: %s is Programmed to Frequency %d driven by %s\n", __FUNCTION__, domainName, (int)m_pClkInfos[0].actualFreq, sourceName);
    Printf(Tee::PriHigh, "%s: Expected Frequency for %s Domain was %d\n", __FUNCTION__, domainName, (int)m_pSetClkInfos[0].targetFreq );

     m_clkCounterFreqParams.clkDomain = m_pClkInfos[0].clkDomain;
    // Get the Clock Counter Frequency for this clock.
    if (Platform::GetSimulationMode() != Platform::Fmodel)
    {
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
              LW2080_CTRL_CMD_CLK_MEASURE_FREQ,
              &m_clkCounterFreqParams,
              sizeof (m_clkCounterFreqParams)));
        Printf(Tee::PriHigh, "%s: Measured Clock Counter Frequency for %s Domain was %d\n",
               __FUNCTION__, domainName, (int)m_clkCounterFreqParams.freqKHz );
    }

    if (m_pClkInfos[0].clkSource != LW2080_CTRL_CLK_SOURCE_MPLL &&
        m_pClkInfos[0].clkSource != LW2080_CTRL_CLK_SOURCE_REFMPLL)
    {
        // Allowing 2 % tolerance on Alt path
        rangePercent = AltPathTolerance100X;
        bFinalPathBypass = true;
    }
    else
    {
        // Allowing 0.5 % tolerance on Ref Path
        rangePercent = RefPathTolerance100X;
        bFinalPathBypass = false;
    }

    if (!m_ClkUtil.clkIsFreqWithinRange(m_pSetClkInfos[0].targetFreq,
            m_pClkInfos[0].actualFreq,
            rangePercent))
    {
        Printf(Tee::PriHigh,
            "%s: Setting new target frequency Failed\n",
             __FUNCTION__);
    }
    else
    {
        Printf(Tee::PriHigh,
            "%s: Setting new target frequency Successful\n", __FUNCTION__);
    }

    switch (m_targetPath)
    {
        case TARGET_PATH_DEFAULT:
            // Any path is okay.
            break;
        case TARGET_PATH_BYPASS:
            if (!bFinalPathBypass)
            {
                Printf(Tee::PriHigh, "%s: MCLK did not use forced BYPASS path for targetFreq %d KHz\n",
                       __FUNCTION__, m_targetFreq);
                return RC::LWRM_ERROR;
            }
            break;
        case TARGET_PATH_VCO:
            if (bFinalPathBypass)
            {
                Printf(Tee::PriHigh, "%s: MCLK did not use forced VCO path for targetFreq %d KHz\n",
                       __FUNCTION__, m_targetFreq);
                return RC::LWRM_ERROR;
            }
            break;
    }

    m_presentFreq = m_pClkInfos[0].actualFreq;

    Printf(Tee::PriHigh, "\n");

    // After switching MCLK, check whether the memory information is consistent.
    CHECK_RC(VerifyVidMem());

    return rc;
}

//! \brief AllocAndFillVidem
//!
//!This function is used to fill pre-allocated memory with some known data for verification
//!
//! \return OK if passed.
RC MCLKSwitchTest::FillVidMem()
{
    RC rc;
    UINT32 *pVidMapMemIterator;
    UINT32 data = 0xbabe1234;
    UINT32 memVal;

    //
    // Fill some known values to the allocated heap
    //
    pVidMapMemIterator = (UINT32 *)m_pMapVidAddr;

    Printf(Tee::PriHigh,
          "%s: %d: BEFORE SWITCH: Writing 0xbabe1234 to memory\n",
          __FUNCTION__, __LINE__);

    MEM_WR32(pVidMapMemIterator + 0, data);   // beginning of 4k page
    MEM_WR32(pVidMapMemIterator + 500, data);  // middle of 4k page
    MEM_WR32(pVidMapMemIterator + 1000, data); // near the end of 4k page

    if((memVal = MEM_RD32(pVidMapMemIterator + 0)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: BEFORE SWITCH: Expected 0xbabe1234 at offset 0 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 500)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: BEFORE SWITCH: Expected 0xbabe1234 at offset 500 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 1000)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: BEFORE SWITCH: Expected 0xbabe1234 at offset 1000 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    Printf(Tee::PriHigh,
          "%s: %d: BEFORE SWITCH: Memory writes/reads got expected values\n",
          __FUNCTION__, __LINE__);

    return rc;
}

//! \brief VerifyVidMem:
//!
//! This function verify the contents of Video memory which was populated by
//! function FillVidMem. It also writes to modify the memory, and
//! makes sure it works.
//!
//! \return OK if passed
RC MCLKSwitchTest::VerifyVidMem()
{
    RC rc;
    UINT32 *pVidMapMemIterator;
    UINT32 data = 0x5678dead;
    UINT32 prevData = 0xbabe1234;
    UINT32 memVal;

    pVidMapMemIterator = (UINT32 *)m_pMapVidAddr;

    Printf(Tee::PriHigh,
          "%s: %d: AFTER SWITCH: Reading memory\n",
          __FUNCTION__, __LINE__);

    if((memVal = MEM_RD32(pVidMapMemIterator + 0)) != prevData)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0xbabe1234 at offset 0 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 500)) != prevData)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0xbabe1234 at offset 500 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 1000)) != prevData)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0xbabe1234 at offset 1000 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }

    Printf(Tee::PriHigh,
          "%s: %d: AFTER SWITCH: Memory reads got expected values\n",
          __FUNCTION__, __LINE__);

    Printf(Tee::PriHigh,
          "%s: %d: AFTER SWITCH: Writing 0x5678dead to memory\n",
          __FUNCTION__, __LINE__);

    MEM_WR32(pVidMapMemIterator + 0, data);   // beginning of 4k page
    MEM_WR32(pVidMapMemIterator + 500, data);  // middle of 4k page
    MEM_WR32(pVidMapMemIterator + 1000, data); // near the end of 4k page

    if((memVal = MEM_RD32(pVidMapMemIterator + 0)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0x5678dead at offset 0 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 500)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0x5678dead at offset 500 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    if((memVal = MEM_RD32(pVidMapMemIterator + 1000)) != data)
    {
        Printf(Tee::PriHigh,
              "%s: %d: AFTER SWITCH: Expected 0x5678dead at offset 1000 but got 0x%x\n",
              __FUNCTION__, __LINE__, memVal);
        return RC::LWRM_ERROR;
    }
    Printf(Tee::PriHigh,
          "%s: %d: AFTER SWITCH: Memory writes/reads got expected values\n",
          __FUNCTION__, __LINE__);

    return rc;
}

//! \brief SetSwitchFreqArray function.
//!
//! This function is used to parse the list of frequencies provided
//! provided by the caller and saved in the vector m_SwitchFreqArray
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC MCLKSwitchTest::SetSwitchFreqArray(JsArray *freqArr)
{
    RC rc;
    JavaScriptPtr pJavaScript;
    string freqPath;
    UINT32 freq;
    TargetPath path;
    char *pPathSuffix;

    m_SwitchFreqArray.clear();
    m_SwitchPathArray.clear();

    for (size_t i = 0; i < freqArr->size(); ++i)
    {
        CHECK_RC(pJavaScript->FromJsval((*freqArr)[i], &freqPath));

        freq = strtol(freqPath.c_str(), &pPathSuffix, 10);

        if (freq <= 0)
        {
            Printf(Tee::PriHigh, "%s: Invalid frequency '%s' == %d KHz\n",
                   __FUNCTION__, freqPath.c_str(), freq);
            return RC::BAD_PARAMETER;
        }

        if (strcmp(pPathSuffix, "") == 0)
        {
            path = TARGET_PATH_DEFAULT;
        }
        else if (strcmp(pPathSuffix, "b") == 0)
        {
            path = TARGET_PATH_BYPASS;
        }
        else if (strcmp(pPathSuffix, "v") == 0)
        {
            path = TARGET_PATH_VCO;
        }
        else
        {
            Printf(Tee::PriHigh, "%s: Invalid path suffix '%s'\n",
                   __FUNCTION__, pPathSuffix);
            return RC::BAD_PARAMETER;
        }

        m_SwitchFreqArray.push_back(freq);
        m_SwitchPathArray.push_back(path);
    }

    return rc;
}

//! \brief GetSwitchFreqArray function.
//!
//! This function is used to return the array of frequencies to the caller.
//!
//! \return OK if the verification passed, specific RC if failed
//------------------------------------------------------------------------------
RC MCLKSwitchTest::GetSwitchFreqArray(JsArray *freqArr)
{
    JavaScriptPtr pJavaScript;
    return pJavaScript->ToJsArray(m_SwitchFreqArray, freqArr);
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ MCLKSwitchTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(MCLKSwitchTest, RmTest,
                 "Test used to verify the switching capability of MCLK");
CLASS_PROP_READWRITE_JSARRAY(MCLKSwitchTest, SwitchFreqArray, JsArray, "Array of Frequencies(in KHz) to switch MCLK to.");
CLASS_PROP_READWRITE(MCLKSwitchTest, L2Bypass, bool, "Bypass L2 during test");
