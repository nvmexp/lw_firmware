/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"
#include "soe_bar0.h"
#include "soe_objpmgr.h"
#include "soe_objspi.h"
#include "soe_objsaw.h"

/* ------------------------ Application Includes --------------------------- */
#include "config/g_spi_hal.h"
#include "dev_pmgr.h"
#include "dev_ext_devices.h"
#include "dev_lwlsaw_ip.h"
#include "dev_lwlsaw_ip_addendum.h"

/* ------------------------- Static Variables ------------------------------- */
sysTASK_DATA static SPI_DEVICE_ROM_INFO  spiDevicesSupported_LS10[] =
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
        // Do not add any devices after this entry_______
        {     0x00,                          0x00,  0x0000 }
};

/* ------------------------- Prototypes ------------------------------------- */
sysSYSLIB_CODE static void s_spiRomAddrAdjust_LS10(LwU32 *pAddress);
sysTASK_DATA SPI_DEVICE_ROM spiRom;
sysTASK_DATA INFOROM_REGION inforomRegion;
sysTASK_DATA ERASE_LEDGER_REGION ledgerRegion;
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Pre-STATE_INIT for SPI
 */
sysSYSLIB_CODE void
spiPreInit_LS10(void)
{
    // TODO: Add any early SPI init
    spiRom.super.busId          = 0;
    spiRom.super.chipSelectId   = 0;
    spiRom.super.clockPolarity  = 0;
    spiRom.super.clockPhase     = 0;
    spiRom.super.clockPeriod    = 50;
    spiRom.pInforom = &inforomRegion;
    spiRom.pLedgerRegion = &ledgerRegion;
}

/*!
 * @brief   Service SPI interrupts
 */
sysSYSLIB_CODE void
spiService_LS10(void)
{
    //
    // TODO: Remove once we have actual handling in place.
    // Clear the enables (mask) for any interrupts we get so we dont come back
    // if we somehow got here.
    //
    PMGR_REG_WR32(LW_PMGR_PMU_INTR_MSK_SPI, 0);
}

