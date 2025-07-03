/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_OBJEI_H
#define PMU_OBJEI_H

/*!
 * @file pmu_objei.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"

/* ------------------------ Application includes --------------------------- */
#include "config/g_ei_hal.h"
/* ------------------------ Macros ----------------------------------------- */

/*!
 * @brief Abort reasons for EI FSM Entry
 */
#define EI_ABORT_REASON_IDLE_FLIPPED  BIT(0)

// Get the EI Client object from the OBJEI
#define EI_GET_CLIENT(clientId)                     (Ei.pEiClient[clientId])

// Get the client support status
#define EI_IS_CLIENT_SUPPORTED(clientId)            ((Ei.clientSupportMask & BIT(clientId)) != 0)

// Get the Client's notification status
#define EI_IS_CLIENT_NOTIFICATION_ENABLED(clientId) ((Ei.clientNotificationEnableMask & BIT(clientId)) != 0)

// Get the EI Callback object from OBJEI
#define EI_GET_CALLBACK()                           (Ei.pEiCallback)

/*!
 * @brief EI Notification type for the EI Clients
 */
#define LPWR_EI_NOTIFICATION_NONE          0U
#define LPWR_EI_NOTIFICATION_ENTRY_DONE    1U
#define LPWR_EI_NOTIFICATION_EXIT_DONE     2U

/* ------------------------ External definitions --------------------------- */

typedef struct
{
    // basePerioudMs is the EI's Base callback period
    LwU32 basePeriodMs;

    // base counter
    LwU16 baseCount;

} EI_CALLBACK;

/*!
 * EI Client Object Defintion
 */
typedef struct
{
    //
    // Base multiplier from the client for notification delay
    // This will help to define the callback period for this client
    //
    // Callback Period = baseMultiplier * basePeriodMs
    //
    // basePerioudMs is the EI's Base callback period
    //
    LwU32 baseMultiplier;

    //
    // PMU Task Queue Id to which this client belongs
    // For RM based client, this filed is no op
    //
    LwU8 taskQueueId;

    // Client's Task Event Type to which task will respond
    LwU8 taskEventType;

    //
    // Client Target Type i.e if client is PMU based or RM based
    // LW_TRUE -> PMU based client
    // LW_FALSE -> RM Based client
    //
    // Based on this flag, we will send the notification to Client
    //
    LwBool  bIsPmuClient;
} EI_CLIENT;

/*!
 * EI object Definition
 */
typedef struct
{
    // Bit Mask of idle signals without non GR related signals.
    LwU32          idleMaskNonGr[RM_PMU_PG_IDLE_BANK_SIZE];

    // EI Framework related fields

    // Bitmask of all the clients supported by EI
    LwU32          clientSupportMask;

    // Bitmask of all the clients for which notification is enabled
    LwU32          clientNotificationEnableMask;

    // Bitmask of all the client for which entry notification is sent
    LwU32          clientEntryNotificationSentMask;

    //
    // Client Mask for which callback has to be scheduled
    // so that we can send the notification once callback
    // time is expired
    //
    LwU32          clientCallbackScheduleMask;

    // EI Client's structure to hold client information
    EI_CLIENT     *pEiClient[RM_PMU_LPWR_EI_CLIENT_ID__COUNT];

    // EI Central Callback Structure
    EI_CALLBACK   *pEiCallback;
} OBJEI;

extern OBJEI Ei;

/* ------------------------ Private Variables ------------------------------ */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS eiPostInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "eiPostInit");

void        eiInit(RM_PMU_RPC_STRUCT_LPWR_LOADIN_PG_CTRL_INIT *pParams)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "eiInit");

FLCN_STATUS eiClientInit(LwU8 clientId)
             GCC_ATTRIB_SECTION("imem_lpwrLoadin", "eiClientInit");

FLCN_STATUS eiProcessClientRequest(DISPATCH_LPWR_LP_EI_CLIENT_REQUEST *pEiClientRequest)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiProcessClientRequest");

FLCN_STATUS eiClientNotificationEnable(LwU8 clientId, LwBool bEnable)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiClientNotificationEnable");

FLCN_STATUS eiClientDeferredCallbackUpdate(LwU8 clientId, LwU32 callbackTimeMs)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiClientDeferredCallbackUpdate");

FLCN_STATUS eiProcessFsmEvents(DISPATCH_LPWR_LP_EI_FSM_IDLE_EVENT *pEiFsmIdleEvent)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiProcessFsmEvents");

FLCN_STATUS eiClientEntryNotificationSend(void)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiClientEntryNotificationSend");

FLCN_STATUS eiClientExitNotificationSend(void)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiClientExitNotificationSend");

FLCN_STATUS eiNotificationSend(LwU8 clientId, LwU8 notificationType)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiNotificationSend");

FLCN_STATUS eiCallbackParamsInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLoadin", "eiCallbackParamsInit");

void        eiCallbackSchedule(void)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiCallbackSchedule");

void        eiCallbackDeschedule(void)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiCallbackDeschedule");

void        eiPassiveInit(void)
            GCC_ATTRIB_SECTION("imem_lpwrLp", "eiPassiveInit")
            GCC_ATTRIB_NOINLINE();

#endif // PMU_OBJEI_H
