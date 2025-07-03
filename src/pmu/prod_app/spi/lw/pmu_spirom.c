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
 * @file   pmu_spirom.c
 * @brief  Container-object for PMU SPI ROM routines.
 *
 * This file contains software protocols to initialize/read/write ROM devices
 * using HW-SPI Controller interface. Software instructions to program HW-SPI
 * controller to transact with ROM devices can be found here:
 * //hw/doc/gpu/display/GPUDisplay/2.7/specfications/GP100_SPI_uarch.docx
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu/ssurface.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objtimer.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
#define LW_GET_BYTE(v, n)            (((v) >> (8 * (n))) & 0xff)
#define SPI_READ_BACK_BUFFER_SIZE 16

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_spiRomInitAccessRange(SPI_DEVICE_ROM *pSpiRom, LwU32 start,
    LwU32 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spiLibRomInit", "s_spiRomInitAccessRange");
static FLCN_STATUS s_spiRomReadId(SPI_DEVICE_ROM *pSpiRom, LwU32 *pJedecId);
static FLCN_STATUS s_spiRomReadStatusReg(SPI_DEVICE_ROM *pSpiRom, LwU32 *pStatus);
static FLCN_STATUS s_spiRomSendPPCmd(SPI_DEVICE_ROM *pSpiRom, LwU32 address);
static FLCN_STATUS s_spiRomPollForReady(SPI_DEVICE_ROM *pSpiRom, LwU32 waitTimeUs);

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
FLCN_STATUS
spiRomInit
(
    SPI_DEVICE *pSpiDev,
    LwU32       offset,
    LwU32       sizeInBytes
)
{
    FLCN_STATUS     status;
    SPI_DEVICE_ROM *pSpiRom = (SPI_DEVICE_ROM *)pSpiDev;
    LwU32           jedecId = 0x0;
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

    status = s_spiRomInitAccessRange(pSpiRom, offset, sizeInBytes);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

    status = spiBusConfigure_HAL(&Spi, pSpiDev);
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
    status = spiRomLookup_HAL(&Spi, pSpiRom, jedecId);
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
        if ((s_spiRomReadStatusReg(pSpiRom, &romStatus) == FLCN_OK) &&
             ((romStatus & writeProtectMask) != 0x0))
        {
            pSpiRom->bIsWriteProtected = LW_TRUE;

            status = spiRomWriteProtect_HAL(&pSpi, pSpiRom, LW_FALSE);
            if (status != FLCN_OK)
            {
                goto spiRomInit_exit;
            }
        }
    }

    // Enable CPU Write Protection on ROM Interfaces.
    status = spiPrivSecRegisters_HAL(&Spi, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto spiRomInit_exit;
    }

spiRomInit_exit:
    (void)spiBusReset_HAL(&Spi, pSpiDev);
    return status;
}

