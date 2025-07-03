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
 * @file    nne_var_power_dn.h
 * @brief   Neural Net Engine (NNE) normalized dynamic power variable
 *          class
 */

#ifndef NNE_VAR_POWER_DN_H
#define NNE_VAR_POWER_DN_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne/nne_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR_POWER_DN NNE_VAR_POWER_DN;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc NneVarInputNormalize
 */
#define nneVarInputNormalize_POWER_DN                                          \
    nneVarInputNormalize_SUPER

/*!
 * @copydoc NneVarInputNormalizationGet
 */
#define nneVarInputNormalizationGet_POWER_DN                                   \
    nneVarInputNormalizationGet_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a measurement of observed normalized dynamic
 * power on a given rail.
 *
 * A power measurement is "normalized" w.r.t. to voltage by scaling
 * the dynamic power values by the ratio between the observed voltage
 * and the normalization voltage (@ref NNE_VAR_POWER_DN::voltageuV).
 *
 * @extends NNE_VAR
 */
struct NNE_VAR_POWER_DN
{
    /*!
     * @brief Base class.

     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_VAR                     super;

    /*!
     * @brief Tuple of data that uniquely identifies an NNE_VAR_POWER_DN.
     * @protected
     */
    LW2080_CTRL_NNE_VAR_ID_POWER_DN   id;

    /*!
     * @brief Voltage normalization value for the dynamic "normalized"
     * power.  NNE will normalize the power from 1.0 V client input to
     * this voltage value on inference.
     */
    LwU32 voltageuV;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneVarGrpIfaceModel10ObjSet_POWER_DN)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_POWER_DN");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch   (nneVarInputIDMatch_POWER_DN)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch_POWER_DN");

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 */
NneVarsVarIdMapArrIdxGet   (nneVarsVarIdMapArrIdxGet_POWER_DN)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapArrIdxGet_POWER_DN");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get   (nneVarLwF32Get_POWER_DN)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get_POWER_DN");

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_VAR_POWER_DN_H
