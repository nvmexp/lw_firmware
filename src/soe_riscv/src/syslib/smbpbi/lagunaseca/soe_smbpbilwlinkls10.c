/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    soe_smbpbilwlinklr10.c
 * @brief   SOE LWlink HAL functions for LR10 SMBPBI
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"
#include "lwrtos.h"

#include "soe_bar0.h"
#include "dev_fuse.h"
#include "lwlw_discovery_ip.h"
#include "dev_lwlipt_lnk_ip.h"
#include "dev_lwldl_ip.h"
#include "dev_lwltlc_ip.h"
#include "dev_lwlsaw_ip.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"
#include "config/g_smbpbi_hal.h"
#include "config/g_intr_hal.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */

// LWLink Throughput counter type indexes based on raw/data flits
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA  0
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW   2

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
sysTASK_DATA SMBPBI_LWLINK SmbpbiLwlink[LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_LR10]; // = {{{0}}};

/* ------------------------- Private Functions ------------------------------ */

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
sysSYSLIB_CODE static LwBool
_smbpbiIsLwlinkAccessible
(
    LwU32  lwliptIdx,
    LwU32  localLinkIdx
)
{
    LwU32 reg;
    LwU32 lwLiptLinkBase;

    // Check if LWLIPT is in reset
    if (intrIsLwlwInReset_HAL(&Intr, lwliptIdx))
    {
        return LW_FALSE;
    }

    lwLiptLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLIPT_LNK_0);

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
sysSYSLIB_CODE static LwU8
_smbpbiGetIndexesFromLwlink
(
    LwU32 linkNum,
    LwU32 *pLwliptIdx,
    LwU32 *pLocalLinkIdx
)
{
    LwU8  status;
    LwU32 numLwlipt;
    LwU32 numLinkPerLwlipt;
    LwU32 lwliptIdx;
    LwU32 localLinkIdx;

    status = smbpbiGetLwlinkNumLwlipt_HAL(&Smbpbi, &numLwlipt);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetIndexesFromLwlink_exit;
    }

    status = smbpbiGetLwlinkNumLinkPerLwlipt_HAL(&Smbpbi, &numLinkPerLwlipt);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetIndexesFromLwlink_exit;
    }

    lwliptIdx    = linkNum / numLinkPerLwlipt;
    localLinkIdx = linkNum % numLinkPerLwlipt;

    if (lwliptIdx >= numLwlipt)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto _smbpbiGetIndexesFromLwlink_exit;
    }

    *pLwliptIdx    = lwliptIdx;
    *pLocalLinkIdx = localLinkIdx;

_smbpbiGetIndexesFromLwlink_exit:
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
 * @return LW_TRUE/LW_FALSE
 */
sysSYSLIB_CODE static LwBool
_smbpbiLwlinkPageInNumPages
(
    LwU32 pageNum,
    LwU32 numLinks,
    LwU32 numLinksPerPage
)
{
    LwU8 numPages = LW_CEIL(numLinks, numLinksPerPage);

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
sysSYSLIB_CODE static LwU8
_smbpbiColwertLwLinkState
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
 * @param[in]  linkNum  Input link number
 * @param[out] pState   Pointer to the colwerted status
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if linkNum is out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if link status is invalid
 */
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkStatePerLink
(
    LwU32 linkNum,
    LwU32 *pState
)
{
    LwU8  status;
    LwU32 reg;
    LwU32 linkState;
    LwU32 lwliptIdx;
    LwU32 localLinkIdx;
    LwU32 numLwlinks;
    LwU32 lwldlLinkBase;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkStatePerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto _smbpbiGetLwlinkStatePerLink_exit;
    }

    status = _smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkStatePerLink_exit;
    }

    if (!_smbpbiIsLwlinkAccessible(lwliptIdx, localLinkIdx))
    {
        *pState = LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2_OFF;
        status  = LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto _smbpbiGetLwlinkStatePerLink_exit;
    }

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);

    // Get link state
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TOP_LINK_STATE);
    linkState = DRF_VAL(_LWLDL, _TOP_LINK_STATE, _STATE, reg);

    status = _smbpbiColwertLwLinkState(linkState, pState);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkStatePerLink_exit;
    }

