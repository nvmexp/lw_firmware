/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "soe_objspi.h"
#include "soeifcmn.h"
#include "config/g_spi_private.h"
#include "config/g_soe_hal.h"

/* ------------------------- Prototypes ------------------------------------- */
sysSYSLIB_CODE static FLCN_STATUS s_spiRomInitAccessRange(PSPI_DEVICE_ROM pSpiRom);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomReadId(PSPI_DEVICE_ROM pSpiRom, LwU32 *pJedecId);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomWriteProtect(PSPI_DEVICE_ROM pSpiRom, LwBool bEnable);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomReadStatusReg(PSPI_DEVICE_ROM pSpiRom, LwU32 *pStatus);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomWriteStatusReg(PSPI_DEVICE_ROM pSpiRom, LwU8 wrMask);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomWriteEnable(PSPI_DEVICE_ROM pSpiRom);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomSendPPCmd(PSPI_DEVICE_ROM pSpiRom, LwU32 address);
sysSYSLIB_CODE static FLCN_STATUS s_spiRomPollForReady(PSPI_DEVICE_ROM pSpiRom, LwU32 waitTimeUs);

/* ------------------------- Macros and Defines ----------------------------- */
#define LW_GET_BYTE(v, n)            (((v) >> (8 * (n))) & 0xff)

/* ------------------------- Global Variables ------------------------------- */
sysTASK_DATA LwU8 dmaBuffer[SOE_DMA_MAX_SIZE] __attribute__ ((aligned (SOE_DMA_MAX_SIZE)));
static sysTASK_DATA LwBool _spiRomInitialized = LW_FALSE;
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Initialize SPI ROM device.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  offset       Starting offset of accessible memory range in EEPROM
 * @param[in]  sizeInBytes  Size of accessible memory range in EEPROM
 *
 * @return FLCN_OK
 *     If SPI device was initialized successfully
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomInit
(
    SPI_DEVICE *pSpiDev
)
{
    FLCN_STATUS     status;
    LwU32           jedecId = 0x0;
    PSPI_DEVICE_ROM pSpiRom = (PSPI_DEVICE_ROM)pSpiDev;
    //
    // For some reason (yet to be root-caused), romStatus must be 4-byte
    // aligned, or else we may read an incorrect value from it in the loop
    // below and exit early, before the device is really ready. Just make it
    // a 32-bit value to force the compiler to align it accordingly for now.
    //
    // See bug 1916433 for more details.
    //
    LwU32           romStatus        = 0x0;
    LwU8            writeProtectMask = 0x0;

    if (_spiRomInitialized)
        return FLCN_OK;

    status = s_spiRomInitAccessRange(pSpiRom);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

    status = spiBusConfigure_HAL(&SpiHal, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

    // Read JEDEC ID from the SPI ROM.
    status = s_spiRomReadId(pSpiRom, &jedecId);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

    // Lookup SPI ROM in the table and initialize.
    status = spiRomLookup_HAL(&SpiHal, pSpiRom, jedecId);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

    // Disable Write Protection on SPI ROM Device.
    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _RDONLY, _FALSE,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        // Check if ROM is write protected or not
        writeProtectMask = (LwU8)(DRF_VAL(_ROM, _PACKED_INFO, _WPMASK,
                                          pSpiRom->spiRomInfo.packedInfoBits) << 2);
        if ((FLCN_OK == s_spiRomReadStatusReg(pSpiRom, &romStatus)) &&
             ((romStatus & writeProtectMask) != 0x0))
        {
            pSpiRom->bIsWriteProtected = LW_TRUE;
        }

        status = s_spiRomWriteProtect(pSpiRom, LW_FALSE);
        if (status != FLCN_OK)
        {
            goto spiRomInit_exit;
        }
    }

spiRomInit_exit:

    if (status == FLCN_OK)
        _spiRomInitialized = LW_TRUE;

    return status;
}

sysSYSLIB_CODE LwBool
spiRomIsInitialized
(
    void
)
{
    return _spiRomInitialized;
}

