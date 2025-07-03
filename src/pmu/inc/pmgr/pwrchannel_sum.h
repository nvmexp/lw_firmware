/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_sum.h
 * @brief @copydoc pwrchannel_sum.c
 */

#ifndef PWRCHANNEL_SUM_H
#define PWRCHANNEL_SUM_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"
#include "pmgr/i2cdev.h"

/* ------------------------- Types Definitions ----------------------------- */
typedef struct PWR_CHANNEL_SUMMATION PWR_CHANNEL_SUMMATION;

struct PWR_CHANNEL_SUMMATION
{
    /*!
     * @copydoc PWR_CHANNEL
     */
    PWR_CHANNEL super;

    /*!
     * Index of the first power channel relationship entry for this power
     * channel.
     */
    LwU8        relIdxFirst;
    /*!
     * Index of the last power channel relationship entry for this power
     * channel.
     */
    LwU8        relIdxLast;
};

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHANNEL interfaces
 */
BoardObjGrpIfaceModel10ObjSet           (pwrChannelGrpIfaceModel10ObjSetImpl_SUMMATION)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelGrpIfaceModel10ObjSetImpl_SUMMATION");
PwrChannelRefresh           (pwrChannelRefresh_SUMMATION)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelRefresh_SUMMATION");
PwrChannelContains          (pwrChannelContains_SUMMATION)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelContains_SUMMATION");
PwrChannelTupleStatusGet    (pwrChannelTupleStatusGet_SUMMATION)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusGet_SUMMATION");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHANNEL_SUM_H
