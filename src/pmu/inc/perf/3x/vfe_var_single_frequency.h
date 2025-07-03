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
 * @file    vfe_var_single_frequency.h
 * @brief   VFE_VAR_SINGLE_FREQUENCY class interface.
 */

#ifndef VFE_VAR_SINGLE_FREQUENCY_H
#define VFE_VAR_SINGLE_FREQUENCY_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_FREQUENCY VFE_VAR_SINGLE_FREQUENCY;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_FREQUENCY class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_FREQUENCY class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR)
#define PERF_VFE_VAR_SINGLE_FREQUENCY_VTABLE() &VfeVarSingleFrequencyVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleFrequencyVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_FREQUENCY_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE class providing attributes of VFE Frequency Single
 * Variable.
 *
 * Evaluator-provided frequency value in MHz. The frequency is associated with a
 * clock domain, if the clock domain index availability flag is set.

 * @extends VFE_VAR_SINGLE
 */
struct VFE_VAR_SINGLE_FREQUENCY
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
     * @brief Clock domain index.
     *
     * To specify any clock domain, this value shall be set
     * @ref LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID.
     *
     * @protected
     */
    LwU8    clkDomainIdx;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_FREQUENCY");

// VFE_VAR interfaces.
VfeVarSingleClientInputMatch (vfeVarSingleClientInputMatch_SINGLE_FREQUENCY);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_FREQUENCY_H