/*!
 * Read from SPI ROM device to a local buffer.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  devOffset    SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be read.
 * @param[in]  pDmemBuf     Pointer to SOE dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful read.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomReadFromRegionLocal
(
    PSPI_DEVICE_ROM      pSpiRom,
    PROM_REGION          pRegion,
    LwU32                offset,
    LwU16                sizeInBytes,
    LwU8                 *pDmemBuf
)
{
    LwU32       readAddress = (pRegion->startAddress +
                               offset);
    FLCN_STATUS status;

    // Sanity checks
    if (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pRegion, readAddress, sizeInBytes))
    {
        SOE_HALT();
    }

    status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, readAddress, sizeInBytes,
                                  pDmemBuf);

    return status;
}

/*!
 * Read from SPI ROM device.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  devOffset    SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be read.
 * @param[in]  pDmemBuf     Pointer to SOE dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful read.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomReadFromRegion
(
    PSPI_DEVICE_ROM      pSpiRom,
    PROM_REGION          pRegion,
    LwU32                offset,
    LwU32                sizeInBytes,
    RM_FLCN_U64          dmaHandle
)
{
    LwU32       readAddress = (pRegion->startAddress +
                                   offset);
    LwU32       bytesRead   = 0;
    LwU32       readSize;
    FLCN_STATUS status      = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks
    if (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pRegion, readAddress, sizeInBytes))
    {
        SOE_HALT();
        goto spiRomReadFromRegion_exit;
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    {
        while (bytesRead < sizeInBytes)
        {
            readSize = LW_MIN((sizeInBytes - bytesRead),
                              SOE_DMA_MAX_SIZE);

            // Align buffer size if not aligned already.
            readSize = LW_ALIGN_UP(readSize, sizeof(LwU32));

            // Zero out DMA buffer before we populate it
            memset(dmaBuffer, 0, SOE_DMA_MAX_SIZE);

            status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, readAddress, readSize,
                                          dmaBuffer);
            if (status != FLCN_OK)
            {
                goto spiRomReadFromRegion_exit;
            }

            // Write next block
            if (FLCN_OK != soeDmaWriteToSysmem_HAL(&Soe,
                                                   dmaBuffer,
                                                   dmaHandle,
                                                   bytesRead,
                                                   SOE_DMA_MAX_SIZE))
            {
                SOE_HALT();
                goto spiRomReadFromRegion_exit;
            }

            bytesRead   += readSize;
            readAddress += readSize;
        }
    }
spiRomReadFromRegion_exit:
    return status;
}

/* Read to a local buffer */
sysSYSLIB_CODE FLCN_STATUS
spiRomReadLocal
(
    SPI_DEVICE_ROM      *pSpiRom,
    LwU32                offset,
    LwU32                sizeInBytes,
    LwU8                *pLocalBuf
)
{
    PROM_REGION pRegion = pSpiRom->pInforom;
    return spiRomReadFromRegionLocal(pSpiRom, pRegion, offset, sizeInBytes, pLocalBuf);
}

sysSYSLIB_CODE FLCN_STATUS
spiRomWriteLocal
(
    SPI_DEVICE_ROM      *pSpiRom,
    LwU32                offset,
    LwU32                sizeInBytes,
    LwU8                *pLocalBuf
)
{
    PSPI_DEVICE     pSpiDev      = &pSpiRom->super;
    LwU32           bytesWritten = 0;
    LwU32           writeAddress = (pSpiRom->pInforom->startAddress +
                                    offset);
    LwU32           writeSize;
    FLCN_STATUS     status       = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks
    if ((!LW_ROM_REGION_INITIALIZED(pSpiRom->pInforom)) ||
        (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pSpiRom->pInforom, writeAddress,
                                       sizeInBytes)))
    {
        SOE_HALT();
    }

    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _RDONLY, _TRUE,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomWriteLocal_exit;
    }

    status = spiBusConfigure_HAL(&SpiHal, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiRomWriteLocal_exit;
    }

    while (bytesWritten < sizeInBytes)
    {
        writeSize = LW_MIN((sizeInBytes - bytesWritten),
                            RM_SOE_SPI_XFER_MAX_BLOCK_SIZE);

        // Write back the page read from buffer to ROM.
        status = spiRomWritePage(pSpiRom, &pLocalBuf[bytesWritten], writeAddress, writeSize);
        if (status != FLCN_OK)
        {
            goto spiRomWriteLocal_exit;
        }

        bytesWritten += writeSize;
        writeAddress += writeSize;
    }

