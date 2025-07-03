/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_PORT_H
#define LIBOS_PORT_H

#include "kernel.h"
#include "compressed-pointer.h"
#include "list.h"
#include "libos.h"
#include "mm/objectpool.h"
#if !LIBOS_TINY
#include "librbtree.h"
#endif // !LIBOS_TINY

/**
 * @brief Waitable structure
 */
struct Port
{
    ListHeader waitingReceivers;
    ListHeader waitingSenders;

#if !LIBOS_TINY
    LwU32           globalPort;
    LibosTreeHeader globalPortMapNode;
    LwBool          globalPortLost;
#endif // !LIBOS_TINY
};


typedef enum __attribute__ ((__packed__)) {
    ShuttleStateReset,
    ShuttleStateCompleteIdle,
    ShuttleStateCompletePending,
    ShuttleStateSend,
    ShuttleStateRecv,
} ShuttleState;


#define SHUTTLE_MAX_OBJECTS 8

#define LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER      1
#define LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS        2
#define LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES        4
#define LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID 8

// 32 bytes
struct Shuttle
{
    union
    {
        // During completion
        // state == ShuttleStateCompletePending (intermediate)
        BORROWED_PTR struct Shuttle *retiredPair;

        // state == ShuttleStateCompleteIdle (final)
        ListHeader shuttleCompletedLink;
    };

    // state == ShuttleStateSend || state == ShuttleStateRecv
    ListHeader portLink;

    // state == ShuttleStateSend || state == ShuttleStateRecv
    BORROWED_PTR Port *waitingOnPort;

    union
    {
        // state == ShuttleStateSend
        BORROWED_PTR LIBOS_PTR32(struct Shuttle) partnerRecvShuttle;

        // state == ShuttleStateRecv
        BORROWED_PTR LIBOS_PTR32(struct Shuttle) grantedShuttle;

        // state == ShuttleStateCompleteIdle
        BORROWED_PTR LIBOS_PTR32(struct Shuttle) replyToShuttle;
    };

    ShuttleState state;

    LwU8         errorCode;
#if !LIBOS_TINY
    struct Task* owner;
#else
    LibosTaskHandle  owner;
#endif

    LwU64            payloadAddress;
    LwU32            payloadSize;
    LibosPortHandle  taskLocalHandle;

#if !LIBOS_TINY
    LwU8             copyFlags;
    LwU8             targetPartition; // LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID
#endif
};


// SYSCALL 
LibosStatus PortSyscallSendRecvWait(
    LibosShuttleHandle sendShuttleId, LibosPortHandle sendPortId, LwU64 sendPayload, LwU64 sendPayloadSize,
    LibosShuttleHandle recvShuttleId, LibosPortHandle replyPortId, LwU64 recvPayload, LwU64 recvPayloadSize);


LibosStatus PortSendRecvWait(
    BORROWED_PTR Shuttle * sendShuttle, 
    BORROWED_PTR Port * sendPort, 
    BORROWED_PTR Shuttle * recvShuttle,  
    BORROWED_PTR Port * recvPort,
    LwU64 newTaskState, 
    BORROWED_PTR Shuttle * waitShuttle, 
    LwU64 waitTimeout,
    LwU64 flags);


void KernelPortDestroy(Port * port);

LwBool LIBOS_SECTION_IMEM_PINNED ShuttleIsIdle(Shuttle * shuttle);
void LIBOS_SECTION_IMEM_PINNED KernelSyscallShuttleReset(Shuttle * resetShuttle);

void LIBOS_SECTION_IMEM_PINNED ShuttleBindSend(Shuttle * sendShuttle, LwU8 copyFlags, LwU64 sendPayload, LwU64 sendPayloadSize);
void LIBOS_SECTION_IMEM_PINNED ShuttleBindGrant(Shuttle * sendShuttle, Shuttle * recvShuttle);
void LIBOS_SECTION_IMEM_PINNED ShuttleBindRecv(Shuttle * recvShuttle, LwU8 copyFlags, LwU64 recvPayload, LwU64 recvPayloadSize);
LIBOS_SECTION_IMEM_PINNED void ShuttleRetireSingle(Shuttle *shuttle);

LwBool PortHasSender(Port * port);
LwBool PortHasReceiver(Port * port);
Shuttle * PortDequeSender(Port * port);
Shuttle * PortDequeReceiver(Port * port);
void      PortEnqueSender(Port * port, Shuttle * sender);
void      PortEnqueReceiver(Port * port, Shuttle * receiver);
LibosStatus KernelCopyMessage(BORROWED_PTR Shuttle * recvShuttle, BORROWED_PTR Shuttle * sendShuttle, LwU64 size);
void KernelShuttleResetAndLeaveObjects(Shuttle * resetShuttle);
LibosStatus PortPortDetachObject(
  Shuttle           *shuttle,
  LwU64              permissionRequired,
  void             **outObject);
LibosStatus PortPortAttachObject(
  Shuttle            *shuttle,
  void               *object,
  LwU64               permissionGrant);

#if LIBOS_TINY
extern LwU8                 NotaddrShuttleTableMask;
extern Shuttle              shuttleTable[];
extern LwU8                 NotaddrPortTableMask;
extern LwU8                 portArray[];
#endif

#endif
