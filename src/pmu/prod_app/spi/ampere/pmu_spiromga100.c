/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_spiromga100.c
 * @brief  SPI ROM routines specific to Ampere and later.
 */

#include "pmu_objspi.h"
#include "pmu_objpmu.h"
#include "dev_pmgr.h"
#include "config/g_spi_private.h"


/*!
 * @brief Adjust the address by ROM_ADDR_OFFSET_AMOUNT. This will only be
 *        non-zero if the secondary image is in use.
 *
 * @param[in/out] pAddress  address to adjust/adjusted address
 *
 * @return void
 */
FLCN_STATUS
spiRomAddrAdjust_GA100
(
    LwU32              *pAddress
)
{
    LwU32 reg;
    LwU32 offset;

    reg = REG_RD32(BAR0, LW_PMGR_ROM_ADDR_OFFSET);
    if (FLD_TEST_DRF(_PMGR, _ROM_ADDR_OFFSET, _EN, _ENABLED, reg))
    {
        // When LW_PMGR_ROM_ADDR_OFFSET is enabled, HW adds
        // _AMOUNT to offset passed to LW_PROM
        offset = DRF_VAL(_PMGR, _ROM_ADDR_OFFSET, _AMOUNT, reg);
        *pAddress -= (offset << 2);
    }

    return FLCN_OK;
}
