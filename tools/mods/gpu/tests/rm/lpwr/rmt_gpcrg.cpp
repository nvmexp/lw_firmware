/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @Brief: This is an RM test for testing the GPC-RG Low Power Feature.
 *         of the GPU.
 */

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"
#include "core/include/memcheck.h"
#include "core/include/lwrm.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include <string> // Only use <> for built-in C++ stuff
#include "core/utility/errloggr.h"
#include "gpu/include/lpwrctrlimpl.h"
#include "gpu/tests/rm/utility/rmtestutils.h"
#include "gpu/utility/rmclkutil.h"

#include "class/cl902d.h"                            // for FERMI_TWOD_A

#include "ampere/ga102/dev_graphics_nobundle.h"      // for LW_PGRAPH_PRI_SCC_DEBUG reg-access.

FLOAT64 g_WaitTimeGpcRgMs = 0;
FLOAT64 g_TimeOutGpcRgMs  = 0;


// The test controller class.
class GpcRgTest : public RmTest
{
public:
    GpcRgTest();
    virtual ~GpcRgTest();

    virtual string         IsTestSupported();
    virtual RC             Setup();
    virtual RC             Run();
    virtual RC             Cleanup();
private:
//
// Private Member Functions.
//
    // GPU subdevice handle for calling RM API
    GpuSubdevice          *m_pSubdev;
    unsigned int           m_Platform;
    bool                   m_bIsGpcRgSupported;
    bool                   m_bIsGpcRgEnabled;
    LwU32                  m_InitialExitCount;
    LwU32                  m_InitialEntryCount; 
    LwU32                  m_ctrlId;
    Channel*               m_pCh;
    LwRm::Handle           m_hCh;
    LwRm::Handle           m_hObject;
    RmtClockUtil           m_ClkUtil;

    // Clocks specific variables
    LW2080_CTRL_CLK_INFO                    *m_pClkInfos;
    LW2080_CTRL_CLK_ADC_DEVICES_INFO_PARAMS  m_clkAdcInfoParams;
    LwU32                                    m_numClkDomains;
    LwU32                                    m_clkDomainMask;
    LwU32                                    m_clkAdcDevMask;

    RC                     GpcRgEngageFeature(LwU32 timeToWaitMs);
    RC                     GpcRgDisengageFeature(LwU32 timeToWaitMs);
    RC                     DumpClkState();
    RC                     DumpClockFrequencies();
    RC                     DumpAdcSensedVoltage();
    RC                     GetClkMeasuredFreq(LwU32 clkDomain, LwU32 *measuredFreqKHz);
};


//!
//! @brief: The constructor for the GpcRgTest class.
//!         The responsibility for the constructor is to assign a name to the test, 
//!         which will be used to access the test from the RM Test infra.
//!         The constructor also initializes some of the parameters used in the test.
//!
//------------------------------------------------------------------------------
GpcRgTest::GpcRgTest()
{
    SetName("GpcRgTest");
    m_Platform          = Platform::Hardware;
    m_ctrlId            = LW2080_CTRL_LPWR_SUB_FEATURE_ID_GR_RG;
    m_bIsGpcRgSupported = false;
    m_bIsGpcRgEnabled   = false;
    m_InitialEntryCount = 0;
    m_InitialExitCount  = 0;
    m_pCh               = NULL;
    m_hCh               = 0;
    m_hObject           = 0;
}

//! @brief: The destructor for the GpcRgTest is empty, as all the de-allocations 
//!         are already done in the cleanup()
//!
//------------------------------------------------------------------------------
GpcRgTest::~GpcRgTest()
{

}

