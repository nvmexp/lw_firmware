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
 * @file    spidev.h
 * @copydoc spidev.c
 */

#ifndef SPIDEV_H
#define SPIDEV_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- OpenRTOS Includes ----------------------------- */
#include "falcon.h"

/* ------------------------- Application Includes -------------------------- */
#include "ctrl/ctrl2080/ctrl2080spi.h"
#include "pmu/pmuifcmn.h"
#include "pmu/pmuifspi.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct SPI_DEVICE SPI_DEVICE, SPI_DEVICE_BASE;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_SPI_DEVICE \
    (&(Spi.devices.super))

/*!
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define SPI_DEVICE_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(SPI_DEVICE, (_objIdx)))

/*!
 * SPI Control flags used to transact with an SPI Device.
 */
#define SPI_CTRL_FLAGS_NONE                             0
#define SPI_CTRL_FLAGS_READ_ENABLE                      BIT(0)
#define SPI_CTRL_FLAGS_INTERRUPT_ENABLE                 BIT(1)
#define SPI_CTRL_FLAGS_REVERSE_ENABLE                   BIT(2)
#define SPI_CTRL_FLAGS_TARGET_SELECT                    BIT(3)
#define SPI_CTRL_FLAGS_TARGET_DEASSERT                  BIT(4)

/* ------------------------- Types Definitions ----------------------------- */

struct SPI_DEVICE
{
    BOARDOBJ      super;

    /*!
     * Bus id for the device.
     */
    LwU32        busId;

    /*!
     * Chip select id for the device.
     */
    LwU32        chipSelectId;

    /*!
     * Clock polarity (base value of clock) for the device.
     */
    LwU8         clockPolarity;

    /*!
     * Clock phase (sampling on leading/trailing edge of the clock) for the
     * device.
     */
    LwU8         clockPhase;

    /*!
     * Clock period in nano seconds.
     */
    LwU32        clockPeriod;
};

/*!
 * A generic control structure used to program HW-SPI controller
 * to trigger a transaction with SPI device (Target).
 */
typedef struct
{
    /*!
     * Chip select id of SPI device (target) to be asserted.
     */
    LwU8    chipSelectId;

    /*!
     * HW-SPI control flags.
     */
    LwU8    ctrlFlags;

    /*!
     * Number of bytes to be transmitted to Target.
     */
    LwU16   txSizeBytes;

    /*!
     * Number of bytes to be received from Target.
     */
    LwU16    rxSizeBytes;
} SPI_HW_CTRL;

/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10CmdHandler   (spiDeviceBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_spiLibConstruct", "spiDeviceBoardObjGrpIfaceModel10Set");

BoardObjGrpIfaceModel10ObjSet       (spiDeviceGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_spiLibConstruct", "spiDeviceGrpIfaceModel10ObjSetImpl_SUPER");

/*!
 * Board Object interfaces.
 */
BoardObjGrpIfaceModel10SetEntry   (spiDeviceIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_spiLibConstruct", "spiDeviceIfaceModel10SetEntry");

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/* ------------------------- Child Class Includes -------------------------- */

#include "spi/spidev_rom.h"

#endif // SPIDEV_H
