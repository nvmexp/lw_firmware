/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file nne_desc_fc_10.h
 * @brief Neural-net descriptor class interface
 */

#ifndef NNE_DESC_FC_10_H
#define NNE_DESC_FC_10_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "nne_desc.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct NNE_DESC_FC_10 NNE_DESC_FC_10;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Descriptor for a single neural-net.
 *
 * This class represents a single NNE compatible neural-net and conains all data needed to evaluate it.
 *
 * @extends BOARDOBJ
 */
struct NNE_DESC_FC_10
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure to allow casting to parent class.
     *
     * @protected
     */
    NNE_DESC                             super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
#define nneDescGrpIfaceModel10ObjSet_FC_10(_pModel10, _ppBoardObj, _size, _pBoardObjDesc)  \
    nneDescGrpIfaceModel10ObjSet_SUPER((_pModel10), (_ppBoardObj), (_size), (_pBoardObjDesc))

#endif // NNE_DESC_H
