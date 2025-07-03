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
#include "libos.h"
#include "sched/port.h"
#include "libriscv.h"
#include "sched/timer.h"
#include <lwtypes.h>
#include "diagnostics.h"
#include "scheduler.h"
#include "sched/lock.h"
#if !LIBOS_TINY
#include "../root/root.h"
#include "sched/global_port.h"
#include "libbit.h"
#endif // !LIBOS_TINY

// @bug: Shuttle::ObjectCount cannot be reset in shuttleReset as bind does this in the first step



/**
 *
 * @file port.c
 *
 * @brief Asynchronous Inter-process Communication
 */

#if LIBOS_TINY

LibosShuttleNameDecl(shuttleAny);

LibosStatus KernelCopyMessage(Shuttle * recvShuttle, Shuttle * sendShuttle, LwU64 size)
{
    Task * recvShuttleOwner = &taskTable[recvShuttle->owner];
    Task * sendShuttleOwner = &taskTable[sendShuttle->owner];

    // @todo: Security Issue
    //        Document that we don't lwrrently validate the caller.  This will be fixed in Blackwell
    //        now that we have full pagetable support.

    // Enable all MPU entries to allow kernel to touch both processes address spaces
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  recvShuttleOwner->mpuEnables[0] | sendShuttleOwner->mpuEnables[0]);
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  recvShuttleOwner->mpuEnables[1] | sendShuttleOwner->mpuEnables[1]);

    // Perform the copy
    memcpy((void *)recvShuttle->payloadAddress, (void *)sendShuttle->payloadAddress, size);

    // Restore permissions
    Task * KernelInterruptedTask = KernelSchedulerGetTask();
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 0);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[0]);
    KernelCsrWrite(LW_RISCV_CSR_SMPUIDX2, 1);
    KernelCsrWrite(LW_RISCV_CSR_SMPUVLD,  KernelInterruptedTask->mpuEnables[1]);

    return LibosOk;
}


#else

