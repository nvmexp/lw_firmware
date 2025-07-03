/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pwrchannel_1x.h
 * @copydoc pwrchannel_1x.c
 */

#ifndef PWRCHANNEL_1X_H
#define PWRCHANNEL_1X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------- Macros ----------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * PWR_CHANNEL 1X interfaces
 */
FLCN_STATUS pwrChannelConstructSuper_1X(PWR_CHANNEL *pChannel, RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *pDescTmp, LwBool bFirstConstruct)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelConstructSuper_1X");

/* ------------------------- Debug Macros ----------------------------------- */
/* ------------------------- Child Class Includes --------------------------- */

#endif // PWRCHANNEL_1X_H

