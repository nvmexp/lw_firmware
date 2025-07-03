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
 * @file    poll.c
 * @brief   Timer polling functions
 *
 * Helper functions for time tracking, callbacks etc.
 */

/* ------------------------ Module Includes -------------------------------- */
#include "drivers/drivers.h"

/* ------------------------ Functions -------------------------------------- */
LwBool csbPollFunc(void *pArgs)
{
    LW_RISCV_REG_POLL *pParams = (LW_RISCV_REG_POLL*)pArgs;
    return (((csbRead(pParams->addr) & pParams->mask) == pParams->val) ==
            pParams->pollForEq);
}
