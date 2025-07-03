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
#include "scheduler-tables.h"

void * ObjectFindOrRaiseErrorToTask(LibosPortId id, LwU64 grant);

typedef enum {
    LibosPortLockNone      = 0U,
    LibosPortLockOperation = 1U,
} PortSendRecvWaitFlags;

// SYSCALL 
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN PortSyscallSendRecvWait(
    LibosShuttleId sendShuttleId, LibosPortId sendPortId, LwU64 sendPayload, LwU64 sendPayloadSize,
    LibosShuttleId recvShuttleId, LibosPortId replyPortId, LwU64 recvPayload, LwU64 recvPayloadSize);


void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN PortSendRecvWait(
    Shuttle * sendShuttle, Port * sendPort, Shuttle * recvShuttle,  Port * recvPort, LwU64 newTaskState, 
    Shuttle * waitShuttle, LwU64 waitTimeout, PortSendRecvWaitFlags flags);

LwBool LIBOS_SECTION_IMEM_PINNED ShuttleIsIdle(Shuttle * shuttle);
void LIBOS_SECTION_IMEM_PINNED ShuttleReset( Shuttle * resetShuttle);

void LIBOS_SECTION_IMEM_PINNED ShuttleBindSend(Shuttle * sendShuttle, LwU64 sendPayload, LwU64 sendPayloadSize);
void LIBOS_SECTION_IMEM_PINNED ShuttleBindGrant(Shuttle * sendShuttle, Shuttle * recvShuttle);
void LIBOS_SECTION_IMEM_PINNED ShuttleBindRecv(Shuttle * recvShuttle, LwU64 recvPayload, LwU64 recvPayloadSize);
LIBOS_SECTION_IMEM_PINNED void ShuttleRetireSingle(Shuttle *shuttle);


#endif