//! @brief: Checks the support status of the feature on the chip.
//!
//!  1. checks the family of GPU (kepler, maxwell, pascal)
//!  2. checks the platform for current testing (Fmodel, silicon, amodel, RTL)
//!  3. checks the support status of the feature (GPCRG supported and enabled)
//!
//!   Returns: if the test can be run on the system or not.
//------------------------------------------------------------------------------
string GpcRgTest::IsTestSupported()
{
    RC rc;

    Printf(Tee::PriHigh,"*********************************** In IsTestSupported() ***********************************\n");

    m_pSubdev = GetBoundGpuSubdevice();

    unique_ptr<LpwrCtrlImpl> lpwrCtrlImpl(new LpwrCtrlImpl(m_pSubdev));

    // Detect if the GPU is from a supported family.
    Gpu::LwDeviceId chipName = m_pSubdev->DeviceId();
    if(chipName >= Gpu::GM107)
        Printf(Tee::PriHigh, "Gpu is supported.\n");
    else
        return "Unsupported GPU type.\n";

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

    rc = lpwrCtrlImpl->GetLpwrCtrlSupported(m_ctrlId, &m_bIsGpcRgSupported);
    if(m_bIsGpcRgSupported == false)
    {
        return "GPC-RG not supported on the GPU.\n";
    }

    return RUN_RMTEST_TRUE;
}

//! @brief: Setups the test environment for the feature.
//!
//!  1. Get timeout duration for the test, and waiting time based on the platform being tested.
//!  2. Setup the GR engine for sending methods to it.
//!
//!   Returns: OK, if the setup was a success.
//------------------------------------------------------------------------------

RC GpcRgTest::Setup()
{
    LW2080_CTRL_CLK_GET_DOMAINS_PARAMS      clkGetDomainParams;
    RC rc;
    LwRmPtr pLwRm;

    ErrorLogger::StartingTest();
    ErrorLogger::IgnoreErrorsForThisTest();

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    Printf(Tee::PriHigh,"*********************************** In Setup() ***********************************\n");

    // The Test will wait for g_WaitTimeMs time for allowing the feature to allow the gating of the feature
    g_TimeOutGpcRgMs = GetTestConfiguration()->TimeoutMs();

    switch(m_Platform)
    {
        case Platform::Fmodel:
            // increase the time out on fmodel and base
            // it as a multiple of TimeoutMs
            g_WaitTimeGpcRgMs = g_TimeOutGpcRgMs*10;
            break;
        case Platform::Hardware:
            g_WaitTimeGpcRgMs = g_TimeOutGpcRgMs;
            break;
        default:
            // increasing the time out on RTL. This time-out period is not tested yet.
            g_WaitTimeGpcRgMs = g_TimeOutGpcRgMs*100;
            break;
    };

    Printf(Tee::PriHigh, "The Time-Out period is :%f\n",g_WaitTimeGpcRgMs);

    // Override any JS settings for the channel type, this test requires
    // a GpFifoChannel channel.
    m_TestConfig.SetChannelType(TestConfiguration::GpFifoChannel);

    // Allocate channel and object for GR engine
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));
    CHECK_RC(pLwRm->Alloc(m_hCh, &m_hObject, FERMI_TWOD_A, NULL));

   // Get all the supported clock domains
    memset(&clkGetDomainParams, 0, sizeof(clkGetDomainParams));

    clkGetDomainParams.clkDomainsType = LW2080_CTRL_CLK_DOMAINS_TYPE_ALL;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_GET_DOMAINS,
        &clkGetDomainParams,
        sizeof(clkGetDomainParams)));

    // Updated the object state now with the data fetched
    m_clkDomainMask = clkGetDomainParams.clkDomains;

    // Number of frequency domains
    m_numClkDomains = m_clkDomainMask;
    NUMSETBITS_32(m_numClkDomains);

    Printf(Tee::PriHigh,"\n Supported clock domain mask = %u\n", m_clkDomainMask);

    // Allocate memory for all clkInfos
    m_pClkInfos = new LW2080_CTRL_CLK_INFO[m_numClkDomains * sizeof(LW2080_CTRL_CLK_INFO)];
    if (m_pClkInfos == NULL)
    {
        Printf(Tee::PriHigh,
               "%d: Clock Infos allocation failed, \n", __LINE__);
        rc = RC::CANNOT_ALLOCATE_MEMORY;
        return rc;
    }

    // Get static ADC info
    memset(&m_clkAdcInfoParams, 0, sizeof(m_clkAdcInfoParams));
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_INFO,
        &m_clkAdcInfoParams,
        sizeof(m_clkAdcInfoParams)));

    m_clkAdcDevMask = m_clkAdcInfoParams.super.objMask.super.pData[0];

    return rc;
}

