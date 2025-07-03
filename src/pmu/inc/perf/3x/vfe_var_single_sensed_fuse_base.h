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
 * @file    vfe_var_single_sensed_fuse_base.h
 * @brief   VFE_VAR_SINGLE_SENSED_FUSE_BASE class interface.
 */

#ifndef VFE_VAR_SINGLE_SENSED_FUSE_BASE_H
#define VFE_VAR_SINGLE_SENSED_FUSE_BASE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single_sensed.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_SENSED_FUSE_BASE VFE_VAR_SINGLE_SENSED_FUSE_BASE;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */

/*!
 * Information stored in @ref RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_BASE
 */
typedef struct
{
    /*!
     * Allows the host driver to override the value of the fuse.
     */
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_OVERRIDE_INFO   overrideInfo;

    /*!
     * Default fuse value.
     */
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE           fuseValDefault;

    /*!
     * When set to true, use
     * @ref LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE::signedValue as
     * the value; otherwise, use
     * @ref LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE::unsignedValue.
     */
    LwBool                                                      bFuseValueSigned;

    /*!
     * Information related to fuse version
     */
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VER_INFO        verInfo;

    /*!
     * HW Correction Scale factor - used to multiply the HW read fuse value to
     * correct the units of the fuse on the GPU. Default value = 1.0
     */
    LwUFXP20_12                                                 hwCorrectionScale;

    /*!
     * HW Correction offset factor - used to offset the HW read fuse value to
     * correct the units of the fuse on the GPU. Default value = 0
     */
    LwS32                                                       hwCorrectionOffset;
} VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED;

/*!
 * @brief VFE_VAR_SINGLE_SENSED_FUSE_BASE class providing attributes of Fuse Sensed Single
 * VFE Variable.
 *
 * Virtual class
 *
 * @extends VFE_VAR_SINGLE_SENSED
 */
struct VFE_VAR_SINGLE_SENSED_FUSE_BASE
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_SINGLE_SENSED   super;

    /*!
     * @brief Fuse value read from HW.
     * @protected
     */
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE   fuseValueHwInteger;

    /*!
     * @brief Current Fuse value represented as integer. Reflects fuse value
     * overridden through regkey and default value override.
     * @protected
     */
    LW2080_CTRL_PERF_VFE_VAR_SINGLE_SENSED_FUSE_VALUE   fuseValueInteger;

    /*!
     * @brief Current Fuse value (as IEEE-754 32-bit floating point).
     * @protected
     */
    LwF32   fuseValue;

    /*!
     * @brief Fuse version read from HW.
     * @protected
     */
    LwU8    fuseVersion;

    /*!
     * @brief Boolean indicating whether version check failed.
     * @protected
     */
    LwBool  bVersionCheckFailed;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED)
    /*!
     * Information stored in @ref RM_PMU_VFE_VAR_SINGLE_SENSED_FUSE_BASE
     */
    VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED fuseBaseExtended;
#endif
};

/* ------------------------ Inline functions -------------------------------- */

/*!
 * @brief   Accessor for VFE_VAR_SINGLE_SENSED_FUSE_BASE::fuseBaseExtended.
 *
 * @param[in]   pFuseBase  VFE_VAR_SINGLE_SENSED_FUSE_BASE pointer.
 *
 * @return
 *     @ref VFE_VAR_SINGLE_SENSED_FUSE_BASE::fuseBaseExtended pointer if feature enabled
 *     NULL pointer if feature is disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED *
vfeVarSingleSensedFuseBaseExtendedGet
(
    VFE_VAR_SINGLE_SENSED_FUSE_BASE *pFuseBase
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_SENSED_FUSE_BASE_EXTENDED)
    return &(pFuseBase->fuseBaseExtended);
#else
    return NULL;
#endif
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_FUSE_BASE");

BoardObjIfaceModel10GetStatus (vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarIfaceModel10GetStatus_SINGLE_SENSED_FUSE_BASE");

// VFE_VAR interfaces.
VfeVarEval          (vfeVarEval_SINGLE_SENSED_FUSE_BASE);
/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_var_single_sensed_fuse.h"
#include "perf/3x/vfe_var_single_sensed_fuse_20.h"

#endif // VFE_VAR_SINGLE_SENSED_FUSE_BASE_H
