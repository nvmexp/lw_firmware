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
 * @note    Provides unions of all possible source structures so that it can be
 *          used generically by clients.
 */

#ifndef BOARDOBJGRP_SRC_H
#define BOARDOBJGRP_SRC_H

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src_base.h"
#include "boardobj/boardobjgrp_src_vbios.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Structure abstracting the source of a @ref BOARDOBJGRP
 */
typedef union
{
    /*!
     * Type of the source.
     *
     * @note Common to all members, so always valid.
     */
    BOARDOBJGRP_SRC_TYPE type;

    /*!
     * Base implementation.
     */
    BOARDOBJGRP_SRC base;

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    /*!
     * VBIOS implementation.
     *
     * @note    Valid when @ref BOARDOBJGRP_SRC_UNION::type is
     *          @ref BOARDOBJGRP_SRC_TYPE_VBIOS
     */
    BOARDOBJGRP_SRC_VBIOS vbios;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
} BOARDOBJGRP_SRC_UNION;

/*!
 * Structure representing the header/group-level data for a @ref BOARDOBJGRP
 */
typedef union
{
    /*!
     * Base data
     */
    BOARDOBJGRP_SRC_HEADER_DATA base;

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    /*!
     * VBIOS implementation.
     *
     * @note    Valid when associated @ref BOARDOBJGRP_SRC_UNION::type is
     *          @ref BOARDOBJGRP_SRC_TYPE_VBIOS
     */
    BOARDOBJGRP_SRC_HEADER_DATA_VBIOS vbios;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
} BOARDOBJGRP_SRC_HEADER_DATA_UNION;

/*!
 * Structure representing the entry/group-level data for a single @ref BOARDOBJ
 */
typedef union
{
    /*!
     * Base data
     */
    BOARDOBJGRP_SRC_ENTRY_DATA base;

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    /*!
     * VBIOS implementation.
     *
     * @note    Valid when associated @ref BOARDOBJGRP_SRC_UNION::type is
     *          @ref BOARDOBJGRP_SRC_TYPE_VBIOS
     */
    BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS vbios;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
} BOARDOBJGRP_SRC_ENTRY_DATA_UNION;

/* ------------------------ Compile-Time Asserts ---------------------------- */
//
// If BOARDOBJGRP_SRC is enabled, we need at least one concrete implementation
// supported, so assert that here.
//
ct_assert(
    !PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC) ||
    PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS));

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc boardObjGrpSrcHeaderDataGet
 *
 * @note    Wrapper to call the type-specific implementation.
 */
FLCN_STATUS boardObjGrpSrlwnionHeaderDataGet(
    const BOARDOBJGRP_SRC_UNION *pSrc, BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "boardObjGrpSrlwnionHeaderDataGet");

/*!
 * @copydoc boardObjGrpSrcEntryDataGet
 *
 * @note    Wrapper to call the type-specific implementation.
 */
