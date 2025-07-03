/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   sec2_icgh100.c
 * @brief  Contains all Interrupt Control routines specific to SEC2 GH100.
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "dev_sec_csb.h"
#include "dev_ctrl.h"

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
icSetNonstallIntrCtrlGfid_GH100
(
    LwU32 gfid
)
{
    // The notification (nonstall) tree is at index 1
    LwU32 intrCtrl = REG_RD32(CSB, LW_CSEC_FALCON_INTR_CTRL(1));
    //
    // HW unit registers, including SEC2's, don't have field defines since the units
    // send a transparent interrupt message; the field defines are in LW_CTRL.
    //
    intrCtrl = FLD_SET_DRF_NUM(_CTRL, _INTR_CTRL_ACCESS_DEFINES, _GFID, gfid, intrCtrl);
    REG_WR32(CSB, LW_CSEC_FALCON_INTR_CTRL(1), intrCtrl);
}


/*
*
* @brief Unmask CTXSW interrupt only 
*/
void
icHostIfIntrCtxswUnmask_GH100(void)
{
    REG_WR32(CSB, LW_CSEC_FALCON_IRQMSET,
             DRF_DEF(_CSEC, _FALCON_IRQMSET, _CTXSW,  _SET));
}