//! @brief: run the actual test.
//!
//------------------------------------------------------------------------------
RC GpcRgTest::Run()
{
    RC rc = RC::OK;

    Printf(Tee::PriHigh,"*********************************** In Run() ************************************\n");

    CHECK_RC(GpcRgEngageFeature(g_WaitTimeGpcRgMs));

    // Dump clocks state while we're in GPC-RG
    CHECK_RC(DumpClkState());

    CHECK_RC(GpcRgDisengageFeature(g_WaitTimeGpcRgMs));

    return rc;
}

//! @brief: Engaging the feature
//!  1. Disable GPC-RG feature.
//!  2. Get initial entry and exit count for the feature.
//!  3. Enable GPC-RG feature.
//!  4. Sleep for some time to ensure that the pre-condition is met.
//!  5. Check for the Pre Condition before actual feature gets engaged.
//!  6. Check if feature is engaged.
//!
//!   Returns: OK, if the feature was engaged.
//-------------------------------------------------------------------------------------------
RC GpcRgTest::GpcRgEngageFeature(LwU32 timeToWaitMs)
{
    RC rc;

    LwU64 initialTimeMs         = 0;
    LwU64 finalTimeMs           = 0;
    vector<UINT32> idleMask(3, 0);
    UINT32 lwrrentEntryCount    = 0;
    bool bGpcRgLwrrentlyEngaged = false;
    bool bPreConditionMet       = false;

    unique_ptr<LpwrCtrlImpl> lpwrCtrlImpl(new LpwrCtrlImpl(m_pSubdev));

    // Step 1. Disable GPC-RG feature.
    CHECK_RC(lpwrCtrlImpl->SetLpwrCtrlEnabled(m_ctrlId, false));

    // Step 2. Get initial entry and exit count for the feature.
    CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlEntryCount(m_ctrlId, &m_InitialEntryCount));
    Printf(Tee::PriHigh, "Initial entry count = %d\n", m_InitialEntryCount);

    CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlExitCount(m_ctrlId, &m_InitialExitCount));
    Printf(Tee::PriHigh, "Initial exit count = %d\n", m_InitialExitCount);

    // Step 3. Enable GPC-RG feature.
    CHECK_RC(lpwrCtrlImpl->SetLpwrCtrlEnabled(m_ctrlId, true));

    initialTimeMs = finalTimeMs = Utility::GetSystemTimeInSeconds()*1000;

    // Poll on the feature to be enabled
    while(finalTimeMs - initialTimeMs <= timeToWaitMs)
    {
        CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlEnabled(m_ctrlId, &m_bIsGpcRgEnabled));
        if(m_bIsGpcRgEnabled == false)
        {
            finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
        }
        else
        {
            Printf(Tee::PriHigh, "\nGPC-RG enabled on the GPU.\n");
            break;
        }
    }

    if (finalTimeMs-initialTimeMs > timeToWaitMs)
    {
        Printf(Tee::PriHigh, "Timed-out while enabling GPC-RG\n");
        return RC::TIMEOUT_ERROR;
    }

    // Step 4. Sleep for some time to ensure that the pre-condition is met.    
    Printf(Tee::PriHigh, "\nGoing to sleep.\n");
    Tasker::Sleep(1000);

    finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;

    // Step 5. Check for the Pre Condition before actual feature gets engaged.
    CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlIdleMask(m_ctrlId, &idleMask));

    while (finalTimeMs - initialTimeMs <= timeToWaitMs)
    {
        CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlPreCondition(m_ctrlId, &bPreConditionMet, &idleMask));

        if(bPreConditionMet == false)
        {
            finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
        }
        else
        {
            Printf(Tee::PriHigh, "\nPre condition met to enter into GPC-RG.\n");
            break;
        }
    }

    if (finalTimeMs-initialTimeMs > timeToWaitMs)
    {
        Printf(Tee::PriHigh, "Timed-out while meeting GPC-RG pre-conditions.\n");
        return RC::TIMEOUT_ERROR;
    }

    // Step 6. Check if feature is engaged.
    while (finalTimeMs - initialTimeMs <= timeToWaitMs)
    {
        CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlEntryCount(m_ctrlId, &lwrrentEntryCount));
        CHECK_RC(lpwrCtrlImpl->GetFeatureEngageStatus(m_ctrlId, &bGpcRgLwrrentlyEngaged));

        if((lwrrentEntryCount - m_InitialEntryCount > 0) || bGpcRgLwrrentlyEngaged)
        {
            Printf(Tee::PriHigh, "\nGPC-RG is now engaged.\n");
            Printf(Tee::PriHigh, "Current entry count = %d\n", lwrrentEntryCount);
            break;
        }
        else
        {
            finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
        }
    }

    if (finalTimeMs-initialTimeMs > timeToWaitMs)
    {
        Printf(Tee::PriHigh, "Timed-out while engaging into GPC-RG\n");
        return RC::TIMEOUT_ERROR;
    }

    return rc;
}