/*!
 * @brief Prepares a bus for a transfer.
 *
 * Lwrrently there is a single SPI slave (the on-board EEPROM) on a single
 * SPI bus.
 *
 * Longer-term, if we need multiple SPI devices on multiple SPI buses
 * then SPI Bus ID would also need to be specified here.
 *
 * @param[in]  pSpiDev  SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If SPI Bus was configured successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiBusConfigure_LS10
(
    SPI_DEVICE    *pSpiDev
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBusConfigure_LS10_exit;
    }

    status =  spiRomSerialBypassEnable_HAL(&SpiHal, (PSPI_DEVICE_ROM)pSpiDev,
                   LW_FALSE, NULL);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_LS10_exit;
    }

    // Set Master frequency before transacting with slave.
    status = spiSetFrequency_HAL(&SpiHal, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_LS10_exit;
    }

spiBusConfigure_LS10_exit:
    return status;
}

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
 * single SPI slave (the on-board ROM) on a single SPI bus.
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
sysSYSLIB_CODE FLCN_STATUS
spiRomSerialBypassEnable_LS10
(
    PSPI_DEVICE_ROM pSpiRom,
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
        goto spiRomSerialBypassEnable_LS10_exit;
    }

    if ((pSpiRom->super.busId != 0) || (pSpiRom->super.chipSelectId != 0))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomSerialBypassEnable_LS10_exit;
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

spiRomSerialBypassEnable_LS10_exit:
    return status;
}

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
sysSYSLIB_CODE FLCN_STATUS
spiRomLookup_LS10
(
    PSPI_DEVICE_ROM pSpiRom,
    LwU32           jedecId
)
{
    LwU8                        manufacturerCode;
    LwU16                       deviceCode;
    PSPI_DEVICE_ROM_INFO        pTable;
    FLCN_STATUS                 status = FLCN_ERR_NOT_SUPPORTED;

    manufacturerCode = (LwU8)(DRF_VAL(_ROM, _HW_JEDECID,
                                      _MANUFACTURERCODE, jedecId));
    deviceCode       = (LwU16)(((DRF_VAL(_ROM, _HW_JEDECID,
                                         _DEVICECODEHI, jedecId)) << 8) |
                                (DRF_VAL(_ROM, _HW_JEDECID,
                                         _DEVICECODELO, jedecId)));

    for (pTable = spiDevicesSupported_LS10; pTable->manufacturerCode != 0; pTable++)
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
 * @brief Transfer data between SPI Controller and SPI Device.
 *
 * @param[in]  pSpiDev     SPI_DEVICE pointer
 * @param[in]  pSpiCtrl    Pointer to @ref SPI_HW_CTRL.
 * @param[out] pRxBuffer   Pointer to data received from Device.
 * @param[in]  pTxBuffer   Pointer to data transmitted to Device.
 *
 * @return FLCN_OK
 *     Upon success.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiBufferTransfer_LS10
(
    PSPI_DEVICE   pSpiDev,
    PSPI_HW_CTRL  pSpiCtrl,
    LwU8         *pRxBuffer,
    LwU8         *pTxBuffer
)
{
    FLCN_STATUS  status     = FLCN_ERR_NOT_SUPPORTED;
    LwU32        readOffset = 0x0;
    // TODO: Fix buffer overrun/alignment in spiReadData_LR10(). Bug 3463175.
    LwU32        rxSize     = LW_ALIGN_UP(pSpiCtrl->rxSizeBytes, sizeof(LwU32));
    LwU32        txSize     = pSpiCtrl->txSizeBytes;

    if ((pSpiDev == NULL) || ((rxSize == 0) && (txSize == 0)))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBufferTransfer_LS10_exit;
    }

    // Write HW-SPI DATA register.
    if (txSize != 0x0)
    {
        status = spiWriteData_HAL(&SpiHal, pTxBuffer, txSize);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_LS10_exit;
        }
    }

    // Configure HW-SPI Control register and trigger the transaction.
    status = spiWriteCtrl_HAL(&SpiHal, pSpiCtrl, LW_ROM_MAX_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        goto spiBufferTransfer_LS10_exit;
    }


    // Read back data in Input buffer
    if (rxSize != 0x0)
    {
        // Adjust readOffset when data is received along with transmit.
        if (txSize != 0x0)
        {
            readOffset = LW_ROM_READ_START_OFFSET;
        }

        status = spiReadData_HAL(&SpiHal, pRxBuffer, rxSize, readOffset);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_LS10_exit;
        }
    }

spiBufferTransfer_LS10_exit:
    return status;
}

/*!
 * @brief Write Data which needs to be transmitted to SPI device.
 *
 * @param[in]  pData         Pointer to data buffer.
 * @param[in]  size     Size of data to be written which is limited to
 *                           256 byte DATA_ARRAY registers.
 *
 * @return FLCN_OK
 *     If WRITE was successful.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiWriteData_LS10
(
    LwU8  *pData,
    LwU16  size
)
{
    LwU32       i;
    LwU32       val;
    LwU16       numBytes;

    if (size > RM_SOE_SPI_XFER_MAX_BLOCK_SIZE)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }


    //
    // To correctly write an LwU8 buffer into 32-bit fields
    // we must ensure the following:
    //
    // 1) Do not overrun the input buffer.
    // 2) Do not perform unaligned memory accesses.
    //
    // See bug 3463175 for more info.
    //
    for (i = 0; size > 0; i++)
    {
        val = 0;
        numBytes = LW_MIN(size, sizeof(LwU32));

        memcpy(&val, pData, numBytes);
        REG_WR32(BAR0, LW_PMGR_SPI_DATA_ARRAY(i), val);

        pData += numBytes;
        size -= numBytes;
    }

    return FLCN_OK;
}

/*!
 * @brief Read Data received from SPI Device.
 *
 * @param[out] pData         Pointer to data buffer.
 * @param[in]  size     Size of data to be read limited to
 *                           256 byte DATA_ARRAY registers.
 * @param[in]  dataArrayIdx  Index of DATA_ARRAY.
 *
 * @return FLCN_OK
 *     If READ was successful.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiReadData_LS10
(
    LwU8  *pData,
    LwU16  size,
    LwU8   dataArrayIdx
)
{
    LwU32      *pBuf   = (LwU32 *)pData;
    LwU32       i;

    if (!(LW_IS_ALIGNED(size, sizeof(LwU32))))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    for (i = 0; i < size / sizeof(LwU32); i++)
    {
        *pBuf = REG_RD32(BAR0, LW_PMGR_SPI_DATA_ARRAY(dataArrayIdx + i));
        pBuf++;
    }

    return FLCN_OK;
}

/*!
 * Configure HW-SPI(Master) Control register to transact with Slave.
 *
 * @param[in] pCtrl        Pointer to @ref SPI_HW_CTRL.
 * @param[in] timeout      Timeout value in Micro Seconds.
 *
 * @return FLCN_OK
 *     If HW-SPI transaction triggered by controller was successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiWriteCtrl_LS10
(
    PSPI_HW_CTRL pCtrl,
    LwU32        timeout
)
{
    LwU32 spiCtrl = 0x0;

    spiCtrl |= DRF_DEF(_PMGR, _SPI_CTRL, _TRANSFER, _TRIGGER) |
               DRF_NUM(_PMGR, _SPI_CTRL, _CSID, pCtrl->chipSelectId) |
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

    if (pCtrl->ctrlFlags & SPI_CTRL_FLAGS_SLAVE_DEASSERT)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _DESELECT, _YES, spiCtrl);
    }

    if (pCtrl->ctrlFlags & SPI_CTRL_FLAGS_REVERSE_ENABLE)
    {
        spiCtrl = FLD_SET_DRF(_PMGR, _SPI_CTRL, _REVERSE, _YES, spiCtrl);
    }

    REG_WR32(BAR0, LW_PMGR_SPI_CTRL, spiCtrl);

    return spiHwWaitForIdle_HAL(&SpiHal, timeout);
}

/*!
 * @brief Wait for the HW SPI controller to idle, or until an internal timeout.
 *
 * @param[in] timeoutUs     The timeout value in Micro Seconds which
 *                          cannot exceed ((2^32 - 1)/1000).
 *
 * @return FLCN_OK, on success.
 * @return FLCN_ERR_TIMEOUT, if timeout is exceeded.
 */
