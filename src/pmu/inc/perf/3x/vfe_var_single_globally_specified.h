/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_globally_specified.h
 * @brief   VFE_VAR_SINGLE_GLOBALLY_SPECIFIED class interface.
 */

#ifndef VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_H
#define VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_GLOBALLY_SPECIFIED VFE_VAR_SINGLE_GLOBALLY_SPECIFIED;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_GLOBALLY_SPECIFIED class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_GLOBALLY_SPECIFIED class
 *          vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED)
#define PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_VTABLE() &VfeVarSingleGloballySpecifiedVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleGloballySpecifiedVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Public interface for clients to set globally specified variable
 *        value.
 *
 * @memberof VFE_VAR_SINGLE_GLOBALLY_SPECIFIED
 * @public
 *
 * @param[in]   pValues         Array of input VFE Variable values.
 * @param[in]   valCount        Size of @ref pValues array.
 * @param[out]  pBTriggerIlw    Output tracking whether perf ilwalidation required.
 *
 * @return FLCN_OK
 *          The globally specified var value successfully updated.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *          Invalid input passed by client.
 * @return Other errors from child function calls.
 */
#define VfeVarSingleGloballySpecifiedValueSet(fname) FLCN_STATUS (fname)(RM_PMU_PERF_VFE_VAR_VALUE *pValues, LwU8 valCount, LwBool *pBTriggerIlw)

/*!
 * @brief VFE_VAR_SINGLE class providing attributes of VFE Globally Specified
 * Single Variable.
 *
 * A generic type not associated with voltage or frequency but identified by a
 * Unique ID.
 *
 * @extends VFE_VAR_SINGLE
 */
struct VFE_VAR_SINGLE_GLOBALLY_SPECIFIED
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_SINGLE  super;

    /*!
     * Unique Identification for the generic caller specified class.
     * @ref LW2080_CTRL_PERF_VFE_VAR_SINGLE_GLOBALLY_SPECIFIED_UID_<xyz>
     * @protected
     */
    LwU8    uniqueId;

    /*!
     * @brief Number of fractional bits in @ref valDefault.
     * @protected
     */
    LwU8    numFracBits;

    /*!
     * @brief Default value specificed by POR.
     * @protected
     */
    LwS32   valDefault;

    /*!
     * @brief Client override value of globally specificed single variable.
     * RM / PMU will use this when this is not equal to the _ILWALID
     * @protected
     */
    LwS32   valOverride;

    /*!
     * @brief Client specified value of globally specificed single variable.
     * RM / PMU will use this when @ref valOverride is equal to the _ILWALID
     * @protected
     */
    LwS32   valSpecified;

    /*!
     * @brief Current active value of globally specificed single variable.
     * @protected
     */
    LwS32   valLwrr;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_GLOBALLY_SPECIFIED");
BoardObjIfaceModel10GetStatus       (vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarIfaceModel10GetStatus_SINGLE_GLOBALLY_SPECIFIED");

// VFE_VAR interfaces.
VfeVarEval                      (vfeVarEval_SINGLE_GLOBALLY_SPECIFIED);

// VFE_VAR_SINGLE_GLOBALLY_SPECIFIED interfaces.
VfeVarSingleGloballySpecifiedValueSet (vfeVarSingleGloballySpecifiedValueSet)
    GCC_ATTRIB_SECTION("imem_perfVfe", "vfeVarSingleGloballySpecifiedValueSet");

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_FREQUENCY_H
