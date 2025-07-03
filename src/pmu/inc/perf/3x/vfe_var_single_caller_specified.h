/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_var_single_caller_specified.h
 * @brief   VFE_VAR_SINGLE_CALLER_SPECIFIED class interface.
 */

#ifndef VFE_VAR_SINGLE_CALLER_SPECIFIED_H
#define VFE_VAR_SINGLE_CALLER_SPECIFIED_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_CALLER_SPECIFIED VFE_VAR_SINGLE_CALLER_SPECIFIED;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_CALLER_SPECIFIED class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_CALLER_SPECIFIED class
 *          vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR_35)
#define PERF_VFE_VAR_SINGLE_CALLER_SPECIFIED_VTABLE() &VfeVarSingleCallerSpecifiedVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleCallerSpecifiedVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_CALLER_SPECIFIED_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE class providing attributes of VFE Caller Specified
 * Single Variable.
 *
 * A generic type not associated with voltage or frequency but identified by a
 * Unique ID.
 *
 * @extends VFE_VAR_SINGLE
 */
struct VFE_VAR_SINGLE_CALLER_SPECIFIED
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
     * @brief Unique ID.
     * @protected
     */
    LwU8   uniqueId;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_CALLER_SPECIFIED");

// VFE_VAR interfaces.
VfeVarSingleClientInputMatch (vfeVarSingleClientInputMatch_SINGLE_CALLER_SPECIFIED);

// VFE_VAR_SINGLE interfaces.
VfeVarInputClientValueGet (vfeVarInputClientValueGet_SINGLE_CALLER_SPECIFIED);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_FREQUENCY_H
