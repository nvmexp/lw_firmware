/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    boardobjgrp_e512.c
 * @copydoc boardobjgrp_e512.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief   _E512 implementation of @ref BoardObjGrpIfaceModel10Set().
 *
 * @copydetails BoardObjGrpIfaceModel10Set()
 */
FLCN_STATUS
boardObjGrpIfaceModel10Set_E512
(
    BOARDOBJGRP_IFACE_MODEL_10                *pModel10,
    LwU8                        groupType,
    PMU_DMEM_BUFFER            *pBuffer,
    BoardObjGrpIfaceModel10SetHeader  (hdrFunc),
    LwLength                    hdrSize,
    BoardObjGrpIfaceModel10SetEntry   (entryFunc),
    LwLength                    entrySize,
    LwU32                       ssOffset,
    LwU8                        ovlIdxDmem,
    BOARDOBJGRPMASK            *pMask,
    BOARDOBJ_VIRTUAL_TABLE    **ppObjectVtables,
    LwLength                    numObjectVtables
)
{
    BOARDOBJGRPMASK_E512 mask;

    return boardObjGrpIfaceModel10Set(pModel10, groupType, pBuffer, hdrFunc, hdrSize,
                                entryFunc, entrySize, ssOffset, ovlIdxDmem,
                                &mask.super, ppObjectVtables, numObjectVtables);
}

/*!
 * @brief   _E512 implementation of @ref BoardObjGrpIfaceModel10Set() using AutoDma feature
 *
 * @copydetails BoardObjGrpIfaceModel10Set()
 */
FLCN_STATUS
boardObjGrpIfaceModel10SetAutoDma_E512
(
    BOARDOBJGRP_IFACE_MODEL_10                *pModel10,
    LwU8                        groupType,
    PMU_DMEM_BUFFER            *pBuffer,
    BoardObjGrpIfaceModel10SetHeader  (hdrFunc),
    LwLength                    hdrSize,
    BoardObjGrpIfaceModel10SetEntry   (entryFunc),
    LwLength                    entrySize,
    LwU32                       ssOffset,
    LwU8                        ovlIdxDmem,
    BOARDOBJGRPMASK            *pMask,
    BOARDOBJ_VIRTUAL_TABLE    **ppObjectVtables,
    LwLength                    numObjectVtables
)
{
    BOARDOBJGRPMASK_E512 mask;

    return boardObjGrpIfaceModel10SetAutoDma(pModel10, groupType, pBuffer, hdrFunc, hdrSize,
                                entryFunc, entrySize, ssOffset, ovlIdxDmem,
                                &mask.super, ppObjectVtables, numObjectVtables);
}

/*!
 * @brief   _E512 implementation of @ref BoardObjGrpIfaceModel10GetStatus().
 *
 * @copydetails BoardObjGrpIfaceModel10GetStatus()
 */
FLCN_STATUS
boardObjGrpIfaceModel10GetStatus_E512
(
    BOARDOBJGRP_IFACE_MODEL_10                    *pModel10,
    PMU_DMEM_BUFFER                *pBuffer,
    BoardObjGrpIfaceModel10GetStatusHeader      (hdrFunc),
    LwLength                        hdrSize,
    BoardObjIfaceModel10GetStatus                   (entryFunc),
    LwLength                        entrySize,
    LwU32                           ssOffset,
    BOARDOBJGRPMASK                *pMask
)
{
    BOARDOBJGRPMASK_E512 mask;

    return boardObjGrpIfaceModel10GetStatus_SUPER(pModel10, pBuffer, hdrFunc, hdrSize,
                                      entryFunc, entrySize, ssOffset, &mask.super);
}

/*!
 * @brief   _E512 implementation of @ref BoardObjGrpIfaceModel10GetStatus() using AutoDma feature
 *
 * @copydetails BoardObjGrpIfaceModel10GetStatus()
 */
FLCN_STATUS
boardObjGrpIfaceModel10GetStatusAutoDma_E512
(
    BOARDOBJGRP_IFACE_MODEL_10                    *pModel10,
    PMU_DMEM_BUFFER                *pBuffer,
    BoardObjGrpIfaceModel10GetStatusHeader      (hdrFunc),
    LwLength                        hdrSize,
    BoardObjIfaceModel10GetStatus                   (entryFunc),
    LwLength                        entrySize,
    LwU32                           ssOffset,
    BOARDOBJGRPMASK                *pMask
)
{
    BOARDOBJGRPMASK_E512 mask;

    return boardObjGrpIfaceModel10GetStatusAutoDma_SUPER(pModel10, pBuffer, hdrFunc, hdrSize,
                                      entryFunc, entrySize, ssOffset, &mask.super);
}

/* ------------------------ Private Functions ------------------------------- */
