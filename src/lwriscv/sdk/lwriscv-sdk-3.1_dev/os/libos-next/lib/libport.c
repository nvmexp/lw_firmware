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

void LibosTaskYield(void)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_YIELD;
#else
    extern   LibosStatus KernelSchedulerPreempt();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSchedulerPreempt;
#endif    

register LwU64 a0 __asm__("a0");
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "=r"(a0) : "r"(t0) : "memory");
}

LibosStatus LibosShuttleReset(LibosShuttleHandle shuttleIndex)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_SHUTTLE_RESET;
#else
    extern   LibosStatus ShuttleSyscallReset();
    register LwU64 t0 __asm__("t0") = (LwU64)ShuttleSyscallReset;
#endif    

    register LwU64 a0 __asm__("a0") = shuttleIndex;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0) : "memory");

    return (LibosStatus)a0;
}

LibosStatus LibosPortSendRecvAndWait(
    LibosShuttleHandle sendShuttle,     // A0
    LibosPortHandle    sendPort,        // A1
    void *             sendPayload,     // A2
    LwU64              sendPayloadSize, // A3

    LibosShuttleHandle recvShuttle,     // A4
    LibosPortHandle    recvPort,        // A5
    void *             recvPayload,     // A6
    LwU64              recvPayloadSize, // A7

    LibosShuttleHandle waitId,                // T1
    LibosShuttleHandle *completedShuttle,     // A6 [out]
    LwU64 *            completedSize,         // A4 [out]
    LwU64              timeoutNs,              // T2
    LwU64              flags                // T3
)
{
    // IOCTL marker
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PORT_SEND_RECV_WAIT;
#else
    extern   LibosStatus PortSyscallSendRecvWait();
    register LwU64 t0 __asm__("t0") = (LwU64)PortSyscallSendRecvWait;
#endif
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
    register LwU64 t3 __asm__("t3") = flags;

    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION
                         : "+r"(a0), "+r"(a4), "+r"(a5), "+r"(a6)
                         : "r"(t0), "r"(t1), "r"(t2), "r"(t3), "r"(a1), "r"(a2), "r"(a3), "r"(a7)
                         : "memory");

    if (completedShuttle)
        *completedShuttle = a6;
    if (completedSize)
        *completedSize = (LwU64)a4;
    return (LibosStatus)a0;
}

LibosStatus LibosPortAsyncSend(
    LibosShuttleHandle sendShuttle, LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        0, 0, 0, 0,
        
        //Flags
        flags);

    return status;
}

LibosStatus LibosPortSyncSend(
    LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        LibosShuttleSyncSend, sendPort, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        LibosShuttleSyncSend, 0, completedSize, timeout,
        
        //Flags
        flags);
    if (status == LibosErrorTimeout)
        LibosShuttleReset(LibosShuttleSyncSend);
    return status;
}

// @todo Document that reply that only be sent to a combined SendRecv
//       Since the water is guaranteed to already be waiting -- the operation is guaranteed not to
//       wait.
LibosStatus LibosPortAsyncReply(
    LibosShuttleHandle sendShuttle, // must be the shuttle we received the message on
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, 0, sendPayload, sendPayloadSize,

        // Recv
        0, 0, 0, 0,

        // Wait
        sendShuttle, 0, completedSize, 0 /* Operation cannot block */,
        
        //Flags
        flags
        );
    if (status == LibosErrorTimeout)
        LibosShuttleReset(sendShuttle);
    return status;
}

LibosStatus LibosPortSyncReply(
    void *sendPayload, LwU64 sendPayloadSize, LwU64 *completedSize, LwU64 flags)
{
    return LibosPortAsyncReply(LibosShuttleSyncRecv, sendPayload, sendPayloadSize, completedSize, flags);
}


LibosStatus LibosPortSyncRecv(
    LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        0, 0, 0, 0, LibosShuttleSyncRecv, recvPort, recvPayload, recvPayloadCapacity, LibosShuttleSyncRecv, 0,
        completedSize, timeout, flags);
    if (status == LibosErrorTimeout)
        LibosShuttleReset(LibosShuttleSyncRecv);
    return status;
}

LibosStatus LibosPortAsyncRecv(
    LibosShuttleHandle recvShuttle, LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        0, 0, 0, 0, recvShuttle, recvPort, recvPayload, recvPayloadCapacity,
        0, 0, 0, 0,
        flags);
    return status;
}

LibosStatus LibosPortSyncSendRecv(
    LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity,
    LwU64 *completedSize, LwU64 timeout, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        LibosShuttleSyncSend, sendPort, sendPayload, sendPayloadSize,

        // Recv
        LibosShuttleSyncRecv, recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        LibosShuttleSyncRecv, 0, completedSize, timeout,
        
        //Flags
        flags
        );

    if (status == LibosErrorTimeout)
    {
        LibosShuttleReset(LibosShuttleSyncSend);
        LibosShuttleReset(LibosShuttleSyncRecv);
    }
    return status;
}

