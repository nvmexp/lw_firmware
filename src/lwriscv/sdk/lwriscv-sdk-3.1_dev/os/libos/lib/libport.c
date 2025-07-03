/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libos.h"

// @see libos.h for API definition

void LibosCacheIlwalidate(void *base, LwU64 size)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_DCACHE_ILWALIDATE;
    register void *a0 __asm__("a0") = base;
    register LwU64 a1 __asm__("a1") = size;
    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0), "r"(a1) : "memory");
}

void LibosTaskYield(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_YIELD;
    // coverity[set_but_not_used]
    register LwU64 a0 __asm__("a0");
    __asm__ __volatile__("ecall" : "=r"(a0) : "r"(t0) : "memory");
}

LibosStatus LibosShuttleReset(LibosShuttleId shuttleIndex)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SHUTTLE_RESET;
    register LwU64 a0 __asm__("a0") = shuttleIndex;

    __asm__ __volatile__("ecall" : "+r"(a0) : "r"(t0) : "memory");

    return (LibosStatus)a0;
}

LibosStatus LibosPortSendRecvAndWait(
    LibosShuttleId sendShuttle,     // A0
    LibosPortId    sendPort,        // A1
    void *         sendPayload,     // A2
    LwU64          sendPayloadSize, // A3

    LibosShuttleId recvShuttle,     // A4
    LibosPortId    recvPort,        // A5
    void *         recvPayload,     // A6
    LwU64          recvPayloadSize, // A7

    LibosShuttleId waitId,                // T1
    LibosShuttleId *completedShuttle,      // A6 [out]
    LwU64 *        completedSize,         // A4 [out]
    LibosTaskId *  completedRemoteTaskId, // A5 [out]
    LwU64          timeoutNs              // T2
)
{
    // IOCTL marker
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT;

    // Send
    register LwU64 a0 __asm__("a0") = sendShuttle;
    register LwU64 a1 __asm__("a1") = sendPort;
    register LwU64 a2 __asm__("a2") = (LwU64)sendPayload;
    register LwU64 a3 __asm__("a3") = sendPayloadSize;

    // Recv
    register LwU64 a4 __asm__("a4")   = recvShuttle;
    register LwU64 a5 __asm__("a5")   = recvPort;
    register LwU64 a6 __asm__("a6")   = (LwU64)recvPayload;
    register LwU64 a7 __asm__("a7")   = recvPayloadSize;

    // Wait
    register LwU64 t1 __asm__("t1") = waitId;
    register LwU64 t2 __asm__("t2") = timeoutNs;

    __asm__ __volatile__("ecall"
                         : "+r"(a0), "+r"(a4), "+r"(a5), "+r"(a6)
                         : "r"(t0), "r"(t1), "r"(t2), "r"(a1), "r"(a2), "r"(a3), "r"(a7)
                         : "memory");

    if (completedShuttle)
        *completedShuttle = a6;
    if (completedRemoteTaskId)
        *completedRemoteTaskId = a5;
    if (completedSize)
        *completedSize = (LwU64)a4;
    return (LibosStatus)a0;
}

LibosStatus LibosPortAsyncSend(
    LibosShuttleId sendShuttle, LibosPortId sendPort, void *sendPayload, LwU64 sendPayloadSize)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        0, 0, 0, 0, 0);

    return status;
}

LibosStatus LibosPortSyncSend(
    LibosPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        ID(shuttleSyncSend), sendPort, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        ID(shuttleSyncSend), 0, completedSize, 0, timeout);
    if (status == LibosErrorTimeout)
        LibosShuttleReset(ID(shuttleSyncSend));
    return status;
}

// @todo Document that reply that only be sent to a combined SendRecv
//       Since the water is guaranteed to already be waiting -- the operation is guaranteed not to
//       wait.
LibosStatus LibosPortAsyncReply(
    LibosShuttleId sendShuttle, // must be the shuttle we received the message on
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, 0, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        sendShuttle, 0, completedSize, 0, 0 /* Operation cannot block */);
    if (status == LibosErrorTimeout)
        LibosShuttleReset(sendShuttle);
    return status;
}

LibosStatus LibosPortSyncReply(
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize)
{
    return LibosPortAsyncReply(ID(shuttleSyncRecv), sendPayload, sendPayloadSize, completedSize);
}


LibosStatus LibosPortSyncRecv(
    LibosPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        0, 0, 0, 0, ID(shuttleSyncRecv), recvPort, recvPayload, recvPayloadCapacity, ID(shuttleSyncRecv), 0,
        completedSize, 0, timeout);
    if (status == LibosErrorTimeout)
        LibosShuttleReset(ID(shuttleSyncRecv));
    return status;
}

LibosStatus LibosPortAsyncRecv(
    LibosShuttleId recvShuttle, LibosPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        0, 0, 0, 0, recvShuttle, recvPort, recvPayload, recvPayloadCapacity,
        0, 0, 0, 0, 0);
    return status;
}

LibosStatus LibosPortSyncSendRecv(
    LibosPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        ID(shuttleSyncSend), sendPort, sendPayload, sendPayloadSize,

        // Recv
        ID(shuttleSyncRecv), recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        ID(shuttleSyncRecv), 0, completedSize, 0, timeout);

    if (status == LibosErrorTimeout)
    {
        LibosShuttleReset(ID(shuttleSyncSend));
        LibosShuttleReset(ID(shuttleSyncRecv));
    }
    return status;
}

LibosStatus LibosPortAsyncSendRecv(
    LibosShuttleId sendShuttle, LibosPortId sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosShuttleId recvShuttle, LibosPortId recvPort, void *recvPayload, LwU64 recvPayloadCapacity)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        recvShuttle, recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        0, 0, 0, 0, 0);
    return status;
}

LibosStatus LibosPortWait(
    LibosShuttleId waitShuttle,
    LibosShuttleId *completedShuttle,      // A5
    LwU64 *completedSize,                  // T2
    LibosTaskId *completedRemoteTaskId,    // T1
    LwU64 timeoutNs                        // A6
)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        0, 0, 0, 0,

        // Recv
        0, 0, 0, 0,

        // Wait
        waitShuttle, completedShuttle, completedSize, completedRemoteTaskId, timeoutNs);
    return status;
}


void LibosCriticalSectionLeave(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_CRITICAL_SECTION_LEAVE;
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");    
}

void LibosCriticalSectionEnter(LibosCriticalSectionBehavior behavior)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_CRITICAL_SECTION_ENTER;
#if LIBOS_FEATURE_USERSPACE_TRAPS
    register LwU64 a0 __asm__("a0") = behavior;
#endif    
    __asm__ __volatile__(
        "ecall" 
    :: "r"(t0)
#if LIBOS_FEATURE_USERSPACE_TRAPS  
       , "r"(a0) 
#endif
       : "memory");    
}
