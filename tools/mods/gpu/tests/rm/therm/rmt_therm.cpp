/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_therm.cpp
//! \brief Runs different thermal halified tests
//!

#include <sstream>      // for stringstream
#include <iostream>
#include <string>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/memcheck.h"
#include "rmt_therm.h"
#include "ctrl/ctrl2080/ctrl2080thermal.h"
using namespace std;

#define OPTION_TEST_EXHAUSTIVE 0
#define OPTION_TEST_INCLUDE    1
#define OPTION_TEST_EXCLUDE    2

#define THERMGENERICTESTPARAMS_INDEX_ILWALID 0xFFFFFFFF

class ThermGenericTest: public RmTest
{
public:
    ThermGenericTest();
    virtual ~ThermGenericTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(uiNumCmdLineOption, UINT32); // user supplied command-line option 0: exhaustive (dafault), 1: include, 2: exclude
    SETGET_PROP(sThermgenerictestLwstomList, string); // user supplied command-line set of tests to include or exclude
    SETGET_PROP(uiSkipOnFirstFailure, UINT32); // user supplied command-line option to skip further tests, upon detecting a failure

private:
    UINT32 m_uiNumCmdLineOption = 0;
    string m_sThermgenerictestLwstomList;
    UINT32 m_uiSkipOnFirstFailure = 0;
    stringstream ssInputOptions;

#if defined(DEBUG) || defined(DEVELOP) || defined(LW_VERIF_FEATURES)
    LW2080_CTRL_THERMAL_GENERIC_TEST_PARAMS thermGenericTestParams = {};
#endif

    RC ParseCommandLineOptions();
};

//! \brief ThermGenericTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests
//! \sa Setup
//-----------------------------------------------------------------------------
ThermGenericTest::ThermGenericTest()
{
    SetName("ThermGenericTest");
}

//! \brief ThermGenericTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ThermGenericTest::~ThermGenericTest()
{
    // Nothing to do here.
}

