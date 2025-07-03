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
 * @file   biftu10xga10x.c
 * @brief  Contains BIF routines for Turing_thru_Ampere.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_objpmu.h"
#include "dev_trim.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_bif_private.h"
#include "pmu_objbif.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

/*!
 *@brief Set PLL lock delay
 *
 *@param[in] dlyLength WAIT_DLY_LENGTH
 */
void
bifSetPllLockDelay_TU10X
(
    LwU32 dlyLength
)
{
    //
    // Once we set the delay in _WAIT_DLY_LENGTH, to actually apply it we need
    // to set _OVERRIDE_PLL_LOCKDET to _NO.
    //
    REG_WR32(BAR0, LW_PTRIM_SYS_PEXPLL_LOCKDET,
             DRF_NUM(_PTRIM, _SYS_PEXPLL_LOCKDET, _WAIT_DLY_LENGTH, dlyLength) |
             DRF_DEF(_PTRIM, _SYS_PEXPLL_LOCKDET, _OVERRIDE_PLL_LOCKDET, _NO));
}