_smbpbiGetLwlinkStatePerLink_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiIsLwlinkStateActive
(
    LwU32  linkNum,
    LwBool *pIsLinkActive
)
{
    LwU8  status;
    LwU32 linkState = LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID;

    status = _smbpbiGetLwlinkStatePerLink(linkNum, &linkState);
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
sysSYSLIB_CODE static LwU8
_smbpbiColwertLwLinkLineRate
(
    LwU8  linkLineRate,
    LwU32 *pColwertedLineRate
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

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
        goto _smbpbiColwertLwLinkLineRate_exit;
    }

    *pColwertedLineRate = lineRatesMbps[linkLineRate];

_smbpbiColwertLwLinkLineRate_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkLineRatePerLink
(
    LwU8  linkNum,
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
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

    status = _smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

    status = _smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

    if (!isLinkActive)
    {
        *pLinkLineRate = LW_MSGBOX_DATA_LWLINK_LINE_RATE_00000_MBPS;
        status         =  LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

    lwLiptLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLIPT_LNK_0);

    //
    // Get link line rate of individual links.
    // Link line rate should all be the same inside an LWLIPT.
    //
    reg = REG_RD32(BAR0, lwLiptLinkBase + LW_LWLIPT_LNK_CTRL_SYSTEM_LINK_CLK_CTRL);
    linkLineRate = DRF_VAL(_LWLIPT_LNK,
                           _CTRL_SYSTEM_LINK_CLK_CTRL,
                           _LINE_RATE,
                           reg);

    status = _smbpbiColwertLwLinkLineRate(linkLineRate, pLinkLineRate);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkLineRatePerLink_exit;
    }

_smbpbiGetLwlinkLineRatePerLink_exit:
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkErrorCountPerLink_LS10
(
    LwU32 linkNum,
    LwU8  errType,
    LwU32 *pErrorCount
)
{
    LwU8  status;
    LwU32 lwliptIdx    = 0;
    LwU32 localLinkIdx = 0;
    LwU32 reg;
    LwU32 lwldlLinkBase;
    LwU32 numLwlinks;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkErrorCountPerLink_LS10_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto smbpbiGetLwlinkErrorCountPerLink_LS10_exit;
    }

    status = _smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkErrorCountPerLink_LS10_exit;
    }

    if (!_smbpbiIsLwlinkAccessible(lwliptIdx, localLinkIdx))
    {
        *pErrorCount = 0;
        status       = LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto smbpbiGetLwlinkErrorCountPerLink_LS10_exit;
    }

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);

    switch (errType)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TX_ERROR_COUNT4);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _TX_ERROR_COUNT4, _REPLAY_EVENTS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TOP_ERROR_COUNT1);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _TOP_ERROR_COUNT1, _RECOVERY_EVENTS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_ERROR_COUNT1);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _RX_ERROR_COUNT1, _FLIT_CRC_ERRORS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_ERROR_COUNT2_LANECRC);
            *pErrorCount = (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L0, reg);
            *pErrorCount += (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L1, reg);
            break;
        default:
            break;
    }

smbpbiGetLwlinkErrorCountPerLink_LS10_exit:
    return status;
}

/*!
 * @brief Init sublink width data to Invalid
 *
 * @param[out] pSublinkWidth   Pointer to Sublink width output
 *
 */
sysSYSLIB_CODE static void
_smbpbiInitLwlinkSublinkWidth
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
 * @brief Get Sublink lane width.
 *
 * @param[in]  lwliptIdx Index of top level LWLIPT
 * @param[in]  linkIdx   Index of link in LWLIPT
 * @param[out] pTxWidth  Pointer to TX sublink width output
 * @param[out] pRxWidth  Pointer to RX sublink width output
 *
 */
