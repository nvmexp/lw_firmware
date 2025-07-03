/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_spiromgf10x.c
 * @brief   SPI ROM routines specific to Kepler and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_bar0.h"

#include "dev_pmgr.h"
#include "dev_ext_devices.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_spiRomAddrOffsetEnable_GMXXX( LwBool bEnable);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Toggle the ROM serial bypass mode for the on-board PCI Expansion ROM.
 *
 * Serial bypass mode enables the manual driving of the SPI bus that the PCI
 * expansion ROM is attached to, via the LW_PMGR_ROM_SERIAL_BYPASS interface.
 * This is distinct from the SPI HW read-only controller (accessed via
 * LW_PROM_DATA) and the full SPI HW controller present on Pascal and later
 * (accessed via LW_PMGR_SPI_CTRL and LW_PMGR_SPI_DATA_ARRAY).
 *
 * Both LW_PROM_DATA and LW_PMGR_ROM_SERIAL_BYPASS are lwrrently tied to a
 * single SPI target (the on-board ROM) on a single SPI bus.
 *
 * @param[in]   pSpiRom         The SPI_DEVICE_ROM to enable or disable
 *                              serial bypass for.
 * @param[in]   bEnable         Whether to enable or disable serial bypass
 *                              for the SPI ROM device.
 * @param[out]  pbBypassToggled Whether serial bypass mode was changed as
 *                              a result of this call.
 *
 * @return FLCN_ERR_ILWALID_STATE   If the SPI ROM device is invalid.
 * @return FLCN_ERR_NOT_SUPPORTED   If the SPI ROM device cannot be accessed by
 *                                  this SPI interface.
 * @return FLCN_OK                  If the ROM serial bypass is in the
 *                                  requested mode.
 */
FLCN_STATUS
spiRomSerialBypassEnable_GMXXX
(
    SPI_DEVICE_ROM *pSpiRom,
    LwBool          bEnable,
    LwBool         *pbBypassToggled
)
{
    LwU32       reg;
    FLCN_STATUS status         = FLCN_OK;
    LwBool      bBypassToggled = LW_FALSE;

    if (pSpiRom == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomSerialBypassEnable_GMXXX_exit;
    }

    if ((pSpiRom->super.busId != 0) || (pSpiRom->super.chipSelectId != 0))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomSerialBypassEnable_GMXXX_exit;
    }

    reg = REG_RD32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS);

    if (!FLD_TEST_DRF_NUM(_PMGR, _ROM_SERIAL_BYPASS, _BYPASS, bEnable, reg))
    {
        reg = FLD_SET_DRF(_PMGR, _ROM_SERIAL_BYPASS, _CS, _DISABLED, reg);
        reg = FLD_SET_DRF(_PMGR, _ROM_SERIAL_BYPASS, _SI, _LOW, reg);
        reg = FLD_SET_DRF_NUM(_PMGR, _ROM_SERIAL_BYPASS, _SCK,
                              pSpiRom->super.clockPolarity, reg);
        reg = FLD_SET_DRF_NUM(_PMGR, _ROM_SERIAL_BYPASS, _BYPASS, bEnable,
                              reg);
        REG_WR32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS, reg);
        bBypassToggled = LW_TRUE;

        //
        // Toggling the ROM_SERIAL_BYPASS_BYPASS bit can cause the next read
        // from the ROM via BAR0 to be invalid. Reading back the value written
        // here prevents such invalid reads.
        //
        (void)REG_RD32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS);
    }

    if (pbBypassToggled != NULL)
    {
        *pbBypassToggled = bBypassToggled;
    }

spiRomSerialBypassEnable_GMXXX_exit:
    return status;
}

