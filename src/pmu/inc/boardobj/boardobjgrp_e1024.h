/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRP_E1024_H
#define BOARDOBJGRP_E1024_H

/*!
 * @file    boardobjgrp_e1024.h
 *
 * @brief   Provides PMU-specific definitions for BOARDOBJGRP_E1024
 *          infrastructure.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct BOARDOBJGRP_E1024 BOARDOBJGRP_E1024;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief   A @ref BOARDOBJGRP child class allowing storage of up to 1024 @ref
 *          BOARDOBJ object pointers.
 */
struct BOARDOBJGRP_E1024
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
    BOARDOBJGRPMASK_E1024    objMask;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
BoardObjGrpIfaceModel10Set        (boardObjGrpIfaceModel10Set_E1024);
BoardObjGrpIfaceModel10Set        (boardObjGrpIfaceModel10SetAutoDma_E1024);
BoardObjGrpIfaceModel10GetStatus        (boardObjGrpIfaceModel10GetStatus_E1024);
BoardObjGrpIfaceModel10GetStatus        (boardObjGrpIfaceModel10GetStatusAutoDma_E1024);

/* ------------------------ Include Derived Types --------------------------- */

#endif // BOARDOBJGRP_E1024_H
