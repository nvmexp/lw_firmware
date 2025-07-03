/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_volt.h
//! \brief List of different volt halified tests.
//!
#include <string>

/*
 ****** Important Notice ******
 * Please ensure that the test name identifiers below match exactly with the
 * test name identifiers in ctrl2080volt.h file.
 */
string volttest_name_a[] =
{
    "VOLTTEST_VMIN_CAP",
    "VOLTTEST_VMIN_CAP_NEGATIVE",
    "VOLTTEST_DROOPY_ENGAGE",
    "VOLTTEST_EDPP_THERM_EVENTS",
    "VOLTTEST_THERM_MON_EDPP",
    "VOLTTEST_FIXED_SLEW_RATE",
    "VOLTTEST_FORCE_VMIN",
    "VOLTTEST_VID_PWM_BOUND_FLOOR",
    "VOLTTEST_VID_PWM_BOUND_CEIL",
    "VOLTTEST_POSITIVE_CLVC_OFFSET",
    "VOLTTEST_NEGATIVE_CLVC_OFFSET",
    "VOLTTEST_DEFAULT"
};
