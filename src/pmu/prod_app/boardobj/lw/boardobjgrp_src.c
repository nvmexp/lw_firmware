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
 * @copydoc boardobjgrp_src.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp_src.h"

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
/*!
 * @copydoc boardObjGrpSrcVbiosHeaderDataGet
 *
 * @note    Wrapper to extract the proper arguments from the union
 */
static FLCN_STATUS s_boardObjGrpSrlwnionHeaderDataGetVbios(
    const BOARDOBJGRP_SRC_UNION *pSrc, BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "s_boardObjGrpSrlwnionHeaderDataGetVbios");

/*!
 * @copydoc boardObjGrpSrcVbiosEntryDataGet
 *
 * @note    Wrapper to extract the proper arguments from the union
 */
static FLCN_STATUS s_boardObjGrpSrlwnionEntryDataGetVbios(
    const BOARDOBJGRP_SRC_UNION *pSrc, BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData, LwBoardObjIdx entryIdx)
    GCC_ATTRIB_SECTION("imem_libBoardObj", "s_boardObjGrpSrlwnionEntryDataGetVbios");

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Compile-Time Asserts ---------------------------- */
/* ------------------------ Public Functions -------------------------------- */
FLCN_STATUS
boardObjGrpSrlwnionHeaderDataGet
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pHeaderData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrlwnionHeaderDataGet_exit);

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (pSrc->type)
    {
        case BOARDOBJGRP_SRC_TYPE_VBIOS:
            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS))
            {
                status = s_boardObjGrpSrlwnionHeaderDataGetVbios(
                    pSrc, pHeaderData);
            }
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        boardObjGrpSrlwnionHeaderDataGet_exit);

boardObjGrpSrlwnionHeaderDataGet_exit:
    return status;
}

FLCN_STATUS
boardObjGrpSrlwnionEntryDataGet
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData,
    LwBoardObjIdx entryIdx
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pSrc != NULL) &&
        (pEntryData != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        boardObjGrpSrlwnionEntryDataGet_exit);

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (pSrc->type)
    {
        case BOARDOBJGRP_SRC_TYPE_VBIOS:
            if (PMUCFG_FEATURE_ENABLED(PMU_BOARDOBJGRP_SRC_VBIOS))
            {
                status = s_boardObjGrpSrlwnionEntryDataGetVbios(
                    pSrc, pEntryData, entryIdx);
            }
            break;
        default:
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        status,
        boardObjGrpSrlwnionEntryDataGet_exit);

boardObjGrpSrlwnionEntryDataGet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
static FLCN_STATUS
s_boardObjGrpSrlwnionHeaderDataGetVbios
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    BOARDOBJGRP_SRC_HEADER_DATA_UNION *pHeaderData
)
{
    FLCN_STATUS  status;
    const BOARDOBJGRP_SRC_VBIOS *pSrcVbios;
    BOARDOBJGRP_SRC_HEADER_DATA_VBIOS *pHeaderDataVbios;

    // Retrieve the pointers
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrlwniolwbiosGetConst(pSrc, &pSrcVbios),
        s_boardObjGrpSrlwnionHeaderDataGetVbios_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcHeaderDataUniolwbiosGet(pHeaderData, &pHeaderDataVbios),
        s_boardObjGrpSrlwnionHeaderDataGetVbios_exit);

    // Call the implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcVbiosHeaderDataGet(pSrcVbios, pHeaderDataVbios),
        s_boardObjGrpSrlwnionHeaderDataGetVbios_exit);

s_boardObjGrpSrlwnionHeaderDataGetVbios_exit:
    return status;
}

static FLCN_STATUS
s_boardObjGrpSrlwnionEntryDataGetVbios
(
    const BOARDOBJGRP_SRC_UNION *pSrc,
    BOARDOBJGRP_SRC_ENTRY_DATA_UNION *pEntryData,
    LwBoardObjIdx entryIdx
)
{
    FLCN_STATUS  status;
    const BOARDOBJGRP_SRC_VBIOS *pSrcVbios;
    BOARDOBJGRP_SRC_ENTRY_DATA_VBIOS *pEntryDataVbios;

    // Retrieve the pointers
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrlwniolwbiosGetConst(pSrc, &pSrcVbios),
        s_boardObjGrpSrlwnionEntryDataGetVbios_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcEntryDataUniolwbiosGet(pEntryData, &pEntryDataVbios),
        s_boardObjGrpSrlwnionEntryDataGetVbios_exit);

    // Call the implementation
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpSrcVbiosEntryDataGet(pSrcVbios, pEntryDataVbios, entryIdx),
        s_boardObjGrpSrlwnionEntryDataGetVbios_exit);

s_boardObjGrpSrlwnionEntryDataGetVbios_exit:
    return status;
}
