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
 * @file    pcieinfotu10x.c
 * @brief   PMU HAL functions for TU10X and later, handling SMBPBI queries for
 *          PCIe status and errors.
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objsmbpbi.h"
#include "dev_bus.h"
#include "dev_bus_addendum.h"
#include "dev_lw_xve.h"
#include "dev_lw_xp.h"

/* ------------------------ Application Includes --------------------------- */
#include "pmu_bar0.h"

#include "config/g_smbpbi_private.h"
#include "config/g_bif_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */

// TODO: Remove after fixed manuals are available
#if !defined(LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0)
#define LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0       0x00000004 /* R---V */
#endif

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
smbpbiGetPcieSpeedWidth_TU10X
(
    LwU32   *pSpeed,
    LwU32   *pWidth
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_LINK_CONTROL_STATUS);

    // Get PCIe Speed
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _LINK_SPEED, reg))
    {
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_2P5:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_2500_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_5P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_5000_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_8P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_8000_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_LINK_SPEED_16P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_16000_MTPS;
            break;
        }
        default:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_SPEED_UNKNOWN;
            break;
        }
    }

    // Get PCIe Width
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS, _NEGOTIATED_LINK_WIDTH, reg))
    {
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X1:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X1;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X2:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X2;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X4:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X4;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X8:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X8;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_NEGOTIATED_LINK_WIDTH_X16:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_X16;
            break;
        }
        default:
        {
            *pWidth = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_0_LINK_WIDTH_UNKNOWN;
        }
    }
}

/*!
 * @brief       Get PCIe fatal, non-fatal, unsupported req error counts
 *
 * @param[out]  pFatalCount       Pointer to PCIe link fatal error count
 * @param[out]  pNonFatalCount    Pointer to PCIe link non-fatal error count
 * @param[out]  pUnsuppReqCount   Pointer to PCIe link unsupported req error count
 */
void
smbpbiGetPcieFatalNonFatalUnsuppReqCount_TU10X
(
    LwU32 *pFatalCount,
    LwU32 *pNonFatalCount,
    LwU32 *pUnsuppReqCount
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_ERROR_COUNTER);

    *pNonFatalCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                              _NON_FATAL_ERROR_COUNT_VALUE, reg);

    *pFatalCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                           _FATAL_ERROR_COUNT_VALUE, reg);

    *pUnsuppReqCount = DRF_VAL(_XVE, _ERROR_COUNTER,
                               _UNSUPP_REQ_COUNT_VALUE, reg);
}

/*!
 * @brief       Get PCIe link correctable error count
 *
 * @param[out]  pCorrErrCount  Pointer to returned PCIe link
 *                             correctable error count
 */
void
smbpbiGetPcieCorrectableErrorCount_TU10X
(
    LwU32 *pCorrErrCount
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_ERROR_COUNTER1);

    *pCorrErrCount = DRF_VAL(_XVE, _ERROR_COUNTER1,
                             _CORR_ERROR_COUNT_VALUE, reg);
}

/*!
 * @brief       Get PCIe link L0 to recovery count
 *
 * @param[out]  pL0ToRecCount Pointer to returned PCIe link
 *                            L0 to recovery count
 */
void
smbpbiGetPcieL0ToRecoveryCount_TU10X
(
    LwU32 *pL0ToRecCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_L0_TO_RECOVERY_COUNT(0));

    *pL0ToRecCount = DRF_VAL(_XP, _L0_TO_RECOVERY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay count
 *
 * @param[out]  pReplayCount  Pointer to returned PCIe link
 *                            replay count
 */
void
smbpbiGetPcieReplayCount_TU10X
(
    LwU32 *pReplayCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_REPLAY_COUNT(0));

    *pReplayCount = DRF_VAL(_XP, _REPLAY_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link replay rollover count
 *
 * @param[out]  pReplayRolloverCount    Pointer to returned PCIe link
 *                                      replay rollover count
 */
void
smbpbiGetPcieReplayRolloverCount_TU10X
(
    LwU32 *pReplayRolloverCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_REPLAY_COUNT(0));

    *pReplayRolloverCount = DRF_VAL(_XP, _REPLAY_ROLLOVER_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS received count
 *
 * @param[out]  pNaksRcvdCount  Pointer to returned PCIe link
 *                              NAKS received count
 */
void
smbpbiGetPcieNaksRcvdCount_TU10X
(
    LwU32 *pNaksRcvdCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_NAKS_RCVD_COUNT(0));

    *pNaksRcvdCount = DRF_VAL(_XP, _NAKS_RCVD_COUNT, _VALUE, reg);
}

/*!
 * @brief       Get PCIe link NAKS sent count
 *
 * @param[out]  pNaksSentCount  Pointer to returned PCIe link
 *                              NAKS sent count
 */
void
smbpbiGetPcieNaksSentCount_TU10X
(
    LwU32 *pNaksSentCount
)
{
    LwU32 reg = 0;

    reg = REG_RD32(BAR0, LW_XP_NAKS_SENT_COUNT(0));

    *pNaksSentCount = DRF_VAL(_XP, _NAKS_SENT_COUNT, _VALUE, reg);
}
