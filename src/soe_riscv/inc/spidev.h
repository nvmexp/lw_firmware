/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SPIDEV_H
#define SPIDEV_H

/*!
 * SPI Control flags used to transact with an SPI Device.
 */
#define SPI_CTRL_FLAGS_NONE                             0
#define SPI_CTRL_FLAGS_READ_ENABLE                      BIT(0)
#define SPI_CTRL_FLAGS_INTERRUPT_ENABLE                 BIT(1)
#define SPI_CTRL_FLAGS_REVERSE_ENABLE                   BIT(2)
#define SPI_CTRL_FLAGS_SLAVE_SELECT                     BIT(3)
#define SPI_CTRL_FLAGS_SLAVE_DEASSERT                   BIT(4)

#define RM_SOE_SPI_XFER_MAX_BLOCK_SIZE (256)

/* ------------------------- Types Definitions ----------------------------- */

typedef struct
{
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
} SPI_DEVICE, *PSPI_DEVICE;

/*!
 * A generic control structure used to program HW-SPI controller
 * to trigger a transaction with SPI device (Slave).
 */
typedef struct
{
    /*!
     * Chip select id of SPI device (slave) to be asserted.
     */
    LwU8    chipSelectId;

    /*!
     * HW-SPI control flags.
     */
    LwU8    ctrlFlags;

    /*!
     * Number of bytes to be transmitted to Slave.
     */
    LwU16   txSizeBytes;

    /*!
     * Number of bytes to be received from Slave.
     */
    LwU16    rxSizeBytes;
} SPI_HW_CTRL, *PSPI_HW_CTRL;

/* ------------------------- Child Class Includes -------------------------- */
#include "soe_spirom.h"

#endif //SPIDEV_H
