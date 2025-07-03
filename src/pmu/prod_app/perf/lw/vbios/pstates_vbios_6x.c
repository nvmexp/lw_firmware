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
 * @copydoc pstates_vbios_6x.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "perf/vbios/pstates_vbios_6x.h"
#include "perf/vbios/pstates_vbios.h"
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
pstatesVbiosBoardObjGrpSrcInit_6X
(
    BOARDOBJGRP_SRC_VBIOS *pSrc
)
{
    FLCN_STATUS status;
    const PSTATES_VBIOS_6X_HEADER *pHeader;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pstatesVbiosBoardObjGrpSrcInit_6X_exit);

    // Get just the version and size to start
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGetTyped(
            &Vbios,
            LW_GFW_DIRT_PERFORMANCE_TABLE,
            &pHeader,
            0U,
            2U),
        pstatesVbiosBoardObjGrpSrcInit_6X_exit);

    //
    // Assert that the version matches what we expect and that the header is
    // larger than the minimum expected size.
    //
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeader->version == PSTATES_VBIOS_6X_TABLE_VERSION) &&
        (pHeader->headerSize >= PSTATES_VBIOS_6X_HEADER_SIZE_0A),
        FLCN_ERR_ILWALID_STATE,
        pstatesVbiosBoardObjGrpSrcInit_6X_exit);

    //
    // Get the full header now. Note that this will automatically fail if the
    // size is too large for the table.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        vbiosImageDataGetTyped(
            &Vbios, 
            LW_GFW_DIRT_PERFORMANCE_TABLE,
            &pHeader,
            0U,
            pHeader->headerSize),
        pstatesVbiosBoardObjGrpSrcInit_6X_exit);

    // Assert that the entry sizes are larger than the minimum support
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeader->baseEntrySize >= PSTATES_VBIOS_6X_BASE_ENTRY_SIZE_2) &&
        (pHeader->clockEntrySize >= PSTATES_VBIOS_6X_ENTRY_CLOCK_SIZE_06),
        FLCN_ERR_ILWALID_STATE,
        pstatesVbiosBoardObjGrpSrcInit_6X_exit);

    pSrc->super.type = BOARDOBJGRP_SRC_TYPE_VBIOS;
    pSrc->super.pHeader = pHeader;
    pSrc->super.numEntries = pHeader->baseEntryCount;

    pSrc->entriesOffset = pHeader->headerSize;
    pSrc->entrySize = pHeader->baseEntrySize;
    pSrc->entryStride = pHeader->baseEntrySize +
        pHeader->clockEntrySize * pHeader->clockEntryCount;
    pSrc->dirtId = LW_GFW_DIRT_PERFORMANCE_TABLE;
    pSrc->version = pHeader->version;

pstatesVbiosBoardObjGrpSrcInit_6X_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