/*!
 * Read from SPI ROM device.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  devOffset    SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be read.
 * @param[in]  pDmemBuf     Pointer to PMU dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful read.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomRead
(
    SPI_DEVICE *pSpiDev,
    LwU32       offset,
    LwU32       sizeInBytes,
    void       *pDmemBuf
)
{
    SPI_DEVICE_ROM *pSpiRom     = (SPI_DEVICE_ROM *)pSpiDev;
    LwU32           readAddress = (pSpiRom->pInforom->startAddress +
                                   offset);
    LwU32           bytesRead   = 0;
    LwU32           readSize;
    FLCN_STATUS     status      = FLCN_ERR_NOT_SUPPORTED;

    // Sanity checks
    if ((!LW_ROM_REGION_INITIALIZED(pSpiRom->pInforom)) ||
        (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pSpiRom->pInforom, readAddress,
                                       sizeInBytes)))
    {
        PMU_HALT();
        goto spiRomRead_exit;
    }

    status = spiArbiterXusbBlock_HAL(&Spi, pSpiDev, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto spiRomRead_exit;
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        while (bytesRead < sizeInBytes)
        {
            readSize = LW_MIN((sizeInBytes - bytesRead),
                              RM_PMU_SPI_XFER_MAX_BLOCK_SIZE);

            // Align buffer size if not aligned already.
            readSize = LW_ALIGN_UP(readSize, sizeof(LwU32));

            status = spiRomReadDirect_HAL(&Spi, pSpiRom, readAddress, readSize,
                                          (LwU8 *)pDmemBuf);
            if (status != FLCN_OK)
            {
                goto spiRomRead_exit;
            }

            // Write next block
            if (FLCN_OK != dmaWrite(pDmemBuf, &PmuInitArgs.superSurface,
                                    RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.spi.spiPayload) +
                                    bytesRead,
                                    RM_PMU_SPI_XFER_MAX_BLOCK_SIZE))
            {
                PMU_HALT();
                goto spiRomRead_exit;
            }

            bytesRead   += readSize;
            readAddress += readSize;
        }
    }
    lwosDmaLockRelease();

    status = FLCN_OK;

spiRomRead_exit:
    (void)spiArbiterXusbBlock_HAL(&Spi, pSpiDev, LW_FALSE);
    return status;
}

/*!
 * Write to SPI ROM device.
 *
 * @param[in]  pSpiDev      SPI_DEVICE pointer
 * @param[in]  devOffset    SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be written.
 * @param[in]  pDmemBuf     Pointer to PMU dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful write.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomWrite
(
    SPI_DEVICE *pSpiDev,
    LwU32       offset,
    LwU32       sizeInBytes,
    void       *pDmemBuf
)
{
    SPI_DEVICE_ROM *pSpiRom      = (SPI_DEVICE_ROM *)pSpiDev;
    LwU32           bytesWritten = 0;
    LwU32           writeAddress = (pSpiRom->pInforom->startAddress +
                                    offset);
    LwU32           writeSize;
    FLCN_STATUS     status       = FLCN_ERR_NOT_SUPPORTED;
    LwU8            pReadBackBuf[SPI_READ_BACK_BUFFER_SIZE];

    // Sanity checks
    if ((!LW_ROM_REGION_INITIALIZED(pSpiRom->pInforom)) ||
        (LW_ROM_ADDRESS_OUTSIDE_BOUNDS(pSpiRom->pInforom, writeAddress,
                                       sizeInBytes)))
    {
        PMU_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomWrite_exit;
    }

    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _RDONLY, _TRUE,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomWrite_exit;
    }

    status = spiBusConfigure_HAL(&Spi, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiRomWrite_exit;
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        while (bytesWritten < sizeInBytes)
        {
            writeSize = LW_MIN((sizeInBytes - bytesWritten),
                               RM_PMU_SPI_XFER_MAX_BLOCK_SIZE);

            // Read next block
            if (FLCN_OK != ssurfaceRd(pDmemBuf,
                                   RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.spi.spiPayload) +
                                   bytesWritten,
                                   RM_PMU_SPI_XFER_MAX_BLOCK_SIZE))
            {
                PMU_HALT();
                goto spiRomWrite_exit;
            }

            // Write back the page read from fb/sysmem to ROM.
            status = spiRomWritePage(pSpiRom, pDmemBuf, writeAddress, writeSize);
            if (status != FLCN_OK)
            {
                goto spiRomWrite_exit;
            }

            // Read back page just written to confirm successful transaction
            status = spiRomReadDirect_HAL(&Spi, pSpiRom, writeAddress,
                                          SPI_READ_BACK_BUFFER_SIZE,
                                          (LwU8 *)pReadBackBuf);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto spiRomWrite_exit;
            }

            if (memcmp(pReadBackBuf, pDmemBuf, LW_MIN(SPI_READ_BACK_BUFFER_SIZE, writeSize)))
            {
                // TODO: Add in TRUE_BP when available
                status = FLCN_ERR_ILWALID_STATE;
                goto spiRomWrite_exit;
            }

            bytesWritten += writeSize;
            writeAddress += writeSize;
        }
    }
    lwosDmaLockRelease();

    status = FLCN_OK;

spiRomWrite_exit:
    (void)spiBusReset_HAL(&Spi, pSpiDev);
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
FLCN_STATUS
spiRomErase
(
    SPI_DEVICE *pSpiDev,
    LwU32       offset,
    LwU32       sizeInBytes
)
{
    SPI_DEVICE_ROM *pSpiRom      = (SPI_DEVICE_ROM *)pSpiDev;
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
        PMU_HALT();
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRomErase_exit;
    }

    if (FLD_TEST_DRF(_ROM, _PACKED_INFO, _RDONLY, _TRUE,
            pSpiRom->spiRomInfo.packedInfoBits))
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto spiRomErase_exit;
    }

    status = spiBusConfigure_HAL(&Spi, pSpiDev);
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
            bLimitErase = spiRomIsEraseLimitReached_HAL(&Spi, pSpiRom, eraseAddress);
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
            status = spiRomEraseTally_HAL(&Spi, pSpiRom, eraseAddress);
            if (status != FLCN_OK)
            {
                break;
            }
        }

        bytesWritten += LW_ROM_SECTOR_SIZE;
        eraseAddress += LW_ROM_SECTOR_SIZE;
    }

spiRomErase_exit:
    (void)spiBusReset_HAL(&Spi, pSpiDev);
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
FLCN_STATUS
spiRomDeInit
(
    SPI_DEVICE *pSpiDev
)
{
    FLCN_STATUS     status    = FLCN_ERR_NOT_SUPPORTED;
    FLCN_STATUS     retStatus = FLCN_OK;
    SPI_DEVICE_ROM *pSpiRom   = (SPI_DEVICE_ROM *)pSpiDev;

    //
    // Restore soft Write Protection on SPI ROM Device if it
    // was enabled before PMU load.
    //
    if (pSpiRom->bIsWriteProtected)
    {
        status = spiRomWriteProtect_HAL(&pSpi, pSpiRom, LW_TRUE);
        if (status != FLCN_OK)
        {
            retStatus = status;
        }
    }

    // Disable CPU Write Protection on ROM Interfaces.
    status    = spiPrivSecRegisters_HAL(&Spi, LW_FALSE);
    retStatus = (retStatus == FLCN_OK) ? status : retStatus;

    return retStatus;
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
FLCN_STATUS
spiRomWritePage
(
    SPI_DEVICE_ROM  *pSpiRom,
    void            *pBuf,
    LwU32            address,
    LwU16            sizeInBytes
)
{
    SPI_HW_CTRL  spiCtrl = {0};
    SPI_DEVICE  *pSpiDev = (SPI_DEVICE *)pSpiRom;
    FLCN_STATUS  status;

    if ((pSpiRom == NULL) ||
        (sizeInBytes > RM_PMU_SPI_XFER_MAX_BLOCK_SIZE))
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
    spiCtrl.ctrlFlags     = SPI_CTRL_FLAGS_TARGET_DEASSERT;
    spiCtrl.txSizeBytes   = sizeInBytes;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit data buffer.
    status = spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, NULL, (LwU8 *)pBuf);
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
 * Transmit Sector Erase command to the device.
 *
 * @param[in]  pSpiRom  SPI_DEVICE_ROM pointer
 * @param[in]  address  Absolute SPI ROM address
 *
 * @return FLCN_OK
 *     If sector was erased successfully.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomEraseSector
(
    SPI_DEVICE_ROM  *pSpiRom,
    LwU32            address
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;
    FLCN_STATUS  status;

    // Transmit Write Enable Command to ROM.
    status = spiRomWriteEnable(pSpiRom);
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
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_TARGET_SELECT |
                            SPI_CTRL_FLAGS_TARGET_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_DATA_LEN;
    spiCtrl.rxSizeBytes   = 0x0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, NULL, spiCmdData);
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
static FLCN_STATUS
s_spiRomInitAccessRange
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32           start,
    LwU32           sizeInBytes
)
{
    FLCN_STATUS           status        = FLCN_ERR_ILWALID_STATE;
    FLCN_STATUS           ledgerStatus  = FLCN_ERR_ILWALID_STATE;
    INFOROM_REGION       *pInforom      = pSpiRom->pInforom;
    ERASE_LEDGER_REGION  *pLedgerRegion = pSpiRom->pLedgerRegion;
    LwU32                 inforomSign;

    //
    // Initialize InfoROM bounds from scratch registers 
    // (if provided by firmware)
    //
    status = spiRomInitInforomRegionFromScratch_HAL(&Spi, pSpiRom);
    if ((status != FLCN_OK) && (status != FLCN_ERR_FEATURE_NOT_ENABLED))
    {
        goto s_spiRomInitAccessRange_exit;
    }

    //
    // InfoROM specific initialization and validation. If the InfoROM
    // bounds aren't already known (e.g., provided by firmware),
    // take the less-secure option provided by the RM as input to the
    // INIT command.
    //
    if (!LW_ROM_REGION_INITIALIZED(pInforom))
    {
        pInforom->startAddress = start;
        pInforom->endAddress   = start + sizeInBytes;
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
    status = spiRomReadDirect_HAL(&Spi, pSpiRom, pInforom->startAddress,
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
        status = spiRomInitEraseLedger_HAL(&Spi, pSpiRom);
    }

s_spiRomInitAccessRange_exit:
    if (status != FLCN_OK)
    {
        // Deinitialize the InfoROM and Erase ledger bounds so ROM operations fail
        pInforom->startAddress      = ~((LwU32)0);
        pInforom->endAddress        = ~((LwU32)0);
        pLedgerRegion->startAddress = ~((LwU32)0);
        pLedgerRegion->endAddress   = ~((LwU32)0);
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
static FLCN_STATUS
s_spiRomReadId
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32          *pJedecId
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_RDID_JEDEC;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_READ_ENABLE |
                            SPI_CTRL_FLAGS_TARGET_SELECT |
                            SPI_CTRL_FLAGS_TARGET_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = LW_ROM_ID_LEN;

    // Transmit command and read back the JEDEC ID.
    return spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, (LwU8 *)pJedecId, spiCmdData);
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
FLCN_STATUS
spiRomWriteEnable
(
    SPI_DEVICE_ROM *pSpiRom
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_WREN;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_TARGET_SELECT |
                            SPI_CTRL_FLAGS_TARGET_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit command.
    return spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, NULL, spiCmdData);
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
FLCN_STATUS
spiRomWriteStatusReg
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU8            wrMask
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    FLCN_STATUS  status                          = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;

    // Configure ROM Command to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_WRSR;
    spiCmdData[1] = wrMask;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_TARGET_SELECT |
                            SPI_CTRL_FLAGS_TARGET_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN + LW_ROM_STATUS_REG_LEN;
    spiCtrl.rxSizeBytes   = 0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, NULL, spiCmdData);
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
static FLCN_STATUS
s_spiRomSendPPCmd
(
    SPI_DEVICE_ROM  *pSpiRom,
    LwU32            address
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;
    FLCN_STATUS  status;

    // Transmit Write Enable Command to ROM.
    status = spiRomWriteEnable(pSpiRom);
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
    spiCtrl.ctrlFlags     = SPI_CTRL_FLAGS_TARGET_SELECT;
    spiCtrl.txSizeBytes   = LW_ROM_CMD_DATA_LEN;
    spiCtrl.rxSizeBytes   = 0x0;

    // Transmit command.
    status = spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, NULL, spiCmdData);

spiRomSendPPCmd_exit:
    return status;
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
static FLCN_STATUS
s_spiRomReadStatusReg
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32          *pStatus
)
{
    SPI_HW_CTRL  spiCtrl                         = {0};
    LwU8         spiCmdData[LW_ROM_CMD_DATA_LEN] = {0};
    LwU32        statusReg                       = 0;
    FLCN_STATUS  status                          = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE  *pSpiDev                         = (SPI_DEVICE *)pSpiRom;

    // Configure ROM Command and address to be transmitted.
    spiCmdData[0] = LW_ROM_CMD_RDSR;

    // Configure SPI control required to trigger the transaction.
    spiCtrl.chipSelectId  = (LwU8) pSpiDev->chipSelectId;
    spiCtrl.ctrlFlags     = (SPI_CTRL_FLAGS_READ_ENABLE |
                            SPI_CTRL_FLAGS_TARGET_SELECT |
                            SPI_CTRL_FLAGS_TARGET_DEASSERT);
    spiCtrl.txSizeBytes   = LW_ROM_CMD_LEN;
    spiCtrl.rxSizeBytes   = LW_ROM_STATUS_REG_LEN;

    // Transmit command and read back the ROM status register.
    status = spiBufferTransfer_HAL(&Spi, pSpiDev, &spiCtrl, (LwU8 *)&statusReg, spiCmdData);
    if (status != FLCN_OK)
    {
        goto spiRomReadStatusReg_exit;
    }

    spiRomGetStatus_HAL(&Spi, statusReg, (LwU8 *)pStatus);

spiRomReadStatusReg_exit:
    return status;
}

/*!
 * Poll until device is ready.
 *
 * @param[in]  pSpiRom      SPI_DEVICE_ROM pointer
 * @param[in]  waitTimeUs   Time to wait for device to be ready
 *
 * @return FLCN_OK
 *     If device is ready
 * @return FLCN_ERR_TIMEOUT
 *     If device is not ready
 *
 */
static FLCN_STATUS
s_spiRomPollForReady
(
    SPI_DEVICE_ROM *pSpiRom,
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

    while ((s_spiRomReadStatusReg(pSpiRom, &romStatus) == FLCN_OK) &&
           (FLD_TEST_DRF(_ROM, _STATUS_REG, _WIP, _TRUE, romStatus)) &&
           (elapsedTime < timeoutNs))
    {
        elapsedTime = osPTimerTimeNsElapsedGet(&startTime);
        lwrtosYIELD();
    }

    return (elapsedTime < timeoutNs) ?
           FLCN_OK : FLCN_ERR_TIMEOUT;
}
