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
 * @file    vfe_var_single_voltage.h
 * @brief   VFE_VAR_SINGLE_VOLTAGE class interface.
 *
 * Detailed documentation is located at: [1]
 */
//! [1] https://wiki.lwpu.com/engwiki/index.php/AVFS/Voltage_Frequency_Equation_%28VFE%29_Table_v3.0

#ifndef VFE_VAR_SINGLE_VOLTAGE_H
#define VFE_VAR_SINGLE_VOLTAGE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_single.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE_VOLTAGE VFE_VAR_SINGLE_VOLTAGE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_SINGLE_VOLTAGE class type.
 *
 * @return  Pointer to the location of the VFE_VAR_SINGLE_VOLTAGE class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR)
#define PERF_VFE_VAR_SINGLE_VOLTAGE_VTABLE() &VfeVarSingleVoltageVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarSingleVoltageVirtualTable;
#else
#define PERF_VFE_VAR_SINGLE_VOLTAGE_VTABLE() NULL
#endif

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
#define vfeVarGrpIfaceModel10ObjSetImpl_SINGLE_VOLTAGE(pModel10, ppBoardObj, size, pDesc)  \
    vfeVarGrpIfaceModel10ObjSetImpl_SINGLE((pModel10), (ppBoardObj), (size), (pDesc))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_VAR_SINGLE class providing attributes of VFE Voltage Single
 * Variable.
 *
 * Evaluator-provided voltage value in uV. The voltage is not associated with
 * any particular voltage rail.
 *
 * @extends VFE_VAR_SINGLE
 */
struct VFE_VAR_SINGLE_VOLTAGE
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
// VFE_VAR interfaces.

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_SINGLE_VOLTAGE_H
