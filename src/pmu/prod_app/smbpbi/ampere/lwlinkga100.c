/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwlinkga100.c
 * @brief   PMU HAL functions for GA100 SMBPBI
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "pmu_objsmbpbi.h"
#include "pmu_objbif.h"
#include "pmu_oslayer.h"
#include "dev_master.h"
#include "dev_fuse.h"
#include "ioctrl_discovery.h"
#include "dev_lwlipt_lnk_ip.h"
#include "dev_lwldl_ip.h"
#include "dev_lwltlc_ip.h"

/* ------------------------ Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"
#include "config/g_bif_private.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
// LWLink Throughput counter type indexes based on raw/data flits
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA  0
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW   2

/* ------------------------- Static Functions ------------------------------ */
static LwBool s_smbpbiIsLwlinkAccessible(LwU32 linkNum, LwU32 lwliptIdx, LwU32 localLinkIdx)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiIsLwlinkAccessible");
static LwU8 s_smbpbiGetIndexesFromLwlink(LwU32 linkNum, LwU32 *pLwliptIdx, LwU32 *pLocalLinkIdx)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetIndexesFromLwlink");
static LwBool s_smbpbiLwlinkPageInNumPages(LwU32 pageNum, LwU32 numLinks, LwU32 numLinksPerPage)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiLwlinkPageInNumPages");
static LwU8 s_smbpbiColwertLwLinkState(LwU32 linkState, LwU32 *pColwertedState)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiColwertLwLinkState");
static LwU8 s_smbpbiGetLwlinkStatePerLink(LwU32 linkNum, LwU32 *pState)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkStatePerLink");
static LwU8 s_smbpbiIsLwlinkStateActive(LwU32 linkNum, LwBool *pIsLinkActive)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiIsLwlinkStateActive");
static LwU8 s_smbpbiColwertLwLinkLineRate (LwU8 linkLineRate, LwU32 *pColwertedLineRate)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiColwertLwLinkLineRate");
static LwU8 s_smbpbiGetLwlinkLineRatePerLink(LwU32 linkNum, LwU32 *pLinkLineRate)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkLineRatePerLink");
static LwU8 s_smbpbiGetLwlinkErrorCountPerLink(LwU32 linkNum, LwU8 errType, LwU32 *pErrorCount)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkErrorCountPerLink");
static void s_smbpbiInitLwlinkSublinkWidth(LwU32 *pSublinkWidth)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiInitLwlinkSublinkWidth");
static LwU8 s_smbpbiGetLwlinkSublinkWidthPerLink(LwU8 linkNum, LwU32 *pSublinkWidth)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkSublinkWidthPerLink");
static LwU8 s_smbpbiGetLwlinkThroughput(LwU8 arg2, LwU8 counterIdx,
                                        LwU32 *pCmd, LwU64 *pCounterVal)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkThroughput");
static LwU8 s_smbpbiGetLwlinkActiveLinkMask(LwU32 *pLinkMask)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkActiveLinkMask");
static LwU8 s_smbpbiGetLwlinkThroughputCounterValue(LwU32 linkId, LwU32 loOffset,
                                                    LwU32 hiOffset, LwU64 *pCounterVal)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkThroughputCounterValue");
static void s_smbpbiGetLwlinkThroughputSnapshot(LwU32 linkMask)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkThroughputSnapshot");
static LwU8 s_smbpbiGetLwlinkThroughputCounterKib(LwU8 linkId, LwU8 counterIdx, LwU64 *pCounter)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiGetLwlinkThroughputCounterKib");
static LwU8 s_smbpbiLwlinkCommandDispatch(LwU8  arg1, LwU32 link, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiLwlinkCommandDispatch");
static LwU8 s_smbpbiLwlinkAggregateCommandHandler(LwU8  arg1, LwU32 numLwlinks, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiLwlinkAggregateCommandHandler");
static LwU8 s_smbpbiFormatLwlinkQueryData(LwU32 arg1, LwU32 link, LwU32 inputData, LwU32 *pData)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiFormatLwlinkQueryData");
static LwU8 s_smbpbiLwlinkDataAndExtDataHandler(LwU32 arg1, LwU32 linkPage, LwU32 linksInPage,
                                             LwU32 numLwlinks, LwU32 *pVal, LwU32 *pExtVal)
    GCC_ATTRIB_SECTION("imem_smbpbiLwlink", "s_smbpbiLwlinkDataAndExtDataHandler");

/*!
 * @brief Checks if a queried LwLink can be accessed.
 *
 * @param[in]  lwliptIdx     index to the LWLIPT
 *                           containing the link
 * @param[in]  localLinkIdx  index to link within the
                             LWLIPT
 *
 * @return LW_FALSE if the checks fail,
 *         LW_TRUE otherwise
 */
static LwBool
s_smbpbiIsLwlinkAccessible
(
    LwU32  linkNum,
    LwU32  lwliptIdx,
    LwU32  localLinkIdx
)
{
    LwU32 reg;
    LwU32 fuseMask;
    LwU32 lwLiptLinkBase;
    LwU32 disabledLinkMask = 0;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, bif)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBif)
    };

    // Check if LwLink block is in reset
    reg = REG_RD32(BAR0, LW_PMC_ENABLE);
    if (FLD_TEST_DRF(_PMC, _ENABLE, _LWLINK, _DISABLED, reg))
    {
        return LW_FALSE;
    }

    // Read the fuse to get the floorswept IOCTRL
    fuseMask = DRF_VAL(_FUSE, _OPT_LWLINK_DISABLE, _DATA,
                       REG_RD32(BAR0, LW_FUSE_OPT_LWLINK_DISABLE));
    fuseMask &= DRF_MASK(LW_FUSE_OPT_LWLINK_DISABLE_DATA);

    // Check if link requested is floorswept
    if (BIT(lwliptIdx) & fuseMask)
    {
        return LW_FALSE;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        disabledLinkMask = bifGetDisabledLwlinkMask();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Check if the link is enabled in RM.
    if (disabledLinkMask & BIT(linkNum))
    {
        return LW_FALSE;
    }

    lwLiptLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLIPT_LNK_0);

    // Check if individual link is disabled
    reg = REG_RD32(BAR0, lwLiptLinkBase + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_MODE_CTRL);
    if (FLD_TEST_DRF(_LWLIPT_LNK, _CTRL_SYSTEM_LINK_MODE_CTRL, _LINK_DISABLE, _DISABLED, reg))
    {
        return LW_FALSE;
    }

    // Check if link is in Reset
    reg = REG_RD32(BAR0, lwLiptLinkBase + LW_LWLIPT_LNK_RESET_RSTSEQ_LINK_RESET);
    if (FLD_TEST_DRF(_LWLIPT_LNK, _RESET_RSTSEQ, _LINK_RESET_LINK_RESET, _ASSERT, reg))
    {
        return LW_FALSE;
    }

    return LW_TRUE;
}

