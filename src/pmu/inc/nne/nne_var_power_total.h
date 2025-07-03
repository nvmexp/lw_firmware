/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    nne_var_power_total.h
 * @brief   Neural Net Engine (NNE) total power variable class
 */

#ifndef NNE_VAR_POWER_TOTAL_H
#define NNE_VAR_POWER_TOTAL_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne/nne_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR_POWER_TOTAL NNE_VAR_POWER_TOTAL;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc NneVarInputNormalize
 */
#define nneVarInputNormalize_POWER_TOTAL                                       \
    nneVarInputNormalize_SUPER

/*!
 * @copydoc NneVarInputNormalizationGet
 */
#define nneVarInputNormalizationGet_POWER_TOTAL                                \
    nneVarInputNormalizationGet_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a measurement of observed total power on
 * a given rail.
 *
 * @extends NNE_VAR
 */
struct NNE_VAR_POWER_TOTAL
{
    /*!
     * @brief Base class.

     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_VAR                     super;

    /*!
     * @brief Tuple of data that uniquely identifies an NNE_VAR_POWER_TOTAL.
     * @protected
     */
    LW2080_CTRL_NNE_VAR_ID_POWER_TOTAL  id;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneVarGrpIfaceModel10ObjSet_POWER_TOTAL)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_POWER_TOTAL");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch   (nneVarInputIDMatch_POWER_TOTAL)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch_POWER_TOTAL");

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 */
NneVarsVarIdMapArrIdxGet   (nneVarsVarIdMapArrIdxGet_POWER_TOTAL)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapArrIdxGet_POWER_TOTAL");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get   (nneVarLwF32Get_POWER_TOTAL)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get_POWER_TOTAL");

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_VAR_POWER_TOTAL_H
