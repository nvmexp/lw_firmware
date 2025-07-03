/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   dpu_isohub0205.c
 * @brief  MSCG With FRL 02.05 Hal Functions
 *
 * MSCG With FRL Hal Functions implement chip specific initialization, helper functions
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "dev_ihub.h"
#include "dev_disp.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu_objisohub.h"

#include "config/g_isohub_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */

/*!
 * @brief Set Critical Watermark State
 *
 * This function Set Critical Watermark State of specified head
 *
 * @param[in]   headIndex  Index of head
 * @param[in]   bIgnore    LW_TRUE : Ignore; LW_FALSE : Enable
 *
 * @return FLCN_OK
 *         Upon successfully writing register
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *         If invalid head index is passed
 */
FLCN_STATUS
isohubSetCwmState_v02_05
(
    LwU8   headIndex,
    LwBool bIgnore
)
{
    LwU32         regCrit;

    if (headIndex >= LW_PDISP_HEADS)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    regCrit = REG_RD32(CSB, LW_PDISP_ISOHUB_CRITICAL_CTL_HEAD(headIndex));

    if (bIgnore)
    {
        // Ignore CWM
        regCrit = FLD_SET_DRF(_PDISP, _ISOHUB_CRITICAL_CTL_HEAD, _MODE, _IGNORE, regCrit);
    }
    else
    {
        // Enable CWM
        regCrit = FLD_SET_DRF(_PDISP, _ISOHUB_CRITICAL_CTL_HEAD, _MODE, _ENABLE, regCrit);
    }

    REG_WR32(CSB, LW_PDISP_ISOHUB_CRITICAL_CTL_HEAD(headIndex), regCrit);

    return FLCN_OK;
}
