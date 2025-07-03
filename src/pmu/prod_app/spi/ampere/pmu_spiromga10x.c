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
 * @file   pmu_spiromga10x.c
 * @brief  SPI ROM routines specific to Ampere and later.
 */

#include "pmu_objspi.h"
#include "pmu_bar0.h"
#include "dev_pmgr.h"
#include "dev_ext_devices.h"
#include "config/g_spi_private.h"

/* ------------------------- Static Variables ------------------------------- */
static SPI_DEVICE_ROM_INFO  spiDevicesSupported_GA10X[] 
    GCC_ATTRIB_SECTION("dmem_spi", "spiDevicesSupported_GA10X") =
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
        {     LW_ROM_MANUFACTURER_WINBOND,   0x07,  0x8015 },
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
        {     LW_ROM_MANUFACTURER_GD,        0x07,  0x6015 },
        // Micron Technology, Inc________________________
        {     LW_ROM_MANUFACTURER_MICRON,    0x07,  0xBB15 },
        // Cypress_______________________________
        {     LW_ROM_MANUFACTURER_CYPRESS,   0x27,  0x0217 },
        // PUYA__________________________________________
        {     LW_ROM_MANUFACTURER_PUYA,      0x07,  0x6014 },
        {     LW_ROM_MANUFACTURER_PUYA,      0x07,  0x6015 },
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
spiRomLookup_GA10X
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

    for (pTable = spiDevicesSupported_GA10X; pTable->manufacturerCode != 0; pTable++)
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
 * @brief Checks whether the given offset is within accessible
 * region of PROM.
 *
 * @param[in] offset  Offset that needs to be checked
 *
 * @return LwBool
 */
LwBool
spiRomIsValidOffset_GA10X
(
    LwU32 offset
)
{
    if (spiRomIsOffsetWithinDataAperture_HAL(&Spi, offset))
    {
        return LW_TRUE;
    }
    else
    {
        return (offset < LW_PROM_2_DATA_LIMIT);
    }
}

/*!
 * @brief Checks whether the given offset is within LW_PROM_DATA
 * aperture i.e. 1MB.
 *
 * @param[in] offset  Offset that needs to be checked
 *
 * @return LwBool
 */
LwBool
spiRomIsOffsetWithinDataAperture_GA10X
(
    LwU32 offset
)
{
    return (offset < LW_PROM_DATA__SIZE_1);
}

/*!
 * @brief Read a buffer directly from the on-board PCI Expansion ROM, using the
 *        built-in SPI HW read-only controller.
 *
 * This implementation is specific to the on-board PCI Expansion ROM.
 *
 * @param[in]   pSpiRom The SPI_DEVICE_ROM to read.
 * @param[in]   address Start offset of the buffer to read in SPI device
 *                      memory.
 * @param[in]   size    Size of the buffer to read limited to 4KB.
 * @param[out]  pBuf    Pointer to read the data into.
 *
 * @return FLCN_ERR_ILWALID_STATE       If the SPI ROM device is invalid.
 * @return FLCN_ERR_ILWALID_ARGUMENT    If the size is 0 or buffer is NULL.
 * @return FLCN_ERR_NOT_SUPPORTED       If the SPI ROM device cannot be
 *                                      accessed by this SPI interface.
 * @return FLCN_OK                      If the buffer is filled with the
 *                                      requested data from the SPI ROM device.
 */
FLCN_STATUS
spiRomReadDirect_GA10X
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32           address,
    LwU16           size,
    LwU8           *pBuf
)
{
    LwBool      bReenableBypass;
    LwU8        byteIndex;
    LwU8        byteShiftIndex;
    LwU8        bytesToRead;
    LwU8        i;
    LwU32       data;
    FLCN_STATUS status    = FLCN_OK;
    const LwU8  dwordSize = sizeof(LwU32);
    LwU32       offsetPage;
    LwU32       regData;


    if (spiRomIsOffsetWithinDataAperture_HAL(&Spi, address))
    {
        return spiRomReadDirect_GMXXX(pSpiRom, address, size, pBuf);
    }

    if (pSpiRom == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomReadDirect_GA10X_exit;
    }

    if ((size == 0) || (pBuf == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomReadDirect_GA10X_exit;
    }

    if ((pSpiRom->super.busId != 0) || (pSpiRom->super.chipSelectId != 0))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomReadDirect_GA10X_exit;
    }

    //
    // Disable ROM serial bypass while reading from LW_PROM_2_DATA, or we'll get
    // all zeroes back.
    //
    status = spiRomSerialBypassEnable_HAL(&Spi, pSpiRom, LW_FALSE,
                                          &bReenableBypass);
    if (status != FLCN_OK)
    {
        goto spiRomReadDirect_GA10X_exit;
    }

    offsetPage = LW_ALIGN_DOWN( address, LW_PROM_2_DATA__SIZE_1 );
    regData = REF_NUM( LW_PMGR_ROM_ADDR_OFFSET_2_AMOUNT, ( offsetPage / sizeof(LwU32) ) ) |
               REF_DEF( LW_PMGR_ROM_ADDR_OFFSET_2_EN, _ENABLED );

    // Set 4KB ROM aperture base offset and update cached setting
    REG_WR32( BAR0, LW_PMGR_ROM_ADDR_OFFSET_2 , regData);

    address -= offsetPage;

    //
    // Although LW_PROM_2_DATA supports byte-aligned 32-bit reads, this routine
    // intends to optimize by only using dword-aligned 32-bit reads.
    //
    byteIndex = (LwU8)(address % dwordSize);

    for (address -= byteIndex; size > 0; address += dwordSize, byteIndex = 0,
            size -= bytesToRead, pBuf += bytesToRead)
    {
        data = REG_RD32(BAR0, LW_PROM_2_DATA(address));

        // Skip the first byteIndex bytes, and put the rest in the buffer
        bytesToRead = LW_MIN((dwordSize - byteIndex), size);
        for (i = 0; i < bytesToRead; i++)
        {
            byteShiftIndex = (LwU8)((byteIndex + i) % dwordSize);
            pBuf[i] = (data >> (8 * byteShiftIndex)) & 0xFF;
        }
    }

    // Enable ROM serial bypass, if it was enabled to begin with
    status = spiRomSerialBypassEnable_HAL(&Spi, pSpiRom, bReenableBypass, NULL);

spiRomReadDirect_GA10X_exit:
    return status;
}