spiRomWriteLocal_exit:
    return status;
}

/*!
 * Write a page (upto 256 bytes) from ROM.
 *
 * @param[in]  pSpiRom      SPI_DEVICE_ROM pointer
 * @param[in]  pBuf         Pointer to data buffer
 * @param[in]  address      Absolute SPI ROM address
 * @param[in]  sizeInBytes  Size of the data buffer
 *
 * @return FLCN_OK
 *     If page was written successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomWritePage
(
    SPI_DEVICE_ROM  *pSpiRom,
    void            *pBuf,
    LwU32            address,
    LwU16            sizeInBytes
)
{
    SPI_HW_CTRL  spiCtrl = {0};
    PSPI_DEVICE  pSpiDev = (PSPI_DEVICE)pSpiRom;
    FLCN_STATUS  status;

    if ((pSpiRom == NULL) ||
        (sizeInBytes > RM_SOE_SPI_XFER_MAX_BLOCK_SIZE))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomWritePage_exit;
    }

    status = s_spiRomSendPPCmd(pSpiRom, address);
    if (status != FLCN_OK)
    {
        goto spiRomWritePage_exit;
    }

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = SPI_CTRL_FLAGS_SLAVE_DEASSERT;
    spiCtrl.txSizeBytes   = sizeInBytes;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit data buffer.
    status = spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, NULL, (LwU8 *)pBuf);
    if (status != FLCN_OK)
    {
        goto spiRomWritePage_exit;
    }

    status = s_spiRomPollForReady(pSpiRom, LW_ROM_PP_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        goto spiRomWritePage_exit;
    }

spiRomWritePage_exit:
    return status;
}

/*!
 * Erase a region in SPI ROM.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  offset       SPI EEPROM memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be erased.
 *
 *
 * @return FLCN_OK
 *     If SPI device was initialized successfully
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomErase
(
    SPI_DEVICE *pSpiDev,
    LwU32       offset,
    LwU32       sizeInBytes
)
{
    PSPI_DEVICE_ROM pSpiRom      = (PSPI_DEVICE_ROM)pSpiDev;
    LwU32           eraseAddress = (pSpiRom->pInforom->startAddress +
                                    offset);
    LwU32           bytesWritten = 0;
    FLCN_STATUS     status       = FLCN_ERR_NOT_SUPPORTED;
    LwBool          bLimitErase  = LW_FALSE;

    // Sanity checks
    if ((!LW_ROM_REGION_INITIALIZED(pSpiRom->pInforom)) ||
        (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pSpiRom->pInforom, eraseAddress,
                                       sizeInBytes)))
    {
        SOE_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomErase_exit;
    }

    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _RDONLY, _TRUE,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomErase_exit;
    }

    status = spiBusConfigure_HAL(&SpiHal, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiRomErase_exit;
    }

    while (bytesWritten < sizeInBytes)
    {
        if (!LW_IS_ALIGNED(eraseAddress, LW_ROM_SECTOR_SIZE))
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }

        // Check if erase session limit is reached or not.
        if (LW_ROM_REGION_INITIALIZED(pSpiRom->pLedgerRegion))
        {
            bLimitErase = spiRomIsEraseLimitReached_HAL(&SpiHal, pSpiRom, eraseAddress);
            if (bLimitErase)
            {
                status = FLCN_ERR_ILLEGAL_OPERATION;
                goto spiRomErase_exit;
            }
        }

        status = spiRomEraseSector(pSpiRom, eraseAddress);
        if (status != FLCN_OK)
        {
            break;
        }

        if (LW_ROM_REGION_INITIALIZED(pSpiRom->pLedgerRegion))
        {
            status = spiRomEraseTally_HAL(&SpiHal, pSpiRom, eraseAddress);
            if (status != FLCN_OK)
            {
                break;
            }
        }

        bytesWritten += LW_ROM_SECTOR_SIZE;
        eraseAddress += LW_ROM_SECTOR_SIZE;
    }

spiRomErase_exit:
    return status;
}

/*!
 * Transmit Sector Erase command to the device.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 * @param[in]  address  Absolute SPI ROM address
 *
 * @return FLCN_OK
 *     If sector was erased successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomEraseSector
(
    SPI_DEVICE_ROM   *pSpiRom,
    LwU32            address
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;
    FLCN_STATUS  status;

    // Transmit Write Enable Command to ROM.
    status = s_spiRomWriteEnable(pSpiRom);
    if (status != FLCN_OK)
    {
        goto spiRomEraseSector_exit;
    }

    // Configure ROM Command and address to be transmitted.
    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _SECMD, _20H,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        spiCmdData[0] = LW_ROM_CMD_SE_20H;
    }
    else
    {
        spiCmdData[0] = LW_ROM_CMD_SE_D7H;
    }

    spiCmdData[1] = LW_GET_BYTE(address, 2);
    spiCmdData[2] = LW_GET_BYTE(address, 1);
    spiCmdData[3] = LW_GET_BYTE(address, 0);

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_SLAVE_SELECT |
                            SPI_CTRL_FLAGS_SLAVE_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_DATA_LEN;
    spiCtrl.rxSizeBytes   = 0x0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, NULL, spiCmdData);
    if (status != FLCN_OK)
    {
        goto spiRomEraseSector_exit;
    }

    status = s_spiRomPollForReady(pSpiRom, LW_ROM_SE_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        goto spiRomEraseSector_exit;
    }

spiRomEraseSector_exit:
    return status;
}

/*!
 * De-Initialize SPI ROM device.
 *
 * @param[in]  pSpiDev                 SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If SPI device was deinitialized successfully
 * @return Other unexpected error
 */