/*!
 * @brief Colwerts the requested link to the appropriate
 *        LWLIPT and link index.
 *
 * @param[in]  linkNum        Requested link number
 * @param[out] pLwliptIdx     Pointer to LWLIPT index
 * @param[out] pLocalLinkIdx  Pointer to link index
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link number is out of bounds
 */
static LwU8
s_smbpbiGetIndexesFromLwlink
(
    LwU32 linkNum,
    LwU32 *pLwliptIdx,
    LwU32 *pLocalLinkIdx
)
{
    LwU8  status;
    LwU32 numLwlipt;
    LwU32 numLinkPerLwlipt;
    LwU8  lwliptIdx;
    LwU8  localLinkIdx;

    status = smbpbiGetLwlinkNumLwlipt_HAL(&Smbpbi, &numLwlipt);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetIndexesFromLwlink_exit;
    }

    status = smbpbiGetLwlinkNumLinkPerLwlipt_HAL(&Smbpbi, &numLinkPerLwlipt);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetIndexesFromLwlink_exit;
    }

    lwliptIdx    = linkNum / numLinkPerLwlipt;
    localLinkIdx = linkNum % numLinkPerLwlipt;

    if (lwliptIdx >= numLwlipt)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto s_smbpbiGetIndexesFromLwlink_exit;
    }

    *pLwliptIdx    = lwliptIdx;
    *pLocalLinkIdx = localLinkIdx;

s_smbpbiGetIndexesFromLwlink_exit:
    return status;
}

/*!
 * @brief Checks if requested LwLink page is inside the page range
 *        of LwLinks.
 *
 * The data returned from an LwLink info query may be packed, so
 * that one LwU32 could contain multiple LwLink data/state. This
 * checks if the requested LwLink page is within the range.
 *
 * @param[in] pageNum          Requested LwLink page
 * @param[in] numLinks         Total number of LwLinks
 * @param[in] numLinksPerPage  Number of LwLinks per Data page
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkState is invalid
 */
static LwBool
s_smbpbiLwlinkPageInNumPages
(
    LwU32 pageNum,
    LwU32 numLinks,
    LwU32 numLinksPerPage
)
{
    LwU32 numPages = LW_CEIL(numLinks, numLinksPerPage);

    if (pageNum >= numPages)
        return LW_FALSE;

    return LW_TRUE;
}

/*!
 * @brief Colwerts the LwLink state to LwLink MSGBOX state.
 *
 * @param[in]  linkState         Input link state
 * @param[out] pColwertedState   Pointer to the colwerted state output
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkState is invalid
 */
static LwU8
s_smbpbiColwertLwLinkState
(
    LwU32 linkState,
    LwU32 *pColwertedState
)
{
    switch (linkState)
    {
        case LW_LWLDL_TOP_LINK_STATE_STATE_INIT:
        case LW_LWLDL_TOP_LINK_STATE_STATE_SLEEP:
            *pColwertedState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_OFF;
            break;
        case LW_LWLDL_TOP_LINK_STATE_STATE_HWCFG:
        case LW_LWLDL_TOP_LINK_STATE_STATE_SWCFG:
        case LW_LWLDL_TOP_LINK_STATE_STATE_TRAIN:
            *pColwertedState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_SAFE;
            break;
        case LW_LWLDL_TOP_LINK_STATE_STATE_ACTIVE:
        case LW_LWLDL_TOP_LINK_STATE_STATE_RCVY_AC:
        case LW_LWLDL_TOP_LINK_STATE_STATE_RCVY_RX:
            *pColwertedState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_ACTIVE;
            break;
        case LW_LWLDL_TOP_LINK_STATE_STATE_FAULT:
            *pColwertedState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_ERROR;
            break;
        default:
            return LW_MSGBOX_CMD_STATUS_ERR_MISC;
    }

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the status for a single requested LwLink.
 *
 * @param[in]  linkNum   Input link number
 * @param[out] pState    Pointer to the colwerted status
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if linkNum is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link status is invalid
 */
static LwU8
s_smbpbiGetLwlinkStatePerLink
(
    LwU32 linkNum,
    LwU32 *pState
)
{
    LwU32 reg;
    LwU32 linkState;
    LwU32 lwliptIdx;
    LwU32 localLinkIdx;
    LwU8  status;
    LwU32 numLwlinks;
    LwU32 lwldlLinkBase;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkStatePerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiGetLwlinkStatePerLink_exit;
    }

    status = s_smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkStatePerLink_exit;
    }

    if (!s_smbpbiIsLwlinkAccessible(linkNum, lwliptIdx, localLinkIdx))
    {
        *pState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_OFF;
        status  = LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto s_smbpbiGetLwlinkStatePerLink_exit;
    }

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);

    // Get link state
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TOP_LINK_STATE);
    linkState = DRF_VAL(_LWLDL, _TOP_LINK_STATE, _STATE, reg);

    status = s_smbpbiColwertLwLinkState(linkState, pState);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkStatePerLink_exit;
    }

