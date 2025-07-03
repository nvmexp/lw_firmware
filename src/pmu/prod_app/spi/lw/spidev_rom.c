/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
/*!
 * @file    spidev_rom.c
 * @brief   SPI ROM Driver
 * @extends SPI_DEVICE
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "spi/spidev_rom.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
INFOROM_REGION inforom;
ERASE_LEDGER_REGION ledgerRegion;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- SPI_DEVICE Interfaces -------------------------- */
/*!
 * Construct a SPI_DEVICE_ROM object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
spiDeviceGrpIfaceModel10ObjSetImpl_ROM
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    SPI_DEVICE_ROM      *pSpiRom;
    FLCN_STATUS          status;

    // Construct and initialize parent-class object.
    status = spiDeviceGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pSpiRom = (SPI_DEVICE_ROM *)*ppBoardObj;

    // Set member variables.
    pSpiRom->spiRomInfo.manufacturerCode = 0;
    pSpiRom->spiRomInfo.deviceCode       = 0;
    pSpiRom->spiRomInfo.packedInfoBits   =
        DRF_DEF(_ROM, _PACKED_INFO, _WPMASK, _INIT) |
        DRF_DEF(_ROM, _PACKED_INFO, _SECMD,  _INIT) |
        DRF_DEF(_ROM, _PACKED_INFO, _RDONLY, _INIT);
    pSpiRom->pInforom                    = &inforom;
    pSpiRom->pLedgerRegion               = &ledgerRegion;
    pSpiRom->bIsWriteProtected           = LW_FALSE;

    return status;
}
