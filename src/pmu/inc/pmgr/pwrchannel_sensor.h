/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_sensor.h
 * @brief @copydoc pwrchannel_sensor.c
 *
 *
 * @endsection
 */

#ifndef PWRCHANNEL_SENSOR_H
#define PWRCHANNEL_SENSOR_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Structure for HW-channels used in pstate-based power-capping.
 */
typedef struct
{
    /*!
     * @copydoc PWR_CHANNEL
     */
    PWR_CHANNEL super;

    /*!
     * Index into the Power Sensors Table for the PWR_DEVICE from which this
     * PWR_CHANNEL should query power values.
     */
    LwU8 pwrDevIdx;

    /*!
     * Index of the PWR_DEVICE Provider to query.
     */
    LwU8 pwrDevProvIdx;
} PWR_CHANNEL_SENSOR;

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHANNEL Interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR");
PwrChannelSample_IMPL       (pwrChannelSample_SENSOR)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelSample_SENSOR");
PwrChannelLimitSet          (pwrChannelLimitSet_SENSOR)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelLimitSet_SENSOR");
PwrChannelTupleStatusGet    (pwrChannelTupleStatusGet_SENSOR)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusGet_SENSOR");

/* ------------------------- PWR_CHANNEL_SENSOR Interfaces -------------- */

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */
#include "pmgr/pwrchannel_sensor_client_aligned.h"

#endif // PWRCHANNEL_SENSOR_H