sysSYSLIB_CODE void
smbpbiGetSublinkLaneWidth_LS10
(
    LwU8 lwliptIdx,
    LwU8 linkIdx,
    LwU8 *pTxWidth,
    LwU8 *pRxWidth
)
{
    LwU32 reg;
    LwU32 lwldlLinkBase;

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             linkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);

    // Get TX single lane mode
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TX_SLSM_STATUS_TX);
    if (FLD_TEST_DRF(_LWLDL, _TX_SLSM_STATUS_TX, _PRIMARY_STATE, _HS, reg))
        *pTxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_TX_WIDTH_4;
    else
        *pTxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_TX_WIDTH_0;

    // Get RX single lane mode
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_SLSM_STATUS_RX);
    if (FLD_TEST_DRF(_LWLDL, _RX_SLSM_STATUS_RX, _PRIMARY_STATE, _HS, reg))
        *pRxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_RX_WIDTH_4;
    else
        *pRxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_RX_WIDTH_0;
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
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkSublinkWidthPerLink
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
        goto _smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    if (linkNum >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    status = _smbpbiGetIndexesFromLwlink(linkNum, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    status = _smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    if (!isLinkActive)
    {
        // Set width as invalid
        _smbpbiInitLwlinkSublinkWidth(pSublinkWidth);
        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
        goto _smbpbiGetLwlinkSublinkWidthPerLink_exit;
    }

    smbpbiGetSublinkLaneWidth_HAL(&Smbpbi, lwliptIdx, localLinkIdx, &txWidth, &rxWidth);

    sublinkWidth = FLD_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_TX_WIDTH,
                                   txWidth, sublinkWidth);
    sublinkWidth = FLD_SET_DRF_NUM(_MSGBOX_DATA, _LWLINK_INFO, _SUBLINK_RX_WIDTH,
                                   rxWidth, sublinkWidth);

    *pSublinkWidth = sublinkWidth;

_smbpbiGetLwlinkSublinkWidthPerLink_exit:
    return status;
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
sysSYSLIB_CODE static LwU8
_smbpbiLwlinkCommandDispatch
(
    LwU8  arg1,
    LwU32 linkNum,
    LwU32 *pData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch(arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V1:
        {
            LwBool isLinkActive;

            status = _smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto _smbpbiLwlinkCommandDispatch_exit;
            }

            *pData = (isLinkActive) ?
                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1_UP :
                     LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1_DOWN;
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_BANDWIDTH:
        {
            status = _smbpbiGetLwlinkLineRatePerLink(linkNum, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_LINK_STATE_V2:
        {
            status = _smbpbiGetLwlinkStatePerLink(linkNum, pData);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_SUBLINK_WIDTH:
        {
            status = _smbpbiGetLwlinkSublinkWidthPerLink(linkNum, pData);
            break;
        }
        default:
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
            break;
        }
    }

_smbpbiLwlinkCommandDispatch_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiLwlinkAggregateCommandHandler
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

    prevData = LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID;
    for (link = 0; link < numLwlinks; link++)
    {
        status = _smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiLwlinkAggregateCommandHandler_exit;
        }

        if (prevData == LW_MSGBOX_DATA_LWLINK_INFO_DATA_ILWALID)
        {
            prevData = data;
        }
        else if (prevData != data)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            goto _smbpbiLwlinkAggregateCommandHandler_exit;
        }
    }
    *pAggData = data;
_smbpbiLwlinkAggregateCommandHandler_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiFormatLwlinkQueryData
(
    LwU32 arg1,
    LwU32 linkNum,
    LwU32 inputData,
    LwU32 *pFormattedData
)
{
    LwU8 status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    switch(arg1)
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
sysSYSLIB_CODE static LwU8
_smbpbiLwlinkDataAndExtDataHandler
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

    if (!_smbpbiLwlinkPageInNumPages(linkPage,
                                     numLwlinks,
                                     linksInPage *
                                     LW_MSGBOX_DATA_LWLINK_INFO_DATA_WIDTH))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiLwlinkDataAndExtDataHandler_exit;
    }

    // Get packed status for first data output
    for (link = linkStart;
         (link < numLwlinks) && (link < linkStop);
         link++)
    {
        status = _smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiLwlinkDataAndExtDataHandler_exit;
        }

        _smbpbiFormatLwlinkQueryData(arg1, link, data, pVal);
    }

    // Get packed status for second extended data output
    for (;
         (link < numLwlinks) &&
         (link < (linkStop + linksInPage));
         link++)
    {
        status = _smbpbiLwlinkCommandDispatch(arg1, link, &data);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiLwlinkDataAndExtDataHandler_exit;
        }

        _smbpbiFormatLwlinkQueryData(arg1, link, data, pExtVal);
    }