s_smbpbiGetLwlinkStatePerLink_exit:
    return status;
}

/*!
 * @brief Checks if the link state is active.
 *
 * @param[in]  linkNum          Input link number
 * @param[out] pIsLinkActive    Pointer to link state boolean
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link number goes out of bounds
 */
static LwU8
s_smbpbiIsLwlinkStateActive
(
    LwU32  linkNum,
    LwBool *pIsLinkActive
)
{
    LwU8  status;
    LwU32 linkState = LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID;

    status = s_smbpbiGetLwlinkStatePerLink(linkNum, &linkState);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiIsLwlinkStateActive_exit;
    }

    *pIsLinkActive = (linkState == LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_ACTIVE) ?
                      LW_TRUE : LW_FALSE;

s_smbpbiIsLwlinkStateActive_exit:
    return status;
}

/*!
 * @brief Colwerts the LwLink line rate to a Decimal output.
 *
 * @param[in]  linkLineRate        Input link line rate
 * @param[out] pColwertedLineRate  Pointer to the colwerted line rate output
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkLineRate is invalid
 */
static LwU8
s_smbpbiColwertLwLinkLineRate
(
    LwU8  linkLineRate,
    LwU32 *pColwertedLineRate
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    //
    // The ordering and indexing is based off the hwref defines
    // for LINK_CLK_CTRL_LINE_RATE
    //
    static const LwU32 lineRatesMbps[] = {
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_50000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_16000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_20000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_25000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_25781_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_32000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_40000_MBPS,
        LW_MSGBOX_DATA_LWLINK_LINE_RATE_53125_MBPS
    };

    if ((linkLineRate ==
        LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL_LINE_RATE_ILLEGAL_LINE_RATE) ||
        (linkLineRate >
         (LW_ARRAY_ELEMENTS(lineRatesMbps) - 1)))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto s_smbpbiColwertLwLinkLineRate_exit;
    }

    *pColwertedLineRate = lineRatesMbps[linkLineRate];

s_smbpbiColwertLwLinkLineRate_exit:
    return status;
}

/*!
 * @brief Gets the line rate of a single requested LwLink.
 *
 * @param[in]  linkNum        Input link number
 * @param[out] pLinkLineRate  Pointer to the colwerted line rate output
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if linkNum is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkNum is invalid
 */
static LwU8
s_smbpbiGetLwlinkLineRatePerLink
(
    LwU32 linkNum,
    LwU32 *pLinkLineRate
)
{
    LwU8   status;
    LwU32  reg;
    LwU32  lwLiptLinkBase;
    LwU32  lwliptIdx;
    LwU32  localLinkIdx;
    LwU8   linkLineRate;
    LwU32  numLwlinks;
    LwBool isLinkActive;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

    status = s_smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

    status = s_smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

    if (!isLinkActive)
    {
        *pLinkLineRate = LW_MSGBOX_DATA_LWLINK_LINE_RATE_00000_MBPS;
        status         =  LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

    lwLiptLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLIPT_LNK_0);

    //
    // Get link line rate of individual links.
    // Link line rate should all be the same inside an LWLIPT.
    //
    reg = REG_RD32(BAR0, lwLiptLinkBase + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL);
    linkLineRate = DRF_VAL(_LWLIPT_LNK,
                           _CTRL_SYSTEM_LINK_CLK_CTRL,
                           _LINE_RATE,
                           reg);

    //
    // The linkLineRate index is taken from the hwref LINK_CLK_CTRL_LINE_RATE define
    //
    status = s_smbpbiColwertLwLinkLineRate(linkLineRate, pLinkLineRate);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkLineRatePerLink_exit;
    }

s_smbpbiGetLwlinkLineRatePerLink_exit:
    return status;
}

/*!
 * @brief Gets the LwLink Error count per link.
 *
 * @param[in]  errType  Type of error
 * @param[in]  linkNum  Requested link number
 * @param[out] pData    Pointer to number of errors of the requested type for the given link
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success, or if link is not accessible
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      if linkNum is out of bounds
 */
static LwU8
s_smbpbiGetLwlinkErrorCountPerLink
(
    LwU32 linkNum,
    LwU8  errType,
    LwU32 *pErrorCount
)
{
    LwU8  status;
    LwU32 lwliptIdx    = 0;
    LwU32 localLinkIdx = 0;
    LwU32 lwldlLinkBase;
    LwU32 numLwlinks;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkErrorCountPerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiGetLwlinkErrorCountPerLink_exit;
    }

    status = s_smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkErrorCountPerLink_exit;
    }

    if (!s_smbpbiIsLwlinkAccessible(linkNum, lwliptIdx, localLinkIdx))
    {
        *pErrorCount = 0;
        return LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);
        
    smbpbiGetLwlinkLinkErrCount_HAL(&Smbpbi, lwldlLinkBase, 
                                    errType, pErrorCount);

s_smbpbiGetLwlinkErrorCountPerLink_exit:
    return status;
}

/*!
 * @brief Init sublink width data to Invalid
 *
 * @param[out] pSublinkWidth   Pointer to Sublink width output
 *
 */
static void
s_smbpbiInitLwlinkSublinkWidth
(
    LwU32 *pSublinkWidth
)
{
    LwU32 val = 0;

    val = FLD_SET_DRF(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_TX_WIDTH, _ILWALID, val);
    val = FLD_SET_DRF(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_RX_WIDTH, _ILWALID, val);

    *pSublinkWidth= val;
}

