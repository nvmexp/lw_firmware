/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_SSP_H
#define PMU_SSP_H

/*!
 * @file pmu_ssp.h
 *
 * PMU stack smashing protection (SSP) header.
 */

/* ------------------------ System includes --------------------------------- */
#include "lwuproc.h"

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
void __stack_chk_fail(void)
    GCC_ATTRIB_NO_STACK_PROTECT()
    GCC_ATTRIB_SECTION("imem_resident", "__stack_chk_fail");

#endif  // PMU_SSP_H