_smbpbiLwlinkDataAndExtDataHandler_exit:
    return status;
}

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief This callwlates the base register offset by using the stride between
 *        Lwlink unit registers.
 *
 * @param[in] lwliptNum    Index of top level LWLIPT
 * @param[in] linkNum      Index of link in LWLIPT
 * @param[in] unitRegBase  Base unit register offset to access
 *
 * @return Register offset value
 */
sysSYSLIB_CODE LwU32
smbpbiGetLwlinkUnitRegBaseOffset_LS10
(
    LwU32 lwliptNum,
    LwU32 linkNum,
    LwU32 unitRegBase
)
{
    LwU32 lwliptStride;
    LwU32 unitRegStride;

    lwliptStride = LW_DISCOVERY_LWLW_UNICAST_1_SW_DEVICE_BASE_LWLDL_0 -
                   LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_0;

    unitRegStride = LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_1 -
                    LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLDL_0;

    return unitRegBase + (lwliptNum * lwliptStride) + (linkNum * unitRegStride);
}

/*!
 * @brief Gets the total number of Lwlinks. (LWLIPT * linkPerLwlipt)
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkNumLinks_LS10
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_LR10;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the total number of Lwlink LWLIPT's.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkNumLwlipt_LS10
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LWLIPT_LR10;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the total number of links per LWLIPT.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkNumLinkPerLwlipt_LS10
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkMaxSublinkWidth_LS10
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_SUBLINK_WIDTH_MAX_LWL30;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the LwLink Up/Down state as a bitmask.
 *
 * @param[in]  arg1      Lwlink info query command
 * @param[in]  arg2      link page number, or aggregate command
 * @param[out] pData     Pointer to the output Data
 * @param[out] pExtData  Pointer to the output extData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if link number goes out of bounds
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if something internally went wrong
 */
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkStateV1_LS10
(
    LwU8   arg1,
    LwU8   arg2,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    LwU8   status;
    LwU32  numLwlinks;
    LwU32  linkActiveState = 0;
    LwU32  linkActiveStateExt = 0;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkStateV1_LS10_exit;
    }

    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = _smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkActiveState);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV1_LS10_exit;
        }
    }
    else
    {
        status = _smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                    arg2,
                                                    LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V1__SIZE,
                                                    numLwlinks,
                                                    &linkActiveState,
                                                    &linkActiveStateExt);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV1_LS10_exit;
        }
    }

    *pData    = linkActiveState;
    *pExtData = linkActiveStateExt;

smbpbiGetLwlinkStateV1_LS10_exit:
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
 * @param[in]  arg2      link number, or aggregate command
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkBandwidth_LS10
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
        goto smbpbiGetLwlinkBandwidth_LS10_exit;
    }

    status = smbpbiGetLwlinkMaxSublinkWidth_HAL(&Smbpbi, &maxSublinkWidth);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkBandwidth_LS10_exit;
    }

    // If aggregate command, check all link line rates
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = _smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkLineRate);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkBandwidth_LS10_exit;
        }
    }
    else
    {
        status = _smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                    arg2,
                                                    LW_MSGBOX_DATA_LWLINK_INFO_LINK_BANDWIDTH__SIZE,
                                                    numLwlinks,
                                                    &linkLineRate,
                                                    &extLinkLineRate);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkBandwidth_LS10_exit;
        }
    }

    // Get the Bandwidth from the line rate in Mbps
    *pData    = (linkLineRate * maxSublinkWidth) / 8;
    *pExtData = (extLinkLineRate * maxSublinkWidth) / 8;

