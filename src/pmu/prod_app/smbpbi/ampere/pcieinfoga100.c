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
 * @file    pcieinfoga100.c
 * @brief   PMU HAL functions for GA100 and later, handling SMBPBI queries for
 *          PCIe status and errors.
 */

/* ------------------------ System Includes -------------------------------- */
#include "dev_lw_xve.h"
#include "pmu_objsmbpbi.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_bif_private.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- External Definitions -------------------------- */
/* ------------------------- Static Variables ------------------------------ */
/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Static Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------ */
/*!
 * @brief       Get PCIe Target Speed
 *
 * @param[out]  pSpeed   Pointer to returned PCIe link speed value
 */
void
smbpbiGetPcieTargetSpeed_GA100
(
    LwU32   *pSpeed
)
{
    LwU32 reg = 0;

    reg = bifBar0ConfigSpaceRead_HAL(&Bif, LW_FALSE, LW_XVE_LINK_CONTROL_STATUS_2);

    // Get PCIe Target Speed
    switch (DRF_VAL(_XVE, _LINK_CONTROL_STATUS_2, _TARGET_LINK_SPEED, reg))
    {
        case LW_XVE_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_2P5:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_2500_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_5P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_5000_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_8P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_8000_MTPS;
            break;
        }
        case LW_XVE_LINK_CONTROL_STATUS_2_TARGET_LINK_SPEED_16P0:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_16000_MTPS;
            break;
        }
        default:
        {
            *pSpeed = LW_MSGBOX_DATA_PCIE_LINK_INFO_PAGE_3_TARGET_LINK_SPEED_UNKNOWN;
            break;
        }
    }
}

