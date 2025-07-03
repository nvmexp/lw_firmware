/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_volt.cpp
//! \brief Runs different voltage tests.
//!

#include <sstream>
#include <string>
#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/memcheck.h"
#include "rmt_volt.h"
#include "ctrl/ctrl2080/ctrl2080volt.h"
using namespace std;

#define OPTION_TEST_EXHAUSTIVE 0
#define OPTION_TEST_INCLUDE    1
#define OPTION_TEST_EXCLUDE    2

#define VOLTGENERICTESTPARAMS_INDEX_ILWALID 0xFFFFFFFF

class VoltGenericTest: public RmTest
{
public:
    VoltGenericTest();
    virtual ~VoltGenericTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    // user supplied command-line option 0: exhaustive (default), 1: include, 2: exclude.
    SETGET_PROP(uiNumCmdLineOption, UINT32);

    // user supplied command-line set of tests to include or exclude.
    SETGET_PROP(sVoltgenerictestLwstomList, string);

    // user supplied command-line option to skip further tests, upon detecting a failure.
    SETGET_PROP(uiSkipOnFirstFailure, UINT32);

private:
    UINT32 m_uiNumCmdLineOption;
    string m_sVoltgenerictestLwstomList;
    UINT32 m_uiSkipOnFirstFailure;
    stringstream ssInputOptions;
    LW2080_CTRL_VOLT_GENERIC_TEST_PARAMS voltGenericTestParams;

    RC ParseCommandLineOptions();
};

//! \brief VoltGenericTest constructor.
//!
//! Initializes some basic variables as default values in the resepctive tests.
//! \sa Setup
//-----------------------------------------------------------------------------
VoltGenericTest::VoltGenericTest() :
    m_uiNumCmdLineOption(0),
    m_uiSkipOnFirstFailure(0)
{
    SetName("VoltGenericTest");
    memset(&voltGenericTestParams, 0, sizeof(voltGenericTestParams));
}

//! \brief VoltGenericTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
VoltGenericTest::~VoltGenericTest()
{
    // Nothing to do here.
}

//! \brief VoltGenericTest Cleanup.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC VoltGenericTest::Cleanup()
{
    RC rc;
    return rc;
}

//! \brief IsSupported Function.
//!
//! The test is supported on HW.
//! \sa Setup
//-----------------------------------------------------------------------------
string VoltGenericTest::IsTestSupported()
{
#if defined(DEBUG) || defined(DEVELOP) || defined(LW_VERIF_FEATURES)
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return "Volt Tests are supported only on HW";
    }

    return RUN_RMTEST_TRUE;
#else
    return "RM TEST Unsupported";
#endif
}

//! \brief Setup Function.
//!
//-----------------------------------------------------------------------------
RC VoltGenericTest::Setup()
{
    RC rc;
    // Ask our base class(es) to setup internal state based on JS settings.
    CHECK_RC(InitFromJs());
    CHECK_RC(ParseCommandLineOptions());

    return rc;
}