smbpbiGetLwlinkBandwidth_LS10_exit:
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkErrorCount_LS10
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
        goto smbpbiGetLwlinkErrorCount_LS10_exit;
    }

    // If aggregate command, get sum of all specified error count for all links
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        for (link = 0; link < numLwlinks; link++)
        {
            status = smbpbiGetLwlinkErrorCountPerLink_HAL(&Smbpbi, link, errType, &errorCount);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                goto smbpbiGetLwlinkErrorCount_LS10_exit;
            }

            totalErrorCount += errorCount;
        }
    }
    else
    {
        status = smbpbiGetLwlinkErrorCountPerLink_HAL(&Smbpbi, arg2, errType, &totalErrorCount);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkErrorCount_LS10_exit;
        }
    }

    *pData = totalErrorCount;

smbpbiGetLwlinkErrorCount_LS10_exit:
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkStateV2_LS10
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
        goto smbpbiGetLwlinkStateV2_LS10_exit;
    }

    // If aggregate command, check if all links have the same status
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = _smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &linkState);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV2_LS10_exit;
        }

        val = linkState;
    }
    else
    {
        status = _smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                    arg2,
                                                    LW_MSGBOX_DATA_LWLINK_INFO_LINK_STATE_V2__SIZE,
                                                    numLwlinks,
                                                    &val,
                                                    &extVal);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkStateV2_LS10_exit;
        }
    }

    *pData    = val;
    *pExtData = extVal;

smbpbiGetLwlinkStateV2_LS10_exit:
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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkSublinkWidth_LS10
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
        goto smbpbiGetLwlinkSublinkWidth_LS10_exit;
    }

    // If aggregate command, check if all sublink status are the same
    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        status = _smbpbiLwlinkAggregateCommandHandler(arg1, numLwlinks, &sublinkWidth);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkSublinkWidth_LS10_exit;
        }

        val = sublinkWidth;
    }
    else
    {
        status = _smbpbiLwlinkDataAndExtDataHandler(arg1,
                                                    arg2,
                                                    LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_WIDTH__SIZE,
                                                    numLwlinks,
                                                    &val,
                                                    &extVal);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto smbpbiGetLwlinkSublinkWidth_LS10_exit;
        }
    }

    *pData    = val;
    *pExtData = extVal;

smbpbiGetLwlinkSublinkWidth_LS10_exit:
    return status;
}

/*!
 * @brief Gets a 64-bit mask of accessible links
 *
 * @param[out] pLinkMask  Pointer to the 32-bit link mask
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC if there is an internal problem getting the mask
 *         LW_MSGBOX_CMD_STATUS_SUCCESS if no problems getting the mask
 */
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkActiveLinkMask
(
    LwU64 *pLinkMask
)
{
    LwU8   status;
    LwU32  linkNum = 0;
    LwU32  numLwlinks = 0;
    LwU64  linkMask = 0;
    LwBool isLinkActive = LW_FALSE;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkActiveLinkMask_exit;
    }

    for (linkNum = 0; linkNum < numLwlinks; linkNum++)
    {
        status = _smbpbiIsLwlinkStateActive(linkNum, &isLinkActive);
        if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
        {
            goto _smbpbiGetLwlinkActiveLinkMask_exit;
        }

        if (isLinkActive)
        {
            linkMask |= BIT64(linkNum);
        }
    }

    *pLinkMask = linkMask;

_smbpbiGetLwlinkActiveLinkMask_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkThroughputCounterValue
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

    status = _smbpbiGetIndexesFromLwlink(linkId, &lwliptIdx, &localLinkIdx);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkThroughputCounterValue_exit;
    }

    if (!_smbpbiIsLwlinkAccessible(lwliptIdx, localLinkIdx))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto _smbpbiGetLwlinkThroughputCounterValue_exit;
    }

    lwltlcLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_LWLW_UNICAST_0_SW_DEVICE_BASE_LWLTLC_0);

    //
    // The counter must be read in hi-lo-hi since
    // lwlink traffic may be flowing while monitoring
    // when driver is loaded
    //
    lwrtosTaskCriticalEnter();

    do
    {
        counterHi = REG_RD32(BAR0, lwltlcLinkBase + hiOffset);
        counterLo = REG_RD32(BAR0, lwltlcLinkBase + loOffset);
    } while (counterHi != REG_RD32(BAR0, lwltlcLinkBase + hiOffset));

    lwrtosTaskCriticalExit();

    *pCounterVal = (((LwU64)counterHi << 32ull) | counterLo);