//! @brief: Exiting the feature
//!  1. Do a register-read to wakeup the chiplet.
//!  2. Check if feature is disengaged by checking the exit count.
//!
//!   Returns: OK, if the feature was disengaged.
//-------------------------------------------------------------------------------------------
RC GpcRgTest::GpcRgDisengageFeature(LwU32 timeToWaitMs)
{
    RC rc;

    UINT32 lwrrentExitCount;
    LwS32  regRead               = 0;
    LwU64  initialTimeMs         = 0;
    LwU64  finalTimeMs           = 0;

    unique_ptr<LpwrCtrlImpl> lpwrCtrlImpl(new LpwrCtrlImpl(m_pSubdev));

    initialTimeMs = finalTimeMs = Utility::GetSystemTimeInSeconds()*1000;

    while(finalTimeMs - initialTimeMs <= timeToWaitMs)
    {
        // Step 1. Do a register-read to wakeup the chiplet.
        Printf(Tee::PriHigh,"\nNow waking chiplet using Register access...\n");
        regRead = m_pSubdev->RegRd32(LW_PGRAPH_PRI_SCC_DEBUG);
        Printf(Tee::PriHigh, "Following data was read from the Register: 0x%x\n", regRead);

        // Step 2. Check if feature is disengaged by checking the exit count.
        CHECK_RC(lpwrCtrlImpl->GetLpwrCtrlExitCount(m_ctrlId, &lwrrentExitCount));
        if(lwrrentExitCount > m_InitialExitCount)
        {
            Printf(Tee::PriHigh, "\nGPC-RG has now disengaged.\n");
            Printf(Tee::PriHigh, "Exit count after feature exit = %d\n", lwrrentExitCount);
            break;
        }
        else
        {
            finalTimeMs  = Utility::GetSystemTimeInSeconds()*1000;
        }
    }

    if (finalTimeMs-initialTimeMs > timeToWaitMs)
    {
        Printf(Tee::PriHigh, "Timed-out while disengaging from GPC-RG\n");
        return RC::TIMEOUT_ERROR;
    }

    return rc;

}