/*!
 * @brief Read a buffer directly from the on-board PCI Expansion ROM, using the
 *        built-in SPI HW read-only controller.
 *
 * This implementation is specific to the on-board PCI Expansion ROM.
 *
 * @param[in]   pSpiRom The SPI_DEVICE_ROM to read.
 * @param[in]   address Start offset of the buffer to read in SPI device
 *                      memory. This address includes the IFR-hiding ROM
 *                      address offset.
 * @param[in]   size    Size of the buffer to read.
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
spiRomReadDirect_GMXXX
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
    LwBool offsetDisabled = LW_FALSE;

    if (pSpiRom == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomReadDirect_GMXXX_exit;
    }

    if ((size == 0) || (pBuf == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomReadDirect_GMXXX_exit;
    }

    if ((pSpiRom->super.busId != 0) || (pSpiRom->super.chipSelectId != 0))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomReadDirect_GMXXX_exit;
    }

    //
    // On Kepler+, the hardware automatically adds an extra offset to addresses
    // sent to the ROM to hide the IFR at the beginning. Disable it here because
    // the IFR offset is always added to infoROM start.
    //
    // On Ampere+, the PLM for ROM_ADDR_OFFSET restrict writes from PMU.
    // Instead, adjust the address by ROM_ADDRESS_OFFSET_AMOUNT
    //
    if (spiRomAddrAdjust_HAL(&Spi, &address) == FLCN_ERR_NOT_SUPPORTED)
    {
        s_spiRomAddrOffsetEnable_GMXXX(LW_FALSE);
        offsetDisabled = LW_TRUE;
    }



    //
    // Disable ROM serial bypass while reading from LW_PROM_DATA, or we'll get
    // all zeroes back.
    //
    status = spiRomSerialBypassEnable_HAL(&Spi, pSpiRom, LW_FALSE,
                                          &bReenableBypass);
    if (status != FLCN_OK)
    {
        goto spiRomReadDirect_GMXXX_exit;
    }

    //
    // Although LW_PROM_DATA supports byte-aligned 32-bit reads, this routine
    // intends to optimize by only using dword-aligned 32-bit reads.
    //
    byteIndex = (LwU8)(address % dwordSize);

    for (address -= byteIndex; size > 0; address += dwordSize, byteIndex = 0,
            size -= bytesToRead, pBuf += bytesToRead)
    {
        data = REG_RD32(BAR0, LW_PROM_DATA(address));

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

spiRomReadDirect_GMXXX_exit:
    if (offsetDisabled)
    {
        s_spiRomAddrOffsetEnable_GMXXX(LW_TRUE);
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
spiRomIsValidOffset_GMXXX
(
    LwU32 offset
)
{
    return (offset < LW_PROM_DATA__SIZE_1);
}

/*!
 * @brief Enables or disables software write protection on the SPI ROM
 *
 * @param[in] pSpiRom SPI_DEVICE_ROM_ pointer
 * @param[in]  bEnable  Whether or not to enable software protection
 *
 * @return FLCN_OK
 *     If write protection was enabled/disabled successfully.
 * @return Other unexpected error
 */
LW_STATUS
spiRomWriteProtect_GMXXX
(
    SPI_DEVICE_ROM *pSpiRom,
    LwBool          bEnable
)
{
    FLCN_STATUS status;
    LwU8        writeProtectMask = 0x0;

    // Transmit Write Enable Command to ROM.
    status = spiRomWriteEnable(pSpiRom);
    if (status != FLCN_OK)
    {
        goto spiRomWriteProtect_exit;
    }

    if (bEnable)
    {
        // Reconstruct ROM Write Protect Mask from Packed Info byte.
        writeProtectMask = (LwU8)(DRF_VAL(_ROM, _PACKED_INFO, _WPMASK,
                                          pSpiRom->spiRomInfo.packedInfoBits) << 2);
    }

    // Update Status Register bits.
    status = spiRomWriteStatusReg(pSpiRom, writeProtectMask);
    if (status != FLCN_OK)
    {
        goto spiRomWriteProtect_exit;
    }

spiRomWriteProtect_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief Enables or disables the automatic offset that h/w adds when
 *        accessing the ROM via the BAR0 mapping.
 *
 * @param[in] bEnable  Whether to enable or disable the offset
 *
 * @return void
 */
static void
s_spiRomAddrOffsetEnable_GMXXX
(
    LwBool   bEnable
)
{
    LwU32       reg;

    reg = REG_RD32(BAR0, LW_PMGR_ROM_ADDR_OFFSET);

    if (!FLD_TEST_DRF_NUM(_PMGR, _ROM_ADDR_OFFSET, _EN, bEnable, reg))
    {
        reg = FLD_SET_DRF_NUM(_PMGR, _ROM_ADDR_OFFSET, _EN, bEnable, reg);
        REG_WR32(BAR0, LW_PMGR_ROM_ADDR_OFFSET, reg);
    }
}