_smbpbiGetLwlinkThroughputCounterValue_exit:
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
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkThroughputCounterKib
(
    LwU8    linkId,
    LwU8    counterIdx,
    LwU64   *pCounter
)
{
    LwU64 counterValFlits = 0;
    LwU32 numLwlinks = 0;
    LwU8  status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkThroughputCounterKib_exit;
    }

    if (linkId >= numLwlinks)
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        goto _smbpbiGetLwlinkThroughputCounterKib_exit;
    }

    switch(counterIdx)
    {
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_TX:
            status = _smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_DATA_RX:
            status = _smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         LW_LWLTLC_RX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_TX:
            status = _smbpbiGetLwlinkThroughputCounterValue(linkId,
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_LO(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         LW_LWLTLC_TX_LNK_DEBUG_TP_CNTR_HI(SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW),
                         &counterValFlits);
            break;
        case LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_RAW_RX:
            status = _smbpbiGetLwlinkThroughputCounterValue(linkId,
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

_smbpbiGetLwlinkThroughputCounterKib_exit:
    return status;
}

/*!
 * @brief Gets snapshot of all counters, all links
 *        and populate SmbpbiLwlink struct
 */
sysSYSLIB_CODE static void
_smbpbiGetLwlinkThroughputSnapshot
(
    LwU64 linkMask
)
{
    LwU64 counterVal = 0;
    LwU8  linkIdx = 0;
    LwU8  counterIdx = 0;

    FOR_EACH_INDEX_IN_MASK(64, linkIdx, linkMask)
    {
        for (counterIdx = 0;
             counterIdx < LW_MSGBOX_LWLINK_THROUGHPUT_COUNTER_INDEX_MAX;
             counterIdx++)
        {
            LwU8 status;

            status = _smbpbiGetLwlinkThroughputCounterKib(linkIdx, counterIdx,
                                                          &counterVal);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx] = 0;
                //
                // Reset the valid/last queried bits
                // for this unaccessible linkIdx/counterIdx
                //
                Smbpbi.lwlinkTpValidSampleMask[counterIdx] &= ~(BIT64(linkIdx));
                Smbpbi.lwlinkTpLastQueriedMask[counterIdx] &= ~(BIT64(linkIdx));
                continue;
            }
            SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx] = counterVal;

            // Set the valid bit for this linkIdx/counterIdx
            Smbpbi.lwlinkTpValidSampleMask[counterIdx] |= BIT64(linkIdx);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
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
sysSYSLIB_CODE static LwU8
_smbpbiGetLwlinkThroughput
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
    LwU64  linkMask = 0;
    LwU64  activeLinkMask = 0;
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
    status = _smbpbiGetLwlinkActiveLinkMask(&activeLinkMask);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto _smbpbiGetLwlinkThroughput_exit;
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
            goto _smbpbiGetLwlinkThroughput_exit;
        }

        if (arg2 >= numLwlinks)
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            goto _smbpbiGetLwlinkThroughput_exit;
        }

        linkMask = BIT64(arg2);
    }

    //
    // For SOE, even though all counter delta related information
    // is resident in dmem all the time even when driver is unloaded,
    // we keep the behavior same as on the GPU. On GPU,
    // we don't support counter delta when driver is unloaded.
    //
    if (!smbpbiIsSharedSurfaceAvailable())
    {
        FOR_EACH_INDEX_IN_MASK(64, linkIdx, linkMask)
        {
            status = _smbpbiGetLwlinkThroughputCounterKib(linkIdx, counterIdx,
                                                          &counterValKib);
            if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
            {
                // Ignore inaccessible links for aggregate computation
                continue;
            }

            finalCounterValKib += counterValKib;
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // Set delta invalid bit to 1
        // We don't support delta if driver is not loaded
        // (when shared surface is unavailable)
        // Other bits are only valid if delta invalid bit is unset
        //
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                          _TRUE, cmd);

        goto _smbpbiGetLwlinkThroughput_exit;
    }

    //
    // For per link query, if a link is not accessible and a new snapshot needs to be taken,
    // return early with NOT_AVAILABLE without taking a snapshot.
    //
    if ((arg2 != LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE) &&
        (!((Smbpbi.lwlinkTpValidSampleMask[counterIdx] & linkMask) == linkMask)) &&
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

        goto _smbpbiGetLwlinkThroughput_exit;
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
        FOR_EACH_INDEX_IN_MASK(64, linkIdx, linkMask)
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
        _smbpbiGetLwlinkThroughputSnapshot(activeLinkMask);

        //
        // If this is not an aggregate query and even after snapshot,
        // the ValidSample bit is still unset, this link likely went down recently
        // since the "active" check in the beginning of the function passed.
        //
        if ((arg2 != LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE) &&
            ((Smbpbi.lwlinkTpValidSampleMask[counterIdx] & linkMask) != linkMask))
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
            goto _smbpbiGetLwlinkThroughput_exit;
        }

        // Values will be taken from a new sample
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_NEW_SAMPLE,
                          _TRUE, cmd);
    }

    // Get aggregate values over all links
    FOR_EACH_INDEX_IN_MASK(64, linkIdx, linkMask)
    {
        // Get counter value from a snapshot
        counterValKib = SmbpbiLwlink[linkIdx].lwlinkTpSnapshot[counterIdx];

        // Add it to aggregate
        finalCounterValKib += counterValKib;

        // Populate last queried for this counter
        SmbpbiLwlink[linkIdx].lwlinkTpLastQueried[counterIdx] = counterValKib;

        // Reset valid sample bit for this counter
        Smbpbi.lwlinkTpValidSampleMask[counterIdx] &= ~(BIT64(linkIdx));

        // Set last queried bit for this counter so future deltas will be valid
        Smbpbi.lwlinkTpLastQueriedMask[counterIdx] |= BIT64(linkIdx);
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (bDeltaIlwalid)
    {
        cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                          _TRUE, cmd);
        goto _smbpbiGetLwlinkThroughput_exit;
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
        goto _smbpbiGetLwlinkThroughput_exit;
    }

    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_DELTA,
                          counterDelta, cmd);
    cmd = FLD_SET_DRF(_MSGBOX, _CMD, _LWLINK_INFO_THROUGHPUT_ILWALID_DELTA,
                      _FALSE, cmd);