// The sendShuttle->payloadAddress or recvShuttle->payloadAddress might be a kernel address in trap and task start conditions
LibosStatus KernelCopyMessage(BORROWED_PTR Shuttle * recvShuttle, BORROWED_PTR Shuttle * sendShuttle, LwU64 size)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("Copy message dst:%llx src:%llx size:%llx\n", recvShuttle->payloadAddress, sendShuttle->payloadAddress, size);
    if (!size)
        return LibosOk;

    if (size > LIBOS_CONFIG_PORT_APERTURE_SIZE) {
        KernelAssert(0);
        return LibosErrorIncomplete;
    }

    // Ensure both have the header setup or neither
    LwBool receiverHasHeader = !!(recvShuttle->copyFlags & (
            LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS | 
            LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES | 
            LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID));
    LwBool senderHasHeader = !!(sendShuttle->copyFlags & (
            LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS | 
            LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES |
            LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID));
    if (receiverHasHeader != senderHasHeader) {
        KernelAssert(0);
        return LibosErrorIncomplete;
    }

    // Map the recvShuttle->payloadAddress buffer into APERTURE_0
    void * destination = 0;
     
    if (!(recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
    {
        destination = KernelMapUser(0, recvShuttle->owner, recvShuttle->payloadAddress, size);
        if (!destination) 
            goto unmapAndReturnIncomplete;
    }
    else
        destination = (void *)recvShuttle->payloadAddress;

    // Map the sendShuttle->payloadAddress buffer into APERTURE_1
    void * source = 0;
    if (!(sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
    {        
        source = KernelMapUser(1, sendShuttle->owner, sendShuttle->payloadAddress, size);
        if (!source) 
            goto unmapAndReturnIncomplete;
    }
    else
        source = (void *)sendShuttle->payloadAddress;

    // Copy the message
    memcpy(destination, source, size);

    if (receiverHasHeader) 
    {
        LibosMessageHeader * header = (LibosMessageHeader *)destination;

        if ((size - offsetof(LibosMessageHeader, handles)) / sizeof(LibosHandleTransfer) < header->count)
            goto unmapAndReturnIncomplete;

        for (int i = 0; i < header->count; i++) 
        {
            LibosHandleTransfer transfer = header->handles[i];

            // If both the caller and sender are expecting a global port id
            // we don't need to perform translation
            if ((sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID) &&
                 (recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID))
            {
                // @todo: We should find out if we have the grant, and pass that grant on.
                continue;
            }

            // Get the object 
            if (sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                /* transfer.object is already an object and the sender is transfering a reference to us */;
            else if (sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES) {
                LwU64 parentGrant;
                transfer.object = KernelTaskHandleEntry(sendShuttle->owner, transfer.handle, &parentGrant);    
                KernelPoolAddref(transfer.object); 
                if ((parentGrant & (transfer.grant)) != transfer.grant)
                    transfer.object = 0, transfer.grant = 0;
            }
            else if (sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID)
            {
                // Do we already have an object tracking this global port?
                KernelTrace("Handle source is a globalPort id  %x\n", transfer.globalPort);
                Port * port = KernelGlobalPortMapFind(transfer.globalPort);
                KernelTrace("Resolves to port object %p\n", port);

                if (port)
                    KernelPoolAddref(port);     // Take a reference count on it.
                else
                {
                    KernelTrace("Registering a new globalport.\n");

                    LibosStatus status = KernelPortAllocate(&port);
                    if (status == LibosOk)
                    {
                        port->globalPort = transfer.globalPort;
                        KernelGlobalPortMapInsert(port);    // @todo: Look at the reference counting here
                    }
                }
                transfer.object = port;
            }
            else
                transfer.object = 0, transfer.grant = 0;

            // At this point we own the reference count on transfer.object

            // Map the object
            if (!transfer.object)
                /* Did not resolve leave it at 0*/;
            else if (recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                /* transfer.object is already an object and we're transfering a refcnt */;
            else if (recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES) 
            {
                LibosHandle handle;
                LibosStatus status = KernelTaskRegisterObject(recvShuttle->owner, &handle, transfer.object, transfer.grant, 0);
                KernelPoolRelease(transfer.object);
                if (status == LibosOk)
                    transfer.handle = handle;
                else
                    transfer.grant = 0, transfer.handle = 0;
            }
            else if (recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID)
            {
                KernelTrace("Encoding to globalPort\n");
                Port * port = DynamicCastPort(transfer.object);
                if (port) {
                    // Colwert to global port
                    KernelTrace(" id = %x\n", port->globalPort);
                    if (port->globalPort == LibosPortHandleNone)
                    {
                        if (LibosOk == LwosRootGlobalPortAllocate(&port->globalPort))
                            KernelGlobalPortMapInsert(port);
                    }

                    // Compute grant
                    LwosGlobalPortGrant globalGrant = 0;
                    if (transfer.grant & LibosGrantPortSend)
                        globalGrant |= LWOS_ROOTP_GLOBAL_PORT_GRANT_SEND;
                    if (transfer.grant & LibosGrantWait)
                        globalGrant |= LWOS_ROOTP_GLOBAL_PORT_GRANT_RECV;

                    // Notify the root partition of the intention to grant permissions
                    // to this port
                    KernelTrace("Processing grants\n");
                    if (LibosOk == LwosRootGlobalPortGrant(port->globalPort, recvShuttle->targetPartition, globalGrant))
                    {
                        // Fill in the global port id
                        transfer.globalPort = port->globalPort;
                    }
                    else {
                        KernelAssert(0);
                        transfer.globalPort = 0;
                    }
                }
                else {
                    KernelPoolRelease(transfer.object);
                    transfer.grant = 0, transfer.handle = 0;
                }
            }
            else {
                KernelPoolRelease(transfer.object);
                transfer.object = 0, transfer.grant = 0;
            }

            // write it back to the target address space
            header->handles[i] = transfer;
        }
    }

    // Unmap the pages
    if (!(recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
        KernelUnmapUser(0, size);
    if (!(sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
        KernelUnmapUser(1, size);

    return LibosOk;
unmapAndReturnIncomplete:
    // Unmap the pages
    if (!(recvShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
        KernelUnmapUser(0, size);
    if (!(sendShuttle->copyFlags & LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER))
        KernelUnmapUser(1, size);
    return LibosErrorIncomplete;
}

#endif

void KernelContinuation();

LwBool LIBOS_SECTION_IMEM_PINNED ShuttleIsIdle(BORROWED_PTR Shuttle * shuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    return shuttle->state < ShuttleStateSend;
}

static LIBOS_SECTION_IMEM_PINNED void
ShuttleWriteSyscallRegisters(BORROWED_PTR Task *owner, BORROWED_PTR Shuttle *completedShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("    ShuttleWriteSyscallRegisters(task: %d, completedShuttle: %d)\n", owner->id, completedShuttle->taskLocalHandle);
    
    // Only write the completion registers if it was an explicit syscall
    if (KernelSchedulerIsTaskTrapped(owner))
        return;

    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0]           = completedShuttle->errorCode;
    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE] = completedShuttle->payloadSize;

    owner->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE] = completedShuttle->taskLocalHandle;
}

void LIBOS_SECTION_IMEM_PINNED KernelPortTimeout(BORROWED_PTR Task *owner)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("    KernelPortTimeout(task: %d)\n", owner->id);

    owner->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LibosErrorTimeout;
    KernelSchedulerRun(owner);
}

static void LIBOS_SECTION_IMEM_PINNED PortUnlinkReplyCredentials(BORROWED_PTR Shuttle *resetShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    if (resetShuttle->replyToShuttle)
        LIBOS_PTR32_EXPAND(Shuttle, resetShuttle->replyToShuttle)->grantedShuttle = 0;
}

static void LIBOS_SECTION_IMEM_PINNED PortUnlinkRemoteReplyCredentials(BORROWED_PTR Shuttle *resetShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    if (resetShuttle->grantedShuttle)
        LIBOS_PTR32_EXPAND(Shuttle, resetShuttle->grantedShuttle)->replyToShuttle = 0;
}

LIBOS_SECTION_IMEM_PINNED void ShuttleRetireSingle(BORROWED_PTR Shuttle *shuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
#if !LIBOS_TINY
    Task *owner = shuttle->owner;
#else
    Task *owner = &taskTable[shuttle->owner];
#endif

    KernelTrace("KERNEL: ShuttleRetireSingle %d\n",KernelSchedulerIsTaskWaiting(owner));
    if (KernelSchedulerIsTaskWaiting(owner) &&
        (LIBOS_PTR32_EXPAND(Shuttle, owner->shuttleSleeping) == shuttle ||
         !owner->shuttleSleeping))
    {
        shuttle->state = ShuttleStateCompleteIdle;

        if (owner->shuttleSleeping)
            KernelPoolRelease(LIBOS_PTR32_EXPAND(Shuttle, owner->shuttleSleeping));

        owner->shuttleSleeping = 0;

        ShuttleWriteSyscallRegisters(owner, shuttle);

        KernelSchedulerRun(owner);
    }
    else
    {
        ListPushBack(&owner->shuttlesCompleted, &shuttle->shuttleCompletedLink);

        shuttle->state = ShuttleStateCompletePending;
    }
}

// @note Either this was an asynchronous operation (and we return to self), or self was one of the
// waiters
LIBOS_SECTION_IMEM_PINNED void ShuttleRetirePair(Shuttle *source, Shuttle *sink)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
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

void PortCompleteSend(BORROWED_PTR Shuttle *sendShuttle, OWNED_PTR Port *sendPort);
void PortCompleteRecv();
void PortCompleteWait();

void LIBOS_SECTION_IMEM_PINNED ShuttleBindSend(Shuttle * sendShuttle, LwU8 copyFlags, LwU64 sendPayload, LwU64 sendPayloadSize)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("    ShuttleBindSend(shuttle:%x, size:%llu)\n", sendShuttle->taskLocalHandle, sendPayloadSize);
    KernelAssert(!sendShuttle || ShuttleIsIdle(sendShuttle));

#if LIBOS_TINY
    KernelSyscallShuttleReset(sendShuttle);
#else // LIBOS_TINY
    KernelShuttleResetAndLeaveObjects(sendShuttle);
#endif // LIBOS_TINY

    // Fill the sendShuttleId slot out
    sendShuttle->state          = ShuttleStateSend;
    sendShuttle->payloadAddress = sendPayload;
    sendShuttle->payloadSize    = sendPayloadSize;
    sendShuttle->errorCode      = LibosOk;
#if !LIBOS_TINY
    sendShuttle->copyFlags  = copyFlags;
#endif
}

void LIBOS_SECTION_IMEM_PINNED ShuttleBindGrant(Shuttle * sendShuttle, Shuttle * recvShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("    ShuttleBindGrant(sendShuttle:%x, recvShuttle:%x\n", sendShuttle->taskLocalHandle, !recvShuttle ? 0 : recvShuttle->taskLocalHandle);    
    sendShuttle->partnerRecvShuttle = LIBOS_PTR32_NARROW(recvShuttle);
}

void LIBOS_SECTION_IMEM_PINNED ShuttleBindRecv(Shuttle * recvShuttle, LwU8 copyFlags, LwU64 recvPayload, LwU64 recvPayloadSize)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("    ShuttleBindRecv(shuttle:%x, size:%llu)\n", recvShuttle->taskLocalHandle, recvPayloadSize);
    KernelAssert(!recvShuttle || ShuttleIsIdle(recvShuttle));

#if LIBOS_TINY
    KernelSyscallShuttleReset(recvShuttle);
#else // LIBOS_TINY
    KernelShuttleResetAndLeaveObjects(recvShuttle);
#endif // LIBOS_TINY

    // Fill the shuttle_id slot out
    recvShuttle->state          = ShuttleStateRecv;
    recvShuttle->grantedShuttle = 0;
    recvShuttle->payloadAddress = recvPayload;
    recvShuttle->payloadSize    = recvPayloadSize;
    recvShuttle->errorCode      = LibosOk;
#if !LIBOS_TINY
    recvShuttle->copyFlags  = copyFlags;
#endif    
}

void PortEnqueSender(Port * sendPort, Shuttle * sendShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelAssert(sendShuttle->state == ShuttleStateSend);
    ListPushBack(&sendPort->waitingSenders, &sendShuttle->portLink);
#if !LIBOS_TINY
    KernelUpdateGlobalPortSend(sendPort);
#endif
}

void PortEnqueReceiver(Port * recvPort, Shuttle * recvShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelAssert(recvShuttle->state == ShuttleStateRecv);
    ListPushBack(&recvPort->waitingReceivers, &recvShuttle->portLink);
#if !LIBOS_TINY
    KernelUpdateGlobalPortRecv(recvPort);
#endif
}

LwBool PortHasSender(Port * port)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    return !ListEmpty(&port->waitingSenders);
}

LwBool PortHasReceiver(Port * port)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    return !ListEmpty(&port->waitingReceivers);
}

Shuttle * PortDequeSender(Port * port)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelAssert(PortHasSender(port));
    Shuttle *sendShuttle = LIST_POP_FRONT(port->waitingSenders, Shuttle, portLink);
    KernelAssert(sendShuttle->state == ShuttleStateSend);
#if !LIBOS_TINY
    KernelUpdateGlobalPortSend(port);
#endif
    return sendShuttle;
}

Shuttle * PortDequeReceiver(Port * port)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelAssert(PortHasReceiver(port)); 
    Shuttle *recvShuttle = LIST_POP_FRONT(port->waitingReceivers, Shuttle, portLink);
    KernelAssert(recvShuttle->state == ShuttleStateRecv);
#if !LIBOS_TINY
    KernelUpdateGlobalPortRecv(port);
#endif
    return recvShuttle;
}


#if !LIBOS_TINY

/*
 * Try to send the message to another partition.
 * 
 * Preconditions:
 *     1. There is a global port ID associated with the sendPort, and
 *     2. There is a pending receiver on the global port.
 * 
 * If there are pending receivers, we take the one with the lowest partition ID
 * and try to send the message to it. We assume that all objects attached to the
 * sendShuttle are ports and assign a global port ID for each that doesn't have
 * one already.
 */
LibosStatus PortSendGlobal(Shuttle *sendShuttle, Port *sendPort)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("PortSendGlobal\n");
    LwU32 globalId = sendPort->globalPort;

    LwU8 receivingPartition = LibosLog2GivenPow2(LibosBitLowest(
        KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsPendingRecv));
    if ((KernelGlobalPage()->initializedPartitionMask & (1ULL << receivingPartition)) == 0)
    {
        KernelAssert(0);
        return LibosErrorSpoofed;
    }

    if ((KernelGlobalPage()->globalPortGrants[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsGrantRecv & (1ULL << receivingPartition)) == 0)
    {
        KernelAssert(0);
        return LibosErrorAccess;
    }

    if (sendShuttle->payloadSize > sizeof(KernelGlobalPage()->args.globalMessage.message))
    {
        KernelAssert(0);
        return LibosErrorIncomplete;
    }


    Shuttle recvShuttle = {
        .payloadAddress = (LwU64) KernelGlobalPage()->args.globalMessage.message,
        .copyFlags = 
            sendShuttle->copyFlags & (LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES | LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                ?
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER | LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID
                : 
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER,
        .targetPartition = receivingPartition
    };

    KernelGlobalPage()->args.globalMessage.toGlobalPort = globalId;
    KernelGlobalPage()->args.globalMessage.size = sendShuttle->payloadSize;
    LibosStatus status = KernelCopyMessage(&recvShuttle, sendShuttle, sendShuttle->payloadSize);
    if (status != LibosOk)
    {
        KernelAssert(0);
        return status;
    }


    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_RECV, 0), 0, 0, 0, 0
    };
    status = SeparationKernelPartitionCall(receivingPartition, arguments);
    if (status != LibosOk)
    {
        KernelAssert(0);
        return status;
    }
    return (LibosStatus) LWOS_A0_ARG(arguments[0]);
}