FLCN_STATUS boardObjGrpSrlwnionEntryDataGet(
    const BOARDOBJGRP_SRC_UNION *pSrc, BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData, LwBoardObjIdx entryIdx)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "boardObjGrpSrlwnionEntryDataGet");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Returns @ref BOARDOBJGRP_SRC_UNION::vbios, if enabled
 *
 * @param[in]   pSrc        Pointer from which to retrieve member
 * @param[out]  ppSrcVbios  Pointer into which to store pointer to pSrc->vbios,
 *                          if enabled; set to NULL otherwise
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrlwniolwbiosGet
(
    BOARDOBJGRP_SRC_UNION *pSrc,
    BOARDOBJGRP_SRC_VBIOS **ppSrcVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (ppSrcVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrlwniolwbiosGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppSrcVbios = &pSrc->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppSrcVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrlwniolwbiosGet_exit:
    return status;
}

/*!
 * @copydoc boardObjGrpSrlwniolwbiosGet
 *
 * @note    Variant to provide a const @ref BOARDOBJGRP_SRC_UNION and retrieve a
 *          const @ref BOARDOBJGRP_SRC_VBIOS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrlwniolwbiosGetConst
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    const BOARDOBJGRP_SRC_VBIOS **ppSrcVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (ppSrcVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrlwniolwbiosGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppSrcVbios = &pSrc->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppSrcVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrlwniolwbiosGetConst_exit:
    return status;
}

/*!
 * @brief   Returns @ref BOARDOBJGRP_SRC_HEADER_DATA_UNION::vbios, if enabled
 *
 * @param[in]   pHeaderData         Pointer from which to retrieve member
 * @param[out]  ppHeaderDataVbios   Pointer into which to store pointer to
 *                                  pHeaderData->vbios, if enabled; set to
 *                                  NULL otherwise
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcHeaderDataUniolwbiosGet
(
    BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData,
    BOARDOBJGRP_SRC_HEADER_DATA_VBIOS **ppHeaderDataVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeaderData != NULL) &&
        (ppHeaderDataVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcHeaderDataUniolwbiosGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppHeaderDataVbios = &pHeaderData->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppHeaderDataVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrcHeaderDataUniolwbiosGet_exit:
    return status;
}

/*!
 * @copydoc boardObjGrpSrcHeaderDataUniolwbiosGet
 *
 * @note    Variant to provide a const @ref BOARDOBJGRP_SRC_HEADER_DATA_UNION
 *          and retrieve a const @ref BOARDOBJGRP_SRC_HEADER_DATA_VBIOS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcHeaderDataUniolwbiosGetConst
(
    const BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData,
    const BOARDOBJGRP_SRC_HEADER_DATA_VBIOS **ppHeaderDataVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pHeaderData != NULL) &&
        (ppHeaderDataVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcHeaderDataUniolwbiosGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppHeaderDataVbios = &pHeaderData->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppHeaderDataVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrcHeaderDataUniolwbiosGetConst_exit:
    return status;
}

/*!
 * @brief   Returns @ref BOARDOBJGRP_SRC_ENTRY_DATA_UNION::vbios, if enabled
 *
 * @param[in]   pEntryData         Pointer from which to retrieve member
 * @param[out]  ppEntryDataVbios   Pointer into which to store pointer to
 *                                  pEntryData->vbios, if enabled; set to
 *                                  NULL otherwise
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcEntryDataUniolwbiosGet
(
    BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData,
    BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS **ppEntryDataVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pEntryData != NULL) &&
        (ppEntryDataVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcEntryDataUniolwbiosGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppEntryDataVbios = &pEntryData->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppEntryDataVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrcEntryDataUniolwbiosGet_exit:
    return status;
}

/*!
 * @copydoc boardObjGrpSrcEntryDataUniolwbiosGet
 *
 * @note    Variant to provide a const @ref BOARDOBJGRP_SRC_ENTRY_DATA_UNION
 *          and retrieve a const @ref BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrcEntryDataUniolwbiosGetConst
(
    const BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData,
    const BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS **ppEntryDataVbios
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pEntryData != NULL) &&
        (ppEntryDataVbios != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrcEntryDataUniolwbiosGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppEntryDataVbios = &pEntryData->vbios;
#else // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)
    *ppEntryDataVbios = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS)

boardObjGrpSrcEntryDataUniolwbiosGetConst_exit:
    return status;
}

/*!
 * @copydoc boardObjGrpSrcNumEntriesGet
 *
 * @note    This assumes that @ref boardObjGrpSrcNumEntriesGet is something of a
 *          non-virtual interface
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
boardObjGrpSrlwnionNumEntriesGet
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    LwBoardObjIdx *pNumEntries
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pNumEntries != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrlwnionNumEntriesGet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcNumEntriesGet(&pSrc->base, pNumEntries),
        boardObjGrpSrlwnionNumEntriesGet_exit);

boardObjGrpSrlwnionNumEntriesGet_exit:
    return status;
}

#endif // BOARDOBJGRP_SRC_H