//! \brief ThermGenericTest Cleanup
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ThermGenericTest::Cleanup()
{
    RC rc;
    return rc;
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW
//! \sa Setup
//-----------------------------------------------------------------------------
string ThermGenericTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return "Thermal Tests are supported only on HW";
    }

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Function.
//!
//-----------------------------------------------------------------------------
RC ThermGenericTest::Setup()
{
    RC rc;
    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());
    CHECK_RC(ParseCommandLineOptions());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the test i.e. ThermGenericTest.
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC ThermGenericTest::Run()
{
    RC rc = OK;

#if defined(DEBUG) || defined(DEVELOP) || defined(LW_VERIF_FEATURES)
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    bool bIterate = true;
    bool bExclude = false;
    INT32 uiThermTest;

    INT32 i = -1;
    while (bIterate)
    {
        switch(m_uiNumCmdLineOption)
        {
            case OPTION_TEST_EXHAUSTIVE:
                i++;
                break;
            case OPTION_TEST_INCLUDE:
                if (!(ssInputOptions >> i))
                {
                    bIterate = false;
                }
                break;
            case OPTION_TEST_EXCLUDE:
                i++;
                bExclude = false;
                ssInputOptions.clear();
                ssInputOptions.seekg(0,  std::ios::beg);
                while (ssInputOptions >> uiThermTest)
                {
                    if (uiThermTest == i)
                        bExclude = true;
                }
                ssInputOptions.flush();
                break;
        }

        if (bIterate && !bExclude)
        {
            thermGenericTestParams.index = i;
            CHECK_RC(pLwRm->Control(hSubdevice,
                         LW2080_CTRL_CMD_THERMAL_GENERIC_TEST,
                         (void*)&thermGenericTestParams,
                         sizeof(thermGenericTestParams)));

            if ((thermGenericTestParams.outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_IMPLEMENTED) ||
                (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE))
            {
               Printf(Tee::PriHigh, "Test#: %04d\t", i);
               Printf(Tee::PriHigh, "Status: %20s\t",
                  (thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS         ? "SUCCESS":
                   thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_SUPPORTED   ? "NOT_SUPPORTED":
                   thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_IMPLEMENTED ? "NOT_IMPLEMENTED":
                   thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE ? "INSUFFICIENT PRIVILEDGE":
                                                                                                          "ERROR_GENERIC"));
               if (thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE)
               {
                   Printf(Tee::PriHigh, "%s \n","Failure Reason: insufficient priviledge to execute test. Please enable test mode");
               }

               else if (thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC)
               {
                   Printf(Tee::PriHigh, "Test#: %04d\t", i);
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSENSE_VALUE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "%s \n","Failure Reason: Incorrect value of TSENSE (ERROR: TSENSE_VALUE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSENSE_OFFSET_VALUE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Incorrect value of TSENSE OFFSET (ERROR: TSENSE_OFFSET_VALUE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, MAX_VALUE_FAILURE))
                   {
                        Printf(Tee::PriHigh, "Failure Reason: Incorrect value of SENSOR_MAX (ERROR: TSENSE_VALUE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, CONSTANT_VALUE_FAILURE))
                   {
                        Printf(Tee::PriHigh, "Failure Reason: Incorrect value of SENSOR_CONSTANT (ERROR: CONSTANT_VALUE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, EVT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot trigger therm event (ERROR: EVT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SLOWDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot trigger therm slowdown (ERROR: SLOWDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_SLOWDOWN, SLOWDOWN_RESTORE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot restore therm slowdown (ERROR: SLOWDOWN_RESTORE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, PREV_EVT_NOT_CLEARED_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot restore BA slowdown (ERROR: PREV_EVT_CLEAR_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, EVT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot trigger BA event (ERROR: EVT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, HIGH_THRESHOLD_SLOWDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot trigger BA slowdown after violating high threshold (ERROR: HIGH_THRESHOLD_SLOWDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, WINDOW_SLOWDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot retain BA slowdown within window (ERROR: WINDOW_SLOWDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(BA_SLOWDOWN, SLOWDOWN_RESTORE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Cannot restore BA slowdown (ERROR: SLOWDOWN_RESTORE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_RAW_TEMP_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Incorrect value of RAW (ERROR: TSOSC_RAW_TEMP_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_RAW_TEMP_HS_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Incorrect value of RAW (ERROR: TSOSC_RAW_TEMP_HS_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_RAW_CODE_MIN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Incorrect value of RAW (ERROR: TSOSC_RAW_CODE_MIN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_RAW_CODE_MAX_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason: Incorrect value of RAW (ERROR: TSOSC_RAW_CODE_MAX_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_TEMP_FAKING_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_TEMP_FAKING_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_MAX_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_MAX_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_OFFSET_MAX_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_OFFSET_MAX_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_OFFSET_AVG_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_OFFSET_AVG_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, TSOSC_AVG_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_AVG_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SLOPEA_NULL_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SLOPEA_NULL_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, SYS_TSENSE_RAW_TEMP_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SYS_TSENSE_RAW_TEMP_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, GPC_TSENSE_RAW_TEMP_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: GPC_TSENSE_RAW_TEMP_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(INT_SENSORS, LWL_TSENSE_RAW_TEMP_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: LWL_TSENSE_RAW_TEMP_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT, TSOSC_MAX_FAKING_FAILURE ))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_MAX_FAKING_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT, TSENSE_FAKING_FAILURE ))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSENSE_FAKING_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DYNAMIC_HOTSPOT, DYNAMIC_HOTSPOT_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: DYNAMIC_HOTSPOT_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE, GLOBAL_OVERRIDE_PRIORITY_FAILURE ))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: GLOBAL_OVERRIDE_PRIORITY_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE, TSOSC_RAW_TEMP_FAILURE ))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSOSC_RAW_TEMP_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE, RAW_OVERRIDE_PRIORITY_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: RAW_OVERRIDE_PRIORITY_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(TSOSC_OVERRIDE, RAW_CODE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: RAW_CODE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(THERMAL_MONITORS, INCREMENT_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: THERMAL_MONITORS_INCREMENT_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT, TSENSE_RAW_MISMATCH_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TSENSE_RAW_MISMATCH_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT, SHUTDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SHUTDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE, SHUTDOWN_FAILURE_TSENSE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SHUTDOWN_FAILURE_TSENSE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE, SHUTDOWN_FAILURE_TSOSC_LOCAL))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SHUTDOWN_FAILURE_TSOSC_LOCAL)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(DEDICATED_OVERT_NEGATIVE, SHUTDOWN_FAILURE_TSOSC_GLOBAL))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SHUTDOWN_FAILURE_TSOSC_GLOBAL)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, PREV_EVT_NOT_CLEARED_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: PREV_EVT_NOT_CLEARED_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, EVT_LATCH_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVT_LATCH_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, EVT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, CLK_SLOWDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: CLK_SLOWDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, EVT_CLEAR_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVT_CLEAR_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(PEAK_SLOWDOWN, CLK_SLOWDOWN_CLEAR_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: CLK_SLOWDOWN_CLEAR_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, INTR_NOT_CLEARED_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: INTR_NOT_CLEARED_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, EVT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, CLK_SLOWDOWN_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: CLK_SLOWDOWN_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, EVT_CLEAR_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVT_CLEAR_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(HW_ADC_SLOWDOWN, CLK_SLOWDOWN_CLEAR_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: CLK_SLOWDOWN_CLEAR_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, HW_PIPELINE_TEMP_OVERRIDE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: HW_PIPELINE_TEMP_OVERRIDE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, UNDER_TEMP_EVENT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: UNDER_TEMP_EVENT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, OVER_TEMP_EVENT_TRIGGER_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: OVER_TEMP_EVENT_TRIGGER_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, EVENT_TRIGGER_ROLLBACK_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: EVENT_TRIGGER_ROLLBACK_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, SAVE_NON_SNAPSHOT_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SAVE_NON_SNAPSHOT_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, TEMP_OVERRIDE_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: TEMP_OVERRIDE_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, OVERRIDE_SNAPSHOT_FAILURE))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: OVERRIDE_SNAPSHOT_FAILURE)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, LWL_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: LWL_TSENSE_SNAPSHOT_MISMATCH)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, LWL_OFFSET_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: LWL_OFFSET_TSENSE_SNAPSHOT_MISMATCH)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, GPC_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: GPC_TSENSE_SNAPSHOT_MISMATCH)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, GPC_OFFSET_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: GPC_OFFSET_TSENSE_SNAPSHOT_MISMATCH)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, SYS_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SYS_TSENSE_SNAPSHOT_MISMATCH)");
                   }
                   if (thermGenericTestParams.outData == LW2080_CTRL_THERMAL_TEST_STATUS(GLOBAL_SNAPSHOT, SYS_OFFSET_TSENSE_SNAPSHOT_MISMATCH))
                   {
                       Printf(Tee::PriHigh, "Failure Reason:  (ERROR: SYS_OFFSET_TSENSE_SNAPSHOT_MISMATCH)");
                   }
               }
               else
               {
                   Printf(Tee::PriHigh, "\n (ERROR: status = %d, data = %d", thermGenericTestParams.outStatus, thermGenericTestParams.outData );
               }

               if ((unsigned int)i < (sizeof(thermtest_name_a)/sizeof(thermtest_name_a[0])))
               {
                  Printf(Tee::PriHigh, "%s \n", thermtest_name_a[i].c_str());
               }
               else
               {
                  Printf(Tee::PriHigh, "%s \n", "Test not implemented");
               }
            }

            if (thermGenericTestParams.outStatus != LW2080_CTRL_THERMAL_GENERIC_TEST_SUCCESS)
            {
                rc = RC::LWRM_ERROR;
                break;
            }

            // end iteration when we hit LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_IMPLEMENTED
            // skip remaining tests, if the user specifies so and if a failure has oclwred
            // to enable print TEST_NOT_IMPLEMENTED
            bIterate = !((m_uiSkipOnFirstFailure && (thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_ERROR_GENERIC)) ||
                        (thermGenericTestParams.outStatus == LW2080_CTRL_THERMAL_GENERIC_TEST_NOT_IMPLEMENTED)) ||
                        (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE);
        }
    }
#else
    Printf(Tee::PriHigh, "%s \n", "This test is skipped in release build");
#endif

    return rc;
}

//! \brief Setup test using cmd line arguments provided by user
//!
//! Parses the command line arguments given and accordingly, sets up the test
//!
//! \return OK if the options are valid, specific RC if failed
//-----------------------------------------------------------------------------
RC ThermGenericTest::ParseCommandLineOptions()
{
    RC rc;

    switch(m_uiNumCmdLineOption)
    {
        case OPTION_TEST_INCLUDE:
            ssInputOptions << m_sThermgenerictestLwstomList;
            break;
        case OPTION_TEST_EXCLUDE:
            ssInputOptions << m_sThermgenerictestLwstomList;
            break;
        default:
            break;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ThermGenericTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ThermGenericTest, RmTest,
                 "TODORMT - Run a generic set of halified thermal tests");
CLASS_PROP_READWRITE(ThermGenericTest, uiNumCmdLineOption, UINT32,
                      "User supplied command line option: Default = 0.");
CLASS_PROP_READWRITE(ThermGenericTest, sThermgenerictestLwstomList, string,
                      "custom list of thermal tests");
CLASS_PROP_READWRITE(ThermGenericTest, uiSkipOnFirstFailure, UINT32,
                      "User supplied command line option: Default = 0.");