sysSYSLIB_CODE FLCN_STATUS
spiRomDeInit
(
    PSPI_DEVICE pSpiDev
)
{
    FLCN_STATUS     status    = FLCN_OK;
    PSPI_DEVICE_ROM pSpiRom   = (PSPI_DEVICE_ROM)pSpiDev;

    //
    // Restore soft Write Protection on SPI ROM Device if it
    // was enabled before PMU load.
    //
    if (pSpiRom->bIsWriteProtected)
    {
        status = s_spiRomWriteProtect(pSpiRom, LW_TRUE);
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Initializes the accessible range of the ROM and validates it.
 *
 * For security's sake, we assume that the ROM device is being used to access
 * the InfoROM image and Erase ledger image, so the bounds validation are
 * centered around the InfoROM and Erase ledger image.
 *
 * Longer-term, if we need multiple ROM devices, this validation won't
 * be sufficient.
 *
 * @param[in]   pSpiRom         SPI_DEVICE_ROM pointer.
 * @param[in]   start           The RM-provided start address of the accessible
 *                              range within the SPI device.
 * @param[in]   sizeInBytes     The RM-provided size of the accessible range
 *                              within the SPI device.
 *
 * @return FLCN_ERR_ILWALID_ARGUMENT    If the provided access range bounds are
 *                                      not sane.
 * @return FLCN_ERR_ILWALID_STATE       If the ROM contents of the accessible
 *                                      range are not valid for the accessible
 *                                      range (e.g., not a valid InfoROM image)
 * @return FLCN_OK                      The accessible range of the ROM device
 *                                      is valid.
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomInitAccessRange
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    FLCN_STATUS           status        = FLCN_ERR_ILWALID_STATE;
    FLCN_STATUS           ledgerStatus  = FLCN_ERR_ILWALID_STATE;
    PINFOROM_REGION       pInforom      = pSpiRom->pInforom;
    PERASE_LEDGER_REGION  pLedgerRegion = pSpiRom->pLedgerRegion;
    LwU32                 inforomSign   = 0;

    //
    // Initialize InfoROM bounds from scratch registers
    // (if provided by firmware)
    //
    status = spiRomInitInforomRegionFromScratch_HAL(&SpiHal, pSpiRom);
    if ((status != FLCN_OK) && (status != FLCN_ERR_FEATURE_NOT_ENABLED))
    {
        goto s_spiRomInitAccessRange_exit;
    }

    // Sanity check the start and end addresses
    if (pInforom->startAddress >= pInforom->endAddress)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomInitAccessRange_exit;
    }

    if (!LW_IS_ALIGNED(pInforom->startAddress, LW_ROM_IMAGE_ALIGN))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_spiRomInitAccessRange_exit;
    }

    // Validate the common InfoROM image signature is at the start address
    status = spiRomReadDirect_HAL(&SpiHal, pSpiRom, pInforom->startAddress,
                                  LW_INFOROM_IMAGE_SIG_SIZE, (LwU8 *)&inforomSign);
    if (status != FLCN_OK)
    {
        goto s_spiRomInitAccessRange_exit;
    }

    if (inforomSign != LW_INFOROM_IMAGE_SIG)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto s_spiRomInitAccessRange_exit;
    }

    // Erase Ledger specific initialization and validation which is supported on Volta+.
    ledgerStatus = spiRomInitLedgerRegionFromScratch_HAL(&Spi, pSpiRom);
    if ((ledgerStatus == FLCN_OK) && (LW_ROM_REGION_INITIALIZED(pLedgerRegion)))
    {
        status = spiRomInitEraseLedger_HAL(&SpiHal, pSpiRom);
        if (status != FLCN_OK)
        {
            goto s_spiRomInitAccessRange_exit;
        }
    }

