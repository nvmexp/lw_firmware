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
 * @file   pmu_spiromgp100.c
 * @brief  SPI ROM routines specific to Pascal and later.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "config/g_spi_private.h"
#include "dev_ext_devices.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
static SPI_DEVICE_ROM_INFO  spiDevicesSupported_GP100[] 
    GCC_ATTRIB_SECTION("dmem_spi", "spiDevicesSupported_GP100") =
{
        //
        //                                  Packed
        //                                  Info    Dev.
        //       Manufacturer Code          bits    Code
        // =============================    ======  ======
        //
        // Atmel_________________________________________
        // ID=0x9F
        // Serial ROM modelled for Emulator
        {     LW_ROM_MANUFACTURER_ATMEL,     0x0F,  0x4602 },
        // Adesto________________________________________
        {     LW_ROM_MANUFACTURER_ADESTO,    0x0F,  0x4301 },
        {     LW_ROM_MANUFACTURER_ADESTO,    0x0F,  0x4402 },
        {     LW_ROM_MANUFACTURER_ADESTO,    0x0F,  0x4502 },
        // Winbond_______________________________________
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x6012 },
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x6013 },
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x6014 },
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x6015 },
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x8016 },
        // Macronix______________________________________
        {     LW_ROM_MANUFACTURER_MACRONIX,  0x07,  0x2532 },
        {     LW_ROM_MANUFACTURER_MACRONIX,  0x0F,  0x2533 },
        {     LW_ROM_MANUFACTURER_MACRONIX,  0x0F,  0x2534 },
        {     LW_ROM_MANUFACTURER_MACRONIX,  0x0F,  0x2535 },
        // ISSI__________________________________________
        {     LW_ROM_MANUFACTURER_ISSI,      0x07,  0x1152 },
        {     LW_ROM_MANUFACTURER_ISSI,      0x07,  0x1253 },
        {     LW_ROM_MANUFACTURER_ISSI,      0x07,  0x7014 },
        {     LW_ROM_MANUFACTURER_ISSI,      0x07,  0x7015 },
        // GigaDevice____________________________________
        {     LW_ROM_MANUFACTURER_GD,        0x03,  0x6012 },
        {     LW_ROM_MANUFACTURER_GD,        0x07,  0x6013 },
        {     LW_ROM_MANUFACTURER_GD,        0x07,  0x6014 },
        // Micron Technology, Inc________________________
        {     LW_ROM_MANUFACTURER_MICRON,    0x07,  0xBB15 },
        // Cypress_______________________________
        {     LW_ROM_MANUFACTURER_CYPRESS,   0x27,  0x0217 },
        // PUYA__________________________________________
        {     LW_ROM_MANUFACTURER_PUYA,      0x07,  0x6014 },
        // Do not add any devices after this entry_______
        {     0x00,                          0x00,  0x0000 }
};

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Lookup and initialize SPI ROM from JEDEC ID.
 *
 * This function attempts to lookup SPI ROM Device present on-board
 * in supported ROM device table based on JEDEC ID and initilizes 
 * SPI ROM if found. 
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 * @param[in]  jedecId  JEDEC ID of ROM device
 *
 * @return FLCN_OK
 *     If device was identified successfully.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomLookup_GP100
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32           jedecId
)
{
    LwU8                        manufacturerCode;
    LwU16                       deviceCode;
    SPI_DEVICE_ROM_INFO        *pTable;
    FLCN_STATUS                 status = FLCN_ERR_NOT_SUPPORTED;

    manufacturerCode = (LwU8)(DRF_VAL(_ROM, _HW_JEDECID,
                                      _MANUFACTURERCODE, jedecId));
    deviceCode       = (LwU16)(((DRF_VAL(_ROM, _HW_JEDECID,
                                         _DEVICECODEHI, jedecId)) << 8) |
                                (DRF_VAL(_ROM, _HW_JEDECID,
                                         _DEVICECODELO, jedecId)));

    for (pTable = spiDevicesSupported_GP100; pTable->manufacturerCode != 0; pTable++)
    {
        if ((pTable->manufacturerCode == manufacturerCode) &&
            (pTable->deviceCode == deviceCode))
        {
            pSpiRom->spiRomInfo = *pTable;
            status = FLCN_OK;
            break;
        }
    }

    return status;
}

/*!
 * @brief Extract EEPROM Status Register Value.
 *
 * @param[in]   data           4-byte data register value
 * @param[out]  pRomStatus     1-byte ROM Status register value
 *
 * @return FLCN_OK
 *     If SPI EEPROM Status Register value was extracted correctly.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomGetStatus_GP100
(
    LwU32           data,
    LwU8           *pRomStatus
)
{
    if (pRomStatus == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pRomStatus = (LwU8)(DRF_VAL(_ROM, _STATUS_REGISTER,
                                 _HW, data)); 

    return FLCN_OK;
}

/*!
 * @brief Checks whether the given offset is within accessible
 * region of PROM.
 *
 * @param[in] offset  Offset that needs to be checked
 *
 * @return LwBool
 */
LwBool
spiRomIsValidOffset_GP100
(
    LwU32 offset
)
{
    return (offset < LW_PROM_DATA__SIZE_1);
}
