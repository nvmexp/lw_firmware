/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_icga100.c
 * @brief  Contains all Interrupt Control routines specific to SEC2 GA100.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_sec_csb.h"

#include "config/g_ic_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * @brief Trigger a SEMAPHORE_D TRAP interrupt
 */
void
icSetNonstallIntrCtrlGfid_GA100
(
    LwU32 gfid
)
{
    // The notification (nonstall) tree is at index 1
    LwU32 val = REG_RD32(CSB, LW_CSEC_FALCON_INTR_CTRL(1));
    val = FLD_SET_DRF_NUM(_CSEC, _FALCON_INTR_CTRL, _GFID, gfid, val);
    REG_WR32(CSB, LW_CSEC_FALCON_INTR_CTRL(1), val);
}