LibosStatus LibosPortAsyncSendRecv(
    LibosShuttleHandle sendShuttle, LibosPortHandle sendPort, void *sendPayload, LwU64 sendPayloadSize,
    LibosShuttleHandle recvShuttle, LibosPortHandle recvPort, void *recvPayload, LwU64 recvPayloadCapacity, LwU64 flags)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        sendShuttle, sendPort, sendPayload, sendPayloadSize,

        // Recv
        recvShuttle, recvPort, recvPayload, recvPayloadCapacity,

        // Wait
        0, 0, 0, 0,
        
        //Flags
        flags);
    return status;
}

LibosStatus LibosPortWait(
    LibosShuttleHandle waitShuttle,
    LibosShuttleHandle *completedShuttle,      // A5
    LwU64 *completedSize,                  // T2
    LwU64 timeoutNs                        // A6
)
{
    LibosStatus status = LibosPortSendRecvAndWait(
        // Send
        0, 0, 0, 0,

        // Recv
        0, 0, 0, 0,

        // Wait
        waitShuttle, completedShuttle, completedSize, timeoutNs,
        
        //Flags
        0);
    return status;
}

#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
void LibosCriticalSectionLeave(void)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_CRITICAL_SECTION_LEAVE;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(t0) : "memory");    
}

void LibosCriticalSectionEnter()
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_CRITICAL_SECTION_ENTER;
    __asm__ __volatile__(
        LIBOS_SYSCALL_INSTRUCTION 
    :: "r"(t0)
       : "memory");    
}
#endif

__attribute__((noreturn,used)) void LibosTaskExit(LwU64 exitCode)
{
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_EXIT;
#else
    extern   LibosStatus KernelTaskPanic();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelTaskPanic;
#endif

    register LwU64 a0 __asm__("a0") = exitCode;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(t0), "r"(a0) : "memory");
    __builtin_unreachable();
}

LibosStatus LibosInterruptAsyncRecv(LibosShuttleHandle interruptShuttle, LibosPortHandle interruptPort, LwU32 interruptWaitingMask)
{
    // Queue a receive on the interrupt port
    LibosStatus status = LibosPortAsyncRecv(interruptShuttle, interruptPort, 0, 0, 0);
    if (status != LibosOk)
        return status;

    // Reset and clear any prior interrupts to avoid race where we miss the interrupt
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL;
#else
    extern   LibosStatus KernelSyscallExternalInterruptArm();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallExternalInterruptArm;
#endif

    register LwU64 a0 __asm__("a0") = interruptWaitingMask;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(t0), "r"(a0) : "memory");

    return LibosOk;
}

#if !LIBOS_TINY

LibosStatus LibosHandleClose(LibosHandle handle)
{
    // Reset and clear any prior interrupts to avoid race where we miss the interrupt
#ifndef LIBOS_SYSCALL_DIRECT_DISPATCH
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_HANDLE_CLOSE;
#else
    extern   LibosStatus KernelSyscallHandleClose();
    register LwU64 t0 __asm__("t0") = (LwU64)KernelSyscallHandleClose;
#endif
    register LwU64 a0 __asm__("a0") = handle;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : "+r"(a0) : "r"(t0) : "memory");
    return a0;
}

LibosStatus LibosTaskCreate(
    LibosTaskHandle       * handle,
    LibosPriority           priority, 
    LibosAddressSpaceHandle addressSpace, 
    LibosMemorySetHandle    memorySet,
    LwU64                   entryPoint
)
{
    LibosSchedulerTaskCreate message;
    LwU64 returned = 0;
    message.messageId = LibosCommandSchedulerTaskCreate;
    message.handleCount = 2;
    message.handles[0].handle = addressSpace;
    message.handles[0].grant  = LibosGrantAddressSpaceAll;
    message.handles[1].handle = memorySet;
    message.handles[1].grant  = LibosGrantMemorySetAll;
    message.priority = priority;
    message.entryPoint = entryPoint;
    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;
    *handle = message.handles[0].handle;
    return message.status;

}

LibosStatus LibosPortCreate(
    LibosPortHandle * handle)
{
    LibosSchedulerPortCreate message;
    LwU64 returned = 0;
    message.messageId = LibosCommandSchedulerPortCreate;
    message.handleCount = 0;
    LibosStatus status = LibosPortSyncSendRecv(LibosKernelServer, &message, sizeof(message), 0, &message, sizeof(message), &returned, LibosTimeoutInfinite, LibosPortFlagTransferHandles);
    if (status != LibosOk)
        return status;
    *handle = message.handles[0].handle;
    return message.status;
}

#endif
