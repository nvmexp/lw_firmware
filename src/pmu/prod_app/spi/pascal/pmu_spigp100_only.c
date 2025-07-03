/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_spigp100_only.c
 * @brief  SPI routines specific to GP100 only.
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
spiSetFrequency_GP100
(
    SPI_DEVICE *pSpiDev
)
{
    LwU32 spiCfg = 0x0;

    // Configure SPI bus. 
    spiCfg = DRF_NUM(_PMGR, _SPI_CONFIG, _CPOL, pSpiDev->clockPolarity) |
             DRF_NUM(_PMGR, _SPI_CONFIG, _CPHA, pSpiDev->clockPhase) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _PERIOD, _25P0MHZ ) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _READLATENCY, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _REFCLK, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _SCKDIV, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _CS_HOLD, _INIT) |
             DRF_DEF(_PMGR, _SPI_CONFIG, _CS_SETUP, _INIT);

    REG_WR32(BAR0, LW_PMGR_SPI_CONFIG, spiCfg);

    return FLCN_OK;
}

/*!
 * Configure HW-SPI(Initiator) Control register to transact with Target.
 *
 * @param[in] pCtrl        Pointer to @ref SPI_HW_CTRL.
 * @param[in] timeout      Timeout value in Micro Seconds.        
 *
 * @return FLCN_OK
 *     If HW-SPI transaction triggered by controller was successfully.
 * @return Other unexpected error
 */
FLCN_STATUS
spiWriteCtrl_GP100
(
    SPI_HW_CTRL *pCtrl,
    LwU32        timeout
)
{
    LwU32 spiCtrl = 0x0;

    spiCtrl |= DRF_DEF(_PMGR, _SPI_CTRL, _TRANSFER, _TRIGGER) |
               DRF_NUM(_PMGR, _SPI_CTRL, _TRANSFER_SIZE,
               ((pCtrl->rxSizeBytes + pCtrl->txSizeBytes) - 1));

    if (pCtrl->txSizeBytes != 0x0)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _TRANSMIT, _YES, spiCtrl);
        spiCtrl = FLD_SET_DRF_NUM(_PMGR, _SPI_CTRL, _TRANSMIT_SIZE, 
                               ((pCtrl->txSizeBytes) - 1), spiCtrl);
    }

    if (pCtrl->ctrlFlags & SPI_CTRL_FLAGS_READ_ENABLE)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _RECEIVE, _YES, spiCtrl);
    }

    if (pCtrl->ctrlFlags & SPI_CTRL_FLAGS_TARGET_DEASSERT)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _DESELECT, _YES, spiCtrl);
    }

    if (pCtrl->ctrlFlags & SPI_CTRL_FLAGS_REVERSE_ENABLE)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _REVERSE, _YES, spiCtrl);
    }

    REG_WR32(BAR0, LW_PMGR_SPI_CTRL, spiCtrl);

    return spiHwWaitForIdle_HAL(&Spi, timeout);
}
