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
 * @file    pwrchannel_35.h
 * @copydoc pwrchannel_35.c
 */

#ifndef PWRCHANNEL_35_H
#define PWRCHANNEL_35_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrchannel.h"

/* ------------------------------ Macros ------------------------------------ */
/*!
 * List of overlays required to call @ref pwrChannelsStatusGet().
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35)
#define PWR_CHANNEL_35_OVERLAYS_STATUS_GET                                     \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrChannelsStatus)               \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64
#else
#define PWR_CHANNEL_35_OVERLAYS_STATUS_GET                                     \
    OSTASK_OVL_DESC_ILWALID
#endif

/* ------------------------- Types Definitions ----------------------------- */
/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes --------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * BOARDOBJ interfaces
 */
BoardObjIfaceModel10GetStatus (pwrChannelIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrChannelIfaceModel10GetStatus_35");

/*!
 * PWR_CHANNEL interfaces
 */
FLCN_STATUS  pwrChannelsLoad_35(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelsLoad_35");
void         pwrChannelsUnload_35(void)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrChannelsUnload_35");

/* ------------------------- Debug Macros ---------------------------------- */
/* ------------------------- Child Class Includes -------------------------- */

#endif // PWRCHANNEL_35_H
