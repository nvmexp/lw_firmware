/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    smbpbigv10x.c
 * @brief   PMU HAL functions for GV10X+ SMBPBI
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "dev_lwl.h"
#include "dev_lwl_ip.h"

/* ------------------------ Application Includes --------------------------- */
#include "dbgprintf.h"
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
#define LWLINK_BASE_ADDR(link)      (DRF_BASE(LW_PLWL0) + \
                                    (link) * (DRF_BASE(LW_PLWL1) - DRF_BASE(LW_PLWL0)))
#define DLPL_REG_RD32(link, off)    REG_RD32(BAR0, LWLINK_BASE_ADDR(link) + (off))

/* ------------------------- Static Functions ------------------------------- */
/*!
 * @brief   Provide the Lwlink state SCRATCH(30) register contents.
 *          Because the _STATUS and _ENABLED fields are not populated
 *          while the driver is running, we need to emulate the contents
 *          by filling the missing bits
 *
 * @return  the contents of the Lwlink state SCRATCH(30) register
 */
static LwU32
s_smbpbiReadLwlinkScratch
(
    void
)
{
    LwU32       scratchReg;
    LwU32       reg;
    unsigned    link;

    scratchReg = REG_RD32(BAR0, LW_PBUS_SW_SCRATCH(30));

    if (scratchReg == 0)
    {
        //
        // If nothing is set, the scratch register has not been initialized
        // by the driver yet
        //
        return 0;
    }

    for (link = 0; link < LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_VOLTA; ++link)
    {
        if (FLD_IDX_TEST_DRF(_PBUS, _SW_SCRATCH30, _LWLINK_ENABLED,
                            link, _OFF, scratchReg))
        {
            scratchReg = FLD_IDX_SET_DRF_DEF(_PBUS, _SW_SCRATCH30,
                            _LWLINK_STATUS, link, _DOWN, scratchReg);
        }
        else
        {
            reg = DLPL_REG_RD32(link, LW_PLWL_LINK_STATE);

            if (FLD_TEST_DRF(_PLWL, _LINK_STATE, _STATE, _ACTIVE, reg))
            {
                scratchReg = FLD_IDX_SET_DRF_DEF(_PBUS, _SW_SCRATCH30,
                                _LWLINK_STATUS, link, _UP, scratchReg);
            }
            else
            {
                scratchReg = FLD_IDX_SET_DRF_DEF(_PBUS, _SW_SCRATCH30,
                                _LWLINK_STATUS, link, _DOWN, scratchReg);
            }
        }
    }

    return scratchReg;
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Gets the total number of Lwlinks.
 *
 * @param[out] pData  Pointer to the output Data
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 */
LwU8
smbpbiGetLwlinkNumLinks_GV10X
(
    LwU32 *pData
)
{
    *pData = LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_VOLTA;
    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}

/*!
 * @brief Gets the LwLink Up/Down state as a bitmask.
 *
 * @param[in]  arg1      Unsued, but needed for HAL interface
 * @param[in]  arg2      Unused, but needed for HAL interface
 * @param[out] pData     Pointer to the output Data
 * @param[out] pExtData  Unused, but needed for HAL interface
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if links are not initialized
 */
LwU8
smbpbiGetLwlinkStateV1_GV10X
(
    LwU8   arg1,
    LwU8   arg2,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    LwU32    reg;
    LwU32    val = 0;
    unsigned link;
    LwU8     status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    reg = s_smbpbiReadLwlinkScratch();

    if (DRF_VAL(_PBUS, _SW_SCRATCH30, _LWLINK_STATUS_SPEED, reg) == 0)
    {
        //
        // If nothing is set, the scratch register has not been initialized
        // by the driver yet
        //
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto smbpbiGetLwlinkStateV1_exit;
    }

    for (link = 0; link < LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_VOLTA; ++link)
    {
        if (FLD_IDX_TEST_DRF(_PBUS, _SW_SCRATCH30, _LWLINK_STATUS, link, _UP, reg))
        {
            val |= BIT(link);
        }
    }

    *pData = val;

smbpbiGetLwlinkStateV1_exit:
    return status;
}

/*!
 * @brief Gets the Lwlink bandwidth.
 *
 * @param[in]  arg1      Unused
 * @param[in]  arg2      Unused
 * @param[out] pData     Pointer to output Data
 * @param[out] pExtData  Unused
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *     if success
 *  @return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *     if links are not initialized
 */
LwU8
smbpbiGetLwlinkBandwidth_GV10X
(
    LwU8   arg1,
    LwU8   arg2,
    LwU32  *pData,
    LwU32  *pExtData
)
{
    LwU32    reg;
    LwU32    val = 0;
    unsigned link;
    LwU8     status = LW_MSGBOX_CMD_STATUS_SUCCESS;

    reg = s_smbpbiReadLwlinkScratch();

    if (DRF_VAL(_PBUS, _SW_SCRATCH30, _LWLINK_STATUS_SPEED, reg) == 0)
    {
        //
        // If nothing is set, the scratch register has not been initialized
        // by the driver yet
        //
        status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
        goto smbpbiGetLwlinkBandwidth_exit;
    }

    for (link = 0; link < LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_VOLTA; ++link)
    {
        if (FLD_IDX_TEST_DRF(_PBUS, _SW_SCRATCH30, _LWLINK_STATUS, link, _UP, reg))
        {
            switch (DRF_IDX_VAL(_PBUS, _SW_SCRATCH30, _LWLINK_SPEED, link, reg))
            {
                case LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_19_GHZ:
                    val = LW_MSGBOX_DATA_LWLINK_LINE_RATE_19200_MBPS;
                    break;
                case LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_20_GHZ:
                    val = LW_MSGBOX_DATA_LWLINK_LINE_RATE_20000_MBPS;
                    break;
                case LW_PBUS_SW_SCRATCH30_LWLINK_SPEED_25_GHZ:
                    val = LW_MSGBOX_DATA_LWLINK_LINE_RATE_25000_MBPS;
                    break;
                default:
                    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
                    goto smbpbiGetLwlinkBandwidth_exit;
            }

            *pData = val;
            goto smbpbiGetLwlinkBandwidth_exit;
        }
    }

    status = LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;

smbpbiGetLwlinkBandwidth_exit:
    return status;
}

/*!
 * @brief   Return the requested Lwlink error counter
 *
 * @param[in]   arg1    request sub-type (error counter type)
 * @param[in]   link    Lwlink index
 * @param[out]  pData   SMBPBI data-out register value
 *
 * @return  LW_MSGBOX_CMD_STATUS_SUCCESS
 *          LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE
 *              - LW_PBUS_SW_SCRATCH(30) has not been populated yet.
 *              - requested lwlink is not enabled
 *          LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *              - invalid link index
 */
LwU8
smbpbiGetLwlinkErrorCount_GV10X
(
    LwU8    arg1,
    LwU8    link,
    LwU32   *pData
)
{
    LwU32       reg;

    if (link >= LW_MSGBOX_LWLINK_STATE_NUM_LWLINKS_VOLTA)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    reg = s_smbpbiReadLwlinkScratch();

    if (DRF_VAL(_PBUS, _SW_SCRATCH30, _LWLINK_STATUS_SPEED, reg) == 0)
    {
        //
        // If nothing is set, the scratch register has not been initialized
        // by the driver yet
        //
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
    }

    if (FLD_IDX_TEST_DRF(_PBUS, _SW_SCRATCH30, _LWLINK_ENABLED, link, _OFF, reg))
    {
        // This link is disabled
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_AVAILABLE;
    }

    switch (arg1)
    {
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_REPLAY:
            reg = DLPL_REG_RD32(link, LW_PLWL_SL0_ERROR_COUNT4);
            *pData = (LwU16) DRF_VAL(_PLWL, _SL0_ERROR_COUNT4, _REPLAY_EVENTS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_RECOVERY:
            reg = DLPL_REG_RD32(link, LW_PLWL_ERROR_COUNT1);
            *pData = (LwU16) DRF_VAL(_PLWL, _ERROR_COUNT1, _RECOVERY_EVENTS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_FLIT_CRC:
            reg = DLPL_REG_RD32(link, LW_PLWL_SL1_ERROR_COUNT1);
            *pData = (LwU16) DRF_VAL(_PLWL, _SL1_ERROR_COUNT1, _FLIT_CRC_ERRORS, reg);
            break;
        case LW_MSGBOX_CMD_ARG1_GET_LWLINK_INFO_ERR_COUNTER_DATA_CRC:
            reg = DLPL_REG_RD32(link, LW_PLWL_SL1_ERROR_COUNT2_LANECRC);
            *pData = (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT2_LANECRC, _L0, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT2_LANECRC, _L1, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT2_LANECRC, _L2, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT2_LANECRC, _L3, reg);
            reg = DLPL_REG_RD32(link, LW_PLWL_SL1_ERROR_COUNT3_LANECRC);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT3_LANECRC, _L4, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT3_LANECRC, _L5, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT3_LANECRC, _L6, reg);
            *pData += (LwU8) DRF_VAL(_PLWL, _SL1_ERROR_COUNT3_LANECRC, _L7, reg);
            break;
        default:    // to pacify Coverity
            break;
    }

    return LW_MSGBOX_CMD_STATUS_SUCCESS;
}
