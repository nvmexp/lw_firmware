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
 * @file   pmu_spigp100.c
 * @brief  SPI routines specific to Pascal and later.
 */

/* ------------------------- System Includes -------------------------------- */
#include "dev_pmgr.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objpmu.h"
#include "pmu_objtimer.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */

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
FLCN_STATUS
spiReadData_GP100
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
FLCN_STATUS
spiWriteData_GP100
(
    LwU8  *pData,
    LwU16  size
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
        REG_WR32(BAR0, LW_PMGR_SPI_DATA_ARRAY(i), *pBuf);
        pBuf++;
    }

    return FLCN_OK;
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
FLCN_STATUS
spiHwWaitForIdle_GP100
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
 * @brief Prepares a bus for a transfer.
 *
 * Lwrrently there is a single SPI target (the on-board EEPROM) on a single 
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
FLCN_STATUS
spiBusConfigure_GP100
(
    SPI_DEVICE    *pSpiDev
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBusConfigure_GP100_exit;
    }

    status = spiArbiterXusbBlock_HAL(&Spi, pSpiDev, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_GP100_exit;
    }

    status =  spiRomSerialBypassEnable_HAL(&Spi, (SPI_DEVICE_ROM *)pSpiDev,
                   LW_FALSE, NULL);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_GP100_exit;
    }

    // Set Initiator frequency before transacting with target.
    status = spiSetFrequency_HAL(&Spi, pSpiDev);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_GP100_exit;
    }

spiBusConfigure_GP100_exit:
    return status;
}

/*!
 * @brief Raises or reduces the priv sec level for selected
 *        registers.
 *
 * @param[in] bRaisePrivSec  Whether to enable or disable the protection
 *
 * @return FLCN_OK
 *     Upon success.
 * @return Other unexpected error
 */
FLCN_STATUS
spiPrivSecRegisters_GP100
(
    LwBool   bRaisePrivSec
)
{
    LwU32 privMask;
    LwU8  writeProtectVal = (bRaisePrivSec ? 0x6 : 0x7);

    // Read initial register value.
    privMask = REG_RD32(BAR0, LW_PMGR_ROM_1_PRIV_LEVEL_MASK);

    privMask = FLD_SET_DRF_NUM(_PMGR, _ROM_1_PRIV_LEVEL_MASK,
                               _WRITE_PROTECTION, writeProtectVal, privMask);
    REG_WR32(BAR0, LW_PMGR_ROM_1_PRIV_LEVEL_MASK, privMask);

    return FLCN_OK;
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
FLCN_STATUS
spiBufferTransfer_GP100
(
    SPI_DEVICE   *pSpiDev,
    SPI_HW_CTRL  *pSpiCtrl, 
    LwU8         *pRxBuffer,
    LwU8         *pTxBuffer
)
{
    FLCN_STATUS  status     = FLCN_ERR_NOT_SUPPORTED;
    LwU32        readOffset = 0x0;
    LwU32        rxSize     = LW_ALIGN_UP(pSpiCtrl->rxSizeBytes, sizeof(LwU32));
    LwU32        txSize     = LW_ALIGN_UP(pSpiCtrl->txSizeBytes, sizeof(LwU32));

    if ((pSpiDev == NULL) || ((rxSize == 0) && (txSize == 0)))
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBufferTransfer_GP100_exit;
    }

    // Write HW-SPI DATA register.
    if (txSize != 0x0)
    {
        status = spiWriteData_HAL(&Spi, pTxBuffer, txSize);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_GP100_exit;
        }
    }

    // Configure HW-SPI Control register and trigger the transaction.
    status = spiWriteCtrl_HAL(&Spi, pSpiCtrl, LW_ROM_MAX_TIMEOUT_US);
    if (status != FLCN_OK)
    {
        goto spiBufferTransfer_GP100_exit;
    }

    // Read back data in Input buffer
    if (rxSize != 0x0)
    {
        // Adjust readOffset when data is received along with transmit.
        if (txSize != 0x0)
        {
            readOffset = LW_ROM_READ_START_OFFSET;
        }

        status = spiReadData_HAL(&Spi, pRxBuffer, rxSize, readOffset);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_GP100_exit;
        }
    }

spiBufferTransfer_GP100_exit:
    return status;
}
