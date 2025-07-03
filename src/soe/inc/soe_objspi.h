/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SOE_OBJSPI_H
#define SOE_OBJSPI_H

/*!
 * @file    soe_objspi.h
 * @copydoc soe_objspi.c
 */

/* ------------------------ System Includes -------------------------------- */
#include "soesw.h"

/* ------------------------ Application Includes --------------------------- */
#include "unit_dispatch.h"
#include "spidev.h"
#include "config/soe-config.h"
#include "config/g_spi_hal.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Global Variables ------------------------------- */
extern SPI_HAL_IFACES SpiHal;
LwrtosQueueHandle Disp2QSpiThd;

/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Macros & Defines ------------------------------- */
#define LW_INFOROM_IMAGE_SIG_SIZE   (4)
#define LW_INFOROM_IMAGE_SIG        (0x5346464A)

/* ------------------------ Public Functions ------------------------------- */
void constructSpi(void) GCC_ATTRIB_SECTION("imem_init", "constructSpi");
void spiPreInit(void)   GCC_ATTRIB_SECTION("imem_init", "spiPreInit");

FLCN_STATUS spiRomInit(PSPI_DEVICE pSpiDev);
FLCN_STATUS spiRomRead(PSPI_DEVICE pSpiDev, LwU32 offset, LwU32 sizeInBytes,
                        RM_FLCN_MEM_DESC *pMemDesc);
FLCN_STATUS spiRomReadLocal(PSPI_DEVICE_ROM pSpiRom, LwU32 offset, LwU32 sizeInBytes,
                        LwU8 *pLocalBuf);

FLCN_STATUS spiRomReadFromRegion(PSPI_DEVICE_ROM pSpiDev, PROM_REGION pRegion, LwU32 offset, LwU32 sizeInBytes,
                        RM_FLCN_U64 dmaHandle);

FLCN_STATUS spiRomWrite(PSPI_DEVICE pSpiDev, LwU32 offset, LwU32 sizeInBytes,
                        RM_FLCN_MEM_DESC *pMemDesc);
FLCN_STATUS spiRomWriteLocal(PSPI_DEVICE_ROM pSpiRom, LwU32 offset, LwU32 sizeInBytes,
                        LwU8 *pLocalBuf);
FLCN_STATUS spiRomErase(PSPI_DEVICE pSpiDev, LwU32 offset, LwU32 sizeInBytes);
FLCN_STATUS spiRomDeInit(PSPI_DEVICE pSpiDev);
FLCN_STATUS spiRomEraseSector(PSPI_DEVICE_ROM pSpiRom, LwU32 address);
FLCN_STATUS spiRomWritePage(PSPI_DEVICE_ROM pSpiRom, void *pBuf,
                            LwU32 address, LwU16 sizeInBytes);
LwBool  spiRomIsInitialized(void);
#endif // SOE_OBJSPI_H
