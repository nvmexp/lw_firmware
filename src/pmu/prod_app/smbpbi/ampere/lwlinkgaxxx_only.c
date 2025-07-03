/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright    2020   by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    lwlinkgaxxx_only.c
 * @brief   PMU HAL functions for SMBPBI, specific to only Ampere
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

#include "pmu_objsmbpbi.h"
#include "ioctrl_discovery.h"
#include "dev_lwldl_ip.h"

/* ------------------------ Application Includes ---------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Static Variables -------------------------------- */
/* ------------------------ Macros ------------------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

/*!
 * @brief Get Sublink lane width.
 *
 * @param[in]  lwliptIdx     Index of top level IOCTRL
 * @param[in]  localLinkIdx  Index of link in IOCTRL
 * @param[out] pTxWidth      Pointer to TX sublink width output
 * @param[out] pRxWidth      Pointer to RX sublink width output
 *
 */
LwU8
smbpbiGetLwlinkSublinkLaneWidth_GA100
(
    LwU32 lwliptIdx,
    LwU32 localLinkIdx,
    LwU8  *pTxWidth,
    LwU8  *pRxWidth
)
{
    LwU32 reg;
    LwU32 lwldlLinkBase;

    lwldlLinkBase =
        smbpbiGetLwlinkUnitRegBaseOffset_HAL(&Smbpbi,
                                             lwliptIdx,
                                             localLinkIdx,
                                             LW_DISCOVERY_IOCTRL_UNICAST_0_SW_DEVICE_BASE_LWLDL_0);

    // Get TX single lane mode
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TX_SLSM_STATUS_TX);
    if (FLD_TEST_DRF(_LWLDL, _TX_SLSM_STATUS_TX, _PRIMARY_STATE, _HS, reg))
    {
        *pTxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_TX_WIDTH_4;
    }
    else if (FLD_TEST_DRF(_LWLDL, _TX_SLSM_STATUS_TX, _PRIMARY_STATE, _EIGHTH, reg))
    {
        *pTxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_TX_WIDTH_1;
    }
    else
    {
        *pTxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_TX_WIDTH_0;
    }

    // Get RX single lane mode
    reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_SLSM_STATUS_RX);
    if (FLD_TEST_DRF(_LWLDL, _RX_SLSM_STATUS_RX, _PRIMARY_STATE, _HS, reg))
    {
        *pRxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_RX_WIDTH_4;
    }
    else if (FLD_TEST_DRF(_LWLDL, _RX_SLSM_STATUS_RX, _PRIMARY_STATE, _EIGHTH, reg))
    {
        *pRxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_RX_WIDTH_1;
    }
    else
    {
        *pRxWidth = LW_MSGBOX_DATA_LWLINK_INFO_SUBLINK_RX_WIDTH_0;
    }

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

 /*!
  * @brief Gets the LwLink Error count per link.
  *
  * @param[in]  lwldlLinkBsae  Base register offset to query for a particular link
  * @param[in]  errType        Type of error
  * @param[out] pErrorCount    Count of errors on the link
  *
  */
 void
 smbpbiGetLwlinkLinkErrCount_GA100
 (
     LwU32 lwldlLinkBase,
     LwU8  errType,
     LwU32 *pErrorCount
 )
 {
     LwU32 reg;
     
    switch (errType)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
        {
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TX_ERROR_COUNT4);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _TX_ERROR_COUNT4, _REPLAY_EVENTS, reg);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
        {
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_TOP_ERROR_COUNT1);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _TOP_ERROR_COUNT1, _RECOVERY_EVENTS, reg);
            break;
        } 
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
        {
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_ERROR_COUNT1);
            *pErrorCount = (LwU16) DRF_VAL(_LWLDL, _RX_ERROR_COUNT1, _FLIT_CRC_ERRORS, reg);
            break;
        }
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
        {
            reg = REG_RD32(BAR0, lwldlLinkBase + LW_LWLDL_RX_ERROR_COUNT2_LANECRC);
            *pErrorCount = (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L0, reg);
            *pErrorCount += (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L1, reg);
            *pErrorCount += (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L2, reg);
            *pErrorCount += (LwU8) DRF_VAL(_LWLDL, _RX_ERROR_COUNT2_LANECRC, _L3, reg);
            break;
        } 
        default:
             break;
     }
 }