sysSYSLIB_CODE FLCN_STATUS
spiHwWaitForIdle_LS10
(
    LwU32 timeoutUs
)
{
    FLCN_TIMESTAMP  startTime;
    LwU32           spiCtrl;
    LwU32           elapsedTime = 0;
    LwU32           timeoutNs   = (timeoutUs * 1000);

    osPTimerTimeNsLwrrentGet(&startTime);

    do
    {
        spiCtrl = REG_RD32(BAR0, LW_PMGR_SPI_CTRL);
        if (FLD_TEST_DRF(_PMGR, _SPI_CTRL, _TRANSFER, _DONE, spiCtrl))
        {
            return FLCN_OK;
        }

        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
        if (elapsedTime > timeoutNs)
        {
            break;
        }
        lwrtosYIELD();
    }
    while (LW_TRUE);

    return FLCN_ERR_TIMEOUT;
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
sysSYSLIB_CODE FLCN_STATUS
spiRomReadDirect_LS10
(
    PSPI_DEVICE_ROM pSpiRom,
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

    if (pSpiRom == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomReadDirect_LS10_exit;
    }

    if ((size == 0) || (pBuf == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto spiRomReadDirect_LS10_exit;
    }

    if ((pSpiRom->super.busId != 0) || (pSpiRom->super.chipSelectId != 0))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomReadDirect_LS10_exit;
    }

    //
    // The hardware automatically adjusts the address passed to LW_PROM if
    // offset mode is enabled in HW. Subtract the offset the HW will add here.
    //
    s_spiRomAddrAdjust_LS10(&address);

    //
    // Disable ROM serial bypass while reading from LW_PROM_DATA, or we'll get
    // all zeroes back.
    //
    status = spiRomSerialBypassEnable_HAL(&Spi, pSpiRom, LW_FALSE,
                                          &bReenableBypass);
    if (status != FLCN_OK)
    {
        goto spiRomReadDirect_LS10_exit;
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

spiRomReadDirect_LS10_exit:
    return status;
}

/*!
 * Set the speed of the HW SPI controller before transacting with slave.
 *
 * @param[in]  pSpiDev     SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If HW-SPI controller speed was configured correctly.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiSetFrequency_LS10
(
    PSPI_DEVICE pSpiDev
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

    REG_WR32(BAR0, LW_PMGR_SPI_CONFIG(pSpiDev->busId), spiCfg);

    return FLCN_OK;
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
sysSYSLIB_CODE FLCN_STATUS
spiRomGetStatus_LS10
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
sysSYSLIB_CODE FLCN_STATUS
spiRomInitInforomRegionFromScratch_LS10
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    LwU32    reg;
    LwU32    offset4K;
    LwU32    sizeIn4K;

    if ((pSpiRom == NULL) || ((pSpiRom->pInforom) == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    // Initialize InfoROM region from secure scratch register.
    reg = REG_RD32(SAW, LW_LWLSAW_SW_SCRATCH_2);

    // Sanity checks for valid InfoROM bounds
    if (FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_OFFSET,
                            0xfff, reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_OFFSET, 0x0,
                                    reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_SIZE, 0xfff,
                                    reg) ||
        FLD_TEST_DRF_NUM(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_SIZE, 0x0,
                                    reg))
    {
        return FLCN_ERR_FEATURE_NOT_ENABLED;
    }

    offset4K = DRF_VAL(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_OFFSET,
                            reg);
    sizeIn4K = DRF_VAL(_LWLSAW_SW, _SCRATCH_2, _INFOROM_CARVEOUT_SIZE,
                            reg);

    pSpiRom->pInforom->startAddress = (offset4K << 12);
    pSpiRom->pInforom->endAddress   = pSpiRom->pInforom->startAddress +
                                          (sizeIn4K << 12);

    pSpiRom->pBiosRegion->startAddress = 0; // Start at 0 
    pSpiRom->pBiosRegion->endAddress = pSpiRom->pInforom->startAddress - 1; // Inforom is at the end 

    return FLCN_OK;
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
sysSYSLIB_CODE static void
s_spiRomAddrAdjust_LS10
(
    LwU32 *pAddress
)
{
    LwU32 reg;
    LwU32 offset;

    reg = REG_RD32(BAR0, LW_PMGR_ROM_ADDR_OFFSET);

    if (FLD_TEST_DRF(_PMGR, _ROM_ADDR_OFFSET, _EN, _ENABLED, reg))
    {
        offset = DRF_VAL(_PMGR, _ROM_ADDR_OFFSET, _AMOUNT, reg);
        *pAddress -= (offset << 2);
    }
}