//! @brief: Interface to dump all the relevant clock state
//! 
//!   We do not expect GPC-RG to be disengaged with this call. This is more to
//!   ensure that the clocks/ADCs are correctly measured and reported even
//!   during GPC-RG.
//!
//!   Returns: OK, if clocks(includes ADCs) state was correctly fetched and printed
//-------------------------------------------------------------------------------------------
RC GpcRgTest::DumpClkState()
{
    RC rc;

    Printf(Tee::PriHigh,"\n\n\n****************** Dumping clocks/ADC state during GPC-RG *********************\n\n");

    rc = DumpClockFrequencies();
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "Clocks features not supported - skipping clock frequencies dump\n");
        rc = RC::OK;
    }
    CHECK_RC(rc);

    rc = DumpAdcSensedVoltage();
    if (rc == RC::LWRM_NOT_SUPPORTED)
    {
        Printf(Tee::PriLow, "AVFS not supported - skipping ADC dump\n");
        rc = RC::OK;
    }
    CHECK_RC(rc);

    Printf(Tee::PriHigh,"\n\n***********************************************************************************\n");

    return rc;
}

//! @brief: Interface to dump all ADC sensed voltage
//!
//!   Returns: OK, if ADC sensed voltage was correctly fetched and printed
//-------------------------------------------------------------------------------------------
RC GpcRgTest::DumpAdcSensedVoltage()
{
    RC rc;

    LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS clkAdcStatusParams;
    LwU8  numAdcs = 0;
    LwU32 adcMask;

    // Get ADC status information
    memset(&clkAdcStatusParams, 0, sizeof(clkAdcStatusParams));
    clkAdcStatusParams.super.objMask.super.pData[0] = m_clkAdcDevMask;
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_ADC_DEVICES_GET_STATUS,
        &clkAdcStatusParams,
        sizeof(clkAdcStatusParams)));

    Printf(Tee::PriHigh,"\n\n");

    // Iterate over all the supported ADCs to print relevant data
    adcMask = m_clkAdcDevMask;
    while (adcMask)
    {
        LwU8 index = adcMask;
        LOWESTBITIDX_32(index);

        Printf(Tee::PriHigh, "%8s:", m_ClkUtil.GetAdcName(m_clkAdcInfoParams.devices[index].id));
        Printf(Tee::PriHigh, "    Sensed Voltage - %12u uV,", clkAdcStatusParams.devices[index].correctedVoltageuV);
        Printf(Tee::PriHigh, "    Latest Sampled Code - %10u\n", clkAdcStatusParams.devices[index].sampledCode);

        numAdcs++;
        adcMask &= ~LOWESTBIT(adcMask);
    }
    MASSERT(numAdcs <= LW2080_CTRL_CLK_ADC_MAX_DEVICES);

    return rc;
}

//! @brief: Interface to dump actual and measured clock frequencies
//!
//!   Returns: OK, if the clock frequencies were correctly fetched and printed
//-------------------------------------------------------------------------------------------
RC GpcRgTest::DumpClockFrequencies()
{
    RC rc;

    LW2080_CTRL_CLK_GET_INFO_PARAMS clkGetInfoParams;
    LwU8                            clkDomIdx;
    LwU8                            arrIdx;
    LwU32                           clkDomMask;
    LwU32                           clkDomain;

    memset((void *)m_pClkInfos, 0, m_numClkDomains * sizeof (LW2080_CTRL_CLK_INFO));

    // Set up the input param struct
    clkGetInfoParams.flags              = 0;
    clkGetInfoParams.clkInfoListSize    = m_numClkDomains;
    clkGetInfoParams.clkInfoList        = (LwP64)m_pClkInfos;

    // Initialize each param object with the domain it represents
    arrIdx = 0;
    clkDomMask = m_clkDomainMask;
    while (clkDomMask)
    {
        clkDomain = LOWESTBIT(clkDomMask);
        m_pClkInfos[arrIdx].clkDomain = clkDomain;

        // Update the state for the next iteration
        clkDomMask &= ~clkDomain;
        arrIdx++;
    }
    MASSERT(arrIdx == m_numClkDomains);

    // Get the actual clock frequencies now
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_GET_INFO,
        &clkGetInfoParams,
        sizeof(clkGetInfoParams)));

    // Iterate over all the supported clock domains to print relevant data
    for (clkDomIdx = 0; clkDomIdx < m_numClkDomains; clkDomIdx++)
    {
        LwU32 actualFreqKHz;
        LwU32 measuredFreqKHz;

        clkDomain     = m_pClkInfos[clkDomIdx].clkDomain;
        actualFreqKHz = m_pClkInfos[clkDomIdx].actualFreq;

        // Get measured clock frequency
        CHECK_RC(GetClkMeasuredFreq(clkDomain, &measuredFreqKHz));

        Printf(Tee::PriHigh, "%12s: ", m_ClkUtil.GetClkDomainName(clkDomain));
        Printf(Tee::PriHigh, "    Actual Frequency - %8uKHz,", actualFreqKHz);
        Printf(Tee::PriHigh, "    Measured Frequency - %8uKHz\n", measuredFreqKHz);
    }

    return rc;
}

