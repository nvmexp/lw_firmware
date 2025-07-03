/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   voltdev_vid.h
 * @brief  @copydoc voltdev_vid.c
 */

#ifndef VOLT_DEV_VID_H
#define VOLT_DEV_VID_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "volt/voltdev.h"

/* ------------------------- Macros ---------------------------------------- */
/* ------------------------- Datatypes ------------------------------------- */
typedef struct VOLT_DEVICE_VID VOLT_DEVICE_VID;

/*!
 * Extends VOLT_DEVICE providing attributes specific to Wide VID voltage devices
 */
struct VOLT_DEVICE_VID
{
    /*!
     * VOLT_DEVICE super-class. This should always be the first member!
     */
    VOLT_DEVICE    super;

    /*!
     * Base voltage is the y-intercept of liner equation y = mx + c
     */
    LwS32   voltageBaseuV;

    /*!
     * Offset scale is the slope(m) of liner equation y = mx + c
     */
    LwS32   voltageOffsetScaleuV;

    /*!
     * GPIO pin array for describing the GPIO pin numbers for the VSEL GPIOs
     */
    LwU8    gpioPin[LW2080_CTRL_VOLT_VOLT_DEV_VID_VSEL_MAX_ENTRIES];

    /*!
     * VSEL mask of the programmable GPIO pins
     */
    LwU8    vselMask;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet    (voltDeviceGrpIfaceModel10ObjSetImpl_VID)
    GCC_ATTRIB_SECTION("imem_libVoltConstruct", "voltDeviceGrpIfaceModel10ObjSetImpl_VID");

VoltDeviceGetVoltage (voltDeviceGetVoltage_VID);

VoltDeviceSetVoltage (voltDeviceSetVoltage_VID);

/* ------------------------ Include Derived Types -------------------------- */

#endif // VOLT_DEV_VID_H
