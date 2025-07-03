/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_CHNMGMT_H
#define SEC2_CHNMGMT_H

#include "sec2_hostif.h"
#include "unit_dispatch.h"

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/*!
 * @brief Defines for types of events to chnmgmt
 */
enum
{
    DISP2CHNMGMT_EVT_CTXSW = 0,   // Context switch event
    DISP2CHNMGMT_EVT_METHOD,      // Methods in FIFO
    DISP2CHNMGMT_EVT_COMPLETION,  // Completion command from application tasks
    DISP2CHNMGMT_EVT_CMD,         // Command from RM
    DISP2CHNMGMT_EVT_WDTMR,       // Watchdog Timer
};

/*!
 * @brief Iterates over the valid (set) bits of the mask.
 *
 * @param [out] iter    An integer lvalue used to iterate over the bits in the
 *                      provided mask.
 * @param [in]  mask    A bitmask of type @ref SEC2_APP_METHOD_VALID_MASK
 *                      to iterate over.
 */
#define FOR_EACH_VALID_METHOD(iter, mask)                                   \
    for ((iter) = 0; (iter) < APP_METHOD_ARRAY_SIZE; (iter)++)              \
        if (((mask).mthdGrp[(iter) / 32] & BIT((iter) % 32)) != 0)

/*!
 * Signal the channel management task that a method or ctxsw has completed
 * and it is safe to unmask interrupts and set the engine to idle.
 */
#define NOTIFY_EXELWTE_COMPLETE() notifyExelwteCompleted(LW95A1_ERROR_NONE)

/*!
 * Signal the channel management task that a method or ctxsw encountered
 * a fault on a virtual DMA which must be handled by a handshake with RM.
 */
#define NOTIFY_EXELWTE_NACK() notifyExelwteCompleted(LW95A1_ERROR_DMA_NACK)

/*!
 * Signal the channel management task that a method or ctxsw encountered
 * an error during method processing which must be handled by a handshake with RM.
 */
#define NOTIFY_EXELWTE_TRIGGER_RC(error) notifyExelwteCompleted(error)

/*!
 * Identifier used to represent a command sent to an individual app task.
 */
#define DISP2APP_EVT_METHOD     (0x00)

/* ------------------------ Types definitions ------------------------------ */
/*!
 * This structure is used to indicate whether an exelwtion or context switch
 * has completed. When (nackReceived = LW_TRUE) a nack was received
 * due to a virtual DMA fault, and should be handled.
 */
typedef struct
{
    LwU8   eventType;
    LwU32  classErrorCode;
} DISP2CHNMGMT_COMPLETION;

/*!
 * This is the generic structure that may be used to send any type of event to
 * the command/channel dispatcher.
 */
typedef union
{
    LwU8                    eventType;
    DISP2CHNMGMT_COMPLETION completionEvent;
    DISP2UNIT_CMD           cmd;
} DISPATCH_CHNMGMT;

/*!
 * Mask that indicates which app methods are lwrrently valid. This mask is 
 * shared by the chnmgmt task and individual app tasks. Chnmgmt task will set 
 * the valid masks when it sees methods coming in, and individual app tasks 
 * will clear the mask when it handles them. 
 */
typedef struct
{
    LwU32 mthdGrp[LW_CEIL(APP_METHOD_ARRAY_SIZE, 32)];
} SEC2_APP_METHOD_VALID_MASK;

/*!
 * This is the generic structure that may be used to send any type of event to
 * an app task.
 */
typedef union
{
    LwU8 eventType;
} DISP2APP_EVT_CMD;

/* ------------------------ External definitions --------------------------- */
extern LwU32 appMthdArray[APP_METHOD_ARRAY_SIZE];
extern SEC2_APP_METHOD_VALID_MASK appMthdValidMask;

/* ------------------------ Function Prototypes ---------------------------- */
void notifyExelwteCompleted(LwU32 completionCode)
    GCC_ATTRIB_SECTION("imem_resident", "_notifyExelwteCompleted");

/*
 * @brief Send trivial ack for the pending CTXSW Intr request
 *
 */
void sec2SendCtxSwIntrTrivialAck(void)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "_sec2SendCtxSwIntrTrivialAck");

/*!
 * Send an event to RM to trigger RC recovery and notify ourselves that the
 * task faulted so that we can do our own recover process.
 */
void RCRecoverySendTriggerEvent(LwU32 rcErrorCode)
    GCC_ATTRIB_SECTION("imem_chnmgmt", "RCRecoverySendTriggerEvent");

#endif // SEC2_CHNMGMT_H
