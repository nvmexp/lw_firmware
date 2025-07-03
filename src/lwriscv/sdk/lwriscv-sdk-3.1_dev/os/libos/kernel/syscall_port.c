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

/**
 * @brief Lookups and validates access in the task-local resource table.
 *
 * @note These are used for ports/shuttles.  requires_any_of access bits imply a specific type.
 */
static Shuttle * ShuttleFindOrRaiseErrorToTask(LibosShuttleId id)
{
    Shuttle * shuttle = (Shuttle *)KernelTaskResolveObject(KernelSchedulerGetTask(), id, ShuttleGrantAll);
    if (!shuttle)
        KernelSyscallReturn(LibosErrorAccess);

    return shuttle;
}

void * ObjectFindOrRaiseErrorToTask(LibosPortId id, LwU64 grant)
{
    void * object = (Shuttle *)KernelTaskResolveObject(KernelSchedulerGetTask(), id, grant);
    if (!object)
        KernelSyscallReturn(LibosErrorAccess);

    return object;
}

Port * PortFindOrRaiseErrorToTask(LibosPortId id, LwU64 grant)
{
    KernelAssert((grant & PortGrantAll) == grant);
    return (Port *)ObjectFindOrRaiseErrorToTask(id, grant);
}

LibosStatus LIBOS_SECTION_IMEM_PINNED ShuttleSyscallReset(LibosShuttleId reset_shuttle_id)
{
    Shuttle *resetShuttle = ShuttleFindOrRaiseErrorToTask(reset_shuttle_id);

    ShuttleReset(resetShuttle);
    
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
 *       LIBOS_REG_IOCTL_PORT_WAIT_REMOTE_TASK RISCV_A5
*/
void LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN PortSyscallSendRecvWait(
    LibosShuttleId sendShuttleId, LibosPortId sendPortId, LwU64 sendPayload, LwU64 sendPayloadSize,
    LibosShuttleId recvShuttleId, LibosPortId recvPortId, LwU64 recvPayload, LwU64 recvPayloadSize)
{
    Shuttle *sendShuttle = 0;
    Shuttle *recvShuttle = 0;
    Shuttle *waitShuttle = 0;
    Port    *sendPort = 0;
    Port    *recvPort = 0;
    PortSendRecvWaitFlags flags = LibosPortLockNone;
    TaskState   newTaskState = TaskStateReady;

    KernelTrace("PortSyscallSendRecvWait(sendShuttle: %d, sendPort: %d, send_size: %d, \n"
           "                              recvShuttle: %d, recvPort: %d, recv_size: %d)\n",
         sendShuttleId, sendPortId, sendPayloadSize,
         recvShuttleId, recvPortId, recvPayloadSize);

    if (recvShuttleId)
    {
        recvShuttle = ShuttleFindOrRaiseErrorToTask(recvShuttleId);

        if (recvPortId)
            recvPort = ObjectFindOrRaiseErrorToTask(recvPortId, ObjectGrantWait);
	
        if (KernelTaskResolveObject(KernelSchedulerGetTask(), recvPortId, LockGrantAll))
            flags |= LibosPortLockOperation;

#if LIBOS_FEATURE_PAGING
        if (recvPayloadSize)
            KernelValidateMemoryAccessOrReturn(
                recvPayload, recvPayloadSize, LW_TRUE /* request write */);
#else
//BUGBUG: Add validation code for non-paging path
#endif
    }

    if (sendShuttleId)
    {
        // Lookup the shuttle
        sendShuttle = ShuttleFindOrRaiseErrorToTask(sendShuttleId);

#if LIBOS_FEATURE_PAGING
        if (sendPayloadSize)
            KernelValidateMemoryAccessOrReturn(
                sendPayload, sendPayloadSize, LW_FALSE /* request read */);
#else
//BUGBUG: Add validation code for non-paging path
#endif
        // Can we locate the port?
        if (sendPortId)
        {
            sendPort = ObjectFindOrRaiseErrorToTask(sendPortId, PortGrantSend);
        }
        // Prior state was either complete or reset, so replyToShuttle field must be valid
        else if (sendShuttle->replyToShuttle)
        {
            // @todo: This line is trouble as we won't have a sendPort to reply to :(
            sendPort = LIBOS_PTR32_EXPAND(Port, LIBOS_PTR32_EXPAND(Shuttle, sendShuttle->replyToShuttle)->waitingOnPort);
        }
        else
            KernelSyscallReturn(LibosErrorAccess);
    }

    // @bug: Can't tell different between "wait on any" and "wait on none"
    LibosShuttleId wait_shuttle_id = KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_ID];
    if (wait_shuttle_id)
    {
        // Can't enter wait state if we're in a critical section
        if(criticalSection != LibosCriticalSectionNone)
            KernelSyscallReturn(LibosErrorAccess);

        if (KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT] != LibosTimeoutInfinite)
            newTaskState = TaskStateWaitTimeout;
        else
            newTaskState = TaskStateWait;

        if (wait_shuttle_id != ID(shuttleAny))
        {
            waitShuttle = ShuttleFindOrRaiseErrorToTask(wait_shuttle_id);
        }
    }

    if (sendShuttle)
    {
        // Is the sendShuttleId slot empty?
        if (!ShuttleIsIdle(sendShuttle))
            KernelSyscallReturn(LibosErrorAccess);

    }

    if (recvShuttle)
    {
        if (!ShuttleIsIdle(recvShuttle))
            KernelSyscallReturn(LibosErrorAccess);
        
        if (!recvPort)
            recvPort = &KernelSchedulerGetTask()->portSynchronousReply;
    }    

    if (sendShuttle)
    {
        ShuttleBindSend(sendShuttle, sendPayload, sendPayloadSize);
        ShuttleBindGrant(sendShuttle, recvShuttle);
    }

    if (recvShuttle)
    {
        ShuttleBindRecv(recvShuttle, recvPayload, recvPayloadSize);
    }    

    PortSendRecvWait(sendShuttle, sendPort, recvShuttle, recvPort, newTaskState, waitShuttle, KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_PORT_WAIT_TIMEOUT], flags);
}

