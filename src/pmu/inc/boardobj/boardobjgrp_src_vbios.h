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
 * @copydoc boardobjgrp_src_base.h
 *
 * @note    Concrete implementation to represent a @ref BOARDOBJGRP sourced from
 *          the VBIOS.
 */

#ifndef BOARDOBJGRP_SRC_VBIOS_H
#define BOARDOBJGRP_SRC_VBIOS_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "data_id_reference_table_addendum.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src_base.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure abstracting the source of a @ref BOARDOBJGRP
 */
typedef struct
{
    /*!
     * Base class.
     */
    BOARDOBJGRP_SRC super;

    /*!
     * Offset of the entries within the table
     */
    LwLength entriesOffset;

    /*!
     * Size of an individual entry in the table
     */
    LwLength entrySize;

    /*!
     * Stride between entries in the table
     *
     * @note    Normally only different from
     *          @ref BOARDOBJGRP_SRC_VBIOS::entryStride in the case where there
     *          are "sub-entries" per entry.
     */
    LwLength entryStride;

    /*!
     * DIRT of the VBIOS table source the @ref BOARDOBJGRP
     */
    LW_GFW_DIRT dirtId;

    /*!
     * Version of the VBIOS table
     */
    LwU8 version;
} BOARDOBJGRP_SRC_VBIOS;

/*!
 * Structure representing the header/group-level data for a @ref BOARDOBJGRP
 */
typedef struct
{
    /*!
     * Base class.
     */
    BOARDOBJGRP_SRC_HEADER_DATA super;
} BOARDOBJGRP_SRC_HEADER_DATA_VBIOS;

/*!
 * Structure representing the entry/group-level data for a single @ref BOARDOBJ
 */
typedef struct
{
    /*!
     * Base class.
     */
    BOARDOBJGRP_SRC_ENTRY_DATA super;

    /*!
     * Offset of this entry within the VBIOS table.
     */
    LwLength entryOffset;
} BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS;

/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc boardObjGrpSrcEntryDataGet
 *
 * @note    @ref BOARDOBJGRP_SRC_VBIOS implementation
 */
FLCN_STATUS boardObjGrpSrcVbiosEntryDataGet(
    const BOARDOBJGRP_SRC_VBIOS *pSrc, BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS *pEntryData, LwBoardObjIdx entryIdx)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "boardObjGrpSrcVbiosEntryDataGet");

/*!
 * @brief   Helper function for the common case of retrieving a "sub-entry" from
 *          an object's entry in the VBIOS
 *
 * @param[in]   pSrc            Pointer to source from which to retrieve
 *                              sub-entry
 * @param[in]   pEntryData      Pointer to entry data for entry from which to
 *                              retrieve sub-entry
 * @param[in]   ppSubEntry      Pointer to pointer into which to store pointer
 *                              to sub-entry
 * @param[in]   subEntryIdx     Index of the sub-entry within the entry
 * @param[in]   subEntrySize    Size of the sub-entry
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 * @return  Others                          Errors propagated from callees.
 */
FLCN_STATUS boardObjGrpSrcVbiosEntryDataSubEntryGet(
    const BOARDOBJGRP_SRC_VBIOS *pSrc,
    const BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS *pEntryData,
    const void **ppSubEntry,
    LwU32 subEntryIdx,
    LwLength subEntrySize)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "boardObjGrpSrcVbiosEntryDataSubEntryGet");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @copydoc boardObjGrpSrcHeaderDataGet
 *
 * @note    @ref BOARDOBJGRP_SRC_VBIOS implementation
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcVbiosHeaderDataGet
(
    const BOARDOBJGRP_SRC_VBIOS *pSrc,
    BOARDOBJGRP_SRC_HEADER_DATA_VBIOS *pHeaderData
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pHeaderData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcVbiosHeaderDataGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcHeaderDataGet(&pSrc->super, &pHeaderData->super),
        boardObjGrpSrcVbiosHeaderDataGet_exit);

    // No VBIOS-specific implementation details

boardObjGrpSrcVbiosHeaderDataGet_exit:
    return status;
}

/*!
 * @copydoc boardObjGrpSrcVbiosEntryDataSubEntryGet
 *
 * @note    This is a wrapper that allows for ppSubEntry to be passed as a
 *          pointer to a typed variable, rather than a pointer to a void
 *          pointer, to avoid the need for explicit casting.
 */
#define boardObjGrpSrcVbiosEntryDataSubEntryGetTyped(_pSrc, _pEntryData, _ppSubEntry, _subEntryIdx, _subEntrySize) \
    ({ \
        __label__ boardObjGrpSrcVbiosEntryDataSubEntryGetTyped_exit; \
        FLCN_STATUS boardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalStatus; \
        const void *pBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry; \
        typeof(_ppSubEntry) ppBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry = \
            (_ppSubEntry); \
    \
        PMU_ASSERT_TRUE_OR_GOTO(boardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalStatus, \
            (ppBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry != NULL), \
            FLCN_ERR_ILWALID_ARGUMENT, \
            boardObjGrpSrcVbiosEntryDataSubEntryGetTyped_exit); \
    \
        PMU_ASSERT_OK_OR_GOTO(boardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalStatus, \
            boardObjGrpSrcVbiosEntryDataSubEntryGet( \
                (_pSrc),\
                (_pEntryData), \
                &pBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry, \
                (_subEntryIdx), \
                (_subEntrySize)), \
            boardObjGrpSrcVbiosEntryDataSubEntryGetTyped_exit); \
    \
        *ppBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry = \
            pBoardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalSubEntry; \
    \
    boardObjGrpSrcVbiosEntryDataSubEntryGetTyped_exit: \
        boardObjGrpSrcVbiosEntryDataSubEntryGetTypedLocalStatus; \
    })

#endif // BOARDOBJGRP_SRC_VBIOS_H
