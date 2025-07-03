/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_therm.h
//! \brief List of different thermal halified tests
//!
#include <string>

/*
 ****** Important Notice ******
 * Please ensure that the test name identifiers below, match exactly with the test name identifiers in ctrl2080thermal.h file
 */
string thermtest_name_a[] =
{
    "THERMTEST_VERIFY_SMBPBI",
    "THERMTEST_INT_SENSORS",
    "THERMTEST_THERMAL_SLOWDOWN",
    "THERMTEST_BA_SLOWDOWN",
    "THERMTEST_DYNAMIC_HOTSPOT",
    "THERMTEST_TEMP_OVERRIDE",
    "THERMTEST_THERMAL_MONITORS",
    "THERMTEST_DEDICATED_OVERT",
    "THERMTEST_DEDICATED_OVERT_NEGATIVE",
    "THERMTEST_PEAK_SLOWDOWN",
    "THERMTEST_HW_ADC_SLOWDOWN",
    "THERMTEST_GLOBAL_SNAPSHOT",
    "THERMTEST_GPC_BCAST_ACCESS",
    "THERMTEST_DEFAULT"
};