//! @brief: Get clock counter frequency for the requested clock domain.
//! 
//! \param[in]  clkDomain         Clock domain
//! \param[out] measuredFreq      Measured frequency in KHz
//!
//!   Returns: OK, if the clock counter frequency was correctly fetched
//-------------------------------------------------------------------------------------------
RC GpcRgTest::GetClkMeasuredFreq(LwU32 clkDomain, LwU32 *measuredFreqKHz)
{
    LW2080_CTRL_CLK_CNTR_MEASURE_AVG_FREQ_PARAMS clkCntrMeasureAvgFreqParams;
    RC rc;

    // Initialize frequency to zero
    *measuredFreqKHz = 0;

    // Reset the input parameters
    memset(&clkCntrMeasureAvgFreqParams, 0, sizeof(clkCntrMeasureAvgFreqParams));

    clkCntrMeasureAvgFreqParams.clkDomain   = clkDomain;
    clkCntrMeasureAvgFreqParams.b32BitCntr  = LW_FALSE;
    clkCntrMeasureAvgFreqParams.numParts    = 1;
    clkCntrMeasureAvgFreqParams.parts[0].partIdx =
        LW2080_CTRL_CLK_DOMAIN_PART_IDX_UNDEFINED;

    // Take first sample of clock counters
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ,
        &clkCntrMeasureAvgFreqParams,
        sizeof(clkCntrMeasureAvgFreqParams)));

    // Wait 1ms for re-sampling
    Utility::Delay(1000);

    // Take second sample of clock counters
    CHECK_RC(LwRmPtr()->ControlBySubdevice(
        m_pSubdev,
        LW2080_CTRL_CMD_CLK_CNTR_MEASURE_AVG_FREQ,
        &clkCntrMeasureAvgFreqParams,
        sizeof(clkCntrMeasureAvgFreqParams)));

    // We now have the frequency
    *measuredFreqKHz = clkCntrMeasureAvgFreqParams.parts[0].freqkHz;

    return rc;
}

//! @brief: Cleanup the test environment.
//!
//!
//------------------------------------------------------------------------------
RC GpcRgTest::Cleanup()
{

    Printf(Tee::PriHigh, "*********************************** In Cleanup() ********************************\n");

    LwRmPtr          pLwRm;

    pLwRm->Free(m_hObject);
    m_hObject = 0;
    m_hCh = 0;
    m_TestConfig.FreeChannel(m_pCh);

    m_bIsGpcRgSupported = false;
    m_bIsGpcRgEnabled   = false;
    m_InitialEntryCount = 0;
    m_InitialExitCount  = 0;
    m_ctrlId      = 0xFFFFFFFF;

    delete[] m_pClkInfos;

    return RC::OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(GpcRgTest, RmTest,
    "RM Test for verifying the GPC-RG feature.");

