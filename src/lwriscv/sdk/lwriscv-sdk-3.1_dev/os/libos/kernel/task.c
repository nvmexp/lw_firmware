/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libos-config.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include "task.h"
#include "../include/libos.h"
#include "kernel.h"
#include "libos.h"
#include "libos_syscalls.h"
#include "memory.h"
#include "port.h"
#include "profiler.h"
#include "libriscv.h"
#include "timer.h"
#include "lwtypes.h"
#include "extintr.h"
#include "lwriscv-2.0/sbi.h"
#include "diagnostics.h"
#include "scheduler.h"
#include "trap.h"
#include "libos.h"
/**
 *
 * @file task.c
 *
 * @brief Scheduler implementation
 */

LibosShuttleNameDecl(shuttleSyncSend)
LibosShuttleNameDecl(shuttleSyncRecv)
LibosShuttleNameDecl(shuttleTrap)
LibosShuttleNameDecl(shuttleReplay)

LibosPortNameDecl(portTrap);


/**
 * @brief Scheduler critical section state
 * 
 * Scheduler critical sections temporarily suspend timeslice pre-emption.
 * Generally, the top-half kernel ISR will still run to handle error conditions
 * such as ECC double bit errors.  All further interrupt handling is deferred
 * until the end of the critical section.
 * 
 * 
 *  LibosCriticalSectionNone::          Not in a critical section
 *  LibosCriticalSectionPanicOnTrap::   In Critical Section. Release on crash.
 *  LibosCriticalSectionReleaseOnTrap:: In Critical Section. Kernel panic on crash.
 */
LibosCriticalSectionBehavior LIBOS_SECTION_DMEM_PINNED(criticalSection) = LibosCriticalSectionNone;

__attribute__(( used, section(".ManifestPorts"))) struct LibosManifestObjectInstance  manifestPortTrap;

#if LIBOS_FEATURE_PARTITION_SWITCH
/**
 * @brief Task to start at the end of KernelInit.  Used to ensure that a bootstrap after suspend
 * returns to the prior active task.
 *   * KernelTaskInit - initializes this to the init-task
 *   * kernel_processor_suspend - sets this to the lwrrently running task at time of suspend
 */
Task * LIBOS_SECTION_DMEM_COLD(resumeTask) = 0;
#endif


LwU64 LIBOS_SECTION_DMEM_COLD(userXStatus);

/**
 * @brief One time initialization of the task state.
 *
 */
LIBOS_SECTION_TEXT_COLD void KernelTaskInit(void)
{

#if LIBOS_FEATURE_PARTITION_SWITCH
    resumeTask = &TaskInit;
#endif

    // @todo: We should explicitly initialize all fields
    userXStatus = KernelCsrRead(LW_RISCV_CSR_XSTATUS);
    userXStatus = userXStatus & ~DRF_SHIFTMASK64(LW_RISCV_CSR_XSTATUS_XPP);
    userXStatus |= REF_DEF64(LW_RISCV_CSR_XSTATUS_XPP, _USER);
    userXStatus |= REF_DEF64(LW_RISCV_CSR_XSTATUS_XPIE, _ENABLE);
}


LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelTaskPanic(void)
{
    KernelAssert(!KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()));
#if LIBOS_FEATURE_USERSPACE_TRAPS
    Shuttle *crashSendShuttle = &KernelSchedulerGetTask()->crashSendShuttle;
    Shuttle *replayWaitShuttle = &KernelSchedulerGetTask()->replayWaitShuttle;
    Port *replayPort = &KernelSchedulerGetTask()->replayPort;

    // Find the global panic port
    Port * panicPortInstance = (Port *)KernelTaskResolveObject(&TaskInit, ID(panicPort), PortGrantSend);
    KernelAssert(panicPortInstance);

    ShuttleBindSend(crashSendShuttle, 0, 0);
    ShuttleBindGrant(crashSendShuttle, replayWaitShuttle);
    ShuttleBindRecv(replayWaitShuttle, (LwU64)&KernelSchedulerGetTask()->state, sizeof(KernelSchedulerGetTask()->state));

    // Release any critical sections held by the faulting task.
    if (criticalSection == LibosCriticalSectionPanicOnTrap)
        KernelPanic();
    criticalSection = LibosCriticalSectionNone;

    // Set the trapped task id field
    KernelSchedulerGetTask()->state.id = KernelSchedulerGetTask()->id;
    
    PortSendRecvWait(crashSendShuttle, panicPortInstance, replayWaitShuttle, replayPort, TaskStateWaitTrapped, replayWaitShuttle, -1ULL, 0);
#else
    // Gracefully let the task exit.
    KernelSchedulerGetTask()->shuttleSleeping = 0;
    KernelSchedulerWait(0);
    KernelTaskReturn();
#endif
}

LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallReturn(LibosStatus status) 
{ 
    KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = status;
    KernelTaskReturn();
}



#if LIBOS_FEATURE_PARTITION_SWITCH

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelSyscallPartitionSwitch(
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    LwU64 param1,
    LwU64 param2,
    LwU64 param3,
    LwU64 param4,
    LwU64 param5,
    LwU64 partition_id
#endif
)
{
#if LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND
    KernelProfilerSaveCounters();
#endif
    KernelTrace("--Kernel suspend--\n");

    resumeTask = KernelSchedulerGetTask();

#if LIBOS_FEATURE_PAGING
    KernelPagingPrepareForPartitionSwitch();
#endif

    KernelFenceFull();

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    KernelSbiPartitionSwitch(param1, param2, param3, param4, param5, partition_id);
#elif LIBOS_FEATURE_SHUTDOWN
    KernelSbiShutdown();
#else
    while(1);
#endif
}
#endif

LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn()
{
    KernelSchedulerExit();
    TrapReturn(KernelSchedulerGetTask());
}

void * KernelTaskResolveObject(Task * self, LwU64 id, LwU64 grant)
{
    if (grant == ShuttleGrantAll)  
    {
        LwU64 hash = ((id * LIBOS_HASH_MULTIPLIER) >> 32) ^ self->tableKey;
        LwU64 mask = (LwU64)&NotaddrShuttleTableMask;
        LwU64 slot = hash & mask;

        for (int displacement = 0; displacement <= LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT; displacement++)
        {
            if (shuttleTable[slot].owner == self->id &&
                shuttleTable[slot].taskLocalHandle == id)
                    return &shuttleTable[slot];
            slot = (slot + 1) & mask;
        }
        
        KernelTrace("    KernelTaskResolveObject failed to find shuttle %d\n", (LwU32)id);
        return 0;
    }
    else
    {
        LwU64 hash = ((id * LIBOS_HASH_MULTIPLIER) >> 32) ^ self->tableKey;
        LwU64 mask = (LwU64)&NotaddrPortTableMask;
        LwU64 slot = hash & mask;
        // @todo: The mask, objectTable and portArray computations could use offsets in place of indices
        for (int displacement = 0; displacement <= LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT; displacement++) {
            if (objectTable[slot].owner == self->id &&
                objectTable[slot].id == id &&
                (objectTable[slot].grant & grant))
                    return (Port *)((LwU64)portArray + objectTable[slot].offset);
            slot = (slot + 1) & mask;
        }
        KernelTrace("    KernelTaskResolveObject failed to find port %d of grant-mask %llx hash:%llx slot:%lld\n", (LwU32)id, grant, hash, hash & mask);
        return 0;
    }
}