_smbpbiGetLwlinkThroughput_exit:
    *pCounterVal = finalCounterValKib;
    *pCmd = cmd;

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
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkThroughput_LS10
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

    status = _smbpbiGetLwlinkThroughput(arg2, counterIdx,
                                        &cmd, &counterValKib);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkThroughput_LS10_exit;
    }

    // Separate 64-bit value into two 32-bit values
    *pData    = LwU64_LO32(counterValKib);
    *pExtData = LwU64_HI32(counterValKib);

    // Set cmd parameter that contains counter delta if valid
    *pCmd = cmd;

smbpbiGetLwlinkThroughput_LS10_exit:
    return status;
}

/*!
 * @brief Gets the LwLink Error state as a bitmask.
 *
 * @param[in]  arg1      Arg1 command for specific link
 *                       error state.
 * @param[in]  arg2      link page number, clear bitmask,
                         or aggregate command
 * @param[in]  pCmd      Pointer to Cmd status register
 * @param[out] pData     Pointer to the output Data
 * @param[out] pExtData  Pointer to the output extData
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *     if link page goes out of bounds
 *  @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if any link error state is different
 *     for aggregate command
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *     if something internally went wrong
 */
sysSYSLIB_CODE LwU8
smbpbiGetLwlinkErrorState_LS10
(
    LwU8  arg1,
    LwU8  arg2,
    LwU32 *pCmd,
    LwU32 *pData,
    LwU32 *pExtData
)
{
    LwU8  status;
    LwU32 numLwlinks;
    LwU64 attemptedTrainingMask0 = 0;
    LwU64 trainingErrorMask0 = 0;
    LwU64 runtimeErrorMask0 = 0;
    LwU64 linkErrorMask0 = 0;
    LwU32 linkErrorState = 0;
    LwU32 linkErrorStateExt = 0;
    LwU32 errorCount;
    LwU32 linkPage;
    LwU32 cmd = *pCmd;

    RM_FLCN_U64_UNPACK(&attemptedTrainingMask0,
                       &Smbpbi.linkTrainingErrorInfo.attemptedTrainingMask0);
    RM_FLCN_U64_UNPACK(&trainingErrorMask0,
                       &Smbpbi.linkTrainingErrorInfo.trainingErrorMask0);
    RM_FLCN_U64_UNPACK(&runtimeErrorMask0, &Smbpbi.linkRuntimeErrorInfo.mask0);

    //
    // Get the training error bitmask by ANDing the attempted trained links with
    // the links that reported training errors.
    //
    trainingErrorMask0 &= attemptedTrainingMask0;

    switch(arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_TRAINING_ERROR_STATE:
        {
            linkErrorMask0 = trainingErrorMask0;
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_RUNTIME_ERROR_STATE:
        {
            linkErrorMask0 = runtimeErrorMask0;
            break;
        }
        //
        // The default case can never be hit, since the argument check is done
        // in the caller.
        //

    }

    status = smbpbiGetLwlinkNumLinks_HAL(&Smbpbi, &numLwlinks);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiGetLwlinkErrorState_LS10_exit;
    }

    if (arg2 == LW_MSGBOX_CMD_ARG2_LWLINK_INFO_AGGREGATE)
    {
        errorCount = lwPopCount64(linkErrorMask0);

        if ((errorCount != numLwlinks) && (errorCount != 0))
        {
            return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        }

        linkErrorState = !!errorCount;
    }
    else
    {
        linkPage = arg2;

        if (!_smbpbiLwlinkPageInNumPages(linkPage,
                                         numLwlinks,
                                         LW_MSGBOX_DATA_LWLINK_INFO_ERROR_STATE__SIZE *
                                         LW_MSGBOX_DATA_LWLINK_INFO_DATA_WIDTH))
        {
            status = LW_MSGBOX_CMD_STATUS_ERR_ARG2;
            goto smbpbiGetLwlinkErrorState_LS10_exit;
        }

        linkErrorState    = LwU64_LO32(linkErrorMask0);
        linkErrorStateExt = LwU64_HI32(linkErrorMask0);

    }

    // Set cmd parameter with the number of Training errors
    errorCount = lwPopCount64(trainingErrorMask0);
    if ((errorCount > numLwlinks) ||
        (errorCount > LW_MSGBOX_CMD_LWLINK_INFO_ERROR_COUNT_MAX))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto smbpbiGetLwlinkErrorState_LS10_exit;
    }

    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_TRAINING_ERROR_COUNT,
                          errorCount, cmd);

    // Set cmd parameter with the number of Runtime errors
    errorCount = lwPopCount64(runtimeErrorMask0);
    if ((errorCount > numLwlinks) ||
        (errorCount > LW_MSGBOX_CMD_LWLINK_INFO_ERROR_COUNT_MAX))
    {
        status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
        goto smbpbiGetLwlinkErrorState_LS10_exit;
    }

    cmd = FLD_SET_DRF_NUM(_MSGBOX, _CMD, _LWLINK_INFO_RUNTIME_ERROR_COUNT,
                          errorCount, cmd);

    *pCmd     = cmd;
    *pData    = linkErrorState;
    *pExtData = linkErrorStateExt;

smbpbiGetLwlinkErrorState_LS10_exit:
    return status;
}

