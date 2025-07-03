/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @copydoc boardobjgrp_src_vbios.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src_vbios.h"
#include "pmu_objvbios.h"
#include "vbios/vbios_image.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
boardObjGrpSrcVbiosEntryDataGet
(
    const BOARDOBJGRP_SRC_VBIOS *pSrc,
    BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS *pEntryData,
    LwBoardObjIdx entryIdx
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pEntryData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcVbiosEntryDataGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcEntryDataGet(&pSrc->super, &pEntryData->super, entryIdx),
        boardObjGrpSrcVbiosEntryDataGet_exit);

    pEntryData->entryOffset =
        pSrc->entriesOffset + pSrc->entryStride * entryIdx;

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGet(
            &Vbios,
            pSrc->dirtId,
            &pEntryData->super.pEntry,
            pEntryData->entryOffset,
            pSrc->entrySize),
        boardObjGrpSrcVbiosEntryDataGet_exit);

boardObjGrpSrcVbiosEntryDataGet_exit:
    return status;
}

FLCN_STATUS
boardObjGrpSrcVbiosEntryDataSubEntryGet
(
    const BOARDOBJGRP_SRC_VBIOS *pSrc,
    const BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS *pEntryData,
    const void **ppSubEntry,
    LwU32 subEntryIdx,
    LwLength subEntrySize
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pEntryData != NULL) &&
        (ppSubEntry != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcVbiosEntryDataSubEntryGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGet(
            &Vbios,
            pSrc->dirtId,
            ppSubEntry,
            pEntryData->entryOffset + pSrc->entrySize +
                subEntryIdx * subEntrySize,
            subEntrySize),
        boardObjGrpSrcVbiosEntryDataSubEntryGet_exit);

boardObjGrpSrcVbiosEntryDataSubEntryGet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
