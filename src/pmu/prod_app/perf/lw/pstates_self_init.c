/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pstates_self_init.c
 *
 * @copydoc pstates_self_init.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "perf/pstates_self_init.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_src.h"
#include "boardobj/boardobjgrp_src_vbios.h"
#include "perf/vbios/pstates_vbios.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpBoardObjGrpSrcInit
 */
FLCN_STATUS
pstatesBoardObjGrpSrcInit
(
    BOARDOBJGRP_SRC_UNION *pSrc
)
{
    FLCN_STATUS status;
    BOARDOBJGRP_SRC_VBIOS *pSrcVbios;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pstatesBoardObjGrpSrcInit_exit);

    //
    // We lwrrently only support VBIOS sources for the PSTATES, so call into
    // the PSTATES VBIOS module.
    //
    // If the interface returns not supported, then the table isn't available,
    // so propogate the return value to indicate this to callers.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrlwniolwbiosGet(pSrc, &pSrcVbios),
        pstatesBoardObjGrpSrcInit_exit);
    status = pstatesVbiosBoardObjGrpSrcInit(pSrcVbios);
    if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        goto pstatesBoardObjGrpSrcInit_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        pstatesBoardObjGrpSrcInit_exit);

pstatesBoardObjGrpSrcInit_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
