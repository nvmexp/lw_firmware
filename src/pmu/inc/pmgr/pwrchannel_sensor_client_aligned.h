/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_sensor_client_aligned.h
 * @brief @copydoc pwrchannel_sensor_client_aligned.c
 *
 *
 * @endsection
 */

#ifndef PWRCHANNEL_SENSOR_CLIENT_ALIGNED_H
#define PWRCHANNEL_SENSOR_CLIENT_ALIGNED_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"
#include "pmgr/pmgr_acc.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * Structure for a client aligned power sensor class.
 */
typedef struct
{
    /*!
     * @copydoc PWR_CHANNEL_SENSOR
     */
    PWR_CHANNEL_SENSOR super;
    /*!
     * Structure containing aclwmulator data for energy. Source aclwmulator
     * is in nJ and destination is in mJ.
     */
    PMGR_ACC energy;
} PWR_CHANNEL_SENSOR_CLIENT_ALIGNED;

/* ------------------------- Defines --------------------------------------- */
/*!
 * SENSOR_CLIENT_ALIGNED is a sub-class of SENSOR channel class and hence
 * uses the super class functions for construct and set limit since there is
 * no additional sub-class specific functionality here
 */
#define pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR_CLIENT_ALIGNED                         \
    pwrChannelGrpIfaceModel10ObjSetImpl_SENSOR

#define pwrChannelLimitSet_SENSOR_CLIENT_ALIGNED                              \
    pwrChannelLimitSet_SENSOR

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
// Board Object interfaces.
BoardObjIfaceModel10GetStatus           (pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelIfaceModel10GetStatus_SENSOR_CLIENT_ALIGNED");
/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRCHANNEL_SENSOR_CLIENT_ALIGNED_H
