/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2010-2011,2013,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_gpiosanity.cpp
//! \RM-Test to perform a sanity check.
//! This test is primarily written to verify the gpio crossbar behaviour and
//! gpio properties using a pwm-tach crossbar loop.
//!
//! This Test performs the following :-
//! a) Checks if crossbar is supported.
//! b) Fetches DCB entries.
//! c) For entries with Normal function; programs PWM on the corresponding gpio.
//! d) Programs the TACH on the same pin as input function.
//! e) Verifies the change in pwm period can be noticed through tach.
//!    Note that a change in dutycycle wont change the tach edgecount.
//!

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "ctrl/ctrl2080.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "rmpmucmdif.h"
#include "core/include/memcheck.h"

// 27 MHz/2 = 13.5 MHz
#define EFFECTIVE_PMGR_CLK      (54000000)
#define MAX_PIN_COUNT           64
#define DEFAULT_SLEEP_TIME      3000000 // 3 Sec

#ifndef INCLUDED_DTIUTILS_H
#include "gpu/tests/rm/utility/dtiutils.h"
#endif

class GpioSanityTest: public RmTest
{
public:
    GpioSanityTest();
    virtual ~GpioSanityTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_hBasicClient;
    LwRm::Handle m_hBasicSubDev;

    //! control call variables
    LW2080_CTRL_GPIO_GET_CAPABILITIES_PARAMS    m_capParams;
    LW2080_CTRL_GPIO_READ_DCB_ENTRIES_PARAMS    m_readDcbParams;
    UINT32                                      m_numDcbEntries;
    LW2080_CTRL_GPIO_DCB_ENTRY                  m_dcbEntries[MAX_PIN_COUNT];
    UINT32                                      m_delay;
    //
    //! Setup and Test functions
    //
    RC SetupGpioSanityTest();
    RC BasicSanityGpioTest();

};

//! \brief GpioTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
GpioSanityTest::GpioSanityTest()
{
    SetName("GpioSanityTest");
    m_hBasicClient = 0;
    m_hBasicSubDev = 0;
    m_capParams = {};
    m_readDcbParams = {};
    m_numDcbEntries = 0;
    memset(m_dcbEntries, 0x0, MAX_PIN_COUNT * sizeof(LW2080_CTRL_GPIO_DCB_ENTRY));
    m_delay = 0;
}

//! \brief ClockTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
GpioSanityTest::~GpioSanityTest()
{}

//! \brief IsTestSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string GpioSanityTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() == Platform::Fmodel)
    {
        return "Gpio Sanity Test is not supported for F-Model due to incomplete register modelling.";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//! For basic test and Perf table test this setup function allocates the
//! required memory. Calls the sub functions to do appropriate allocations.
//!
//-----------------------------------------------------------------------------
RC GpioSanityTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(SetupGpioSanityTest());

    return OK;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the primary sub-test i.e.
//! BasicSanityGpioTest.
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC GpioSanityTest::Run()
{
    RC rc;
    CHECK_RC(BasicSanityGpioTest());
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
RC GpioSanityTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// Private Member Functions
//-----------------------------------------------------------------------------

//! \brief SetupGpioSanityTest Function.for basic test setup
//!
//! Does some sanity check whether gpio crossbar is supported.
//!
//! \return OK if the allocations success, specific RC if failed
//! \sa Setup
//-----------------------------------------------------------------------------
RC GpioSanityTest::SetupGpioSanityTest()
{
    RC rc;
    LwRmPtr pLwRm;

    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    m_hBasicClient = pLwRm->GetClientHandle();

    m_delay = DEFAULT_SLEEP_TIME;

    // Get the hw capability
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_GPIO_GET_CAPABILITIES,
                            &m_capParams, sizeof (m_capParams)));

    Printf(Tee::PriHigh, " Gpio HW capabilities: \n");
    Printf(Tee::PriHigh, " Num of gpio pins: %d\n", m_capParams.numGpioPins);
    Printf(Tee::PriHigh, " Num of DCB entries: %d\n", m_capParams.numDcbEntries);

    // Now fetch the entries
    m_readDcbParams.dcbEntries = LW_PTR_TO_LwP64(m_dcbEntries);
    m_readDcbParams.pinMaskIn = (LwU64)((LwU64)1 << m_capParams.numGpioPins) - 1;
    m_readDcbParams.size = m_capParams.numDcbEntries;
    m_readDcbParams.pinMaskOut = 0;
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                            LW2080_CTRL_CMD_GPIO_READ_DCB_ENTRIES,
                            &m_readDcbParams, sizeof (m_readDcbParams)));

    // we got the entries. now display them
    Printf(Tee::PriHigh, " Fetching DCB entries: \n");
    Printf(Tee::PriHigh, " Pin Func OffData OffEnable OnData OnEnable PWM Mode OutEnum InEnum Init\n");
    for (UINT32 i = 0; i < m_readDcbParams.numRead; i++)
    {
        LW2080_CTRL_GPIO_DCB_ENTRY * e = &(m_dcbEntries[i]);

        Printf(Tee::PriHigh, "%d %d %d %d %d %d %d %d %d %d %d\n",
               e->GpioPin ,e->GpioFunction, e->OffData, e->OffEnable, e->OnData, e->OnEnable,
               e->PWM, e->Mode, e->OutputHwEnum, e->InputHwEnum, e->Init);
    }
    return rc;
}

