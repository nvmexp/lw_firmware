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
 * @file    lwlinkgh100.c
 * @brief   PMU HAL functions for GH100 SMBPBI
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "pmu_objsmbpbi.h"
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

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
// LWLink Throughput counter type indexes based on raw/data flits
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_DATA  0
#define SMBPBI_LWLINK_THROUGHPUT_TYPE_RAW   2

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
smbpbiGetLwlinkLinkErrorCount_GH100
(
    LwU32 lwldlLinkBaseReg,
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
            break;
        }
        default:
            break;
    }
}