/*!
 * @brief Gets the RX/TX sublink width for a single requested LwLink.
 *
 * @param[in]  linkNum         Input link number
 * @param[out] pSublinkWidth   Pointer to the RM/TX sublink width
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link status is invalid
 */
static LwU8
s_smbpbiGetLwlinkSublinkWidthPerLink
(
    LwU8  linkNum,
    LwU32 *pSublinkWidth
)
{
    LwU8   status;
    LwU32  sublinkWidth = 0;
    LwU32  lwliptIdx;
    LwU32  localLinkIdx;
    LwU8   txWidth;
    LwU8   rxWidth;
    LwU32  numLwlinks;
    LwBool isLinkActive;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    status = s_smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    status = s_smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    if (!isLinkActive)
    {
        // Set width as invalid
        s_smbpbiInitLwlinkSublinkWidth(pSublinkWidth);
        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    status = smbpbiGetLwlinkSublinkLaneWidth_HAL(&Smbpbi,
                                                 lwliptIdx,
                                                 localLinkIdx,
                                                 &txWidth,
                                                 &rxWidth);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    sublinkWidth = FLD_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_TX_WIDTH,
                                   txWidth, sublinkWidth);
    sublinkWidth = FLD_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_RX_WIDTH,
                                   rxWidth, sublinkWidth);

    *pSublinkWidth = sublinkWidth;

s_smbpbiGetLwlinkSublinkWidthPerLink_exit:
    return status;
}

/*!
 * @brief Gets a 32-bit mask of active links
 *
 * @param[out] pLinkMask  Pointer to the 32-bit link mask
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC if there is an internal problem getting the mask
 *         LW_MSGBOX_CMD_STATUS_SUCCESS if no problems getting the mask
 */
static LwU8
s_smbpbiGetLwlinkActiveLinkMask
(
    LwU32 *pLinkMask
)
{
    LwU8   status;
    LwU32  linkNum = 0;
    LwU32  linkMask = 0;
    LwU32  numLwlinks = 0;
    LwBool isLinkActive = LW_FALSE;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkActiveLinkMask_exit;
    }

    for (linkNum = 0; linkNum < numLwlinks; linkNum++)
    {
        status = s_smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiGetLwlinkActiveLinkMask_exit;
        }

        if (isLinkActive)
        {
            linkMask |= BIT(linkNum);
        }
    }

    *pLinkMask = linkMask;

s_smbpbiGetLwlinkActiveLinkMask_exit:
    return status;
}

/*!
 * @brief Reads a 64-bit throughput counter value
 *
 * @param[in]  linkId       Link number for which to read counter value
 * @param[in]  loOffset     Offset of the lower 32-bit of counter value register
 * @param[in]  hiOffset     Offset of the upper 32-bit of counter value register
 * @param[out] pCounterVal  Pointer to the 64-bit counter value
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE if link is in reset/not accessible,
 *         LW_MSGBOX_CMD_STATUS_SUCCESS if counter is read successfully
 */
LwU8
s_smbpbiGetLwlinkThroughputCounterValue
(
    LwU32 linkId,
    LwU32 loOffset,
    LwU32 hiOffset,
    LwU64 *pCounterVal
)
{
    LwU32 counterLo = 0;
    LwU32 counterHi = 0;
    LwU32 lwliptIdx = 0;
    LwU32 localLinkIdx = 0;
    LwU32 lwltlcLinkBase;
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    status = s_smbpbiGetIndexesFromLwlink(linkId, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkThroughputCounterValue_exit;
    }

    if (!s_smbpbiIsLwlinkAccessible(linkId, lwliptIdx, localLinkIdx))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto s_smbpbiGetLwlinkThroughputCounterValue_exit;
    }

    lwltlcLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLTLC_0);

    //
    // The counter must be read in hi-lo-hi since
    // lwlink traffic may be flowing while monitoring
    // when driver is loaded
    //
    do
    {
        counterHi = REG_RD32(BAR0, lwltlcLinkBase + hiOffset);
        counterLo = REG_RD32(BAR0, lwltlcLinkBase + loOffset);
    } while (counterHi != REG_RD32(BAR0, lwltlcLinkBase + hiOffset));

    *pCounterVal = (((LwU64)counterHi << 32ull) | counterLo);

s_smbpbiGetLwlinkThroughputCounterValue_exit:
    return status;
}

/*!
 * @brief Gets a single Lwlink Throughput counter for a link in KiB
 *
 * @param[in]  linkId     Link number
 * @param[in]  counterIdx Index of the counter type
 * @param[out] pCounter   Pointer to 64-bit counter value
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if the given link is unavailable or uninitialized
 */
static LwU8
s_smbpbiGetLwlinkThroughputCounterKib
(
    LwU8    linkId,
    LwU8    counterIdx,
    LwU64   *pCounter
)
{
    LwU64 counterValFlits = 0;
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    //
    // Checking for linkId sanity is not required here because
    // this function is called from the snapshot context where
    // we loop from 0 to (MAX_LINKS - 1). For per link throughput,
    // this check is already added in the PerLink handler.
    //

    switch (counterIdx)
    {
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_TX:
            status = s_smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_RX:
            status = s_smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_TX:
            status = s_smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_RX:
            status = s_smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         &counterValFlits);
            break;
    }

    //
    // Colwert flits to KiB units. 1 flit = 128 bits = 16 bytes.
    // counterValKib = (counterValFlits * 16) / 1024 = counterValFlits / 64;
    //
    *pCounter = counterValFlits >> 6;

    return status;
}

/*!
 * @brief Gets snapshot of all counters, all links
 *        and populate SmbpbiLwlink struct
 */