//! \brief Run Function.
//!
//! Run function is the primary function which exelwtes the test i.e. VoltGenericTest.
//! \return OK if the tests passed, specific RC if failed
//-----------------------------------------------------------------------------
RC VoltGenericTest::Run()
{
    RC rc = OK;
    LwRmPtr pLwRm;
    LwRm::Handle hSubdevice = pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice());
    bool bIterate = true;
    bool bExclude = false;
    INT32 uiVoltTest;

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
                while (ssInputOptions >> uiVoltTest)
                {
                    if (uiVoltTest == i)
                        bExclude = true;
                }
                ssInputOptions.flush();
                break;
        }

        if (bIterate && !bExclude)
        {
            voltGenericTestParams.index = i;
            CHECK_RC(pLwRm->Control(hSubdevice,
                         LW2080_CTRL_CMD_VOLT_VOLT_GENERIC_TEST,
                         (void*)&voltGenericTestParams,
                         sizeof(voltGenericTestParams)));

            if ((voltGenericTestParams.outStatus !=
                    LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED) ||
                (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE))
            {
               Printf(Tee::PriNormal, "Test#: %04d\t", i);
               Printf(Tee::PriNormal, "Status: %20s\t",
                  (voltGenericTestParams.outStatus ==
                      LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS ? "SUCCESS":
                   voltGenericTestParams.outStatus ==
                      LW2080_CTRL_VOLT_GENERIC_TEST_NOT_SUPPORTED ? "NOT_SUPPORTED":
                   voltGenericTestParams.outStatus ==
                      LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED ? "NOT_IMPLEMENTED":
                   voltGenericTestParams.outStatus ==
                      LW2080_CTRL_VOLT_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE ?
                      "INSUFFICIENT PRIVILEDGE" : "ERROR_GENERIC"));
               if (voltGenericTestParams.outStatus ==
                    LW2080_CTRL_VOLT_GENERIC_TEST_INSUFFICIENT_PRIVILEDGE)
               {
                   Printf(Tee::PriNormal, "%s \n",
                       "Failure Reason: insufficient priviledge to execute test. Please enable test mode");
               }
               else if (voltGenericTestParams.outStatus ==
                            LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC)
               {
                   Printf(Tee::PriNormal, "Test#: %04d\t", i);
                   if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(VMIN_CAP, VMIN_CAP_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Voltage Cap to IPC Vmin failed (ERROR: VMIN_CAP_FAILURE)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(VMIN_CAP_NEGATIVE, UNEXPECTED_VOLTAGE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected Voltage Value (ERROR: UNEXPECTED_VOLTAGE)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE, UNEXPECTED_VOLTAGE_ON_ZERO_HI_OFFSET))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected Voltage when HI_OFFSET is zero (ERROR: UNEXPECTED_VOLTAGE_ON_ZERO_HI_OFFSET)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE, UNEXPECTED_VOLTAGE_ON_HI_OFFSET))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected Voltage when some value in HI_OFFSET is set (ERROR: UNEXPECTED_VOLTAGE_ON_HI_OFFSET)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(DROOPY_ENGAGE, VMIN_CAP_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Voltage Cap to IPC Vmin failed (ERROR: VMIN_CAP_FAILURE)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, UNEXPECTED_ASSERTION))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected assertion of EDPP* thermal events. (ERROR: VMIN_CAP_FAILURE)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_VMIN_UNEXPECTED_ASSERTION))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected assertion of EDPP_VMIN thermal event. (ERROR: EDPP_VMIN_UNEXPECTED_ASSERTION)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_FONLY_UNEXPECTED_ASSERTION))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected assertion of EDPP_FONLY thermal event. (ERROR: EDPP_FONLY_UNEXPECTED_ASSERTION)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_VMIN_ASSERTION_FAILED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Assertion of EDPP_VMIN thermal event failed. (ERROR: EDPP_VMIN_ASSERTION_FAILED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_FONLY_ASSERTION_FAILED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Assertion of EDPP_FONLY thermal event failed. (ERROR: EDPP_FONLY_ASSERTION_FAILED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_VMIN_DEASSERTION_FAILED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Deassertion of EDPP_VMIN thermal event failed. (ERROR: EDPP_VMIN_DEASSERTION_FAILED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(EDPP_THERM_EVENTS, EDPP_FONLY_DEASSERTION_FAILED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Deassertion of EDPP_FONLY thermal event failed. (ERROR: EDPP_FONLY_DEASSERTION_FAILED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(THERM_MON_EDPP, INCREMENT_FAILURE))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Increment failure of thermal monitor over EDPP* thermal event. (ERROR: INCREMENT_FAILURE)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(THERM_MON_EDPP, UNKNOWN_EDPP_EVENT))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unknown EDPP event. (ERROR: UNKNOWN_EDPP_EVENT)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE, UNEXPECTED_VOLTAGE_ON_FSR_DISABLED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected voltage when fixed slew rate is disabled. (ERROR: UNEXPECTED_VOLTAGE_ON_FSR_DISABLED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE, UNEXPECTED_VOLTAGE_ON_FSR_ENABLED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected voltage when fixed slew rate is enabled. (ERROR: UNEXPECTED_VOLTAGE_ON_FSR_ENABLED)");
                   }
                   else if (voltGenericTestParams.outData == LW2080_CTRL_VOLT_TEST_STATUS(FIXED_SLEW_RATE, UNEXPECTED_VOLTAGE_ON_FSR_IPC_ENABLED))
                   {
                       Printf(Tee::PriNormal, "%s \n", "Failure Reason: Unexpected voltage when fixed slew rate and IPC is enabled. (ERROR: UNEXPECTED_VOLTAGE_ON_FSR_IPC_ENABLED)");
                   }
               }
               else
               {
                   Printf(Tee::PriNormal, "\n (ERROR: status = %d, data = %d",
                       voltGenericTestParams.outStatus, voltGenericTestParams.outData );
               }

               if ((UINT32)i < (sizeof(volttest_name_a)/sizeof(volttest_name_a[0])))
               {
                  Printf(Tee::PriNormal, "%s \n", volttest_name_a[i].c_str());
               }
               else
               {
                  Printf(Tee::PriNormal, "%s \n", "Test not implemented");
               }
            }

            if (voltGenericTestParams.outStatus != LW2080_CTRL_VOLT_GENERIC_TEST_SUCCESS)
            {
                rc = RC::LWRM_ERROR;
                break;
            }

            // End iteration when we hit LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED.
            // Skip remaining tests, if the user specifies so and if a failure has oclwred
            // to enable print TEST_NOT_IMPLEMENTED.
            bIterate = !((m_uiSkipOnFirstFailure &&
                         (voltGenericTestParams.outStatus ==
                             LW2080_CTRL_VOLT_GENERIC_TEST_ERROR_GENERIC)) ||
                         (voltGenericTestParams.outStatus ==
                             LW2080_CTRL_VOLT_GENERIC_TEST_NOT_IMPLEMENTED)) ||
                         (m_uiNumCmdLineOption == OPTION_TEST_INCLUDE);
        }
    }

    return rc;
}

//! \brief Setup test using cmd line arguments provided by user.
//!
//! Parses the command line arguments given and accordingly, sets up the test.
//!
//! \return OK if the options are valid, specific RC if failed
//-----------------------------------------------------------------------------
RC VoltGenericTest::ParseCommandLineOptions()
{
    RC rc;

    switch(m_uiNumCmdLineOption)
    {
        case OPTION_TEST_INCLUDE:
            ssInputOptions << m_sVoltgenerictestLwstomList;
            break;
        case OPTION_TEST_EXCLUDE:
            ssInputOptions << m_sVoltgenerictestLwstomList;
            break;
        default:
            break;
    }
    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript.
//!
//! Set up a JavaScript class that creates and owns a C++ VoltGenericTest.
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(VoltGenericTest, RmTest,
                      "Run a generic set of halified volt tests");
CLASS_PROP_READWRITE(VoltGenericTest, uiNumCmdLineOption, UINT32,
                      "User supplied command line option: Default = 0.");
CLASS_PROP_READWRITE(VoltGenericTest, sVoltgenerictestLwstomList, string,
                      "custom list of volt tests");
CLASS_PROP_READWRITE(VoltGenericTest, uiSkipOnFirstFailure, UINT32,
                      "User supplied command line option: Default = 0.");
