/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntrad10x.c
 * @brief  PMU Hal Functions for generic AD102+ clock counter functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_objclk.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*
 * Special macro to define maximum number of supported clock counters.
 * {SYS, LWD, XBAR, HOST, PWR, HUB, UTILS, DISP, 4 VCLKs, MCLK,
 *  Bcast GPC, max of 12 unicast GPCs}
 */
#define CLK_CNTR_ARRAY_SIZE_MAX_AD10X              26

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Returns pointer to the static clock counter array.
 */
CLK_CNTR *
clkCntrArrayGet_AD10X()
{
    static CLK_CNTR ClkCntrArray_AD10X[CLK_CNTR_ARRAY_SIZE_MAX_AD10X] = {{ 0 }};
    return ClkCntrArray_AD10X;
}

/* ------------------------- Private Functions ------------------------------ */