/*
 * Try to receive a message from another partition.
 * 
 * Preconditions:
 *     1. There is a global port ID associated with the recvPort, and
 *     2. There is a pending sender on the global port.
 * 
 * If there are pending senders, we take the one with the lowest partition ID
 * and try to receive a message from it. We create a local port for all global
 * port attached to the message.
 */
LibosStatus PortRecvGlobal(Shuttle *recvShuttle, Port *recvPort)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    KernelTrace("PortRecvGlobal\n");
    LwU32 globalId = recvPort->globalPort;

    LwU8 sendingPartition = LibosLog2GivenPow2(LibosBitLowest(
        KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsPendingSend));
    if ((KernelGlobalPage()->initializedPartitionMask & (1ULL << sendingPartition)) == 0)
    {
        return LibosErrorSpoofed;
    }

    if ((KernelGlobalPage()->globalPortGrants[LWOS_ROOTP_GLOBAL_PORT_SEQ(globalId)].partitionsGrantSend & (1ULL << sendingPartition)) == 0)
    {
        return LibosErrorAccess;
    }

    LwU64 arguments[5] = {
        LWOS_A0(LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_SEND, 0), globalId, recvShuttle->payloadSize, 0, 0
    };
    LibosStatus status = SeparationKernelPartitionCall(sendingPartition, arguments);
    if (status != LibosOk)
    {
        return status;
    }
    status = (LibosStatus) LWOS_A0_ARG(arguments[0]);
    if (status != LibosOk)
    {
        return status;
    }

    Shuttle sendShuttle = {
        .payloadAddress = (LwU64) KernelGlobalPage()->args.globalMessage.message,
        .copyFlags = 
            recvShuttle->copyFlags & (LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES | LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS)
                ?
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER | LIBOS_SHUTTLE_COPY_FLAG_MAP_GLOBAL_PORT_ID
                : 
                    LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER,
        .targetPartition = sendingPartition
    };

    return KernelCopyMessage(recvShuttle, &sendShuttle, KernelGlobalPage()->args.globalMessage.size);
}

