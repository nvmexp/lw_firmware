/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_sensed.h
 * @brief   VFE_VAR_SINGLE_SENSED class interface.
 */

#ifndef VFE_VAR_SINGLE_SENSED_H
#define VFE_VAR_SINGLE_SENSED_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_SENSED VFE_VAR_SINGLE_SENSED;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet()
 * @memberof VFE_VAR_SINGLE_SENSED
 * @protected
 */
#define vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED(pModel10, ppBoardObj, size, pDesc)   \
    vfeVarGrpIfaceModel10ObjSetImpl_SINGLE((pModel10), (ppBoardObj), (size), (pDesc))

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 * @memberof VFE_VAR_SINGLE_SENSED
 * @protected
 */
#define vfeVarGetStatus_SINGLE_SENSED(pBoardObj, pBuf)                         \
    vfeVarIfaceModel10GetStatus_SINGLE((pModel10), (pBuf))

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_SENSED
 * @protected
 */
#define vfeVarEval_SINGLE_SENSED(pContext, pVfeVar, pResult)                   \
    vfeVarEval_SINGLE((pContext), (pVfeVar), (pResult))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE class providing attributes of VFE Sensed Single
 * Variable.
 *
 * VFE_VAR_SINGLE_SENSED variable types obtain their values by reading hardware
 * registers, sensors, fuses, etc. directly from the GPU or board.
 *
 * @extends VFE_VAR_SINGLE
 */
struct VFE_VAR_SINGLE_SENSED
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_SINGLE  super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_var_single_sensed_fuse_base.h"
#include "perf/3x/vfe_var_single_sensed_temp.h"

#endif // VFE_VAR_SINGLE_SENSED_H
