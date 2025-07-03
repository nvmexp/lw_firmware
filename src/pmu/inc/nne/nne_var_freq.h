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
 * @file    nne_var_freq.h
 * @brief   Neural Net Engine (NNE) clock frequency variable class
 */

#ifndef NNE_VAR_FREQ_H
#define NNE_VAR_FREQ_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne/nne_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR_FREQ NNE_VAR_FREQ;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc NneVarInputNormalize
 */
#define nneVarInputNormalize_FREQ                                              \
    nneVarInputNormalize_SUPER

/*!
 * @copydoc NneVarInputNormalizationGet
 */
#define nneVarInputNormalizationGet_FREQ                                       \
    nneVarInputNormalizationGet_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a clcok frequency as input to a neural-net 
 *
 * @extends NNE_VAR
 */
struct NNE_VAR_FREQ
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_VAR                       super;

    /*!
     * @brief Tuple of data that uniquely identifies an NNE_VAR_FREQ
     * @protected
     */
    LW2080_CTRL_NNE_VAR_ID_FREQ   varFreqId;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneVarGrpIfaceModel10ObjSet_FREQ)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_FREQ");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch   (nneVarInputIDMatch_FREQ)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch_FREQ");

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 */
NneVarsVarIdMapArrIdxGet   (nneVarsVarIdMapArrIdxGet_FREQ)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapArrIdxGet_FREQ");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get   (nneVarLwF32Get_FREQ)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get_FREQ");

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_VAR_FREQ_H
