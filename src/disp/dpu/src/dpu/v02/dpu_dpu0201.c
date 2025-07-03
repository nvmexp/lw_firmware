/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_dpu0201.c
 * @brief  DPU 02.01 Hal Functions
 *
 * DPU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "dev_disp.h"
#include "dispflcn_regmacros.h"

/* ------------------------- Application Includes --------------------------- */
#include "dpu_objdpu.h"
#include "config/g_dpu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

#ifndef GSPLITE_RTOS
/*!
 * @brief Get the number of usable heads
 */
LwU32
dpuGetNumHeads_v02_01(void)
{
    LwU32 val;

    val = EXT_REG_RD32(LW_PDISP_CLK_REM_MISC_CONFIGA);
    return DRF_VAL(_PDISP, _CLK_REM_MISC_CONFIGA, _NUM_HEADS, val);
}
#endif