static void
s_smbpbiGetLwlinkThroughputSnapshot
(
    LwU32 linkMask
)
{
    LwU64 counterVal = 0;
    LwU8  linkIdx = 0;
    LwU8  counterIdx = 0;

    FOR_EACH_INDEX_IN_MASK(32, linkIdx, linkMask)
    {
        for (counterIdx = 0;
             counterIdx < LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX;
             counterIdx++)
        {
            LwU8 status;

            status = s_smbpbiGetLwlinkThroughputCounterKib(linkIdx, counterIdx,
                                                           &counterVal);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx] = 0;
                //
                // Reset the valid/last queried bits
                // for this unaccessible linkIdx/counterIdx
                //
                Smbpbi.lwlinkTpValidSampleMask[counterIdx] &= ~(BIT(linkIdx));
                Smbpbi.lwlinkTpLastQueriedMask[counterIdx] &= ~(BIT(linkIdx));
                continue;
            }
            SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx] = counterVal;

            // Set the valid bit for this linkIdx/counterIdx
            Smbpbi.lwlinkTpValidSampleMask[counterIdx] |= BIT(linkIdx);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * @brief Dispatches the requested Lwlink info query, and
 *        returns the corresponding data
 *
 * @param[in]  arg1     Lwlink info query command
 * @param[in]  linkNum  Input link number
 * @param[out] pData    Pointer to the output val
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if incorrect Lwlink info query or internal error
 */
static LwU8
s_smbpbiLwlinkCommandDispatch
(
    LwU8  arg1,
    LwU32 linkNum,
    LwU32 *pData
)
{
    LwU8 status;

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
        {
            LwBool isLinkActive;

            status = s_smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto s_smbpbiLwlinkCommandDispatch_exit;
            }

            *pData = (isLinkActive) ?
                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1_UP :
                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1_DOWN;

            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
        {
            status = s_smbpbiGetLwlinkLineRatePerLink(linkNum, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
        {
            status = s_smbpbiGetLwlinkStatePerLink(linkNum, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
        {
            status = s_smbpbiGetLwlinkSublinkWidthPerLink(linkNum, pData);
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
            break;
        }
    }

s_smbpbiLwlinkCommandDispatch_exit:
    return status;
}

/*!
 * @brief Obtains the aggregate output for the requested
 *        Lwlink info query, if the output for all the
 *        links are the same.
 *
 * @param[in]  arg1         Lwlink info query command
 * @param[in]  numLwlinks   total number of links
 * @param[out] pAggData     Pointer to the aggregate output
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if data output is not the same for all links
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if internal error
 */
static LwU8
s_smbpbiLwlinkAggregateCommandHandler
(
    LwU8  arg1,
    LwU32 numLwlinks,
    LwU32 *pAggData
)
{
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32 data;
    LwU32 prevData;
    LwU32 link;

    if (numLwlinks == 0U)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto s_smbpbiLwlinkAggregateCommandHandler_exit;
    }

    prevData = LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID;
    for (link = 0; link < numLwlinks; link++)
    {
        status = s_smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiLwlinkAggregateCommandHandler_exit;
        }

        if (prevData == LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID)
        {
            prevData = data;
        }
        else if (prevData != data)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            goto s_smbpbiLwlinkAggregateCommandHandler_exit;
        }
    }

    *pAggData = data;

s_smbpbiLwlinkAggregateCommandHandler_exit:
    return status;
}

/*!
 * @brief Formats the data for the requested Lwlink
 *        info query.
 *
 * @param[in]  arg1       Lwlink info query command
 * @param[in]  linkNum    Input link number
 * @param[in]  inputData  The input data to be processed/formatted
 * @param[out] pData      Pointer to the output data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if incorrect Lwlink info query
 */
static LwU8
s_smbpbiFormatLwlinkQueryData
(
    LwU32 arg1,
    LwU32 linkNum,
    LwU32 inputData,
    LwU32 *pFormattedData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
        {
            if (inputData == LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1_UP)
            {
                *pFormattedData |=
                    BIT(linkNum % LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1__SIZE);
            }
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
        {
            *pFormattedData = inputData;
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
        {
            *pFormattedData = FLD_IDX_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO,_LINK_STATE_V2,
                                linkNum % LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2__SIZE,
                                inputData,
                                *pFormattedData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
        {
            *pFormattedData = FLD_IDX_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_WIDTH,
                                linkNum % LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_WIDTH__SIZE,
                                inputData,
                                *pFormattedData);
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
            break;
        }
    }

    return status;
}

/*!
 * @brief Retrieves and formats the data/extData for the
 *        requested Lwlink info query.
 *
 * @param[in]  arg1         Lwlink info query command
 * @param[in]  linkPage     link page number
 * @param[in]  linksInPage  number of links per page
 * @param[in]  numLwlinks   total number of links
 * @param[out] pVal         Pointer to the output val
 * @param[out] pExtVal      Pointer to the output extVal
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if request link page is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if internal error
 */
static LwU8
s_smbpbiLwlinkDataAndExtDataHandler
(
    LwU32 arg1,
    LwU32 linkPage,
    LwU32 linksInPage,
    LwU32 numLwlinks,
    LwU32 *pVal,
    LwU32 *pExtVal
)
{
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU32 link;
    LwU32 linkStart;
    LwU32 linkStop;
    LwU32 data;

    linkStart = (linkPage * linksInPage *
                 LW_MSGBOX_DATA_LWLINK_INFO_DATA_WIDTH);
    linkStop  = linkStart + linksInPage;

    if (!s_smbpbiLwlinkPageInNumPages(linkPage,
                                      numLwlinks,
                                      linksInPage *
                                      LW_MSGBOX_DATA_LWLINK_INFO_DATA_WIDTH))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto s_smbpbiLwlinkDataAndExtDataHandler_exit;
    }

    // Get packed status for first data output
    for (link = linkStart;
         (link < numLwlinks) && (link < linkStop);
         link++)
    {
        status = s_smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiLwlinkDataAndExtDataHandler_exit;
        }

        s_smbpbiFormatLwlinkQueryData(arg1, link, data, pVal);
    }

    // Get packed status for second extended data output
    for (;
         (link < numLwlinks) &&
         (link < (linkStop + linksInPage));
         link++)
    {
        status = s_smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiLwlinkDataAndExtDataHandler_exit;
        }

        s_smbpbiFormatLwlinkQueryData(arg1, link, data, pExtVal);
    }

