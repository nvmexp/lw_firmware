/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_spiromgv10x.c
 * @brief  SPI ROM routines specific to Volta and later.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objpmu.h"
#include "dev_gc6_island.h"
#include "dev_gc6_island_addendum.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Initialize InfoROM accessible range.
 *
 * Initialize InfoROM region from secure scratch registers which must be
 * populated by firmware.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 *
 * @return FLCN_ERR_ILWALID_STATE        If any pointer argument is NULL.
 * @return FLCN_ERR_FEATURE_NOT_ENABLED  If scratch register fields are not
 *                                       populated by firmware.
 * @return FLCN_OK                       If SPI EEPROM Accessible range was
 *                                       correctly initialized.
 */
FLCN_STATUS
spiRomInitInforomRegionFromScratch_GV10X
(
    SPI_DEVICE_ROM *pSpiRom
)
{
    LwU32    reg;
    LwU32    offset4K;
    LwU32    sizeIn4K;

    if ((pSpiRom == NULL) || ((pSpiRom->pInforom) == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Check if InfoROM is present
    reg = REG_RD32(FECS, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK,
                    _READ_PROTECTION_LEVEL2, _DISABLE,
                    reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    // Initialize InfoROM region from secure scratch register.
    reg = REG_RD32(FECS, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_1);

    // Sanity checks for valid InfoROM bounds
    if (FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                         _INFOROM_CARVEOUT_OFFSET, 0xfff, reg)  ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                         _INFOROM_CARVEOUT_OFFSET, 0x0, reg)    ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                         _INFOROM_CARVEOUT_SIZE, 0xfff, reg)    ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                         _INFOROM_CARVEOUT_SIZE, 0x0, reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    offset4K = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                       _INFOROM_CARVEOUT_OFFSET, reg);
    sizeIn4K = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_1,
                       _INFOROM_CARVEOUT_SIZE, reg);

    pSpiRom->pInforom->startAddress = (offset4K << 12);
    pSpiRom->pInforom->endAddress   = pSpiRom->pInforom->startAddress +
                                          (sizeIn4K << 12);
    return FLCN_OK;
}

/*!
 * @brief Initialize Erase Ledger accessible range.
 *
 * Initialize Erase Ledger region from secure scratch registers which must be
 * populated by firmware.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 *
 * @return FLCN_ERR_ILWALID_STATE        If any pointer argument is NULL.
 * @return FLCN_ERR_FEATURE_NOT_ENABLED  If scratch register fields are not
 *                                       populated by firmware.
 * @return FLCN_OK                       If SPI EEPROM Accessible range was
 *                                       correctly initialized.
 */
FLCN_STATUS
spiRomInitLedgerRegionFromScratch_GV10X
(
    SPI_DEVICE_ROM *pSpiRom
)
{
    LwU32    reg;
    LwU32    offset4K;
    LwU32    sizeIn4K;

    if ((pSpiRom == NULL) || ((pSpiRom->pLedgerRegion) == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Check if EL is present
    reg = REG_RD32(FECS, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_18_PRIV_LEVEL_MASK);
    if (FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18_PRIV_LEVEL_MASK,
                    _READ_PROTECTION_LEVEL2, _DISABLE,
                    reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    // Initialize EL region from secure scratch register.
    reg = REG_RD32(FECS, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_18);

    // Sanity checks for valid EL bounds;
    if (FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                        _ERASE_LEDGER_CARVEOUT_OFFSET, 0xfff, reg) ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                        _ERASE_LEDGER_CARVEOUT_OFFSET, 0x0, reg) ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                        _ERASE_LEDGER_CARVEOUT_SIZE, 0xfff, reg) ||
        FLD_TEST_DRF_NUM(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                        _ERASE_LEDGER_CARVEOUT_SIZE, 0x0, reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    offset4K = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                       _ERASE_LEDGER_CARVEOUT_OFFSET, reg);
    sizeIn4K = DRF_VAL(_PGC6, _AON_SELWRE_SCRATCH_GROUP_18,
                       _ERASE_LEDGER_CARVEOUT_SIZE, reg);

    pSpiRom->pLedgerRegion->startAddress = (offset4K << 12);
    pSpiRom->pLedgerRegion->endAddress   = pSpiRom->pLedgerRegion->startAddress +
                                               (sizeIn4K << 12);

    return FLCN_OK;
}
