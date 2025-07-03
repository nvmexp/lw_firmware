
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_chnlmgmt.h
 * @brief  Channel management task that is responsible for managing the
 *         work coming for channels from host method.
 */


#ifndef __SOE_CHNLMGMT_H
#define __SOE_CHNLMGMT_H

#include "unit_dispatch.h"
#include "lwos_dma.h"
#include "soe_hostif.h"


#include <osptimer.h>

// TODO: rbhenwal ideally this should be fectched from Profiles.pm Need to check 
// channel interface method ID information
#define APP_METHOD_ARRAY_SIZE  (0x50)
#define APP_METHOD_MIN         (0x100) // # (0x400 >> 2)
#define APP_METHOD_MAX         (0x1CF) //# (0x73C >> 2)

/*!
 * Identifier used to represent a command sent to an individual app task.
 */
#define DISP2APP_EVT_METHOD     (0x00)
/*!
 * This is the generic structure that may be used to send any type of event to
 * an app task.
 */
typedef union
{
    LwU8 eventType;
} DISP2APP_EVT_CMD;

/*!
 * From dev_ram.h, _MODE oclwpies bit 2 of PTR_LO.
 */
#define ENGINE_CTX_STATE_MODE_MASK  (0x4)

/*!
 * From dev_ram.h, _TARGET oclwpies bits 0 and 1 of PTR_LO.
 */
#define ENGINE_CTX_STATE_TARGET_MASK (0x3)

/*!
 * From dev_ram.h, ignore bits 0, 1 and 2 of PTR_LO for address
 */
#define ENGINE_CTX_STATE_PTR_LO_MASK (0xFFFFFFF8)

/*!
 * Signal the channel management task that a method or ctxsw has completed
 * and it is safe to unmask interrupts and set the engine to idle.
 */
#define NOTIFY_EXELWTE_COMPLETE() notifyExelwteCompleted(LW_FALSE)

/*!
 * Signal the channel management task that a method or ctxsw encountered
 * a fault on a virtual DMA which must be handled by a handshake with RM.
 */
#define NOTIFY_EXELWTE_NACK() notifyExelwteCompleted(LW_TRUE)

/*!
 * Mask that indicates which app methods are lwrrently valid. This mask is
 * shared by the chnmgmt task and individual app tasks. Chnmgmt task will set
 * the valid masks when it sees methods coming in, and individual app tasks
 * will clear the mask when it handles them.
 */
typedef struct
{
    LwU32 mthdGrp[LW_CEIL(APP_METHOD_ARRAY_SIZE, 32)];
} SOE_APP_METHOD_VALID_MASK;

/*!
 * The following structure, which is initialized with known values, is saved
 * off at the beginning of the channel context  save area. It is used to
 * distinguish the very first context restore request which should be treated
 * as void, from any other context restore request. If this pattern was stored
 * in the context save area, we assume that there was a context save that
 * happened previously, else, we assume that this is the very first context
 * restore, and do nothing.
 *
 * We have tried to make the dummy structure large enough to avoid a "chance"
 * event that would accidently show up the same sequence of bytes in the
 * context restore area.
 */
typedef struct
{
    LwU32 fill1;
    LwU32 fill2;
    LwU32 fill3;
    LwU32 fill4;
} SOE_CTX_DUMMY_PATTERN;

/*!
 * Following defines are used to initialize a dummy pattern in the channel
 * context to know that a context save has happened there (to distinguish the
 * very first context restore request which should be treated as void, from any
 * other context restore request)
 */
#define ENGINE_CTX_DUMMY_PATTERN_FILL1 (0xF1F1F1F1)
#define ENGINE_CTX_DUMMY_PATTERN_FILL2 (0x2E2E2E2E)
#define ENGINE_CTX_DUMMY_PATTERN_FILL3 (0xD3D3D3D3)
#define ENGINE_CTX_DUMMY_PATTERN_FILL4 (0x4C4C4C4C)

/*!
 * This structure is used to indicate whether an exelwtion or context switch
 * has completed. When (nackReceived = LW_TRUE) a nack was received
 * due to a virtual DMA fault, and should be handled.
 */
typedef struct
{
    LwU8   eventType;
    LwBool bDmaNackReceived;
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
 * All the app-specific state belonging to the chnmgmt task that we need to
 * save/restore
 */
typedef struct
{
    /*!
    * Lwrrently (or last) active application ID if the EXECUTE method was received.
    */
    LwU32 appId;

    /*!
    * Lwrrently (or last) active application's watchdog timer value (in cycles).
    */
    LwU32 wdTmrVal;
} SOE_CTX_CHNMGMT;

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

extern sysTASK_DATA LwrtosQueueHandle SoeChnMgmtCmdDispQueue;

/*
 * @brief Send trivial ack for the pending CTXSW Intr request
 *
 */
void soeSendCtxSwIntrTrivialAck(void);
#endif
