/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file thrmchannel_device.h
 * @brief @copydoc thrmchannel_device.c
 */

#ifndef THERMCHANNEL_DEVICE_H
#define THERMCHANNEL_DEVICE_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "therm/thrmdevice.h"
#include "therm/thrmchannel.h"

/* ------------------------- Macros ---------------------------------------- */
/*!
 * @brief Wrapper interface for @ref BoardObjIfaceModel10GetStatus()
 */
#define thermChannelIfaceModel10GetStatus_DEVICE(pModel10, pBuf)   \
            thermChannelIfaceModel10GetStatus_SUPER(pModel10, pBuf)

/* ------------------------- Datatypes ------------------------------------- */
typedef struct THERM_CHANNEL_DEVICE THERM_CHANNEL_DEVICE;

/*!
 * @brief Extends THERM_CHANNEL providing attributes specific to DEVICE type.
 * 
 * @extends THERM_CHANNEL
 */
struct THERM_CHANNEL_DEVICE
{
    /*!
     * @brief THERM_CHANNEL super class. This should always be the first member!
     */
    THERM_CHANNEL   super;

    /*!
     * @brief Pointer to the corresponding THERM_DEVICE object in the Thermal
     *        Device Table.
     */
    THERM_DEVICE   *pThermDev;

    /*!
     * @brief Provider index to query temperature value.
     */
    LwU8            thermDevProvIdx;
};

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjGrpIfaceModel10ObjSet           (thermChannelGrpIfaceModel10ObjSetImpl_DEVICE)
    GCC_ATTRIB_SECTION("imem_thermLibConstruct", "thermChannelGrpIfaceModel10ObjSetImpl_DEVICE");

ThermChannelTempValueGet    (thermChannelTempValueGet_DEVICE)
    GCC_ATTRIB_SECTION("imem_thermLibSensor2X", "thermChannelTempValueGet_DEVICE");

/* ------------------------ Include Derived Types -------------------------- */
#endif // THERMCHANNEL_DEVICE_H
