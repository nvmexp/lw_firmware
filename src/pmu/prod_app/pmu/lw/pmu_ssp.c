/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_ssp.c
 *
 * PMU stack smashing protection (SSP) implementation.
 */

/* ------------------------ System includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_ssp.h"

/* ------------------------ Global Variables -------------------------------- */
// Constant value present in current (as of 2018-07-19) falcon-gcc libssp.
void * __stack_chk_guard = (void *)0x00002944;

/* ------------------------ Public Functions -------------------------------- */
void __stack_chk_fail(void)
{
    PMU_HALT();
}
