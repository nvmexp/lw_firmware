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
 * @brief   Data structure representing an abstract source of POR data for a
 *          @ref BOARDOBJGRP
 */

#ifndef BOARDOBJGRP_SRC_BASE_H
#define BOARDOBJGRP_SRC_BASE_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @defgroup    BOARDOBJGRP_SRC_TYPE
 *
 * @brief   Define the various types of @ref BOARDOBJGRP_SRC objects
 *
 * @details
 *      BOARDOBJGRP_SRC_TYPE_VBIOS
 *          Data sourced from the VBIOS
 *@{
 */
typedef LwU8 BOARDOBJGRP_SRC_TYPE;
#define BOARDOBJGRP_SRC_TYPE_VBIOS                                          (0U)
/*!@}*/

/*!
 * Structure abstracting the source of a @ref BOARDOBJGRP
 */
typedef struct
{
    /*!
     * Type of the source.
     */
    BOARDOBJGRP_SRC_TYPE type;

    /*!
     * Pointer to the source header data for the @ref BOARDOBJGRP
     */
    const void *pHeader;

    /*!
     * Number of object entries for this source.
     */
    LwBoardObjIdx numEntries;
} BOARDOBJGRP_SRC;

/*!
 * Structure representing the header/group-level data for a @ref BOARDOBJGRP
 */
typedef struct
{
    /*!
     * Pointer to the source header data
     */
    const void *pHeader;
} BOARDOBJGRP_SRC_HEADER_DATA;

/*!
 * Structure representing the entry/group-level data for a single @ref BOARDOBJ
 */
typedef struct
{
    /*!
     * Pointer to the source entry data
     */
    const void *pEntry;
} BOARDOBJGRP_SRC_ENTRY_DATA;

/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Returns the number of object entries for the given source
 *
 * @param[in]   pSrc        Pointer to source for which to retrieve number of
 *                          entries
 * @param[out]  pNumEntries Pointer to header data to populate
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcNumEntriesGet
(
    const BOARDOBJGRP_SRC *pSrc,
    LwBoardObjIdx *pNumEntries
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pNumEntries != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcNumEntriesGet_exit);

    *pNumEntries = pSrc->numEntries;

boardObjGrpSrcNumEntriesGet_exit:
    return status;
}

/*!
 * @brief   Retrieves the header/group data for a given @ref BOARDOBJGRP_SRC
 *
 * @param[in]   pSrc        Pointer to source from which to retrieve header data
 * @param[out]  pHeaderData Pointer to header data to populate
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcHeaderDataGet
(
    const BOARDOBJGRP_SRC *pSrc,
    BOARDOBJGRP_SRC_HEADER_DATA *pHeaderData
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pHeaderData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcHeaderDataGet_exit);

    pHeaderData->pHeader = pSrc->pHeader;

boardObjGrpSrcHeaderDataGet_exit:
    return status;
}

/*!
 * @brief   Retrieves an object entry's data for a given @ref BOARDOBJGRP_SRC
 *
 * @param[in]   pSrc        Pointer to source from which to retrieve header data
 * @param[out]  pEntryData  Pointer to entry data to populate
 * @param[in]   entryIdx    Index of the entry for which to retrieve data
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcEntryDataGet
(
    const BOARDOBJGRP_SRC *pSrc,
    BOARDOBJGRP_SRC_ENTRY_DATA *pEntryData,
    LwBoardObjIdx entryIdx
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pEntryData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcEntryDataGet_exit);

    // Nothing to do in base class

boardObjGrpSrcEntryDataGet_exit:
    return status;
}

#endif // BOARDOBJGRP_SRC_BASE_H
