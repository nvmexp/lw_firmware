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
 * @copydoc pstates_vbios.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/vbios/pstates_vbios.h"
#include "boardobj/boardobjgrp_src_vbios.h"
#include "pmu_objvbios.h"
#include "vbios/vbios_image.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
pstatesVbiosBoardObjGrpSrcInit
(
    BOARDOBJGRP_SRC_VBIOS *pSrc
)
{
    FLCN_STATUS status;
    const LwU8 *pVersion;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pstatesVbiosBoardObjGrpSrcInit_exit);

    //
    // Get the table version to know which version-specific implementation to
    // call.
    //
    // If this interface returns not supported, then do the same to alert the
    // caller.
    //
    // Make all other errors hard errors.
    //
    status = vbiosImageDataGetTyped(
        &Vbios,
        LW_GFW_DIRT_PERFORMANCE_TABLE,
        &pVersion,
        0U,
        sizeof(*pVersion));
    if (status == FLCN_ERR_NOT_SUPPORTED)
    {
        goto pstatesVbiosBoardObjGrpSrcInit_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        pstatesVbiosBoardObjGrpSrcInit_exit);

    status = FLCN_ERR_ILWALID_STATE;
    switch (*pVersion)
    {
        case PSTATES_VBIOS_6X_TABLE_VERSION:
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_VBIOS_6X))
            {
                status = pstatesVbiosBoardObjGrpSrcInit_6X(pSrc);
            }
            break;
        default:
        {
            status = FLCN_ERR_ILWALID_STATE;
            break;
        }
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        pstatesVbiosBoardObjGrpSrcInit_exit);

pstatesVbiosBoardObjGrpSrcInit_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
