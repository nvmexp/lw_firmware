/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file   pmu_objspi.c
 * @brief  Container-object for PMU SPI routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "pmu_objspi.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TIMER     SpiOsTimer;
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Constructor function which initializes all the state related to the SPI
 * task.
 *
 * @return  FLCN_OK     On success
 */
FLCN_STATUS
constructSpi(void)
{
    // placeholder
    return FLCN_OK;
}

/*!
 * SPI pre-init function.  Takes care of any SW initialization which can be
 * called from INIT overlay/task.
 */
void
spiPreInit(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
        osTimerInitTracked(OSTASK_TCB(SPI), &SpiOsTimer, 0);
#endif
}

/*!
 *  @brief Perform an SPI Device Initialization on behalf of the client.
 *
 *  This function will redirect the device initialization request based on 
 *  SPI device type that uniquely identifies each SPI device on board.
 *
 * @param[in]  devIndex     SPI Device Index
 * @param[in]  offset       Starting offset of accessible memory range
 * @param[in]  sizeInBytes  Size of accessible memory range
 *
 * @return FLCN_OK
 *     Upon successful initialization.
 *
 * @return other
 *     Bubbles up any error status returned from device specific function.
 */
FLCN_STATUS
spiInit
(
    LwU8    devIndex,
    LwU32   offset,
    LwU32   sizeInBytes
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE *pSpiDev;

    pSpiDev = SPI_DEVICE_GET(devIndex);
    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiInit_exit;
    }

    switch (pSpiDev->super.type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, spiLibRomInit)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = spiRomInit(pSpiDev, offset, sizeInBytes);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            break;
        default:
            break;
    }

spiInit_exit:
    return status;
}

/*!
 *  @brief Perform an SPI Read Operation on behalf of the client.
 *
 *  This function will redirect the READ (from device memory) request based on 
 *  SPI device type that uniquely identifies each SPI device on board.
 *
 * @param[in]  devIndex     SPI Device Index
 * @param[in]  offset       SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be read.
 * @param[in]  pDmemBuf     Pointer to PMU dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful read.
 *
 * @return other
 *     Bubbles up any error status returned from device specific function.
 */
FLCN_STATUS
spiRead
(
    LwU8    devIndex,
    LwU32   offset,
    LwU32   sizeInBytes,
    void   *pDmemBuf
)
{
    FLCN_STATUS  status = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE  *pSpiDev;

    pSpiDev = SPI_DEVICE_GET(devIndex);
    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiRead_exit;
    }

    switch (pSpiDev->super.type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            status = spiRomRead(pSpiDev, offset, sizeInBytes, pDmemBuf);
            break;
        default:
            break;
    }

spiRead_exit:
    return status;
}

/*!
 *  @brief Perform an SPI Write Operation on behalf of the client.
 *
 *  This function will redirect the WRITE (to device memory) request based on 
 *  SPI device type that uniquely identifies each SPI device on board.
 *
 * @param[in]  devIndex     SPI Device Index
 * @param[in]  offset       SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be erased.
 * @param[in]  pDmemBuf     Pointer to PMU dmem buffer used for payload.
 *
 * @return FLCN_OK
 *     Upon successful write.
 *
 * @return other
 *     Bubbles up any error status returned from device specific function.
 */
FLCN_STATUS
spiWrite
(
    LwU8    devIndex,
    LwU32   offset,
    LwU32   sizeInBytes,
    void   *pDmemBuf
)
{
    FLCN_STATUS  status = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE  *pSpiDev;

    pSpiDev = SPI_DEVICE_GET(devIndex);
    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiWrite_exit;
    }

    switch (pSpiDev->super.type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            status = spiRomWrite(pSpiDev, offset, sizeInBytes, pDmemBuf);
            break;
        default:
            break;
    }

spiWrite_exit:
    return status;
}

/*!
 *  @brief Perform an SPI Erase Operation on behalf of the client.
 *
 *  This function will redirect the ERASE (device memory) request based on 
 *  SPI device type that uniquely identifies each SPI device on board.
 *
 * @param[in]  devIndex     SPI Device Index
 * @param[in]  offset       SPI Device memory offset.
 * @param[in]  sizeInBytes  Size of memory region to be erased.
 *
 * @return FLCN_OK
 *     Upon successful erase.
 *
 * @return other
 *     Bubbles up any error status returned from device specific function.
 */
FLCN_STATUS
spiErase
(
    LwU8    devIndex,
    LwU32   offset,
    LwU32   sizeInBytes
)
{
    FLCN_STATUS  status = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE  *pSpiDev;

    pSpiDev = SPI_DEVICE_GET(devIndex);
    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiErase_exit;
    }

    switch (pSpiDev->super.type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            status = spiRomErase(pSpiDev, offset, sizeInBytes);
            break;
        default:
            break;
    }

spiErase_exit:
    return status;
}

/*!
 * @brief Perform an SPI Device De-Initialization on behalf of the client.
 *
 * This function tears down an SPI Device state before PMU starts
 * unloading.
 *
 * @param[in]  devIndex     SPI Device Index
 *
 * @return FLCN_OK
 *     Upon successful SPI device tear down.
 *
 * @return other
 *     Bubbles up any error status returned from device specific function.
 */
FLCN_STATUS
spiDeInit
(
    LwU8 devIndex 
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    SPI_DEVICE *pSpiDev;

    pSpiDev = SPI_DEVICE_GET(devIndex);
    if (pSpiDev == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto spiDeInit_exit;
    }

    switch (pSpiDev->super.type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            status = spiRomDeInit(pSpiDev);
            break;
        default:
            break;
    }

spiDeInit_exit:
    return status;
}
