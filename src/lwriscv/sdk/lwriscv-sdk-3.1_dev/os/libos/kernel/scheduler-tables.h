/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#pragma once

#include <lwtypes.h>
#include "libos.h"
#include "include/libos_task_state.h"
#include "compressed-pointer.h"
#include "list.h"

typedef LwU8 Pasid;

typedef enum __attribute__ ((__packed__)) {
    TaskStateReady,
    TaskStateWait,
    TaskStateWaitTimeout, 
    TaskStateWaitTrapped       
} TaskState;

/**
 * @brief Waitable structure
 */
typedef struct
{
    ListHeader waitingReceivers;
    ListHeader waitingSenders;
} Port;

/**
 * @brief Port structure
 */
typedef struct
{
    Port        port;
    ListHeader  timerHeader;
    LwU64       timestamp;
    LwBool      pending;
} Timer;

typedef enum __attribute__ ((__packed__)) {
    ObjectGrantNone = 0,
    ObjectGrantWait = 1,               

    PortGrantSend   = 2,
    PortGrantAll    = PortGrantSend | ObjectGrantWait,

    TimerGrantSet   = 4,
    TimerGrantAll   = TimerGrantSet | ObjectGrantWait,

    LockGrantAll    = 16,

    ShuttleGrantAll = 8
} ObjectGrant;

typedef struct
{
    LwU16                  offset;  // relative to portArray 
    LibosTaskId            owner;
    ObjectGrant            grant;
    LibosPortId            id;
} ObjectTableEntry;

typedef enum __attribute__ ((__packed__)) {
    ShuttleStateReset,
    ShuttleStateCompleteIdle,
    ShuttleStateCompletePending,
    ShuttleStateSend,
    ShuttleStateRecv,
} ShuttleState;

// 32 bytes
typedef struct Shuttle
{
    union
    {
        // During completion
        // state == LIBOS_SHUTTLE_STATE_COMPLETE (intermediate)
        struct Shuttle *retiredPair;

        // state == LIBOS_SHUTTLE_STATE_COMPLETE (final)
        ListHeader shuttleCompletedLink;

        // state == ShuttleStateSend || state == ShutttleStateRecv
        ListHeader        portLink;
    };

    // state == ShuttleStateSend || state == ShutttleStateRecv
    LIBOS_PTR32(Port) waitingOnPort;

    union
    {
        // state == ShuttleStateSend
        LIBOS_PTR32(Shuttle) partnerRecvShuttle;

        // state == ShuttleStateRecv
        LIBOS_PTR32(Shuttle) grantedShuttle;

        // state == LIBOS_SHUTTLE_STATE_COMPLETE
        LIBOS_PTR32(Shuttle) replyToShuttle;
    };

    ShuttleState state;

    LibosTaskId  originTask;

    LwU8         errorCode;
    LibosTaskId  owner;

    LwU64        payloadAddress;
    LwU32        payloadSize;
    LibosPortId  taskLocalHandle;

} Shuttle;

typedef struct
{
    LwU64                     timestamp;
    ListHeader                schedulerHeader;
    ListHeader                timerHeader;
    ListHeader                shuttlesCompleted;

    Shuttle                   crashSendShuttle;
    Shuttle                   replayWaitShuttle;
    Port                      replayPort;

    TaskState                 taskState;

    LibosTaskId               id;

#if LIBOS_FEATURE_PRIORITY_SCHEDULER
    LibosPriority             priority;
#endif

#if LIBOS_FEATURE_PAGING
    Pasid                     pasid;
#endif

    LibosTaskState            state;

    Port                      portSynchronousReply;

#if LIBOS_FEATURE_PAGING
    LwU16                     pasidRead;
    LwU16                     pasidWrite;
#endif

#if !LIBOS_FEATURE_PAGING
    LwU64                     mpuEnables[2 /*(DRF_MASK(LW_RISCV_CSR_MMPUIDX_INDEX) + 1 + 63)/64*/];
#endif

    // WAIT
    LIBOS_PTR32(Shuttle)      shuttleSleeping;

    LwU64                     tableKey;
} Task;

typedef struct Lock
{
    Port        port;
    Shuttle     unlockShuttle;
    LwU8        holdCount;
    LibosTaskId holder;
} Lock;


// Large prime
#define LIBOS_HASH_MULTIPLIER     0x72777af862a1faeeULL

extern Task                 taskTable[];
extern Shuttle              shuttleTable[];
extern LwU8                 NotaddrShuttleTableMask;
extern ObjectTableEntry     objectTable[];
extern LwU8                 NotaddrPortTableMask;
extern LwU8                 portArray[];

#define LIBOS_XEPC_SENTINEL_NORMAL_TASK_EXIT 0ULL
