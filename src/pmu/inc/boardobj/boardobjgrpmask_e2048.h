/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRPMASK_E2048_H
#define BOARDOBJGRPMASK_E2048_H

/*!
 * @file    boardobjgrpmask_e2048.h
 *
 * @brief   Holds PMU definitions specific to the _E2048 variant of
 *          BOARDOBJGRPMASK.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "ctrl/ctrl2080/ctrl2080boardobj.h"
#include "boardobj/boardobjgrpmask.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   BOARDOBJGRPMASK child class capable of storing 2048 bits indexed
 *          between 0..254.
 */
typedef struct
{
    /*!
     * @brief   BOARDOBJGRPMASK super-class. Must be the first element of the
     *          structure.
     */
    BOARDOBJGRPMASK super;

    /*!
     * @brief   Continuation of the array of
     *          LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE elements representing the
     *          bit-mask.
     *
     * @note    Must be the second member of the structure.
     */
    LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE  pDataE2048[LW2080_CTRL_BOARDOBJGRP_MASK_ARRAY_EXTENSION_SIZE(LW2080_CTRL_BOARDOBJGRP_E2048_MAX_OBJECTS)];
} BOARDOBJGRPMASK_E2048;

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Inline Functions  ------------------------------- */
/*!
 * @copydoc boardObjGrpMaskInit_SUPER
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskInit_E2048
(
    BOARDOBJGRPMASK_E2048 *pMask
)
{
    boardObjGrpMaskInit_SUPER(&pMask->super,
                              LW2080_CTRL_BOARDOBJGRP_E2048_MAX_OBJECTS);
}

/*!
 * @copydoc boardObjGrpMaskImport_FUNC
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpMaskImport_E2048
(
    BOARDOBJGRPMASK_E2048               *pMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E2048  *pExtMask
)
{
    return boardObjGrpMaskImport_FUNC(&pMask->super,
                                      LW2080_CTRL_BOARDOBJGRP_E2048_MAX_OBJECTS,
                                      &pExtMask->super);
}

/*!
 * @copydoc boardObjGrpMaskExport_FUNC
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpMaskExport_E2048
(
    BOARDOBJGRPMASK_E2048               *pMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E2048  *pExtMask
)
{
    return boardObjGrpMaskExport_FUNC(&pMask->super,
                                      LW2080_CTRL_BOARDOBJGRP_E2048_MAX_OBJECTS,
                                      &pExtMask->super);
}

#endif // BOARDOBJGRPMASK_E2048_H
