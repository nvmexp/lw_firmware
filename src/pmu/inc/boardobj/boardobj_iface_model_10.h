/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJ_IFACE_MODEL_10_H
#define BOARDOBJ_IFACE_MODEL_10_H

#include "g_boardobj_iface_model_10.h"

#ifndef G_BOARDOBJ_IFACE_MODEL_10_H
#define G_BOARDOBJ_IFACE_MODEL_10_H

/*!
 * @file    boardobj_iface_model_10.h
 *
 * @brief   Provides PMU-specific definitions for version 1.0 of the BOARDOBJ
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobj_fwd.h"
#include "boardobj/boardobj.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifboardobj.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @brief   Constructor prototype for BOARDOBJ.
 *
 * All BOARDOBJ derived classes should implement the BoardObjGrpIfaceModel10ObjSet interface
 * for their own construction code.
 *
 * @param[in]   pBObjGrp        pointer to the group that will hold constructed
 *                              object.
 * @param[out]  ppBoardObj      pointer to the BoardOBJ data field to be
 *                              constructed.
 * @param[in]   size            size of the ppBoardObj buffer.
 * @param[in]   pBoardObjDesc   pointer to this BOARDOBJ's description passed
 *                              from RM.
 *
 * @return FLCN_OK  This BOARDOBJ is constructed successfully.
 * @return OTHERS   Implementation specific error oclwrs.
 */
#define BoardObjGrpIfaceModel10ObjSet(fname) FLCN_STATUS (fname)(BOARDOBJGRP_IFACE_MODEL_10 *pModel10, BOARDOBJ **ppBoardObj, LwLength size, RM_PMU_BOARDOBJ *pBoardObjDesc)

/*!
 * @brief   Query prototype for BOARDOBJ/BOARDOBJGRP.
 *
 * All BOARDOBJ derived classes should implement the BoardObjIfaceModel10GetStatus interface for
 * their own query code.
 *
 * @param[in]  pBoardObj    pointer to the BoardOBJ data field to be query.
 * @param[in]  pPayload     pointer to this BOARDOBJ's description that will
 *                          be passed to RM.
 *
 * @return FLCN_OK  Information had been successfully copied.
 * @return OTHERS   Implementation specific error oclwrs.
 */
#define BoardObjIfaceModel10GetStatus(fname) FLCN_STATUS (fname)(BOARDOBJ_IFACE_MODEL_10 *pModel10, RM_PMU_BOARDOBJ *pPayload)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure representing an implementable interface indicating that a
 * @ref BOARDOBJ implements the "1.0" version of the @ref BOARDOBJ "model."
 * This is the "legacy" model that was based upon Resource Manager-based
 * initialization of the @ref BOARDOBJ.
 */
struct BOARDOBJ_IFACE_MODEL_10
{
    /*!
     * Member so that @ref BOARDOBJ_IFACE_MODEL_10 is a strict wrapper around
     * the @ref BOARDOBJ type. This allows for colwersion between the two by
     * direct casts.
     */
    BOARDOBJ boardObj;
};

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * @brief   Given a @ref BOARDOBJ_IFACE_MODEL_10, returns its corresponding
 *          @ref BOARDOBJ
 *
 * @param[in]   pModel10
 *  @ref BOARDOBJ_IFACE_MODEL_10 for which to retrieve @ref BOARDOBJ
 * 
 * @return @ref BOARDOBJ corresponding to pModel10, or @ref NULL if not
 * available
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJ *
boardObjIfaceModel10BoardObjGet
(
    BOARDOBJ_IFACE_MODEL_10 *pModel10
)
{
    return &pModel10->boardObj;
}

/*!
 * @brief   Given a @ref BOARDOBJ, returns its corresponding
 *          @ref BOARDOBJ_IFACE_MODEL_10
 *
 * @param[in]   pBoardObj
 *  @ref BOARDOBJ for which to retrieve @ref BOARDOBJ_IFACE_MODEL_10
 * 
 * @return @ref BOARDOBJ_IFACE_MODEL_10 corresponding to pBoardObj, or
 * @ref NULL if not available
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJ_IFACE_MODEL_10 *
boardObjIfaceModel10FromBoardObjGet
(
    BOARDOBJ *pBoardObj
)
{
    return (BOARDOBJ_IFACE_MODEL_10 *)pBoardObj;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjGrpIfaceModel10ObjSet   (boardObjGrpIfaceModel10ObjSetValidate);
mockable BoardObjGrpIfaceModel10ObjSet   (boardObjGrpIfaceModel10ObjSet);
BoardObjIfaceModel10GetStatus       (boardObjIfaceModel10GetStatus);

/* ------------------------ Include Derived Types --------------------------- */

#endif // G_BOARDOBJ_IFACE_MODEL_10_H
#endif // BOARDOBJ_IFACE_MODEL_10_H