#endif // !LIBOS_TINY

// @todo: Split into smaller functions for building out a port (due to register pressure)
// @precondition: sendShuttle is idle, recvShuttle is idle, recvPort has no waiters.
LibosStatus PortSendRecvWait(
    BORROWED_PTR Shuttle * sendShuttle, 
    BORROWED_PTR Port * sendPort, 
    BORROWED_PTR Shuttle * recvShuttle,  
    BORROWED_PTR Port * recvPort,
    LwU64 newTaskState, 
    BORROWED_PTR Shuttle * waitShuttle, 
    LwU64 waitTimeout,
    LwU64 flags)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
    /*
     * Wait buildout
     */
    KernelAssert(!KernelSchedulerGetTask()->shuttleSleeping);
    KernelSchedulerGetTask()->shuttleSleeping = LIBOS_PTR32_NARROW(waitShuttle);
    if (newTaskState == TaskStateWaitTimeout) 
        KernelSchedulerWait(KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] + LibosTimeNs());
    else if (newTaskState >= TaskStateWait /* TaskStateWait || TaskStateWaitTrapped */)
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
                copy_size         = 0;
                // @todo: Add tests to ensure this code isn't ever return by LibosPortAsyncSend
                waiter->errorCode = sendShuttle->errorCode = LibosErrorIncomplete;
            }
            waiter->payloadSize = copy_size;

            LibosStatus status = KernelCopyMessage(waiter, sendShuttle, copy_size);

            if (status != LibosOk)
            {
                waiter->errorCode = status;
                sendShuttle->errorCode = status;
                sendShuttle->payloadSize = 0;
                waiter->payloadSize = 0;
            }

            Lock * lock;
            if ((lock = DynamicCastLock(sendPort)))
            {
                // The port send is a lock release
                // Because there is an acquirer, we transfer the ownership to them
                lock->holdCount = 1;
                lock->holder = waiter->owner;
                // Retire and reset the unlock shuttle manually, since it's not attached to any task
                sendShuttle->state = ShuttleStateReset;
                sendShuttle = 0;
                ShuttleRetireSingle(waiter);
            }
        }
        else
        {
#if !LIBOS_TINY
            if (sendPort->globalPort != LWOS_ROOTP_GLOBAL_PORT_NONE &&
                KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(sendPort->globalPort)].partitionsPendingRecv != 0)
            {
                LibosStatus status = PortSendGlobal(sendShuttle, sendPort);
                if (status != LibosOk)
                {
                    sendShuttle->errorCode = status;
                    sendShuttle->payloadSize = 0;
                }
                ShuttleRetireSingle(sendShuttle);
                sendShuttle = 0;
            }
            else
