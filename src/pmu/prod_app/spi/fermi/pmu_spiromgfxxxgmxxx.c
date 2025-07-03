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
 * @file    pmu_spiromgfxxxgmxxx.c
 * @brief   SPI ROM routines specific to Kepler through Maxwell.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmu_bar0.h"
#include "dev_pmgr.h"
#include "dev_ext_devices.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objspi.h"
#include "pmu_objtimer.h"
#include "config/g_spi_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_spiChipSelectEnable(SPI_DEVICE *pSpiDev, LwU32 *pReg,
    LwBool bEnable);
static FLCN_STATUS s_spiBitBangBufferTransfer(SPI_DEVICE *pSpiDev, LwU32 *pReg,
    void *pOutBuffer, void *pInBuffer, LwU32 sizeBytes, LwBool bSckReturn,
    LwBool bReverse);
static FLCN_STATUS s_spiBitBangByteTransfer(SPI_DEVICE *pSpiDev, LwU32 *pReg,
    LwU8 *pMiso, LwU8 mosi, LwBool bSckReturn);
static FLCN_STATUS s_spiBitBangBitTransfer(SPI_DEVICE *pSpiDev, LwU32 *pReg,
    LwU8 *pMiso, LwU8 mosi, LwBool bSckReturn);

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
spiRomLookup_GMXXX
(
    SPI_DEVICE_ROM *pSpiRom,
    LwU32           jedecId
)
{
    LwU8                        manufacturerCode;
    LwU16                       deviceCode;
    SPI_DEVICE_ROM_INFO        *pTable;
    FLCN_STATUS                 status = FLCN_ERR_NOT_SUPPORTED;
    static SPI_DEVICE_ROM_INFO  spiDevicesSupported_GMXXX[] =
    {
        //
        //                                  Packed
        //                                  Info    Dev.
        //       Manufacturer Code          bits    Code
        // =============================    ======  ======
        //
        // Atmel__________________________________________
        {    LW_ROM_MANUFACTURER_ATMEL,       0x03, 0x6601 },
        {    LW_ROM_MANUFACTURER_ATMEL,       0x0F, 0x4300 },
        {    LW_ROM_MANUFACTURER_ATMEL,       0x0F, 0x4401 },
        // AMIC Technology Corporation____________________
        {    LW_ROM_MANUFACTURER_AMIC,        0x07, 0x3010 },
        {    LW_ROM_MANUFACTURER_AMIC,        0x07, 0x3011 },
        {    LW_ROM_MANUFACTURER_AMIC,        0x07, 0x3012 },
        // Macronix International, Inc.___________________
        {    LW_ROM_MANUFACTURER_MACRONIX,    0x03, 0x2010 },
        {    LW_ROM_MANUFACTURER_MACRONIX,    0x03, 0x2011 },
        {    LW_ROM_MANUFACTURER_MACRONIX,    0x03, 0x2012 },
        {    LW_ROM_MANUFACTURER_MACRONIX,    0x03, 0x2013 },
        // GigaDevice_____________________________________
        {    LW_ROM_MANUFACTURER_GD,          0x03, 0x4010 },
        {    LW_ROM_MANUFACTURER_GD,          0x03, 0x4011 },
        {    LW_ROM_MANUFACTURER_GD,          0x03, 0x4012 },
        {    LW_ROM_MANUFACTURER_GD,          0x03, 0x4013 },
        // Winbond________________________________________
        {    LW_ROM_MANUFACTURER_WINBOND,     0x02, 0x3011 },
        {    LW_ROM_MANUFACTURER_WINBOND,     0x03, 0x3012 },
        {    LW_ROM_MANUFACTURER_WINBOND,     0x04, 0x3013 },
        {    LW_ROM_MANUFACTURER_WINBOND,     0x05, 0x3014 },
        // Programmable Microelectronics Corp_____________
        {    LW_ROM_MANUFACTURER_PMC,         0x13, 0x9D20 },
        {    LW_ROM_MANUFACTURER_PMC,         0x17, 0x9D21 },
        {    LW_ROM_MANUFACTURER_PMC,         0x17, 0x9D22 },
        // Shanghai Fudan Microelectronics
        {    LW_ROM_MANUFACTURER_SFM,         0x07, 0x3112 },
        // Do not add any devices after this entry________
        {    0x00,                            0x00, 0x0000 }
    };

    manufacturerCode = (LwU8)(DRF_VAL(_ROM, _SW_JEDECID,
                                      _MANUFACTURERCODE, jedecId));
    deviceCode       = (LwU16)(((DRF_VAL(_ROM, _SW_JEDECID,
                                         _DEVICECODEHI, jedecId)) << 8) |
                                (DRF_VAL(_ROM, _SW_JEDECID,
                                         _DEVICECODELO, jedecId)));

    for (pTable = spiDevicesSupported_GMXXX; pTable->manufacturerCode != 0; pTable++)
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
 * @brief Prepares a bus for a transfer.
 *
 * @param[in]  pSpiDev  SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If SPI Bus was configured successfully.
 * @return Other unexpected error
 */
FLCN_STATUS
spiBusConfigure_GMXXX
(
    SPI_DEVICE    *pSpiDev
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBusConfigure_GMXXX_exit;
    }

    status = spiArbiterXusbBlock_HAL(&Spi, pSpiDev, LW_TRUE);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_GMXXX_exit;
    }

    status = spiRomSerialBypassEnable_HAL(&Spi, (SPI_DEVICE_ROM *)pSpiDev,
                                          LW_TRUE, NULL);
    if (status != FLCN_OK)
    {
        goto spiBusConfigure_GMXXX_exit;
    }

