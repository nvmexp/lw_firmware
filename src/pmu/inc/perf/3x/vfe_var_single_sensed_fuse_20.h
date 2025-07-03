/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_sensed_fuse_20.h
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE_20 class interface.
 */

#ifndef VFE_VAR_SINGLE_SENSED_FUSE_20_H
#define VFE_VAR_SINGLE_SENSED_FUSE_20_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single_sensed_fuse_base.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_SENSED_FUSE_20 VFE_VAR_SINGLE_SENSED_FUSE_20;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_SENSED_FUSE_20 class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_SENSED_FUSE_20 class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_20)
#define PERF_VFE_VAR_SINGLE_SENSED_FUSE_20_VTABLE() &VfeVarSingleSensedFuse20VirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedFuse20VirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_SENSED_FUSE_20_VTABLE() NULL
#endif

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_20
 * @protected
 */
#define vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_20(pModel10, pPayload) \
    vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE((pModel10), (pPayload))

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE_20
 * @protected
 */
#define  vfeVarEval_SINGLE_SENSED_FUSE_20(pContext, pVfeVar, pResult) \
    vfeVarEval_SINGLE_SENSED_FUSE_BASE((pContext), (pVfeVar), (pResult))
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE_SENSED_FUSE_20 class providing attributes of
 * Fuse 20 Sensed Single VFE Variable.
 *
 * Measured chip fuse value. Burned into a fuse at ATE.
 *
 * @extends VFE_VAR_SINGLE_SENSED_FUSE_BASE
 */
struct VFE_VAR_SINGLE_SENSED_FUSE_20
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_SINGLE_SENSED_FUSE_BASE   super;
    /*!
     * Fuse ID
     */
    LwU8                              fuseId;
    /*!
     * Fuse version ID
     */
    LwU8                              fuseIdVer;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_20");

BoardObjIfaceModel10GetStatus (vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_20)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_20");

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_SENSED_FUSE_20_H
