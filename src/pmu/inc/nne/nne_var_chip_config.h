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
 * @file    nne_var_chip_config.h
 * @brief   Neural Net Engine (NNE) chip config info input variable.
 *          variable class
 */

#ifndef NNE_VAR_CHIP_CONFIG_H
#define NNE_VAR_CHIP_CONFIG_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne/nne_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_VAR_CHIP_CONFIG NNE_VAR_CHIP_CONFIG;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @copydoc NneVarInputNormalize
 */
#define nneVarInputNormalize_CHIP_CONFIG                                       \
    nneVarInputNormalize_SUPER

/*!
 * @copydoc NneVarInputNormalizationGet
 */
#define nneVarInputNormalizationGet_CHIP_CONFIG                                \
    nneVarInputNormalizationGet_SUPER

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Class representing a chip-config NNE input variable.
 *
 * @extends NNE_VAR_CHIP_CONFIG
 */
struct NNE_VAR_CHIP_CONFIG
{
    /*!
     * @brief Base class.

     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_VAR                              super;

    /*!
     * @brief Tuple of data that uniquely identifies an NNE_VAR_CHIP_CONFIG
     * @protected
     */
    LW2080_CTRL_NNE_VAR_ID_CHIP_CONFIG   varConfigId;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet    (nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG)
    GCC_ATTRIB_SECTION("imem_nneBoardObj", "nneVarGrpIfaceModel10ObjSet_CHIP_CONFIG");

/*!
 * @copydoc NneVarInputIDMatch
 */
NneVarInputIDMatch   (nneVarInputIDMatch_CHIP_CONFIG)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarInputIDMatch_CHIP_CONFIG");

/*!
 * @copydoc NneVarsVarIdMapArrIdxGet
 */
NneVarsVarIdMapArrIdxGet   (nneVarsVarIdMapArrIdxGet_CHIP_CONFIG)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarsVarIdMapArrIdxGet_CHIP_CONFIG");

/*!
 * @copydoc NneVarLwF32Get
 */
NneVarLwF32Get   (nneVarLwF32Get_CHIP_CONFIG)
    GCC_ATTRIB_SECTION("imem_nne", "nneVarLwF32Get_CHIP_CONFIG");

/* ------------------------ Include Derived Types --------------------------- */

#endif // NNE_VAR_CHIP_CONFIG_H
