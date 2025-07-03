/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "task.h"
#include "../include/libos.h"
#include "kernel.h"
#include "libos.h"
#include "libos_syscalls.h"
#include "memory.h"
#include "port.h"
#include "libriscv.h"
#include "timer.h"
#include <lwtypes.h>
#include "diagnostics.h"
#include "scheduler.h"
#include "mm.h"
#include "lock.h"

/**
 *
 * @file port.c
 *
 * @brief Asynchronous Inter-process Communication
 */

LibosShuttleNameDecl(shuttleAny);

void * KernelCopyMessage(void *destination, const void *source, LwU64 n)
{
    LwU8 * dest = (LwU8 *)destination;
    LwU8 * destEnd = dest + n;
    LwU8 const * src = (LwU8 const *)source;

    // Enable all MPU entries to allow kernel to touch both processes address spaces
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  -1ULL);
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  -1ULL);
    
    // Copy remaining bytes.
    while (dest != destEnd)
        *dest++ = *src++;

    // Restore permissions
    Task * KernelInterruptedTask = KernelSchedulerGetTask();
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[0]);
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[1]);

    return dest;
}

LwBool LIBOS_SECTION_IMEM_PINNED ShuttleIsIdle(Shuttle * shuttle)
{
    return shuttle->state < ShuttleStateSend;
}

static LIBOS_SECTION_IMEM_PINNED void
ShuttleWriteSyscallRegisters(Task *owner, Shuttle *completedShuttle)
{
    KernelTrace("    ShuttleWriteSyscallRegisters(task: %d, completedShuttle: %d)\n", owner->id, completedShuttle->taskLocalHandle);
    
    // Only write the completion registers if it was an explicit syscall
    if (KernelSchedulerIsTaskTrapped(owner))
        return;

    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0]           = completedShuttle->errorCode;
    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE] = completedShuttle->payloadSize;

    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK] = completedShuttle->originTask;
    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE] = completedShuttle->taskLocalHandle;
}

void LIBOS_SECTION_IMEM_PINNED KernelPortTimeout(Task *owner)
{
    KernelTrace("    KernelPortTimeout(task: %d)\n", owner->id);

    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LibosErrorTimeout;
    KernelSchedulerRun(owner);
}

static void LIBOS_SECTION_IMEM_PINNED PortUnlinkReplyCredentials(Shuttle *resetShuttle)
{
    if (resetShuttle->replyToShuttle)
        LIBOS_PTR32_EXPAND(Shuttle, resetShuttle->replyToShuttle)->grantedShuttle = 0;
}

static void LIBOS_SECTION_IMEM_PINNED PortUnlinkRemoteReplyCredentials(Shuttle *resetShuttle)
{
    if (resetShuttle->grantedShuttle)
        LIBOS_PTR32_EXPAND(Shuttle, resetShuttle->grantedShuttle)->replyToShuttle = 0;
}

LIBOS_SECTION_IMEM_PINNED void ShuttleRetireSingle(Shuttle *shuttle)
{
    Task *owner = &taskTable[shuttle->owner];

    KernelTrace("KERNEL: ShuttleRetireSingle %d\n",KernelSchedulerIsTaskWaiting(owner));
    if (KernelSchedulerIsTaskWaiting(owner) &&
        (LIBOS_PTR32_EXPAND(Shuttle, owner->shuttleSleeping) == shuttle ||
         !owner->shuttleSleeping))
    {
        shuttle->state = ShuttleStateCompleteIdle;

        ShuttleWriteSyscallRegisters(owner, shuttle);

        KernelSchedulerRun(owner);
    }
    else
    {
        shuttle->shuttleCompletedLink.prev = LIBOS_PTR32_NARROW(&owner->shuttlesCompleted);
        shuttle->shuttleCompletedLink.next = LIBOS_PTR32_NARROW(owner->shuttlesCompleted.next);
        ListLink(&shuttle->shuttleCompletedLink);

        shuttle->state = ShuttleStateCompletePending;
    }
}

