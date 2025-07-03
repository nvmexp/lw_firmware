/*
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pwrgmxxxadxxx.c
 * @brief  PMU Power State (GCx) Hal Functions for GMXXX through ADXXX.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dev_bus.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_gc6.h"

#include "config/g_pmu_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Restores the BAR1 and BAR2 block addresses.
 *
 * @param[in]   pPciCfgAddr PCI configuration structure to use for restoration.
 *
 * @return  @ref FLCN_OK                    Success.
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pPciCfgAddr pointer is NULL.
 */
FLCN_STATUS
pmuBarBlockRegistersRestore_GMXXX
(
    RM_PMU_PCI_CONFIG_ADDR *pPciCfgAddr
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPciCfgAddr != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pmuRestoreBarBlockAddresses_GMXXX_exit);

    REG_WR32(BAR0, LW_PBUS_BAR1_BLOCK, pPciCfgAddr->bar1BlockRegister);
    REG_WR32(BAR0, LW_PBUS_BAR2_BLOCK, pPciCfgAddr->bar2BlockRegister);

pmuRestoreBarBlockAddresses_GMXXX_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
