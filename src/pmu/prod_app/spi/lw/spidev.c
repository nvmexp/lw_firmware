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
 * @file   spidev.c
 * @brief  Represents a physical SPI device on the board.
 *
 * This module views a physical SPI device as any SPI device with a unique
 * chip select ID.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "pmusw.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "spi/spidev.h"
#include "pmu_objspi.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * SPI_DEVICE handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
spiDeviceBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    return BOARDOBJGRP_IFACE_MODEL_10_SET(E32,               // _grpType
        SPI_DEVICE,                                 // _class
        pBuffer,                                    // _pBuffer
        boardObjGrpIfaceModel10SetHeader,                 // _hdrFunc
        spiDeviceIfaceModel10SetEntry,                    // _entryFunc
        all.spi.spiDeviceGrpSet,                    // _ssElement
        OVL_INDEX_DMEM(spi),                        // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
}

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
spiDeviceGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_SPI_SPI_DEVICE_BOARDOBJ_SET *pSet =
        (RM_PMU_SPI_SPI_DEVICE_BOARDOBJ_SET *)pBoardObjDesc;
    SPI_DEVICE   *pSpiDev;
    FLCN_STATUS   status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pSpiDev = (SPI_DEVICE *)*ppBoardObj;

    pSpiDev->busId         = pSet->busId;
    pSpiDev->chipSelectId  = pSet->chipSelectId;
    pSpiDev->clockPolarity = pSet->clockPolarity;
    pSpiDev->clockPhase    = pSet->clockPhase;
    pSpiDev->clockPeriod   = pSet->clockPeriod;

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
spiDeviceIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_SPI_DEVICE_TYPE_ROM:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_SPI_DEVICE_ROM);
            return spiDeviceGrpIfaceModel10ObjSetImpl_ROM(pModel10, ppBoardObj,
                sizeof(SPI_DEVICE_ROM), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
