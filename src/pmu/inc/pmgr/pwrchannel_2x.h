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
 * @file    pwrchannel_2x.h
 * @copydoc pwrchannel_2x.c
 */

#ifndef PWRCHANNEL_2X_H
#define PWRCHANNEL_2X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------------ Macros ------------------------------------ */
/*!
 * Accessor macro PWR_CHANNEL::bIsEnergySupported
 * Return if this channel can support energy reading and
 * supported on Power Topology 2.X Only!.
 *
 * @param[in]   pChannel    PWR_CHANNEL object pointer.
 *
 * @return @ref PWR_CHANNEL::bIsEnergySupported
 */
#define PWR_CHANNEL_IS_ENERGY_SUPPORTED(pChannel)                             \
    (pChannel->bIsEnergySupported)

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * @interface PWR_CHANNEL
 *
 * Retrieves the latest PWR_CHANNEL tuple status.
 *
 * @param[in]   pChannel    PWR_CHANNEL object pointer.
 * @param[out]  pTuple      @ref LW2080_CTRL_PMGR_PWR_TUPLE
 *
 * @return FLCN_OK
 *      Tuple successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This operation is not supported on given channel.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrChannelTupleStatusGet(fname) FLCN_STATUS (fname)(PWR_CHANNEL *pChannel, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * BOARDOBJ interfaces
 */
BoardObjIfaceModel10GetStatus (pwrChannelIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelIfaceModel10GetStatus");

/*!
 * PWR_CHANNEL interfaces
 */
PwrChannelTupleStatusGet (pwrChannelTupleStatusGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusGet");
void                     pwrChannelTupleStatusReset(PWR_CHANNEL *pChannel, LwBool bFromRM)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelTupleStatusReset");

FLCN_STATUS pwrChannelConstructSuper_2X(PWR_CHANNEL *pChannel, RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJ_SET *pDescTmp, LwBool bFirstConstruct)
    GCC_ATTRIB_SECTION("imem_pmgrLibConstruct", "pwrChannelConstructSuper_2X");
FLCN_STATUS pwrChannelGetStatus_SUPER(BOARDOBJ *pBoardObj, RM_PMU_BOARDOBJ *pPayload, LwBool bLwrrAdjRequired)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelGetStatus_SUPER");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRCHANNEL_2X_H
