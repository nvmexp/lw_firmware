/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRP_E255_H
#define BOARDOBJGRP_E255_H

/*!
 * @file    boardobjgrp_e255.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJGRP_E255
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJGRP_E255 BOARDOBJGRP_E255;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   A @ref BOARDOBJGRP child class allowing storage of up to 255 @ref
 *          BOARDOBJ object pointers.
 */
struct BOARDOBJGRP_E255
{
    /*!
     * @brief   BOARDOBJGRP super-class.
     *
     * @note    Must be first element of the structure.
     */
    BOARDOBJGRP             super;

    /*!
     * @brief   Statically allocated BOARDOBJGRPMASK tracking all @ref BOARDOBJ
     *          pointers stored in @ref BOARDOBJGRP::ppObjects, and initialized
     *          by the child constructor.
     *
     * @note    Must be second element of the structure (immediately following
     *          super).
     */
    BOARDOBJGRPMASK_E255    objMask;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjGrpIfaceModel10Set        (boardObjGrpIfaceModel10Set_E255);
BoardObjGrpIfaceModel10Set        (boardObjGrpIfaceModel10SetAutoDma_E255);
BoardObjGrpIfaceModel10GetStatus        (boardObjGrpIfaceModel10GetStatus_E255);
BoardObjGrpIfaceModel10GetStatus        (boardObjGrpIfaceModel10GetStatusAutoDma_E255);

/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJGRP_E255_H
