/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020  by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pcieinfogh100.c
 * @brief   PMU HAL functions for GH100 and later, handling SMBPBI queries for
 *          PCIe status and errors.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objsmbpbi.h"
#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "dev_xtl_ep_pcfg_gpu.h"
#include "dev_lw_xpl.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"
#include "config/g_bif_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Function Prototypes  -------------------- */
/* ------------------------ Static Function Prototypes  -------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief       Get PCIe Speed and Width
 *
 * @param[out]  pSpeed   Pointer to returned PCIe link speed value
 * @param[out]  pWidth   Pointer to returned PCIe link width value
 */
void
smbpbiGetPcieSpeedWidth_GH100
(
    LwU32   *pSpeed,
    LwU32   *pWidth
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_EP_PCFG_GPU_LINK_CONTROL_STATUS);

    // Get PCIe Speed
    switch (DRF_VAL(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _LWRRENT_LINK_SPEED, reg))
    {
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_2P5:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_2500_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_5P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_5000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_8P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_8000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_16P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_16000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_32P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_32000_MTPS;
            break;
        }
        default:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_UNKNOWN;
            break;
        }
    }

    // Get PCIe Width
    switch (DRF_VAL(_EP_PCFG_GPU, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, reg))
    {
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X1;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X2;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X4;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X8;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X16;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X32:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X32;
            break;
        }
        default:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_UNKNOWN;
        }
    }
}

/*!
 * @brief       Get PCIe link L0 to recovery count
 *
 * @param[out]  pL0ToRecCount Pointer to returned PCIe link
 *                            L0 to recovery count
 */
void
smbpbiGetPcieL0ToRecoveryCount_GH100
(
    LwU32 *pL0ToRecCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XPL_PL_LTSSM_L0_TO_RECOVERY_COUNT);

    *pL0ToRecCount = DRF_VAL(_XPL, _PL_LTSSM_L0_TO_RECOVERY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay count
 *
 * @param[out]  pReplayCount  Pointer to returned PCIe link
 *                            replay count
 */
void
smbpbiGetPcieReplayCount_GH100
(
    LwU32 *pReplayCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XPL_DL_REPLAY_COUNT);

    *pReplayCount = DRF_VAL(_XPL, _DL_REPLAY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay rollover count
 *
 * @param[out]  pReplayRolloverCount    Pointer to returned PCIe link
 *                                      replay rollover count
 */
void
smbpbiGetPcieReplayRolloverCount_GH100
(
    LwU32 *pReplayRolloverCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XPL_DL_REPLAY_ROLLOVER_COUNT);

    *pReplayRolloverCount = DRF_VAL(_XPL, _DL_REPLAY_ROLLOVER_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS received count
 *
 * @param[out]  pNaksRcvdCount  Pointer to returned PCIe link
 *                              NAKS received count
 */
void
smbpbiGetPcieNaksRcvdCount_GH100
(
    LwU32 *pNaksRcvdCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XPL_DL_NAKS_RCVD_COUNT);

    *pNaksRcvdCount = DRF_VAL(_XPL, _DL_NAKS_RCVD_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS sent count
 *
 * @param[out]  pNaksSentCount  Pointer to returned PCIe link
 *                              NAKS sent count
 */
void
smbpbiGetPcieNaksSentCount_GH100
(
    LwU32 *pNaksSentCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XPL_DL_NAKS_SENT_COUNT);

    *pNaksSentCount = DRF_VAL(_XPL, _DL_NAKS_SENT_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe Target Speed
 *
 * @param[out]  pSpeed   Pointer to returned PCIe link speed value
 */
void
smbpbiGetPcieTargetSpeed_GH100
(
    LwU32   *pSpeed
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2);

    // Get PCIe Target Speed
    switch (DRF_VAL(_EP_PCFG_GPU, _LINK_CONTROL_STATUS_2, _TARGET_LINK_SPEED, reg))
    {
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_LWRRENT_LINK_SPEED_2P5:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_2500_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_5P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_5000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_8P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_8000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_16P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_16000_MTPS;
            break;
        }
        case LW_EP_PCFG_GPU_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_LINK_SPEED_32P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_32000_MTPS;
            break;
        }
        default:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_UNKNOWN;
            break;
        }
    }
}
