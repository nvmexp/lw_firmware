/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_fanctrl.h
 * @copydoc pmu_fanctrl.c
 */

#ifndef PMU_FANCTRL_H
#define PMU_FANCTRL_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * PWR_CHANNEL query response command. Issued from PMGR task.
 * THERM/FAN task may request PWR_CHANNEL value for implementing
 * Fan Stop sub-policy.
 *
 * This structure defines the command struct PMGR could send to THERM/FAN.
 */
typedef struct
{
    DISP2UNIT_HDR   hdr;    //<! Must be first element of the structure

    /*!
     * PMU status code to describe failures while obtaining PWR_CHANNEL status.
     */
    FLCN_STATUS     pmuStatus;

    /*!
     * PWR_CHANNEL value.
     */
    LwU32           pwrmW;

    /*!
     * Fan Policy index to which the PWR_CHANNEL query response belongs.
     */
    LwU8            fanPolicyIdx;
} DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE;

/*!
 * Generic dispatching structure used to send work to fanctrl code from other
 * tasks, such as CMDMGMT task for CMD dispatching.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;

    /*!
     * Query message response passed from PMGR.
     */
    DISPATCH_FANCTRL_PWR_CHANNEL_QUERY_RESPONSE queryResponse;
} DISPATCH_FANCTRL;

/* ------------------------- Defines --------------------------------------- */

/*!
 * Event type for notification from PMGR task.
 */
#define FANCTRL_EVENT_ID_PMGR_PWR_CHANNEL_QUERY_RESPONSE    \
                                    (DISP2UNIT_EVT__COUNT)

/*!
 * Utility macro for colwerting integer ramp rates from the RM into usable form
 * for PMU fan control.  Milliseconds to seconds, to F16.16, reciprocal, then
 * percent to fraction, gives pwm rate per second used by production code
 *
 * Note that input is usually LwU16.  However, this shouldn't have any problems
 * with colwersion to LwS32, as it won't do sign extension, as it knows the type
 * of the original.
 */
#define MSRATE_TO_SFXP16_16(p)                                                \
    (LW_TYPES_S32_TO_SFXP_X_Y_SCALED(16, 16, 1000, (p) * 100))

/*!
 * Number of retries for waiting for the GPU I2CS interface to be idle/ready for
 * a new command.
 */
#define RM_PMU_FAN_GEMINI_RETRY_COUNT                                         5

/* ------------------------ External Definitions --------------------------- */
/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
FLCN_STATUS fanCtrlDispatch(DISPATCH_FANCTRL *pDispFanctrl)
    GCC_ATTRIB_SECTION("imem_therm", "fanCtrlDispatch");

#endif // PMU_FANCTRL_H