spiBusConfigure_GMXXX_exit:
    return status;
}

/*!
 * @brief Reset a bus after a transfer.
 *
 * @param[in]  pSpiDev  SPI_DEVICE pointer
 *
 * @return FLCN_OK
 *     If SPI Bus was reset correctly.
 * @return Other unexpected error
 */
FLCN_STATUS
spiBusReset_GMXXX
(
    SPI_DEVICE    *pSpiDev
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiBusReset_GMXXX_exit;
    }

    status = spiRomSerialBypassEnable_HAL(&Spi, (SPI_DEVICE_ROM *)pSpiDev,
                                          LW_FALSE, NULL);
    if (status != FLCN_OK)
    {
        goto spiBusReset_GMXXX_exit;
    }

    status = spiArbiterXusbBlock_HAL(&Spi, pSpiDev, LW_FALSE);
    if (status != FLCN_OK)
    {
        goto spiBusReset_GMXXX_exit;
    }

spiBusReset_GMXXX_exit:
    return status;
}

/*!
 * @brief Transfer data between SPI Initiator and Target.
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
spiBufferTransfer_GMXXX
(
    SPI_DEVICE   *pSpiDev,
    SPI_HW_CTRL  *pSpiCtrl, 
    LwU8         *pRxBuffer,
    LwU8         *pTxBuffer
)
{
    FLCN_STATUS  status     = FLCN_ERR_NOT_SUPPORTED;
    LwBool       bDisableCs;
    LwBool       bReverse;
    LwU32        regCache;

    if ((pSpiDev == NULL) || (pSpiCtrl == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if ((pSpiCtrl->rxSizeBytes == 0) && (pSpiCtrl->txSizeBytes == 0))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    bDisableCs = (pSpiCtrl->ctrlFlags & SPI_CTRL_FLAGS_TARGET_DEASSERT);
    bReverse   = (pSpiCtrl->ctrlFlags & SPI_CTRL_FLAGS_REVERSE_ENABLE);

    regCache = REG_RD32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS);

    // Transmit data or command to EEPROM
    if (pSpiCtrl->txSizeBytes != 0x0)
    {
        if (pSpiCtrl->ctrlFlags & SPI_CTRL_FLAGS_TARGET_SELECT)
        {
            status = s_spiChipSelectEnable(pSpiDev, &regCache, LW_TRUE);
            if (status != FLCN_OK)
            {
                goto spiBufferTransfer_GMXXX_exit;
            }
        }

        status = s_spiBitBangBufferTransfer(pSpiDev, &regCache, pTxBuffer, 
                                           NULL, pSpiCtrl->txSizeBytes,
                                           ((pSpiCtrl->rxSizeBytes == 0x0) &&
                                             bDisableCs),
                                           bReverse);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_GMXXX_exit;
        }

        if ((pSpiCtrl->rxSizeBytes == 0x0) && bDisableCs)
        {
            status = s_spiChipSelectEnable(pSpiDev, &regCache, LW_FALSE);
            if (status != FLCN_OK)
            {
                goto spiBufferTransfer_GMXXX_exit;
            }
        }
    }

    // Read back data in Input buffer
    if (pSpiCtrl->rxSizeBytes != 0x0)
    {
        status = s_spiBitBangBufferTransfer(pSpiDev, &regCache, NULL, pRxBuffer,
                     pSpiCtrl->rxSizeBytes, bDisableCs, bReverse);
        if (status != FLCN_OK)
        {
            goto spiBufferTransfer_GMXXX_exit;
        }

        if (bDisableCs)
        {
            status = s_spiChipSelectEnable(pSpiDev, &regCache, LW_FALSE);
            if (status != FLCN_OK)
            {
                goto spiBufferTransfer_GMXXX_exit;
            }
        }
    }

spiBufferTransfer_GMXXX_exit:
    if (status != FLCN_OK)
    {
        status = s_spiChipSelectEnable(pSpiDev, &regCache, LW_FALSE);
    }

    return status;
}

/*!
 * @brief Enable/Disable chip select.
 *
 * Assert (or deassert) SPI device before (or after) an
 * SPI transaction. 
 *
 * @param[in]  pSpiDev       Pointer to SPI_DEVICE
 * @param[in]  pReg          Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  bEnable       Enable/Disable chip select 
 *
 * @return FLCN_OK
 *     Upon success.
 * @return Other unexpected error
 */
