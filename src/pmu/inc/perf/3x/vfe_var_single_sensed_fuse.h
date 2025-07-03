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
 * @file    vfe_var_single_sensed_fuse.h
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE class interface.
 */

#ifndef VFE_VAR_SINGLE_SENSED_FUSE_H
#define VFE_VAR_SINGLE_SENSED_FUSE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single_sensed_fuse_base.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_SENSED_FUSE VFE_VAR_SINGLE_SENSED_FUSE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_SENSED_FUSE class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_SENSED_FUSE class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_LEGACY)
#define PERF_VFE_VAR_SINGLE_SENSED_FUSE_VTABLE() &VfeVarSingleSensedFuseVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedFuseVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_SENSED_FUSE_VTABLE() NULL
#endif

/*!
 * @copydoc BoardObjIfaceModel10GetStatus()
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE
 * @protected
 */
#define vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE(pModel10, pPayload) \
    vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE((pModel10), (pPayload))

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_SINGLE_SENSED_FUSE
 * @protected
 */
#define  vfeVarEval_SINGLE_SENSED_FUSE(pContext, pVfeVar, pResult) \
    vfeVarEval_SINGLE_SENSED_FUSE_BASE((pContext), (pVfeVar), (pResult))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE_SENSED_FUSE class providing attributes of Fuse Sensed Single
 * VFE Variable.
 *
 * Measured chip fuse value. Burned into a fuse at ATE.
 *
 * @extends VFE_VAR_SINGLE_SENSED_FUSE_BASE
 */
struct VFE_VAR_SINGLE_SENSED_FUSE
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_SINGLE_SENSED_FUSE_BASE   super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE");

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION))
FLCN_STATUS vfeVarRegAddrValidateAgainstTable(LwU8 vfieldId, LwU8 segmentIdx, LwU32 regAddr, const LwU32 *pTable, LwU32 tableLength)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarRegAddrValidateAgainstTable");
#endif  // PMUCFG_FEATURE_ENABLED(PMU_PERF_VFIELD_VALIDATION)

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_SENSED_FUSE_H
