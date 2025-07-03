/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrchannel_est.c
 * @brief  Interface specification for a PWR_CHANNEL_ESTIMATION.
 *
 * A "Estimation" Power Channel is a logical construct representing a power
 * channel whose power value is estimated because the channel draws trivially
 * small amount of power.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * _ESTIMATION-specific implementation.
 *
 * @copydoc PwrChannelTupleStatusGet
 */
FLCN_STATUS
pwrChannelTupleStatusGet_ESTIMATION
(
    PWR_CHANNEL                *pChannel,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    pTuple->pwrmW  = 0;
    pTuple->lwrrmA = 0;
    pTuple->voltuV = pChannel->voltFixeduV;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