s_smbpbiLwlinkDataAndExtDataHandler_exit:
    return status;
}

/*!
 * @brief Gets per link or aggregate Lwlink Throughput Counter Value
 *        and the change in value since last query in units
 *        specified in the pCmd parameter
 *
 * @param[in]  arg2         Arg2 indicating link number
 *                          (Aggregate requested if 0xFF)
 * @param[in]  counterIdx   Index of counter type to query
 * @param[out] pCmd         Pointer to cmd parameter that holds
 *                          counter delta and other flags
 * @param[out] pCounterVal  Pointer to 64-bit throughput value
 *                          that will be populated in pData and
 *                          pExtData in the caller
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if the given link is unavailable or uninitialized
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if requested link number is out of bounds
 */
static LwU8
s_smbpbiGetLwlinkThroughput
(
    LwU8  arg2,
    LwU8  counterIdx,
    LwU32 *pCmd,
    LwU64 *pCounterVal
)
{
    LwU8   status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    LwU8   i = 0;
    LwU8   linkIdx = 0;
    LwBool bDeltaIlwalid = LW_TRUE;
    LwU32  cmd = *pCmd;
    LwU32  linkMask = 0;
    LwU32  activeLinkMask = 0;
    LwU64  lastQueried = 0;
    LwU64  finalCounterValKib = 0;
    LwU64  counterValKib = 0;
    LwU64  counterDelta = 0;
    LwU64  deltaLimit = BIT64(20) - 1;

    // Initialize cmd to default state
    cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                      _TRUE, cmd);
    cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_NEW_SAMPLE,
                      _FALSE, cmd);
    cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_GRANULARITY,
                      _KIB, cmd);
    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_DELTA,
                          0, cmd);

    // Get link mask of all active links
    status = s_smbpbiGetLwlinkActiveLinkMask(&activeLinkMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto s_smbpbiGetLwlinkThroughput_exit;
    }

    // Get linkMask for requested links
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        linkMask = activeLinkMask;
    }
    else
    {
        LwU32  numLwlinks = 0;

        status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto s_smbpbiGetLwlinkThroughput_exit;
        }

        if (arg2 >= numLwlinks)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            goto s_smbpbiGetLwlinkThroughput_exit;
        }

        linkMask = BIT(arg2);

        //
        // If a link is not accessible and a new snapshot needs to be taken,
        // return early with NOT_AVAILABLE without taking a snapshot.
        //
        if ((!((Smbpbi.lwlinkTpValidSampleMask[counterIdx] & linkMask) == linkMask)) &&
            (!((activeLinkMask & linkMask) == linkMask)))
        {
            //
            // If this link was once active and now not accessible
            // due to some reason, reset the flags for this link
            // so that next time if/when the link comes back up again,
            // it can start reporting throughput with a fresh init state.
            //
            Smbpbi.lwlinkTpLastQueriedMask[counterIdx] &= ~(linkMask);
            Smbpbi.lwlinkTpValidSampleMask[counterIdx] &= ~(linkMask);

            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;

            goto s_smbpbiGetLwlinkThroughput_exit;
        }
    }

    // Check if deltas for these counters are valid
    if ((Smbpbi.lwlinkTpLastQueriedMask[counterIdx] & linkMask) == linkMask)
    {
        //
        // It's not enough to check just this condition for validity of delta.
        // It is possible that delta can still be invalid if it overflows.
        // So set deltaIlwalid bit to FALSE in cmd later after delta is computed 
        // instead of setting cmd in this place.
        //
        bDeltaIlwalid = LW_FALSE;

        // Get last queried
        FOR_EACH_INDEX_IN_MASK(32, linkIdx, linkMask)
        {
            lastQueried += SmbpbiLwlink[linkIdx].lwlinkTpLastQueried[counterIdx];
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    //
    // Check if samples in current snapshot are valid
    // If invalid, a new snapshot is needed and update cmd
    // with NEW_SAMPLE = TRUE. Else NEW_SAMPLE is FALSE by default.
    //
    if ((Smbpbi.lwlinkTpValidSampleMask[counterIdx] & linkMask) != linkMask)
    {
        //
        // A new snapshot is needed.
        // The snapshot function also sets/resets valid bits
        // for all links/counters depending on accessibility.
        //
        s_smbpbiGetLwlinkThroughputSnapshot(activeLinkMask);

        //
        // If this is not an aggregate query and even after snapshot,
        // the ValidSample bit is still unset, this link likely went down recently
        // since the "active" check in the beginning of the function passed.
        //
        if ((arg2 != LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE) &&
            ((Smbpbi.lwlinkTpValidSampleMask[counterIdx] & linkMask) != linkMask))
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            goto s_smbpbiGetLwlinkThroughput_exit;
        }

        // Values will be taken from a new sample
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_NEW_SAMPLE,
                          _TRUE, cmd);
    }

    // Get aggregate values over all requested links
    FOR_EACH_INDEX_IN_MASK(32, linkIdx, linkMask)
    {
        // Get counter value from a snapshot
        counterValKib = SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx];

        // Add it to aggregate
        finalCounterValKib += counterValKib;

        // Populate last queried for this counter
        SmbpbiLwlink[linkIdx].lwlinkTpLastQueried[counterIdx] = counterValKib;

        // Reset valid sample bit for this counter
        Smbpbi.lwlinkTpValidSampleMask[counterIdx] &= ~(BIT(linkIdx));

        // Set last queried bit for this counter so future deltas will be valid
        Smbpbi.lwlinkTpLastQueriedMask[counterIdx] |= BIT(linkIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (bDeltaIlwalid)
    {
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                          _TRUE, cmd);
        goto s_smbpbiGetLwlinkThroughput_exit;
    }

    // Compute delta
    counterDelta = finalCounterValKib - lastQueried;

    // Get granularity
    for (i = DRF_DEF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_GRANULARITY, _KIB);
         i < DRF_DEF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_GRANULARITY, _TIB);
         i++)
    {
        if (counterDelta < deltaLimit)
        {
            break;
        }

        // Colwert counterDelta to the next higher unit to fit in 20 bits
        counterDelta >>= 10;
    }
    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_GRANULARITY,
                          i, cmd);

    // If delta still doesn't fit in 20-bits, delta is invalid
    if (counterDelta > deltaLimit)
    {
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                          _TRUE, cmd);
        goto s_smbpbiGetLwlinkThroughput_exit;
    }

    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_DELTA,
                          counterDelta, cmd);
    cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                      _FALSE, cmd);

