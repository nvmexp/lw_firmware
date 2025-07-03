/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    soe_smbpbilwlinkls10.c
 * @brief   SOE LWlink HAL functions for LS10 SMBPBI
 */

/* ------------------------- System Includes -------------------------------- */
#include "soesw.h"

#include "soe_bar0.h"
#include "lwlw_discovery_ip.h"
#include "dev_lwldl_ip.h"
#include "dev_lwlipt_lnk_ip.h"

#include "config/g_intr_private.h"

/* ------------------------- Application Includes --------------------------- */
#include "soe_objsmbpbi.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */

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
LwU8
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
 * @brief Get Sublink lane width.
 *
 * @param[in]  lwliptIdx Index of top level LWLIPT
 * @param[in]  linkIdx   Index of link in LWLIPT
 * @param[out] pTxWidth  Pointer to TX sublink width output
 * @param[out] pRxWidth  Pointer to RX sublink width output
 *
 */
void
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

