/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_spitu10x.c
 * @brief  SPI routines specific to Turing and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_XUSB_FLR_TIMEOUT_US        (100000)

/* ------------------------- Prototypes ------------------------------------- */

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Block/Unblock XUSB access to SPI.
 *
 * Blocking XUSB access to SPI prevents XUSB traffic from interrupting
 * SPI operations initiated by PMU. PMU must honor high priority bit set
 * by HW and ensure that ongoing XUSB operation is over before blocking
 * XUSB further access to SPI.
 *
 * @param[in]  pSpiDev  SPI_DEVICE pointer
 * @param[in]  bBlock   block/unblock XUSB access.
 *
 * @return FLCN_OK
 *     If XUSB access to SPI was blocked/unblocked successfully.
 * @return Other unexpected error
 */
FLCN_STATUS
spiArbiterXusbBlock_TU10X
(
    SPI_DEVICE    *pSpiDev,
    LwBool         bBlock
)
{
    LwU32       spiArbiterCtrl;
    FLCN_STATUS status = FLCN_OK;

    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiArbiterBlockXusb_TU10X_exit;
    }

    // Block or Unblock XUSB access to SPI.
    spiArbiterCtrl = REG_RD32(BAR0, LW_PMGR_SPI_ARBITER_CTRL0);
    if (!FLD_TEST_DRF_NUM(_PMGR, _SPI_ARBITER_CTRL0, _BLOCK_BIT, bBlock, spiArbiterCtrl))
    {
        spiArbiterCtrl = FLD_SET_DRF_NUM(_PMGR, _SPI_ARBITER_CTRL0, _BLOCK_BIT,
                                         bBlock, spiArbiterCtrl);
        REG_WR32(BAR0, LW_PMGR_SPI_ARBITER_CTRL0, spiArbiterCtrl);
    }

spiArbiterBlockXusb_TU10X_exit:
    return status;
}