FLCN_STATUS
spiChipSelectEnable_GMXXX
(
    SPI_DEVICE  *pSpiDev,
    LwU32       *pReg,
    LwBool       bEnable
)
{
    if (bEnable)
    {
        *pReg = FLD_SET_DRF(_PMGR, _ROM_SERIAL_BYPASS, _CS,
                                        _ENABLED, *pReg);
    }
    else
    {
        *pReg = FLD_SET_DRF(_PMGR, _ROM_SERIAL_BYPASS, _CS,
                                        _DISABLED, *pReg);
    }

    REG_WR32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS, *pReg);

    return FLCN_OK;
}

/*!
 * @brief Set a bit on the target input line (MOSI).
 *
 * This function updates the cached value of LW_PMGR_ROM_SERIAL_BYPASS.
 * After this immediately call the clock edge set to write the bit value
 * to EEPROM.
 *
 * @param[in]  pSpiDev     Pointer to SPI_DEVICE
 * @param[in]  pReg        Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  val         Bit value to write
 *
 * @return FLCN_OK
 *     Upon success.
 * @return FLCN_ERR_ILWALID_STATE
 *     If input pointer is NULL
 */
FLCN_STATUS
spiMosiSet_GMXXX
(
    SPI_DEVICE  *pSpiDev,
    LwU32       *pReg,
    LwU8         val
)
{
    if ((pSpiDev == NULL) || (pReg == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    *pReg = FLD_SET_DRF_NUM(_PMGR, _ROM_SERIAL_BYPASS, _SI, val,
                            *pReg);

    return FLCN_OK;
}

/*!
 * @brief Get the current bit on the target output line (MISO).
 *
 * @param[in]  pSpiDev     Pointer to SPI_DEVICE
 * @param[in]  pReg        Pointer to cached valued of SERIAL_BYPASS
 * @param[out] pVal        Pointer to location where this bit value will be
 *                         saved.
 *
 * @return FLCN_OK
 *     If successfully received
 * @return FLCN_ERR_ILWALID_STATE
 *     If input pointer is NULL
 */
FLCN_STATUS
spiMisoGet_GMXXX
(
    SPI_DEVICE  *pSpiDev,
    LwU32       *pReg,
    LwU8        *pVal
)
{
    if ((pSpiDev == NULL) || (pReg == NULL) || (pVal == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    *pReg = REG_RD32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS);
    *pVal = (LwU8)DRF_VAL( _PMGR, _ROM_SERIAL_BYPASS, _SO, *pReg);

    return FLCN_OK;
}

/*!
 * @brief set clock edge value on which data will be sampled/toggled.
 *
 * The value to be set here will be 0 or 1. If 0 then data will be 
 * sampled/toggled at the falling edge else on the rising edge.
 *
 * @param[in]  pSpiDev     Pointer to SPI_DEVICE
 * @param[in]  pReg        Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  sckEdgeVal  Bit value to write
 *
 * @return FLCN_OK
 *     If successfully set
 * @return FLCN_ERR_ILWALID_STATE
 *     If input pointer is NULL
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     If invalid sck edge value is passed
 */
FLCN_STATUS
spiSckEdgeSet_GMXXX
(
    SPI_DEVICE  *pSpiDev,
    LwU32       *pReg,
    LwU8         sckEdgeVal
)
{
    if ((pSpiDev == NULL) || (pReg == NULL))
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    if (sckEdgeVal >= 2)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pReg = FLD_SET_DRF_NUM(_PMGR, _ROM_SERIAL_BYPASS, _SCK,
                            sckEdgeVal, *pReg);
    REG_WR32(BAR0, LW_PMGR_ROM_SERIAL_BYPASS, *pReg);

    return FLCN_OK;
}

/*!
 * @brief Extract EEPROM Status Register Value from the 32-bit Data Read.
 *
 * @param[in]   regVal         4-byte data register value
 * @param[out]  pStatusReg     1-byte ROM Status register value
 *
 * @return FLCN_OK
 *     If SPI EEPROM Status Register value was extracted correctly.
 * @return Other unexpected error
 */
FLCN_STATUS
spiRomGetStatus_GMXXX
(
    LwU32           regVal,
    LwU8           *pStatusReg
)
{
    if (pStatusReg == NULL)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *pStatusReg = (LwU8)(DRF_VAL(_ROM, _STATUS_REGISTER, _SW, regVal)); 

    return FLCN_OK;
}

/* ------------------------ Private Functions  ------------------------------ */
/*!
 * @brief Enable/disable Chip select line of the SPI device.
 *
 * @param[in]  pSpiDev     Pointer to SPI_DEVICE
 * @param[in]  pReg        Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  bEnable     Enable/Disable chip select
 *
 * @return FLCN_OK
 *     If chip select is enabled/disabled successfully
 * @return Other unexpected error
 *
 */
static FLCN_STATUS
s_spiChipSelectEnable
(
    SPI_DEVICE    *pSpiDev,
    LwU32         *pReg,
    LwBool         bEnable
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!bEnable)
    {
        // Wait half period
        OS_PTIMER_SPIN_WAIT_NS(pSpiDev->clockPeriod >> 1);
    }

    // 1. Disable CS
    status = spiChipSelectEnable_HAL(&Spi, pSpiDev, pReg, LW_FALSE);
    if (status != FLCN_OK)
    {
        goto s_spiChipSelectEnable_exit;
    }

    //
    // The MOSI data is latched by the SPI memory device on the rising clock
    // edge. It is written along with the falling clock edge. This gives enough
    // time of 1/2 clock cycle to stabilise the output.
    //
    // Optimisation: The MOSI data is updated in the cached value and then
    // written to the GPU along with the falling clock value. Instead of 2
    // seperate PCI reg writes, only 1 is required.
    //
    // NOTE: Do not interchange the sequence of spiMosiSet and spiSckEdgeSet
    // without modifying the functions appropriately.
    //

    // Write SI data 
    status = spiMosiSet_HAL(&Spi, pSpiDev, pReg, 0);
    if (status != FLCN_OK)
    {
        goto s_spiChipSelectEnable_exit;
    }

    // 2. Idle clock
    status = spiSckEdgeSet_HAL(&Spi, pSpiDev, pReg, pSpiDev->clockPolarity);
    if (status != FLCN_OK)
    {
        goto s_spiChipSelectEnable_exit;
    }

    // Wait half period
    OS_PTIMER_SPIN_WAIT_NS(pSpiDev->clockPeriod >> 1);

    if (bEnable)
    {
        // Enable CS
        status = spiChipSelectEnable_HAL(&Spi, pSpiDev, pReg, LW_TRUE);
        if (status != FLCN_OK)
        {
            goto s_spiChipSelectEnable_exit;
        }

        // Wait half period
        OS_PTIMER_SPIN_WAIT_NS(pSpiDev->clockPeriod >> 1);
    }

s_spiChipSelectEnable_exit:
    return status;
}

/*!
 * Transfers buffer to and from spi device using bit banging.
 *
 * @param[in]  pSpiDev       Pointer to SPI_DEVICE
 * @param[in]  pReg          Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  pOutBuffer    Pointer to the location where data will be obtained
 * @param[out] pInBuffer     Pointer to the location where data will be saved
 * @param[in]  sizeBytes     Number of bytes of data to read/write
 * @param[in]  bSckReturn    Return SCK back to its original state
 * @param[in]  bReverse      Send data in reverse order
 *
 * @return FLCN_OK
 *     If buffer transfer is successful
 * @return Other unexpected error
 */
static FLCN_STATUS
s_spiBitBangBufferTransfer
(
    SPI_DEVICE    *pSpiDev,
    LwU32         *pReg,
    void          *pOutBuffer,
    void          *pInBuffer,
    LwU32          sizeBytes,
    LwBool         bSckReturn,
    LwBool         bReverse
)
{
    LwU8 mosi = 0;
    LwU8 *pMosiIter;
    LwU8 *pMisoIter;
    LwS32 mosiInc;
    LwS32 misoInc;
    LwU8 *pMosi = pOutBuffer;
    LwU8 *pMiso = pInBuffer;
    FLCN_STATUS status = FLCN_OK;

    if (pMosi != NULL)
    {
        if (!bReverse)
        {
            pMosiIter = pMosi;
            mosiInc = 1;
        }
        else
        {
            pMosiIter = pMosi + sizeBytes - 1;
            mosiInc = -1;
        }
    }
    else
    {
        pMosiIter = &mosi;
        mosiInc = 0;
    }

    if (pMiso != NULL)
    {
        if (!bReverse)
        {
            pMisoIter = pMiso;
            misoInc = 1;
        }
        else
        {
            pMisoIter = pMiso + sizeBytes - 1;
            misoInc = -1;
        }
    }
    else
    {
        pMisoIter = NULL;
        misoInc = 0;
    }

    while (sizeBytes > 0)
    {
        sizeBytes--;
        status = s_spiBitBangByteTransfer(pSpiDev, pReg,
            pMisoIter, *pMosiIter, bSckReturn && (sizeBytes == 0));
        if (status != FLCN_OK)
        {
            break;
        }
        pMosiIter += mosiInc;
        pMisoIter += misoInc;
    }

    return status;
}

/*!
 * Transfers a byte to and from spi device.
 *
 * @param[in]  pSpiDev         Pointer to SPI_DEVICE
 * @param[in]  pReg            Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  pMiso           Pointer to the location where data will be saved
 * @param[out] mosi            data to be written
 * @param[in]  bSckReturn      Return SCK back to its original state
 *
 * @return FLCN_OK
 *     If buffer transfer is successful
 * @return Other unexpected error
 */
static FLCN_STATUS
s_spiBitBangByteTransfer
(
    SPI_DEVICE    *pSpiDev,
    LwU32         *pReg,
    LwU8          *pMiso,
    LwU8           mosi,
    LwBool         bSckReturn
)
{
    LwU8 i;
    LwU8 misoBit = 0;
    LwU8 misoByte = 0;
    LwU8 *pMisoTemp = (pMiso == NULL) ? NULL : &misoBit;
    FLCN_STATUS status = FLCN_OK;

    for (i = 0; i < 8; ++i)
    {
        status = s_spiBitBangBitTransfer(pSpiDev, pReg,
            pMisoTemp, mosi >> (7 - i), bSckReturn && (i == 7));
        if (status != FLCN_OK)
        {
            break;
        }
        misoByte = (misoByte << 1) | misoBit;
    }

    if (pMiso != NULL)
    {
        *pMiso = misoByte;
    }

    return status;
}

/*!
 * Transfers a bit to and from spi device.
 *
 * @param[in]  pSpiDev         Pointer to SPI_DEVICE
 * @param[in]  pReg            Pointer to cached valued of SERIAL_BYPASS
 * @param[in]  pMiso           Pointer to the location where data will be saved
 * @param[out] mosi            data to be written
 * @param[in]  bSckReturn      Return SCK back to its original state
 *
 * @return FLCN_OK
 *     If bit transfer is successful
 * @return Other unexpected error
 */
static FLCN_STATUS
s_spiBitBangBitTransfer
(
    SPI_DEVICE    *pSpiDev,
    LwU32         *pReg,
    LwU8          *pMiso,
    LwU8           mosi,
    LwBool         bSckReturn
)
{
    LwU8        clock;
    FLCN_STATUS status              = FLCN_OK;
    FLCN_STATUS spiSckEdgeSetStatus = FLCN_OK;

    clock = pSpiDev->clockPolarity ^ pSpiDev->clockPhase;

    //
    // The MOSI data is latched by the SPI memory device on the rising clock
    // edge. It is written along with the falling clock edge. This gives enough
    // time of 1/2 clock cycle to stabilise the output.
    //
    // Optimisation: The MOSI data is updated in the cached value and then
    // written to the GPU along with the falling clock value. Instead of 2
    // seperate PCI reg writes, only 1 is required.
    //
    // NOTE: Do not interchange the sequence of spiMosiSet and spiSckEdgeSet
    // without modifying the functions appropriately.
    //

    // Write SI data
    status = spiMosiSet_HAL(&Spi, pSpiDev, pReg, mosi);
    if (status != FLCN_OK)
    {
        goto s_spiBitBangBitTransfer;
    }

    // Write data toggle clock edge
    status = spiSckEdgeSet_HAL(&Spi, pSpiDev, pReg, clock);
    if (status != FLCN_OK)
    {
        goto s_spiBitBangBitTransfer;
    }

    // Wait half period
    OS_PTIMER_SPIN_WAIT_NS(pSpiDev->clockPeriod >> 1);

    // Write data sample clock edge
    clock = ~clock & 0x1;
    status = spiSckEdgeSet_HAL(&Spi, pSpiDev, pReg, clock);
    if (status != FLCN_OK)
    {
        goto s_spiBitBangBitTransfer;
    }

    // Sample data
    if (pMiso != NULL)
    {
        status = spiMisoGet_HAL(&Spi, pSpiDev, pReg, pMiso);
        if (status != FLCN_OK)
        {
            goto s_spiBitBangBitTransfer;
        }
    }

    // Wait half period
    OS_PTIMER_SPIN_WAIT_NS(pSpiDev->clockPeriod >> 1);

    if (bSckReturn)
    {
        status = spiSckEdgeSet_HAL(&Spi, pSpiDev, pReg, pSpiDev->clockPolarity);
        if (status != FLCN_OK)
        {
            goto s_spiBitBangBitTransfer;
        }
    }

s_spiBitBangBitTransfer:
    if (status != FLCN_OK)
    {
        spiSckEdgeSetStatus = spiSckEdgeSet_HAL(&Spi, pSpiDev, pReg,
                                                pSpiDev->clockPolarity);
    }

    return status;
}
