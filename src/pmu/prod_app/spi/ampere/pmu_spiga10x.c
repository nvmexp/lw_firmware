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
 * @file   pmu_spiga10x.c
 * @brief  GA10X (-GA100) specific SPI routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objpmu.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Set the speed of the HW SPI controller before transacting with target.
 *
 * @param[in]  pSpiDev     SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If HW-SPI controller speed was configured correctly.
 * @return Other unexpected error
 */
FLCN_STATUS
spiSetFrequency_GA10X
(
    SPI_DEVICE *pSpiDev
)
{
    LwU32 spiCfg = 0x0;

    // Configure SPI bus. 
    spiCfg = DRF_NUM(_PMGR, _SPI_CONFIG, _CPOL, pSpiDev->clockPolarity) |
             DRF_NUM(_PMGR, _SPI_CONFIG, _CPHA, pSpiDev->clockPhase) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _PERIOD, _INIT ) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _READLATENCY, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _REFCLK, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _SCKDIV, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _CS_HOLD, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _CS_SETUP, _INIT);

    REG_WR32(BAR0, LW_PMGR_SPI_CONFIG(pSpiDev->busId), spiCfg);

    return FLCN_OK;
}