//! \brief
//! The test basically does a Sanity test on gpio pin by programming tach and pwm on the
//! same pin and verifying them. Goal is to verify crossbar and pin logic.
//!
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC GpioSanityTest::BasicSanityGpioTest()
{
    RC rc;
    UINT32 i;
    LwRmPtr pLwRm;
    RC statusRc = RC::LWRM_ERROR;
    LW2080_CTRL_GPIO_DCB_ENTRY * pTestEntry = NULL;
    LW2080_CTRL_GPIO_DCB_ENTRY * pTachEntry = NULL;

    Printf(Tee::PriHigh, " In BasicSanityGpioTest\n");

    // scan through the list of dcb entries to find out if there is a skip entry.
    for (i=0; i<m_readDcbParams.numRead; i++)
    {
        if ((((m_dcbEntries[i].OutputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_NORMAL) &&
              (m_dcbEntries[i].InputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_IN_ENUM_UNASSIGNED)) ||
             (m_dcbEntries[i].GpioFunction == LW2080_CTRL_GPIO_DCB_ENTRY_GPIO_NOT_VALID)) &&
            (m_dcbEntries[i].GpioPin < m_capParams.numGpioPins))
        {
            pTestEntry = &m_dcbEntries[i];
            break;
        }
    }

    if (!pTestEntry)
    {
        Printf(Tee::PriHigh, "Could not find a test entry with no functions assigned.Test cant proceed.\n");
        Printf(Tee::PriHigh, "This does NOT mean that gpio sanity failed.\n");
        Printf(Tee::PriHigh, "Please use a vbios with atleast one skip entry\n");
        Printf(Tee::PriHigh, "corresponding to an output GPIO.\n");
        return RC::LWRM_ERROR;
    }

    // scan through the list of dcb entries to find out the one with tach programmed.
    // If found we restore it later.
    for (i=0; i<m_readDcbParams.numRead; i++)
    {
        if ((m_dcbEntries[i].InputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_IN_ENUM_TACH) &&
            (m_dcbEntries[i].GpioPin < m_capParams.numGpioPins))
        {
            pTachEntry = &m_dcbEntries[i];
            break;
        }
    }

    // default with pwm_ouput
    UINT32 testFunction = LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_PWM_OUTPUT;
    bool bFoundPwm = false;
    bool bFoundFan = false;

    // scan through the list of dcb entries to find out a fan/pwm entry
    for (i=0; i<m_readDcbParams.numRead; i++)
    {
        if (((m_dcbEntries[i].OutputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_FAN_ALERT) ||
             (m_dcbEntries[i].OutputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_PWM_OUTPUT)) &&
            (m_dcbEntries[i].GpioPin < m_capParams.numGpioPins))
        {
            if (m_dcbEntries[i].OutputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_FAN_ALERT)
            {
                bFoundFan = true;
            }
            else
            {
                bFoundPwm = true;
            }
        }
    }

    // If both the pwm units are in use, use FAN_ALERT pwm unit forcefully
    if (bFoundFan && bFoundPwm)
    {
        Printf(Tee::PriHigh, "Both PWM units are in use. Using Fan Alert pwm for test.\n");
        testFunction = LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_FAN_ALERT;
    }
    else
    {
        // if PWM_OUTPUT was found; use FAN for test.
        testFunction = bFoundPwm ? LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_FAN_ALERT :
                                   LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_PWM_OUTPUT;
    }

    // if no pwm was found in use; testFunction will anyways default to PWM_OUTPUT
    LW2080_CTRL_GPIO_WRITE_DCB_ENTRY_PARAMS writeDcbEntryParams;
    LW2080_CTRL_PMGR_READ_TACH_PARAMS readTachParams;

    memset(&writeDcbEntryParams, 0x0, sizeof(LW2080_CTRL_GPIO_WRITE_DCB_ENTRY_PARAMS));

    // Run the test through the list of dcb entries until it passes.
    // The test cannot run on input GPIOS, it is expected to fail.
    // Unfortunately there is no way to find in an unassigned GPIO is input or output.
    for (i=0; i<m_readDcbParams.numRead; i++)
    {
        if ((((m_dcbEntries[i].OutputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_OUT_ENUM_NORMAL) &&
              (m_dcbEntries[i].InputHwEnum == LW2080_CTRL_GPIO_DCB_ENTRY_IN_ENUM_UNASSIGNED)) ||
             (m_dcbEntries[i].GpioFunction == LW2080_CTRL_GPIO_DCB_ENTRY_GPIO_NOT_VALID)) &&
            (m_dcbEntries[i].GpioPin < m_capParams.numGpioPins))
        {
            pTestEntry = &m_dcbEntries[i];

            // Create a test dcb entry. This will override the testEntry
            LW2080_CTRL_GPIO_DCB_ENTRY * pDcb = &writeDcbEntryParams.dcbEntry;
            pDcb->GpioFunction = LW2080_CTRL_GPIO_DCB_ENTRY_DEBUG_FUNCTION;
            pDcb->GpioPin = pTestEntry->GpioPin;
            pDcb->Init = LW2080_CTRL_GPIO_DCB_ENTRY_INIT_YES;
            pDcb->PWM = LW2080_CTRL_GPIO_DCB_ENTRY_PWM_YES;
            pDcb->InputHwEnum = LW2080_CTRL_GPIO_DCB_ENTRY_IN_ENUM_TACH;
            pDcb->OutputHwEnum = testFunction;
            pDcb->Mode = LW2080_CTRL_GPIO_DCB_ENTRY_MODE_ALT; // this is really a no-op
            pDcb->OnEnable = LW2080_CTRL_GPIO_DCB_ENTRY_ON_ENABLE_YES;
            pDcb->OffEnable = LW2080_CTRL_GPIO_DCB_ENTRY_OFF_ENABLE_YES;

            // Now we have the testpin and the test function. go ahead and override the DCB entry.
            writeDcbEntryParams.commitToHw = LW_TRUE;
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_GPIO_WRITE_DCB_ENTRY,
                                    &writeDcbEntryParams, sizeof (writeDcbEntryParams)));

            // We now need to program pwm. set time period to 0.01 seconds or 100hz
            UINT32 pwmFreq = 100;  // Hz
            UINT32 dutyCycle = 25; // %

            LW2080_CTRL_PMGR_PROGRAM_PWM_PARAMS programPwmParams;
            programPwmParams.flags = LW2080_CTRL_PMGR_PROGRAM_PWM_PARAMS_FLAGS_CONTROL_ENABLE_ON;
            programPwmParams.gpioPin = pTestEntry->GpioPin;
            programPwmParams.period = EFFECTIVE_PMGR_CLK/pwmFreq; // = 0x20F58. PMGR_CLOCK_FREQ_HZ /FAN_FREQUENCY_HZ  = 13500000/100

            // lets keep a 25% dutycycle. wont matter for tach though
            programPwmParams.dutyCycle = (programPwmParams.period*dutyCycle)/100;

            Printf(Tee::PriHigh, "PWM freq (Hz): %d\n", pwmFreq);
            Printf(Tee::PriHigh, "PWM timeperiod wrt clk : %d\n", programPwmParams.period);
            Printf(Tee::PriHigh, "PWM dutycycle percent : %d\n", dutyCycle);

            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_PMGR_PROGRAM_PWM,
                                    &programPwmParams, sizeof (programPwmParams)));

            // setup the tach now. Though should already have been init-ed much before when we overrode the DCB
            readTachParams.bSetup = LW_TRUE;
            readTachParams.gpioPin = pTestEntry->GpioPin;
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_PMGR_READ_TACH,
                                    &readTachParams, sizeof (readTachParams)));

            //
            // wait for some time for pwm to settle down and since tach base is 1 Hz; we should atleast provide
            // a time > 2*tach sampling period (2sec) for tach to sample the pwm correctly.
            //
            Printf(Tee::PriHigh, "Waiting(~3 simulation secs) for pwm to settle and tach to sample at 1 Hz\n");
            if ((GetBoundGpuSubdevice()->IsEmulation()) && (Platform::GetSimulationMode() == Platform::Hardware))
            {
                m_delay = 3;
                GpuUtility::SleepOnPTimer(m_delay, GetBoundGpuSubdevice());
            }
            else
            {
                Utility::Delay(m_delay);
            }
            readTachParams.bSetup = LW_FALSE;
            readTachParams.gpioPin = pTestEntry->GpioPin;
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_PMGR_READ_TACH,
                                    &readTachParams, sizeof (readTachParams)));

            UINT32 lowLimit = 90 * pwmFreq / 100;
            UINT32 highLimit = 110 * pwmFreq / 100;
            Printf(Tee::PriHigh, "Edgecount : %d\n", readTachParams.edgesPerSecond);
            if (readTachParams.edgesPerSecond >= lowLimit &&
                readTachParams.edgesPerSecond <= highLimit )
            {
                Printf(Tee::PriHigh, "Edge count within tolerance of 10 percent.\n");
                statusRc = OK;
            }
            else if (readTachParams.edgesPerSecond == 0)
            {
                statusRc = RC::LWRM_MORE_PROCESSING_REQUIRED;
                Printf(Tee::PriHigh, "Edge count did not update.\n");
                Printf(Tee::PriHigh,"..... TRYING NEXT GPIO\n");
            }
            else
            {
                statusRc = RC::LWRM_ERROR;
                Printf(Tee::PriHigh, "Edge count outside tolerance of 10 percent.\n");
            }

            // we are done with this GPIO ... restore back the overriden DCB entry
            writeDcbEntryParams.commitToHw = LW_TRUE;
            writeDcbEntryParams.dcbEntry = *pTestEntry;
            CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                    LW2080_CTRL_CMD_GPIO_WRITE_DCB_ENTRY,
                                    &writeDcbEntryParams, sizeof (writeDcbEntryParams)));
            if ((statusRc == OK) || (statusRc == RC::LWRM_ERROR))
            {
                break;
            }
        }
    }

    if (statusRc != OK)
    {
        Printf(Tee::PriHigh,"..... Test FAILED\n");
        statusRc = RC::LWRM_ERROR;
    }

    // restore back the overriden DCB entry which had tach.
    if (pTachEntry)
    {
        writeDcbEntryParams.commitToHw = LW_TRUE;
        writeDcbEntryParams.dcbEntry = *pTachEntry;
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_GPIO_WRITE_DCB_ENTRY,
                                &writeDcbEntryParams, sizeof (writeDcbEntryParams)));

        // setup tach again
        readTachParams.bSetup = LW_TRUE;
        readTachParams.gpioPin = pTachEntry->GpioPin;
        CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                                LW2080_CTRL_CMD_PMGR_READ_TACH,
                                &readTachParams, sizeof (readTachParams)));

    }

    Printf(Tee::PriHigh, " BasicSanityGpioTest Done  %d\n", __LINE__);
    return statusRc;

}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ GpioSanityTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(GpioSanityTest, RmTest,
                 "TODORMT - Use a pwm-tach gpio crossbar loop to verify crossbar sanity and pin properties.");

