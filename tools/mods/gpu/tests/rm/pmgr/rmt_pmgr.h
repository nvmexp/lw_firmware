/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_pmgr.h
//! \brief List of different pmgr halified tests.
//!
#include <string>

/*
 ****** Important Notice ******
 * Please ensure that the test name identifiers below match exactly with the
 * test name identifiers in ctrl2080pmgr.h file.
 */
string pmgrtest_name_a[] =
{
    "PMGRTEST_ADC_INIT",
    "PMGRTEST_ADC_CHECK",
    "PMGRTEST_HI_OFFSET_MAX_CHECK",
    "PMGRTEST_IPC_PARAMS_CHECK",
    "PMGRTEST_BEACON_CHECK",
    "PMGRTEST_OFFSET_CHECK",
};