#endif // !LIBOS_TINY
            {
                // Add this shuttle to the ports waiting senders list
                sendShuttle->waitingOnPort = sendPort;
                PortEnqueSender(sendPort, sendShuttle);

                // Don't retire the shuttle
                sendShuttle = 0;
            }
        }
    }

    if (sendShuttle)
        ShuttleRetirePair(sendShuttle, sendShuttle->retiredPair);

    /*
     * Recv Shuttle Queue or Copy
     */

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

            LibosStatus status = KernelCopyMessage(recvShuttle, waiting_sender, copy_size);

            if (status != LibosOk)
            {
                recvShuttle->errorCode = status;
                waiting_sender->errorCode = status;
                waiting_sender->payloadSize = 0;
                recvShuttle->payloadSize = 0;
            }

            Lock * lock;
            if ((lock = DynamicCastLock(recvPort)))
            {
                // The port recv is a lock acquire
                // Because the lock is released, we can claim the owner of the lock
                lock->holdCount = 1;
#if !LIBOS_TINY
                lock->holder = KernelSchedulerGetTask();
#else
                lock->holder = KernelSchedulerGetTask()->id;
#endif
                ShuttleRetireSingle(recvShuttle);
                recvShuttle = 0;

                // Retire and reset the unlock shuttle manually, since it's not attached to any task
                waiting_sender->state = ShuttleStateReset;
            }
        }
        else
        {
            Lock * lock;
#if !LIBOS_TINY
            if (recvPort->globalPort != LWOS_ROOTP_GLOBAL_PORT_NONE &&
                KernelGlobalPage()->globalPortStatuses[LWOS_ROOTP_GLOBAL_PORT_SEQ(recvPort->globalPort)].partitionsPendingSend != 0)
            {
                LibosStatus status = PortRecvGlobal(recvShuttle, recvPort);
                if (status != LibosOk)
                {
                    recvShuttle->errorCode = status;
                    recvShuttle->payloadSize = 0;
                }
                ShuttleRetireSingle(recvShuttle);
                recvShuttle = 0;
            }
            else 
#endif // !LIBOS_TINY
            if ((lock = DynamicCastLock(recvPort)))
            {
                // The port send is a lock acquire
#if !LIBOS_TINY
                if (lock->holder == KernelSchedulerGetTask())
#else
                if (lock->holder == KernelSchedulerGetTask()->id)
#endif
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
                recvShuttle->waitingOnPort = recvPort;

                // Add to end of receiver queue
                PortEnqueReceiver(recvPort, recvShuttle);

                // Don't retire the shuttle
                recvShuttle = 0;
            }
        }
    }

    if (recvShuttle)
        ShuttleRetirePair(recvShuttle->retiredPair, recvShuttle);

    /*
     * Wait Shuttle
     */

    // still waiting? We might be waiting on something we didn't just submit
    // and that item might be "complete already"
    if (KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()))
    {
        Shuttle *wait = LIBOS_PTR32_EXPAND(Shuttle, KernelSchedulerGetTask()->shuttleSleeping);

        // @note wait->state cannot be COMPLETE_IDLE as it is checked in libos_port_send_recv
        if (!wait)
        {
            // Any completed shuttles?
            if (!ListEmpty(&KernelSchedulerGetTask()->shuttlesCompleted))
            {
                // Find the first element in the completed list but do not remove it
                // it will be removed below when we discover the state is ShuttleStateCompletePending
                wait = CONTAINER_OF(KernelSchedulerGetTask()->shuttlesCompleted.next, Shuttle, shuttleCompletedLink);
                KernelAssert(wait->state == ShuttleStateCompletePending);
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

            KernelPoolRelease(wait);
            KernelSchedulerGetTask()->shuttleSleeping = 0;

            ShuttleWriteSyscallRegisters(KernelSchedulerGetTask(), wait);
            KernelSchedulerRun(KernelSchedulerGetTask());
        }
    }
    else
    {
        // Not in a wait state, just return
        KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = LibosOk;
    }

    return LibosOk;
}