// @note Either this was an asynchronous operation (and we return to self), or self was one of the
// waiters
LIBOS_SECTION_IMEM_PINNED void ShuttleRetirePair(Shuttle *source, Shuttle *sink)
{
    sink->originTask = source->owner;

    PortUnlinkRemoteReplyCredentials(sink);

    //
    //  Did we just receive a message from a source that's a sendrecv
    //
    Shuttle *partneredWaiter = LIBOS_PTR32_EXPAND(Shuttle, source->partnerRecvShuttle);
    if (partneredWaiter && partneredWaiter->state == ShuttleStateRecv)
    {
        sink->replyToShuttle            = LIBOS_PTR32_NARROW(partneredWaiter);
        partneredWaiter->grantedShuttle = LIBOS_PTR32_NARROW(sink);
    }
    else
        sink->replyToShuttle = 0;
    source->replyToShuttle = 0;

    ShuttleRetireSingle(source);
    ShuttleRetireSingle(sink);
}

void LIBOS_NORETURN PortContinueSend(Shuttle *sendShuttle, Port *sendPort);
void LIBOS_NORETURN PortContinueRecv();
void LIBOS_NORETURN PortContinueWait();

void LIBOS_SECTION_IMEM_PINNED ShuttleBindSend(Shuttle * sendShuttle, LwU64 sendPayload, LwU64 sendPayloadSize)
{
    KernelTrace("    ShuttleBindSend(shuttle:%llx, size:%llu)\n", sendShuttle->taskLocalHandle, sendPayloadSize);
    KernelAssert(!sendShuttle || ShuttleIsIdle(sendShuttle));

    // coverity[var_deref_model]
    ShuttleReset(sendShuttle);

    // Fill the sendShuttleId slot out
    sendShuttle->state          = ShuttleStateSend;
    sendShuttle->payloadAddress = sendPayload;
    sendShuttle->payloadSize    = sendPayloadSize;
    sendShuttle->errorCode      = LibosOk;
}

void LIBOS_SECTION_IMEM_PINNED ShuttleBindGrant(Shuttle * sendShuttle, Shuttle * recvShuttle)
{
    KernelTrace("    ShuttleBindGrant(sendShuttle:%llx, recvShuttle:%llx)\n", sendShuttle->taskLocalHandle, !recvShuttle ? 0 : recvShuttle->taskLocalHandle);    
    sendShuttle->partnerRecvShuttle = LIBOS_PTR32_NARROW(recvShuttle);
}

void LIBOS_SECTION_IMEM_PINNED ShuttleBindRecv(Shuttle * recvShuttle, LwU64 recvPayload, LwU64 recvPayloadSize)
{
    KernelTrace("    ShuttleBindRecv(shuttle:%llx, size:%llu)\n", recvShuttle->taskLocalHandle, recvPayloadSize);
    KernelAssert(!recvShuttle || ShuttleIsIdle(recvShuttle));

    // coverity[var_deref_model]
    ShuttleReset(recvShuttle);

    // Fill the shuttle_id slot out
    recvShuttle->state           = ShuttleStateRecv;
    recvShuttle->grantedShuttle = 0;
    recvShuttle->payloadAddress = recvPayload;
    recvShuttle->payloadSize    = recvPayloadSize;
    recvShuttle->errorCode      = LibosOk;
}


void PortEnqueSender(Port * sendPort, Shuttle * sendShuttle)
{
    KernelAssert(sendShuttle->state == ShuttleStateSend);
    ListPushBack(&sendPort->waitingSenders, &sendShuttle->portLink);
}

void PortEnqueReceiver(Port * recvPort, Shuttle * recvShuttle)
{
    KernelAssert(recvShuttle->state == ShuttleStateRecv);
    ListPushBack(&recvPort->waitingReceivers, &recvShuttle->portLink);
}

LwBool PortHasSender(Port * port)
{
    return !ListEmpty(&port->waitingSenders);
}

LwBool PortHasReceiver(Port * port)
{
    return !ListEmpty(&port->waitingReceivers);
}