s_smbpbiGetLwlinkThroughput_exit:
    *pCounterVal = finalCounterValKib;
    *pCmd = cmd;

    return status;
}

/* ------------------------- Public Functions ------------------------------ */

/*!
 * @brief Gets the total number of links per LWLIPT.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
LwU8
smbpbiGetLwlinkNumLinkPerLwlipt_GA100
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LINKS_PER_LWLIPT_LWL30;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the Max sublink width.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
LwU8
smbpbiGetLwlinkMaxSublinkWidth_GA100
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_SUBLINK_WIDTH_MAX_LWL30;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*
 * @brief Gets the LwLink Up/Down state as a bitmask.
 *
 * @param[in]  arg1      Lwlink info query command
 * @param[in]  arg2      link page number, or aggregate command
 * @param[out] pData     Pointer to the output Data
 * @param[out] pExtData  Pointer to the output extData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link number goes out of bounds
 */
LwU8
smbpbiGetLwlinkStateV1_GA100
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status;
    LwU32 numLwlinks;
    LwU32 linkActiveState = 0;
    LwU32 linkActiveStateExt = 0;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkStateV1_GA100_exit;
    }

    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = s_smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkActiveState);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV1_GA100_exit;
        }
    }
    else
    {
        status = s_smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                     arg2,
                                                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1__SIZE,
                                                     numLwlinks,
                                                     &linkActiveState,
                                                     &linkActiveStateExt);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV1_GA100_exit;
        }
    }

    *pData    = linkActiveState;
    *pExtData = linkActiveStateExt;

smbpbiGetLwlinkStateV1_GA100_exit:
    return status;
}

/*!
 * @brief Gets the Lwlink bandwidth.
 *
 * If the command is for aggregate bandwidth, value returned
 * will contain the common rate for all links, if they are
 * the same. Otherwise, the bandwidth of 2 individual links
 * are returned in the Data Out and ExtData register.
 *
 * @param[in]  arg1      Lwlink info query command
 * @param[in]  arg2      link page number, or aggregate command
 * @param[out] pData     Pointer to output Data
 * @param[out] pExtData  Pointer to output ExtData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *  @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if any link line rate is different for
 *     aggregate command
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if requested link is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkNum is invalid
 */
LwU8
smbpbiGetLwlinkBandwidth_GA100
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status;
    LwU32 linkLineRate    = 0;
    LwU32 extLinkLineRate = 0;
    LwU32 numLwlinks;
    LwU32 maxSublinkWidth;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkBandwidth_GA100_exit;
    }

    status = smbpbiGetLwlinkMaxSublinkWidth_HAL(&Smbpbi, &maxSublinkWidth);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkBandwidth_GA100_exit;
    }

    // If aggregate command, check all link line rates
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = s_smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkLineRate);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkBandwidth_GA100_exit;
        }
    }
    else
    {
        status = s_smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                     arg2,
                                                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_BANDWIDTH__SIZE,
                                                     numLwlinks,
                                                     &linkLineRate,
                                                     &extLinkLineRate);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkBandwidth_GA100_exit;
        }
    }

    // Get the Bandwidth from the line rate in MB/s
    *pData    = (linkLineRate * maxSublinkWidth) / 8;
    *pExtData = (extLinkLineRate * maxSublinkWidth) / 8;

smbpbiGetLwlinkBandwidth_GA100_exit:
    return status;
}

/*!
 * @brief Gets the LwLink Error count.
 *
 * @param[in]  errType  Type of error
 * @param[in]  arg2     link number, or aggregate command
 * @param[out] pData    Pointer to number of errors of the requested type
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      if success, or if link is not accessible
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      if linkNum is out of bounds
 */
LwU8
smbpbiGetLwlinkErrorCount_GA100
(
    LwU8  errType,
    LwU8  arg2,
    LwU32 *pData
)
{
    LwU32 errorCount      = 0;
    LwU32 totalErrorCount = 0;
    LwU8  status;
    LwU32 link;
    LwU32 numLwlinks;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkErrorCount_GA100_exit;
    }

    // If aggregate command, get sum of all specified error count for all links
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        for (link = 0; link < numLwlinks; link++)
        {
            status = s_smbpbiGetLwlinkErrorCountPerLink(link, errType, &errorCount);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto smbpbiGetLwlinkErrorCount_GA100_exit;
            }

            totalErrorCount += errorCount;
        }
    }
    else
    {
        status = s_smbpbiGetLwlinkErrorCountPerLink(arg2, errType, &totalErrorCount);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkErrorCount_GA100_exit;
        }
    }

    *pData = totalErrorCount;

