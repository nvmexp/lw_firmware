/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_SUITES_H
#define PLM_WPR_SUITES_H

/*!
 * @file        suites.h
 * @brief       Test suites.
 */

// Standard includes.
#include <stdbool.h>


/*
 * @brief Meta test-cases for setting up and verifying the test environment.
 */
void suiteSetup(void);

/*
 * @brief Meta test-cases for cleaning up the test environment.
 */
void suiteTeardown(void);

///////////////////////////////////////////////////////////////////////////////

//
// A list of all available test-suites. Used by the test harness to automate
// exelwtion.
//
// Note that the setup and teardown suites are intentionally omitted here as
// they require special handling and so are called directly by the test
// harness instead.
//
extern void(* const g_suiteList[])(void);

#endif // PLM_WPR_SUITES_H