s_spiRomInitAccessRange_exit:
    if (status != FLCN_OK)
    {
        // Deinitialize the InfoROM and Erase ledger bounds so ROM operations fail
        pInforom->startAddress      = ~((LwU32)0);
        pInforom->endAddress        = ~((LwU32)0);
    }

    return status;
}

/*!
 * Reads the identification of the ROM using the specified command.
 *
 * @param[in]   pSpiRom   SPI_DEVICE_ROM pointer
 * @param[out]  pJedecId  Pointer to JEDEC ID of device i.e. Manfacturer code
 *                        and Device code.
 *
 * @return FLCN_OK
 *     If identification is read successfully
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomReadId
(
    PSPI_DEVICE_ROM pSpiRom,
    LwU32          *pJedecId
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_RDID_JEDEC;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_READ_ENABLE |
                            SPI_CTRL_FLAGS_SLAVE_SELECT |
                            SPI_CTRL_FLAGS_SLAVE_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = LW_ROM_ID_LEN;

    // Transmit command and read back the JEDEC ID.
    return spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, (LwU8 *)pJedecId, spiCmdData);
}

/*!
 * Transmit Page Program command to the device.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 * @param[in]  address  Absolute SPI ROM address
 *
 * @return FLCN_OK
 *     If PP command was transmitted successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomSendPPCmd
(
    PSPI_DEVICE_ROM  pSpiRom,
    LwU32            address
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;
    FLCN_STATUS  status;

    // Transmit Write Enable Command to ROM.
    status = s_spiRomWriteEnable(pSpiRom);
    if (status != FLCN_OK)
    {
        goto spiRomSendPPCmd_exit;
    }

    // Configure ROM Command and address to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_PP;
    spiCmdData[1] = LW_GET_BYTE(address, 2);
    spiCmdData[2] = LW_GET_BYTE(address, 1);
    spiCmdData[3] = LW_GET_BYTE(address, 0);

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = SPI_CTRL_FLAGS_SLAVE_SELECT;
    spiCtrl.txSizeBytes   = LW_ROM_CMD_DATA_LEN;
    spiCtrl.rxSizeBytes   = 0x0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, NULL, spiCmdData);

spiRomSendPPCmd_exit:
    return status;
}

/*!
 * Poll until device is ready.
 *
 * @param[in]  pSpiRom      PSPI_DEVICE_ROM  pointer
 * @param[in]  waitTimeUs   Time to wait for device to be ready
 *
 * @return FLCN_OK
 *     If device is ready
 * @return FLCN_ERR_TIMEOUT
 *     If device is not ready
 *
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomPollForReady
(
    PSPI_DEVICE_ROM pSpiRom,
    LwU32           waitTimeUs
)
{
    FLCN_TIMESTAMP  startTime;
    //
    // For some reason (yet to be root-caused), romStatus must be 4-byte
    // aligned, or else we may read an incorrect value from it in the loop
    // below and exit early, before the device is really ready. Just make it
    // a 32-bit value to force the compiler to align it accordingly for now.
    //
    // See bug 1916433 for more details.
    //
    LwU32           romStatus;
    LwU32           elapsedTime = 0;
    LwU32           timeoutNs   = (waitTimeUs * 1000);

    osPTimerTimeNsLwrrentGet(&startTime);

    while ((FLCN_OK == s_spiRomReadStatusReg(pSpiRom, &romStatus)) &&
           (FLD_TEST_DRF(_ROM, _STATUS_REG, _WIP, _TRUE, romStatus)) &&
           (elapsedTime < timeoutNs))
    {
        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
        lwrtosYIELD();
    }

    return (elapsedTime < timeoutNs) ?
           FLCN_OK : FLCN_ERR_TIMEOUT;
}

/*!
 * Transmit Write Enable (WREN) command to the device.
 *
 * WREN command sets Write Enable Latch(WEL) bit to 1,
 * which means device can accept program/erase/write status
 * register instructions.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 *
 * @return FLCN_OK
 *     If Write Enable Command was transmitted successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomWriteEnable
(
    PSPI_DEVICE_ROM pSpiRom
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_WREN;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_SLAVE_SELECT |
                            SPI_CTRL_FLAGS_SLAVE_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit command.
    return spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, NULL, spiCmdData);
}

/*!
 * Reads the Status Register (RDSR) of ROM using the specified command.
 *
 * @param[in]   pSpiRom     SPI_DEVICE_ROM pointer
 * @param[out]  pStatus     Pointer to ROM Status.
 *
 * @return FLCN_OK
 *     If Status Register is read successfully
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomReadStatusReg
(
    PSPI_DEVICE_ROM pSpiRom,
    LwU32          *pStatus
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    LwU32        statusReg                       = 0;
    FLCN_STATUS  status                          = FLCN_ERR_NOT_SUPPORTED;
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;

    // Configure ROM Command and address to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_RDSR;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_READ_ENABLE |
                            SPI_CTRL_FLAGS_SLAVE_SELECT |
                            SPI_CTRL_FLAGS_SLAVE_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = LW_ROM_STATUS_REG_LEN;

    // Transmit command and read back the ROM status register.
    status = spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, (LwU8 *)&statusReg, spiCmdData);
    if (status != FLCN_OK)
    {
        goto spiRomReadStatusReg_exit;
    }

    spiRomGetStatus_HAL(&SpiHal, statusReg, (LwU8 *)pStatus);

spiRomReadStatusReg_exit:
    return status;
}

/*!
 * Enables or disables software write protection on the device.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 * @param[in]  bEnable  Whether or not to enable software protection
 *
 * @return FLCN_OK
 *     If write protection was enabled/disabled successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomWriteProtect
(
    PSPI_DEVICE_ROM pSpiRom,
    LwBool          bEnable
)
{
    FLCN_STATUS status;
    LwU8        writeProtectMask = 0x0;

    // Transmit Write Enable Command to ROM.
    status = s_spiRomWriteEnable(pSpiRom);
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
    status = s_spiRomWriteStatusReg(pSpiRom, writeProtectMask);
    if (status != FLCN_OK)
    {
        goto spiRomWriteProtect_exit;
    }

spiRomWriteProtect_exit:
    return status;
}

/*!
 * Transmit Write Status Register (WRSR) command to the device.
 *
 * WRSR instruction to ROM is for changing the values of Status Register Bits.
 * The WREN command must be decoded and exelwted before sending WRSR command.
 *
 * @param[in]  pSpiRom   SPI_DEVICE_ROM pointer
 * @param[in]  wrMask    Write Protection Mask to be written to Status Register.
 *
 * @return FLCN_OK
 *     If ROM Status Register was written successfully.
 * @return Other unexpected error
 */
sysSYSLIB_CODE static FLCN_STATUS
s_spiRomWriteStatusReg
(
    PSPI_DEVICE_ROM pSpiRom,
    LwU8            wrMask
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    FLCN_STATUS  status                          = FLCN_ERR_NOT_SUPPORTED;
    PSPI_DEVICE  pSpiDev                         = (PSPI_DEVICE)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_WRSR;
    spiCmdData[1] = wrMask;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_SLAVE_SELECT |
                            SPI_CTRL_FLAGS_SLAVE_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN + LW_ROM_STATUS_REG_LEN;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&SpiHal, pSpiDev, &spiCtrl, NULL, spiCmdData);
    if (status != FLCN_OK)
    {
        goto spiRomWriteStatusReg_exit;
    }

    status = s_spiRomPollForReady(pSpiRom, LW_ROM_WRSR_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        goto spiRomWriteStatusReg_exit;
    }

spiRomWriteStatusReg_exit:
    return status;
}
