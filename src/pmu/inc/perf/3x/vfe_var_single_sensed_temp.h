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
 * @file    vfe_var_single_sensed_temp.h
 * @brief   VFE_VAR_SINGLE_SENSED_TEMP class interface.
 */

#ifndef VFE_VAR_SINGLE_SENSED_TEMP_H
#define VFE_VAR_SINGLE_SENSED_TEMP_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_SENSED_TEMP VFE_VAR_SINGLE_SENSED_TEMP;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_SENSED_TEMP class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_SENSED_TEMP class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR)
#define PERF_VFE_VAR_SINGLE_SENSED_TEMP_VTABLE() &VfeVarSingleSensedTempVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleSensedTempVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_SENSED_TEMP_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE_SENSED class providing attributes of Temperature Sensed
 * Single VFE Variable.
 *
 * A temperature value measured somewhere on the GPU die or board, as provided
 * by a thermal channel.
 *
 * This is a signed FXP 24.8 value (from the RM/PMU infrastructure) colwerted
 * into the corresponding floating point interpretation.
 *
 * @extends VFE_VAR_SINGLE_SENSED
 */
struct VFE_VAR_SINGLE_SENSED_TEMP
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
     * @brief Index of the Thermal Channel that is temperature source of this
     * variable.
     * @protected
     */
    LwU8                    thermChannelIndex;

    /*!
     * @brief Absolute value of Positive Temperature Hysteresis (0 => no
     * hysteresis). Hysteresis to apply when temperature has positive delta.
     * @protected
     */
    LwTemp                  tempHysteresisPositive;

    /*!
     * @brief Absolute value of Negative Temperature Hysteresis (0 => no
     * hysteresis). Hysteresis to apply when temperature has negative delta.
     * @protected
     */
    LwTemp                  tempHysteresisNegative;

    /*!
     * @brief Default temperature value.
     *
     * If set to a non-zero value (in pre-silicon) PMU should use this value
     * instead of getting the temperature from the thermal channel.
     *
     * @protected
     */
    LwTemp                  tempDefault;

    /*!
     * @brief Most recently sampled temperature that exceeded hysteresis range.
     * @protected
     */
    LwTemp                  lwTemp;

    /*!
     * @brief @ref lwTemp stored as IEEE-754 32-bit floating point.
     * @protected
     */
    LwF32                   fpTemp;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_SENSED_TEMP");

// VFE_VAR interfaces.
VfeVarEval          (vfeVarEval_SINGLE_SENSED_TEMP);
VfeVarSample        (vfeVarSample_SINGLE_SENSED_TEMP);

// VFE_VAR_SINGLE interfaces.
VfeVarInputClientValueGet (vfeVarInputClientValueGet_SINGLE_SENSED_TEMP);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_SENSED_TEMP_H
