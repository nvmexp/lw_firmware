/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef BOARDOBJGRPMASK_E32_H
#define BOARDOBJGRPMASK_E32_H

/*!
 * @file    boardobjgrpmask_e32.h
 *
 * @brief   Holds PMU definitions specific to the _E32 variant of
 *          BOARDOBJGRPMASK.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "ctrl/ctrl2080/ctrl2080boardobj.h"
#include "boardobj/boardobjgrpmask.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   BOARDOBJGRPMASK child class capable of storing 32 bits indexed
 *          between 0..31.
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
     *
     * @note    Since 32 bits are already represented in the base class, this
     *          extending field is commented out to pacify compilers that
     *          disallow zero-sized arrays.
     */
    // LW2080_CTRL_BOARDOBJGRP_MASK_PRIMITIVE  pDataE32[LW2080_CTRL_BOARDOBJGRP_MASK_ARRAY_EXTENSION_SIZE(LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS)];
} BOARDOBJGRPMASK_E32;

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Inline Functions  ------------------------------- */
/*!
 * @copydoc boardObjGrpMaskInit_SUPER
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
boardObjGrpMaskInit_E32
(
    BOARDOBJGRPMASK_E32 *pMask
)
{
    boardObjGrpMaskInit_SUPER(&pMask->super,
                              LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS);
}

/*!
 * @copydoc boardObjGrpMaskImport_FUNC
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpMaskImport_E32
(
    BOARDOBJGRPMASK_E32                *pMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E32   *pExtMask
)
{
    return boardObjGrpMaskImport_FUNC(&pMask->super,
                                      LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS,
                                      &pExtMask->super);
}

/*!
 * @copydoc boardObjGrpMaskExport_FUNC
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpMaskExport_E32
(
    BOARDOBJGRPMASK_E32                *pMask,
    LW2080_CTRL_BOARDOBJGRP_MASK_E32   *pExtMask
)
{
    return boardObjGrpMaskExport_FUNC(&pMask->super,
                                      LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS,
                                      &pExtMask->super);
}

#endif // BOARDOBJGRPMASK_E32_H