void KernelShuttleResetAndLeaveObjects(Shuttle * resetShuttle)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);
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

#if LIBOS_TINY
    // Fold the two cases to keep the LIBOS-TINY code smaller
    case ShuttleStateRecv:
    case ShuttleStateSend:
        ListUnlink(&resetShuttle->portLink);
        break;
#else
    case ShuttleStateRecv:
        ListUnlink(&resetShuttle->portLink);
        KernelUpdateGlobalPortRecv(resetShuttle->waitingOnPort);
        break;

    case ShuttleStateSend:
        ListUnlink(&resetShuttle->portLink);
        KernelUpdateGlobalPortSend(resetShuttle->waitingOnPort);
        break;
#endif
        
    case ShuttleStateReset:
      break;
    }

    // Mark transaction as completed
    resetShuttle->state = ShuttleStateReset;
}

#if !LIBOS_TINY

void KernelPortDestroy(Port * port)
{
    KernelAssertSpinlockHeld(&KernelSchedulerLock);

    // Reset any sending shuttle during port destroy
    while(PortHasSender(port)) 
        KernelSyscallShuttleReset(PortDequeSender(port));

    while(PortHasReceiver(port)) 
        KernelSyscallShuttleReset(PortDequeReceiver(port));
    
    if (port->globalPort != LWOS_ROOTP_GLOBAL_PORT_NONE)
        KernelGlobalPortMapRemove(port);
}


#endif


