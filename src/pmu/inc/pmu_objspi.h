/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJSPI_H
#define PMU_OBJSPI_H

/*!
 * @file pmu_objspi.h
 */

/* ------------------------ System Includes -------------------------------- */
#include "flcntypes.h"

/* ------------------------ Application Includes --------------------------- */
#include "spi/spidev.h"
#include "config/g_spi_hal.h"
#include "unit_api.h"
#include "main.h"
#include "rmpmusupersurfif.h"
#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
#include "lwostimer.h"
#endif

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * SPI object Definition
 */
typedef struct
{
    /*!
     * Board Object Group of all SPI_DEVICE objects.
     */
    BOARDOBJGRP_E32 devices;
} OBJSPI;

/* ------------------------ External Definitions --------------------------- */
extern OBJSPI          Spi;
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
extern OS_TMR_CALLBACK OsCBSpi;
#else
extern OS_TIMER        SpiOsTimer;
#endif

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
void spiPreInit(void)
    GCC_ATTRIB_SECTION("imem_init", "spiPreInit");

FLCN_STATUS spiInit(LwU8 devIndex, LwU32 offset, LwU32 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spi", "spiInit");

FLCN_STATUS spiRead(LwU8 devIndex, LwU32 offset, LwU32 sizeInBytes, void *pDmemBuf)
    GCC_ATTRIB_SECTION("imem_spi", "spiRead");

FLCN_STATUS spiWrite(LwU8 devIndex, LwU32 offset, LwU32 sizeInBytes, void *pDmemBuf)
    GCC_ATTRIB_SECTION("imem_spi", "spiWrite");

FLCN_STATUS spiErase(LwU8 devIndex, LwU32 offset, LwU32 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spi", "spiErase");

FLCN_STATUS spiDeInit(LwU8 devIndex)
    GCC_ATTRIB_SECTION("imem_spi", "spiDeInit");

FLCN_STATUS spiRomInit(SPI_DEVICE *pSpiDev, LwU32 offset, LwU32 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spiLibRomInit", "spiRomInit");

FLCN_STATUS spiRomRead(SPI_DEVICE *pSpiDev, LwU32 offset, LwU32 sizeInBytes, void *pDmemBuf)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomRead");

FLCN_STATUS spiRomWrite(SPI_DEVICE *pSpiDev, LwU32 offset, LwU32 sizeInBytes, void *pDmemBuf)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomWrite");

FLCN_STATUS spiRomErase(SPI_DEVICE *pSpiDev, LwU32 offset, LwU32 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomErase");

FLCN_STATUS spiRomDeInit(SPI_DEVICE *pSpiDev)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomDeInit");

FLCN_STATUS spiRomEraseSector(SPI_DEVICE_ROM *pSpiRom, LwU32 address)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomEraseSector");

FLCN_STATUS spiRomWritePage(SPI_DEVICE_ROM *pSpiRom, void *pBuf,
    LwU32 address, LwU16 sizeInBytes)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomWritePage");

FLCN_STATUS spiRomWriteEnable(SPI_DEVICE_ROM *pSpiRom)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomWriteEnable");

FLCN_STATUS spiRomWriteStatusReg(SPI_DEVICE_ROM *pSpiRom, LwU8 wrMask)
    GCC_ATTRIB_SECTION("imem_spi", "spiRomWriteStatusReg");

#endif // PMU_OBJSPI_H