Shuttle * PortDequeSender(Port * port)
{
    KernelAssert(PortHasSender(port));
    Shuttle *sendShuttle = LIST_POP_FRONT(port->waitingSenders, Shuttle, portLink);
    KernelAssert(sendShuttle->state == ShuttleStateSend);
    return sendShuttle;
}

Shuttle * PortDequeReceiver(Port * port)
{
    KernelAssert(PortHasReceiver(port)); 
    Shuttle *recvShuttle = LIST_POP_FRONT(port->waitingReceivers, Shuttle, portLink);
    KernelAssert(recvShuttle->state == ShuttleStateRecv);
    return recvShuttle;
}


// @todo: Split into smaller functions for building out a port (due to register pressure)
// @precondition: sendShuttle is idle, recvShuttle is idle, recvPort has no waiters.
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN PortSendRecvWait(
    Shuttle * sendShuttle, Port * sendPort, Shuttle * recvShuttle,  Port * recvPort,
    LwU64 newTaskState, Shuttle * waitShuttle, LwU64 waitTimeout, PortSendRecvWaitFlags flags)
{

    /*
     * Wait buildout
     */
    KernelSchedulerGetTask()->shuttleSleeping = LIBOS_PTR32_NARROW(waitShuttle);
    if (newTaskState == TaskStateWaitTimeout) 
        KernelSchedulerWait(KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] + LibosTimeNs());
    else if (newTaskState != TaskStateReady)
        KernelSchedulerWait(0);

    /*
     * Send Shuttle Queue or Copy
     */
    if (sendShuttle)
    {
        /*
         *  Is there a waiting receiver? We must copy immediately
         *  otherwise, we just queue everything.
         */
        if (PortHasReceiver(sendPort))
        {
            Shuttle *waiter = PortDequeReceiver(sendPort);

            sendShuttle->retiredPair   = waiter;
            LwU64 copy_size            = sendShuttle->payloadSize;
            if (waiter->payloadSize < copy_size)
            {
                copy_size          = 0;
                // @todo: Add tests to ensure this code isn't ever return by LibosPortAsyncSend
                waiter->errorCode = sendShuttle->errorCode = LibosErrorIncomplete;
            }
            waiter->payloadSize = copy_size;

            if (flags & LibosPortLockOperation)
            {
                // The port send is a lock release
                // Because there is an acquirer, we transfer the ownership to them
                Lock *lock = (Lock *)sendPort;
                lock->holdCount = 1;
                lock->holder = waiter->owner;
                // Retire and reset the unlock shuttle manually, since it's not attached to any task
                sendShuttle->state = ShuttleStateReset;
                sendShuttle = 0;
                ShuttleRetireSingle(waiter);
            }

            else
            {
#if LIBOS_FEATURE_PAGING
                KernelSchedulerGetTask()->pasidWrite = taskTable[waiter->owner].pasid;
                KernelSchedulerGetTask()->pasidRead  = taskTable[sendShuttle->owner].pasid;
#endif
                KernelCopyMessage((void *)waiter->payloadAddress, (void *)sendShuttle->payloadAddress, copy_size);
            }
        }
        else
        {
            // Add this shuttle to the ports waiting senders list
            sendShuttle->waitingOnPort = LIBOS_PTR32_NARROW(sendPort);
            PortEnqueSender(sendPort, sendShuttle);

            // Don't retire the shuttle
            sendShuttle = 0;
        }
    }

    if (sendShuttle)
        ShuttleRetirePair(sendShuttle, sendShuttle->retiredPair);

    if (recvShuttle)
    {
        // Is there a waiting sender?
        if (PortHasSender(recvPort))
        {
            Shuttle *waiting_sender = PortDequeSender(recvPort);

            recvShuttle->retiredPair = waiting_sender;
            LwU64 copy_size = waiting_sender->payloadSize;
            if (waiting_sender->payloadSize < copy_size)
            {
                copy_size = 0;
                recvShuttle->errorCode = waiting_sender->errorCode = LibosErrorIncomplete;
            }
            recvShuttle->payloadSize = copy_size;

#if LIBOS_FEATURE_PAGING
            KernelSchedulerGetTask()->pasidWrite = taskTable[recvShuttle->owner].pasid;
            KernelSchedulerGetTask()->pasidRead  = taskTable[waiting_sender->owner].pasid;
#endif
            KernelCopyMessage(
                (void *)recvShuttle->payloadAddress, (void *)waiting_sender->payloadAddress, copy_size);

            if (flags & LibosPortLockOperation)
            {
                // The port recv is a lock acquire
                // Because the lock is released, we can claim the owner of the lock
                Lock *lock = (Lock *)recvPort;
                lock->holdCount = 1;
                lock->holder = KernelSchedulerGetTask()->id;
                ShuttleRetireSingle(recvShuttle);
                recvShuttle = 0;

                // Retire and reset the unlock shuttle manually, since it's not attached to any task
                waiting_sender->state = ShuttleStateReset;
            }
        }
        else
        {
            if (flags & LibosPortLockOperation)
            {
                // The port send is a lock acquire
                Lock *lock = (Lock *)recvPort;
                if (lock->holder == KernelSchedulerGetTask()->id)
                {
                    // Re-enter the lock
                    lock->holdCount++;
                    ShuttleRetireSingle(recvShuttle);
                    recvShuttle = 0;
                }
            }

            if (recvShuttle)
            {
                // Link into ports waiting senders list
                recvShuttle->waitingOnPort = LIBOS_PTR32_NARROW(recvPort);

                // Add to end of receiver queue
                PortEnqueReceiver(recvPort, recvShuttle);

                // Don't retire the shuttle
                recvShuttle = 0;
            }
        }
    }

    if (recvShuttle)
        ShuttleRetirePair(recvShuttle->retiredPair, recvShuttle);

    // still waiting? We might be waiting on something we didn't just submit
    // and that item might be "complete already"
    if (KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()))
    {
        Shuttle *wait = LIBOS_PTR32_EXPAND(Shuttle, KernelSchedulerGetTask()->shuttleSleeping);

        // @note wait->state cannot be COMPLETE_IDLE as it is checked in libos_port_send_recv
        if (!wait)
        {
            // Any completed shuttles?
            if (LIBOS_PTR32_EXPAND(ListHeader, KernelSchedulerGetTask()->shuttlesCompleted.next) !=
                &KernelSchedulerGetTask()->shuttlesCompleted)
            {
                wait =
                    CONTAINER_OF(KernelSchedulerGetTask()->shuttlesCompleted.next, Shuttle, shuttleCompletedLink);
            }
        }

        // @note wait->state still cannot be COMPLETE_IDLE as that implies it wouldn't have been
        // present in the completed_shuttles list
        if (wait && (wait->state == ShuttleStateCompletePending ||
                     wait->state == ShuttleStateCompleteIdle))
        {
            if (wait->state == ShuttleStateCompletePending)
            {
                ListUnlink(&wait->shuttleCompletedLink);
                wait->state = ShuttleStateCompleteIdle;
            }

            ShuttleWriteSyscallRegisters(KernelSchedulerGetTask(), wait);
            KernelSchedulerRun(KernelSchedulerGetTask());
        }
    }
    else
    {
        // Not in a wait state, just return
        KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LibosOk;
    }

    KernelTaskReturn();
}

void LIBOS_SECTION_IMEM_PINNED ShuttleReset( Shuttle * resetShuttle)
{
    /*
     @note No one is waiting on this shuttle as only a single task has privileges for the shuttle
     (us).
    */
    switch (resetShuttle->state)
    {
    case ShuttleStateCompletePending:
        ListUnlink(&resetShuttle->shuttleCompletedLink);
        /* fallthrough */

    case ShuttleStateCompleteIdle:
        PortUnlinkReplyCredentials(resetShuttle);
        break;

    case ShuttleStateRecv:
	    PortUnlinkRemoteReplyCredentials(resetShuttle);
	    /* fallthrough */

    case ShuttleStateSend:
        ListUnlink(&resetShuttle->portLink);
        break;

    case ShuttleStateReset:
      break;
    }

    // Mark transaction as completed
    resetShuttle->state = ShuttleStateReset;
}
