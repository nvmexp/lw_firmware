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
 * @file    nne_var_pm.h
 * @brief   Neural Net Engine (NNE) block activity (BA) performance monitor (PM)
 *          variable class
 */

#ifndef NNE_VAR_PM_H
#define NNE_VAR_PM_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne/nne_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR_PM NNE_VAR_PM;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a BA PM 
 *
 * @extends NNE_VAR_PM
 */
struct NNE_VAR_PM
{
    /*!
     * @brief Base class.

     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_VAR                     super;

    /*!
     * @brief Tuple of data that uniquely identifies an NNE_VAR_FREQ
     * @protected
     */
    LW2080_CTRL_NNE_VAR_ID_PM   varPmId;

    /*!
     * Secondary normalization value for this PM signal (as IEEE-754
     * 32-bit floating point).  A value of zero (0.0) indicates
     * normalization is disabled for this PM.
     */
    LwF32 secNorm;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneVarGrpIfaceModel10ObjSet_PM)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_PM");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch   (nneVarInputIDMatch_PM)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch_PM");

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 */
NneVarsVarIdMapArrIdxGet   (nneVarsVarIdMapArrIdxGet_PM)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapArrIdxGet_PM");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get   (nneVarLwF32Get_PM)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get_PM");

/*!
 * @copydoc NneVarInputNormalizationGet
 */
NneVarInputNormalizationGet (nneVarInputNormalizationGet_PM)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneVarInputNormalizationGet_PM");

/*!
 * @copydoc NneVarInputNormalize
 */
NneVarInputNormalize (nneVarInputNormalize_PM)
    GCC_ATTRIB_SECTION("imem_nneInferenceClient", "nneVarInputNormalize_PM");

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_VAR_PM_H
