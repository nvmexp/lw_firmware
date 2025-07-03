/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobj_iface_model_10.c
 * @copydoc boardobj_iface_model_10.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "lwos_dma.h"
#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobj.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief Constructor validator helper.
 *
 * @memberof BOARDOBJ
 * @private
 *
 * Performs validation of the parameters passed to the object constructor.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
s_boardObjConstructValidateHelper
(
    BOARDOBJGRP        *pBObjGrp,
    BOARDOBJ           *pBoardObj,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    FLCN_STATUS status = FLCN_OK;

    // Make sure we're re-sending same object.
    if (pBoardObj->type != pBoardObjDesc->type)
    {
        status = FLCN_ERROR;
        PMU_TRUE_BP();
        goto s_boardObjConstructValidateHelper_exit;
    }

    if (pBoardObj->grpIdx != pBoardObjDesc->grpIdx)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_TRUE_BP();
        goto s_boardObjConstructValidateHelper_exit;
    }

s_boardObjConstructValidateHelper_exit:
    return status;
}

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   Board Object construction function.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
boardObjGrpIfaceModel10ObjSet_IMPL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS status = FLCN_OK;

    if (*ppBoardObj == NULL)
    {
        LwU8 ovlIdx = pBObjGrp->ovlIdxDmem;

        *ppBoardObj = (BOARDOBJ *)lwosCalloc(ovlIdx, size);
        if (*ppBoardObj == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_TRUE_BP();
            goto boardObjGrpIfaceModel10ObjSet_IMPL_exit;
        }
        (*ppBoardObj)->type   = pBoardObjDesc->type;
        (*ppBoardObj)->grpIdx = pBoardObjDesc->grpIdx;

        //
        // Populate BOARDOBJ's class with its group's classId; the BOARDOBJ
        // descriptor doesn't have it.
        //
        BOARDOBJ_SET_CLASS_ID((*ppBoardObj), pBObjGrp->classId);
    }
    else
    {
        if (!PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJ_CONSTRUCT_VALIDATE))
        {
            PMU_ASSERT_OK_OR_GOTO(status,
                s_boardObjConstructValidateHelper(pBObjGrp, *ppBoardObj, pBoardObjDesc),
                boardObjGrpIfaceModel10ObjSet_IMPL_exit);
        }
    }

boardObjGrpIfaceModel10ObjSet_IMPL_exit:
    return status;
}

/*!
 * @brief   Board Object construction validation function.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
FLCN_STATUS
boardObjGrpIfaceModel10ObjSetValidate
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS status = FLCN_OK;

    if (*ppBoardObj == NULL)
    {
        //
        // The @ref pBObjGrp->ovlIdxDmem does not require validation as it is
        // not an external parameter (hardcoded within PMU ucode).
        //
        // Dynamic memory allocations cannot be validated on systems without
        // DMEM-VA / dedicated data sections due to conlwrrent tasks' code
        // exelwtion. Since dynamic memory allocations are anyway banned past
        // the PMU's init, there is no point in further checks at this point.
        //
        // The @ref pBoardObjDesc->type was referenced before reaching this
        // code so it's value must be valid.
        //
        // The @ref pBoardObjDesc->grpIdx does not require validation as it is
        // not an external parameter (set within @ref boardObjGrpIfaceModel10Set).
        //
        // The @ref pBObjGrp->classId does not require validation as it was
        // set and sanitized within the @ref boardObjGrpIfaceModel10CmdHandlerDispatch().
        //
    }
    else
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_boardObjConstructValidateHelper(pBObjGrp, *ppBoardObj, pBoardObjDesc),
            boardObjGrpIfaceModel10ObjSetValidate_exit);
    }

boardObjGrpIfaceModel10ObjSetValidate_exit:
    return status;
}

/*!
 * @brief   Board Object Query super implementation.
 *
 * @copydetails BoardObjIfaceModel10GetStatus()
 */
FLCN_STATUS
boardObjIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    pPayload->type = pBoardObj->type;

    return FLCN_OK;
}

/* ------------------------ Private Functions ------------------------------- */
