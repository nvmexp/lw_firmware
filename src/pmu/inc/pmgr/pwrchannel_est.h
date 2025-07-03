/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_est.h
 * @brief @copydoc pwrchannel_est.c
 */

#ifndef PWRCHANNEL_EST_H
#define PWRCHANNEL_EST_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_CHANNEL interfaces
 */
PwrChannelTupleStatusGet    (pwrChannelTupleStatusGet_ESTIMATION)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusGet_ESTIMATION");

/* ------------------------- Debug Macros ---------------------------------- */

#endif // PWRCHANNEL_EST_H
