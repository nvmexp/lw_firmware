/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrmonitor_2x.h
 * @brief @copydoc pwrmonitor_2x.c
 */

#ifndef PWRMONITOR_2X_H
#define PWRMONITOR_2X_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/pwrmonitor.h"
#include "task_pmgr.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * @interface PWR_MONITOR
 *
 * Retrieves the latest PWR_CHANNEL tuple status.
 *
 * @param[in]   pMon        Pointer to a PWR_MONITOR object
 * @param[in]   channelIdx  Index for channel to retrieve
 * @param[out]  pTuple      @ref LW2080_CTRL_PMGR_PWR_TUPLE
 *
 * @return FLCN_OK
 *      Tuple successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      This operation is not supported on given channel.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrMonitorChannelTupleStatusGet(fname) FLCN_STATUS (fname)(PWR_MONITOR *pMon, LwU8 channelIdx, LW2080_CTRL_PMGR_PWR_TUPLE *pTuple)

/*!
 * @interface PWR_MONITOR
 *
 * Handles the RM/PMU RM_PMU_PMGR_CMD_ID_PWR_CHANNELS_TUPLE_QUERY. Iterates
 * over the requested channels (@ref
 * RM_PMU_PMGR_CMD_PWR_CHANNELS_TUPLE_QUERY:channelMask) and calls @ref
 * PwrMonitorChannelTupleStatusGet for each channel.
 *
 * @param[in]  pMon           Pointer to a PWR_MONITOR object
 * @param[in]  pwrChannelMask Mask of PWR_CHANNELs to retrieve
 * @param[out] pPayload       @ref RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD
 * @param[in]  bFromRM        Boolean indicating if this request is from RM
 *
 * @return FLCN_OK
 *      Samples successfully retrieved.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      An unsupported channel is specified in the mask.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
#define PwrMonitorChannelsTupleQuery(fname) FLCN_STATUS (fname)(PWR_MONITOR *pMon, LwU32 channelMask, LW2080_CTRL_BOARDOBJGRP_MASK_E32 *pChRelMask, RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD *pPayload, LwBool bFromRM)

/* ------------------------- Defines --------------------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * PWR_MONITOR interfaces
 */
PwrMonitorChannelTupleStatusGet (pwrMonitorChannelTupleStatusGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrMonitorChannelTupleStatusGet");
PwrMonitorChannelsTupleQuery    (pwrMonitorChannelsTupleQuery);
PwrMonitorChannelTupleStatusGet (pwrMonitorChannelTupleQuery);
LwU32       pwrMonitorDependentPhysicalChMaskGet(PWR_MONITOR *pMon, LwU32 channelMask)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrMonitorDependentPhysicalChMaskGet");
LwU32       pwrMonitorDependentLogicalChMaskGet(PWR_MONITOR *pMon, LwU32 channelMask)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrMonitorDependentLogicalChMaskGet");
void        pwrMonitorChannelsTupleReset(PWR_MONITOR *pMon, LwBool bFromRM)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrMonitorChannelsTupleReset");

#endif // PWRMONITOR_2X_H