smbpbiGetLwlinkErrorCount_GA100_exit:
    return status;
}

/*!
 * @brief Gets the Lwlink status.
 *
 * If the command is for aggregate status, data returned
 * will contain the common status for all links, if the statuses
 * are the same. Otherwise, the statuses of the links will be
 * packed into 2 data pages, in the Data Out and ExtData register,
 * as defined by LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATUS(i).
 *
 * @param[in]  arg1      Lwlink info query command
 * @param[in]  arg2      link number, or aggregate command
 * @param[out] pData     Pointer to output Data
 * @param[out] pExtData  Pointer to output ExtData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *  @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if any link state is different for
 *     aggregate command
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if requested link page is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkNum is invalid
 */
LwU8
smbpbiGetLwlinkStateV2_GA100
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status;
    LwU32 linkState;
    LwU32 val = 0;
    LwU32 extVal = 0;
    LwU32 numLwlinks;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkStateV2_GA100_exit;
    }

    // If aggregate command, check if all links have the same status
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = s_smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkState);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV2_GA100_exit;
        }

        val = linkState;
    }
    else
    {
        status = s_smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                     arg2,
                                                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2__SIZE,
                                                     numLwlinks,
                                                     &val,
                                                     &extVal);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV2_GA100_exit;
        }
    }

    *pData    = val;
    *pExtData = extVal;

smbpbiGetLwlinkStateV2_GA100_exit:
    return status;
}

/*!
 * @brief Gets the Lwlink Sublink width.
 *
 * If the command is for aggregate Sublink width, data returned
 * will contain the common Sublink width for all links, if they are
 * the same. Otherwise, the widths of the sublinks will be
 * packed into 2 data pages, in the Data Out and ExtData register,
 * defined by LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_WIDTH(i).
 *
 * @param[in]  arg1      Lwlink info query command
 * @param[in]  arg2      link number, or aggregate command
 * @param[out] pData     Pointer to output Data
 * @param[out] pExtData  Pointer to output ExtData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *  @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if any link sublink width is different
 *     for aggregate command
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if requested link page is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if linkNum is invalid
 */
LwU8
smbpbiGetLwlinkSublinkWidth_GA100
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU32 status;
    LwU32 val = 0;
    LwU32 extVal = 0;
    LwU32 sublinkWidth;
    LwU32 numLwlinks;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkSublinkWidth_GA100_exit;
    }

    // If aggregate command, check if all sublink status are the same
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = s_smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &sublinkWidth);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkSublinkWidth_GA100_exit;
        }

        val = sublinkWidth;
    }
    else
    {
        status = s_smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                     arg2,
                                                     LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_WIDTH__SIZE,
                                                     numLwlinks,
                                                     &val,
                                                     &extVal);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkSublinkWidth_GA100_exit;
        }
    }

    *pData    = val;
    *pExtData = extVal;

smbpbiGetLwlinkSublinkWidth_GA100_exit:
    return status;
}

/*!
 * @brief Gets Lwlink Throughput on a per link
 *        or aggregate basis
 *
 * User can specify 4 types of throughput values:
 * 1. DATA_TX - counter value of data payload transmitted
 * 2. DATA_RX - counter value of data payload received
 * 3. RAW_TX  - counter value of data with
 *              protocol overhead transmitted
 * 4. RAW_RX  - counter value of data with
 *              protocol overhead received
 *
 * The command parameter is populated as follows:
 * LW_MSGBOX_DATA_LWLINK_INFO_THROUGHPUT_GRANULARITY   1:0
 * LW_MSGBOX_DATA_LWLINK_INFO_THROUGHPUT_NEW_SAMPLE    2:2
 * LW_MSGBOX_DATA_LWLINK_INFO_THROUGHPUT_ILWALID_DELTA 3:3
 * LW_MSGBOX_DATA_LWLINK_INFO_THROUGHPUT_DELTA         23:4
 *
 * @param[in]  arg1      One of the 4 counter types
 * @param[in]  arg2      Link number
 *                       (Aggregate query if 0xff)
 * @param[out] pCmd      Pointer to input command
 * @param[out] pData     Lower 32-bits of counter value
 * @param[out] pExtData  Upper 32-bits of counter value
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if the given link is unavailable or uninitialized
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if requested link number is out of bounds
 */
LwU8
smbpbiGetLwlinkThroughput_GA100
(
    LwU8    arg1,
    LwU8    arg2,
    LwU32   *pCmd,
    LwU32   *pData,
    LwU32   *pExtData
)
{
    LwU8  counterIdx = 0;
    LwU64 counterValKib = 0;
    LwU32 cmd = *pCmd;
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_TX:
            counterIdx = LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_TX;
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_DATA_RX:
            counterIdx = LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_RX;
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_TX:
            counterIdx = LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_TX;
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_THROUGHPUT_RAW_RX:
            counterIdx = LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_RX;
            break;
    }

    status = s_smbpbiGetLwlinkThroughput(arg2, counterIdx,
                                         &cmd, &counterValKib);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkThroughput_GA100_exit;
    }

    // Separate 64-bit value into two 32-bit values
    *pData    = LwU64_LO32(counterValKib);
    *pExtData = LwU64_HI32(counterValKib);

    // Set cmd parameter that contains counter delta if valid
    *pCmd = cmd;

smbpbiGetLwlinkThroughput_GA100_exit:
    return status;
}
