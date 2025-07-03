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
#include "sched/port.h"
#include "libriscv.h"
#include "sched/timer.h"
#include <lwtypes.h>
#include "diagnostics.h"
#include "scheduler.h"
#if !LIBOS_TINY
#include "mm/objectpool.h"
#include "mm/pagetable.h"
#endif

LibosStatus LIBOS_SECTION_IMEM_PINNED ShuttleSyscallReset(LibosShuttleHandle reset_shuttle_id)
{
    OWNED_PTR Shuttle *resetShuttle = KernelObjectResolve(KernelSchedulerGetTask(), reset_shuttle_id, Shuttle, LibosGrantShuttleAll);

    if (!resetShuttle)
        return LibosErrorAccess;

    KernelSyscallShuttleReset(resetShuttle);
    KernelPoolRelease(resetShuttle);
    
    return LibosOk;
}


/**
 *   @brief Handles all port send/recv/wait operations
 *
 *   Additional inputs in registers
 *       LIBOS_REG_IOCTL_PORT_WAIT_ID          RISCV_T1
 *       LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT     RISCV_T2
 *
 *   Outputs in registers:
 *       LIBOS_REG_IOCTL_PORT_WAIT_OUT_SHUTTLE RISCV_A6
 *       LIBOS_REG_IOCTL_PORT_WAIT_RECV_SIZE   RISCV_A4
*/
__attribute__((used)) LibosStatus PortSyscallSendRecvWait(
    LibosShuttleHandle sendShuttleId, LibosPortHandle sendPortId, LwU64 sendPayload, LwU64 sendPayloadSize,
    LibosShuttleHandle recvShuttleId, LibosPortHandle recvPortId, LwU64 recvPayload, LwU64 recvPayloadSize)
{
    Task    *self = KernelSchedulerGetTask();
    Shuttle *sendShuttle = 0;
    Shuttle *recvShuttle = 0;
    Shuttle *waitShuttle = 0;
    Port    *sendPort = 0;
    Port    *recvPort = 0;
    TaskState newTaskState = TaskStateReady;
    LwU64 flags = self->state.registers[LIBOS_REG_IOCTL_PORT_FLAGS];
    LibosStatus status;

    KernelTrace("PortSyscallSendRecvWait(sendShuttle: %d, sendPort: %d, sendSize: %lld, \n"
           "                              recvShuttle: %d, recvPort: %d, recvSize: %lld)\n",
         sendShuttleId, sendPortId, sendPayloadSize,
         recvShuttleId, recvPortId, recvPayloadSize);

    if (recvShuttleId)
    {
        recvShuttle = KernelObjectResolve(self, recvShuttleId, Shuttle, LibosGrantShuttleAll);
        
        if (!recvShuttle) {
            status = LibosErrorAccess;
            goto error;
        }

        if (recvPortId) {
            recvPort = KernelObjectResolve(self, recvPortId, Port, LibosGrantWait);
            if (!recvPort)
            {
                status = LibosErrorAccess;
                goto error;
            }
#if !LIBOS_TINY
            if (recvPort->globalPortLost)
            {
                status = LibosErrorPortLost;
                goto error;
            }
#endif // !LIBOS_TINY
        }

    }

    if (sendShuttleId)
    {
        // Lookup the shuttle
        sendShuttle = KernelObjectResolve(self, sendShuttleId, Shuttle, LibosGrantShuttleAll);

        if (!sendShuttle)
        {
            status = LibosErrorAccess;
            goto error;
        }

        // Can we locate the port?
        if (sendPortId)
        {
            sendPort = KernelObjectResolve(self, sendPortId, Port, LibosGrantPortSend);
            if (!sendPort)
            {
                status = LibosErrorAccess;
                goto error;
            }
#if !LIBOS_TINY
            if (sendPort->globalPortLost)
            {
                status = LibosErrorPortLost;
                goto error;
            }
#endif // !LIBOS_TINY
        }
        // Prior state was either complete or reset, so replyToShuttle field must be valid
        else if (sendShuttle->replyToShuttle)
        {
            // @todo: This line is trouble as we won't have a sendPort to reply to :(
            sendPort = LIBOS_PTR32_EXPAND(Shuttle, sendShuttle->replyToShuttle)->waitingOnPort;
        }
        else
        {
            status = LibosErrorAccess;
            goto error;
        }
    }

    // @bug: Can't tell different between "wait on any" and "wait on none"
    LibosShuttleHandle wait_shuttle_id = self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_ID];
    if (wait_shuttle_id)
    {
        // Can't enter wait state if we're in a critical section
        if(criticalSection)
        {
            status = LibosErrorAccess;
            goto error;
        }

        if (self->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] != LibosTimeoutInfinite)
            newTaskState = TaskStateWaitTimeout;
        else
            newTaskState = TaskStateWait;

        if (wait_shuttle_id != LibosShuttleAny)
        {
            waitShuttle = KernelObjectResolve(self, wait_shuttle_id, Shuttle, LibosGrantShuttleAll);

            if (!waitShuttle)
            {
                status = LibosErrorAccess;
                goto error;
            }
        }
    }

    if (sendShuttle)
    {
        // Is the sendShuttleId slot empty?
        if (!ShuttleIsIdle(sendShuttle))
        {
            status = LibosErrorAccess;
            goto error;
        }

    }

    if (recvShuttle)
    {
        if (!ShuttleIsIdle(recvShuttle))
        {
            status = LibosErrorAccess;
            goto error;
        }
        
        if (!recvPort)
#if LIBOS_TINY
            recvPort = &self->portSynchronousReply;
#else
            recvPort = self->portSynchronousReply;
#endif
    }    

    LwU8 copyFlags = 0;
#if !LIBOS_TINY
    if (flags & LibosPortFlagTransferHandles)
        copyFlags |= LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES;

    // Todo, ideally the kernel servers would have a dedicated lower
    // level port call that did not perform handle resolution.
    // This code would move there
    extern Task * kernelServer;
    if (self == kernelServer && (flags & LibosPortFlagTransferObjects))
        copyFlags |= LIBOS_SHUTTLE_COPY_FLAG_MAP_OBJECTS;
#endif

    if (sendShuttle)
    {
        ShuttleBindSend(sendShuttle, copyFlags, sendPayload, sendPayloadSize);
        ShuttleBindGrant(sendShuttle, recvShuttle);
    }

    if (recvShuttle)
    {
        ShuttleBindRecv(recvShuttle, copyFlags, recvPayload, recvPayloadSize);
    }    

    return PortSendRecvWait(sendShuttle, sendPort, recvShuttle, recvPort, newTaskState, waitShuttle, KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT], flags);

error:
    KernelPoolRelease(sendShuttle);
    KernelPoolRelease(recvShuttle);
    KernelPoolRelease(waitShuttle);
    KernelPoolRelease(sendPort);
    KernelPoolRelease(recvPort);
    return status;
}

void KernelSyscallShuttleReset(Shuttle * shuttle)
{
    KernelShuttleResetAndLeaveObjects(shuttle);
}