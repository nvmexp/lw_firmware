/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <libos-config.h>
#include "lwmisc.h"
#include "lwtypes.h"
#include "task.h"
#include "../include/libos.h"
#include "kernel.h"
#include "libos.h"
#include "libos_syscalls.h"
#include "sched/port.h"
#include "profiler.h"
#include "libriscv.h"
#include "sched/timer.h"
#include "lwtypes.h"
#include "extintr.h"
#include "lwriscv-2.0/sbi.h"
#include "diagnostics.h"
#include "scheduler.h"
#include "trap.h"
#include "mm/memorypool.h"
#include "libbit.h"
#include "mm/objectpool.h"
#include "rootfs.h"
#include "libos.h"
#include "mm/address_space.h"
#if !LIBOS_TINY
#include "../root/root.h"
#include "sched/lock.h"
#endif // !LIBOS_TINY

void KernelTaskDestroy(void * ptr);

/**
 *
 * @file task.c
 *
 * @brief Scheduler implementation
 */

#if !LIBOS_TINY
static LibosStatus KernelShuttleAllocateAndRegister(
    Task            * owner,
    LibosGrantShuttle grant, 
    LwU64             probeHandle,
    Shuttle       * * outShuttle);
#endif

/**
 * @brief Scheduler critical section state
 * 
 * Scheduler critical sections temporarily suspend timeslice pre-emption.
 * Generally, the top-half kernel ISR will still run to handle error conditions
 * such as ECC double bit errors.  All further interrupt handling is deferred
 * until the end of the critical section.
 * 
 * 
 *  criticalSection::   In Critical Section. Release on crash.
 */
LwBool LIBOS_SECTION_DMEM_PINNED(criticalSection) = LW_FALSE;

void KernelShuttleConstructInternal(Task * owner, Shuttle * shuttle, LibosShuttleHandle handle);

LwU64 LIBOS_SECTION_DMEM_PINNED(userXStatus);
#if !LIBOS_TINY
LwU64 supervisorXStatus;
#endif

/**
 * @brief One time initialization of the task state.
 *
 */
LIBOS_SECTION_TEXT_COLD void KernelInitTask()
{
    KernelPrintf("Libos: Setting resume task.\n");

#if !LIBOS_TINY
    KernelPoolConstruct(sizeof(Task), LIBOS_POOL_TASK, KernelTaskDestroy);
    static __attribute__((aligned(4096))) LwU8 bootstrapTask[4096];
    KernelPoolBootstrap(LIBOS_POOL_TASK, &bootstrapTask[0], sizeof(bootstrapTask));

    KernelPoolConstruct(sizeof(Port), LIBOS_POOL_PORT, (void (*)(void *))KernelPortDestroy);
    static __attribute__((aligned(4096))) LwU8 bootstrapPort[4096];
    KernelPoolBootstrap(LIBOS_POOL_PORT, &bootstrapPort[0], sizeof(bootstrapPort));

    KernelPoolConstruct(sizeof(Shuttle), LIBOS_POOL_SHUTTLE, (void (*)(void *))KernelSyscallShuttleReset);
    static __attribute__((aligned(4096))) LwU8 bootstrapShuttle[4096];
    KernelPoolBootstrap(LIBOS_POOL_SHUTTLE, &bootstrapShuttle[0], sizeof(bootstrapShuttle));

    KernelPoolConstruct(sizeof(Lock), LIBOS_POOL_LOCK, (void (*)(void *))KernelLockDestroy);
#endif

    LwU64 xstatus = KernelCsrRead(LW_RISCV_CSR_XSTATUS);
    xstatus = xstatus & ~DRF_SHIFTMASK64(LW_RISCV_CSR_XSTATUS_XPP);
    xstatus |= REF_DEF64(LW_RISCV_CSR_XSTATUS_XPIE, _ENABLE);

    userXStatus =
        xstatus | REF_DEF64(LW_RISCV_CSR_XSTATUS_XPP, _USER);

#if !LIBOS_TINY
    supervisorXStatus = 
        xstatus | REF_DEF64(LW_RISCV_CSR_XSTATUS_XPP, _X);
#endif
}

LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelTaskPanic(void)
{
    KernelTrace("KernelTaskPanic: task-id:%d\n", KernelSchedulerGetTask()->id);
    KernelAssert(!KernelSchedulerIsTaskWaiting(KernelSchedulerGetTask()));

#if !LIBOS_TINY
    Task * self = KernelSchedulerGetTask();

    // Send the task state (registers) to the parent task
    ShuttleBindSend(self->crashSendShuttle,  LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER, (LwU64)&self->state, sizeof(self->state));   
    ShuttleBindGrant(self->crashSendShuttle, self->replayWaitShuttle);
    ShuttleBindRecv(self->replayWaitShuttle, LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER, (LwU64)&self->state, sizeof(self->state));   

    // Release any critical sections held by the faulting task.
    if (criticalSection)
        KernelPanic();

    criticalSection = LW_FALSE;

    PortSendRecvWait(self->crashSendShuttle,  &self->base, 
                     self->replayWaitShuttle, self->portSynchronousReply, 
                     TaskStateWaitTrapped, self->replayWaitShuttle, -1ULL, 0);

    KernelTaskReturn();
#else
    KernelTrace("KernelTaskPanic: Task exited\n");
    KernelSchedulerGetTask()->shuttleSleeping = 0;
    KernelSchedulerWait(0);
    KernelTaskReturn();
#endif
}

// used solely by syscall_dispatch.s
LIBOS_SECTION_IMEM_PINNED LIBOS_NORETURN void KernelSyscallReturn(LibosStatus status) 
{ 
    KernelSchedulerGetTask()->state.registers[LIBOS_REG_IOCTL_STATUS_A0] = status;
    KernelTaskReturn();
}

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

#if LIBOS_FEATURE_PAGING
    KernelPagingPrepareForPartitionSwitch();
#endif

    KernelFenceFull();

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
    SeparationKernelPartitionSwitch(param1, param2, param3, param4, param5, partition_id);
#elif LIBOS_FEATURE_SHUTDOWN
#if LIBOS_TINY
    SeparationKernelShutdown();
#else // LIBOS_TINY
    LwosRootPartitionExit();
#endif // LIBOS_TINY
#else
    while(1);
#endif
}

LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn()
{
    KernelSchedulerExit();
#if !LIBOS_TINY    
    KernelTrace("Restoring task context task-id:%d asid:%p pc:%llx pte:%llx\n", KernelSchedulerGetTask()->id, KernelSchedulerGetTask()->asid, KernelSchedulerGetTask()->state.xepc, KernelSchedulerGetTask()->state.radix);
#endif
    KernelContextRestore(KernelSchedulerGetTask());
}

#if LIBOS_TINY
void * KernelObjectResolveInternal(Task * self, LwU64 id, LwU8 typeMask, LwU64 grant)
{
    if (typeMask == TinyTypeShuttle)  
    {
        LwU64 hash = ((id * LIBOS_HASH_MULTIPLIER) >> 32) ^ KernelSchedulerGetTask()->tableKey;
        LwU64 mask = (LwU64)&NotaddrShuttleTableMask;
        LwU64 slot = hash & mask;

        for (int displacement = 0; displacement <= LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT; displacement++)
        {
            if (shuttleTable[slot].owner == KernelSchedulerGetTask()->id &&
                shuttleTable[slot].taskLocalHandle == id)
                    return &shuttleTable[slot];
            slot = (slot + 1) & mask;
        }
        
        KernelTrace("    KernelTaskHandleEntry failed to find shuttle %d\n", (LwU32)id);
        return 0;
    }
    else
    {
        LwU64 hash = ((id * LIBOS_HASH_MULTIPLIER) >> 32) ^ KernelSchedulerGetTask()->tableKey;
        LwU64 mask = (LwU64)&NotaddrPortTableMask;
        LwU64 slot = hash & mask;

        // @todo: The mask, objectTable and portArray computations could use offsets in place of indices
        for (int displacement = 0; displacement <= LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT; displacement++) {
            if (objectTable[slot].owner == KernelSchedulerGetTask()->id &&
                objectTable[slot].id == id &&
                (objectTable[slot].grant & grant) &&
                (objectTable[slot].typeMask & typeMask) == typeMask)
                    return (Port *)((LwU64)portArray + objectTable[slot].offset);
            slot = (slot + 1) & mask;
        }
        KernelTrace("    KernelTaskHandleEntry failed to find port %d of grant-mask %llx hash:%llx slot:%lld\n", (LwU32)id, grant, hash, hash & mask);
        return 0;
    }
}
#endif


#if !LIBOS_TINY
void * KernelTaskHandleEntry(Task * self, LwU64 handle, LwU64 * grant)
{
    if (handle >= self->tableFilled)
        return 0;
    *grant = self->table[handle].grant;
    return self->table[handle].target;
}

__attribute__((used)) LibosStatus KernelSyscallHandleClose(LibosHandle handle)
{
    struct Task * self = KernelSchedulerGetTask();

    if (handle >= self->tableFilled)
        return LibosErrorArgument;

    void * target = self->table[handle].target;
    KernelPoolRelease(target);

    self->table[handle].grant = 0;
    self->table[handle].target = 0;

    return LibosOk;
}


LibosStatus KernelTaskRegisterObject(Task * target, LibosHandle * outHandle, void * object, LwU64 grants, LwU64 probeHandle)
{   
    // @todo: Handle searching free linked list and growing through
    //        KernelAllocate(...)
    KernelPanilwnless(target->tableFilled < target->tableSize);

    KernelAssert(probeHandle < LibosHandleFirstUser);
    KernelAssert(target->tableFilled >= LibosHandleFirstUser);

    LwU64 handle = probeHandle;
    if (!handle)
        handle=target->tableFilled++;

    if (outHandle)
        *outHandle = handle;

    ObjectTableEntry * entry = &target->table[handle];
    KernelPoolAddref(object);
    entry->target = object;
    entry->grant  = grants;

    KernelTrace("KernelTaskRegisterObject: task-index:%d handle:%lld -> %p\n", target->id, handle, object);
    
    return LibosOk;
}

LibosStatus KernelCreateAddressSpace(struct Task * task, LibosHandle * handle)
{
    struct Task * self = KernelSchedulerGetTask();
    LibosHandle asid = 0;

    AddressSpace * addressSpace;

    // Allocate and initialize a new address space object
    LibosStatus status = KernelAllocateAddressSpace(LW_TRUE, &addressSpace);
    if (status != LibosOk)
        return status;

    status = KernelTaskRegisterObject(self, &asid, addressSpace, LibosGrantAddressSpaceAll, 0);
    KernelPoolRelease(addressSpace);

    if (status != LibosOk)
        return status;
    
    *handle = asid;

    return LibosOk;
}

// @todo: This method should return the VA if va was zero
__attribute__((used)) LibosStatus KernelSyscallAddressSpaceMapPhysical(LwU64 asid, LwU64 va, LwU64 pa, LwU64 size, LwU64 flags)
{
    // @todo: Security check for PA grant
    KernelTrace("KernelSyscallAddressSpaceMapPhysical asid:%lld va:%llx pa:%llx, size:%llx, flags:%llx\n", asid, va, pa, size, flags);
    AddressSpace * addressSpace = KernelObjectResolve(KernelSchedulerGetTask(), asid, AddressSpace, LibosGrantAddressSpaceAll);
    
    if (!addressSpace)
        return LibosErrorArgument;

    AddressSpaceRegion * region = KernelAddressSpaceAllocate(addressSpace, va, size);
    if (!region)
        return LibosErrorOutOfMemory;

    return KernelAddressSpaceMapContiguous(addressSpace, region, 0, pa, size, flags);
}

// @note: Solely for base class constructors
void KernelPortConstruct(Port * port) 
{
    KernelAssert(DynamicCast(Port, port) && "Port objects may exist only on the heap.");
    ListConstruct(&port->waitingSenders);
    ListConstruct(&port->waitingReceivers);
	port->globalPort = LWOS_ROOTP_GLOBAL_PORT_NONE;
    port->globalPortLost = LW_FALSE;
}

LibosStatus KernelPortAllocate(
    Port * * port
)
{
    Port * newPort = (Port *)KernelPoolAllocate(LIBOS_POOL_PORT);
    if (!newPort)
        return LibosErrorOutOfMemory;

    KernelPortConstruct(newPort);
    *port = newPort;

    return LibosOk;
}


LibosStatus KernelTaskConstruct(
    Task * task, 
    LibosPriority priority, 
    LwU64 entryPoint,
    LwBool supervisorMode)
{
    static LwU64 taskId = 0;

    // Initialize the handle table
    task->tableSize = 128;
    task->tableFilled = LibosHandleFirstUser;

    // Tasks are also ports
    KernelPortConstruct(&task->base);
    
    ListConstruct(&task->shuttlesCompleted);

    // @note: Task Id's aren't re-used.  See options above for handling that.
    task->id = ++taskId;

    task->taskState = TaskStateReady;

    task->priority = priority; 

    // Set entry point and arguments
    task->state.registers[RISCV_SP] = 0;
    task->state.registers[RISCV_RA] = 0;
    task->state.xepc = entryPoint;
    task->state.xstatus = supervisorMode ? supervisorXStatus : userXStatus;

    LibosStatus status = KernelPortAllocate(&task->portSynchronousReply);
    if (status != LibosOk)
        return status;

    status = KernelShuttleAllocateAndRegister(task, LibosGrantShuttleAll,  LibosShuttleSyncSend, &task->syncSendShuttle);
    if (status != LibosOk)
        return status;          // @note: Caller will destroy task and release shuttles

    KernelShuttleAllocateAndRegister(task, LibosGrantShuttleAll, LibosShuttleSyncRecv, &task->syncRecvShuttle);
    if (status != LibosOk)
        return status;
    
    // @note: These shuttles are in the handle table with no permissions granted (kernel only)
    KernelShuttleAllocateAndRegister(task, 0, 0, &task->crashSendShuttle);
    if (status != LibosOk)
        return status;

    KernelShuttleAllocateAndRegister(task, 0, 0, &task->replayWaitShuttle);
    if (status != LibosOk)
        return status;

    // Register the kernel server
    extern Port * kernelServerPort; // @todo header
    status = KernelTaskRegisterObject(task, 0, kernelServerPort, LibosGrantPortSend, LibosKernelServer);  
    if (status != LibosOk)
    {
        KernelPoolRelease(task);
        return status;
    }    

    // The non-crashed state of the task holds a reference count on itself
    // This is released on termination.
    
    return LibosOk;
}


void KernelTaskDestroy(void * ptr)
{
    Task * task = (Task *)ptr;

    // Ensure we're not linked into any scheduler structures
    KernelAssert(KernelSchedulerIsTaskWaiting(task));

    // Cancel the timer wakeup on this task
    if (KernelSchedulerIsTaskWaitingWithTimeout(task))
        KernelTimerCancel(task);

    // Tear down any owned pointers
    KernelPoolRelease(task->shuttleSleeping);
    KernelPoolRelease(task->portSynchronousReply);
    KernelPortDestroy(&task->base);

    // Release the address space reference count
    KernelPoolRelease(task->asid);

    // Release handles
    for (LwU32 i = 0; i < task->tableFilled; i++)
        KernelPoolRelease(task->table[i].target);
}

/*
    Option 1. Tasks have the same handle in all tasks
        - Global task-id map ensures unique task-id's
        - Task's can "hold open" an id by keeping the handle
          in their local table.
        - Port won't return the handle unless here.
    Option 2. Tasks have different handles in different tasks
        - Port must perform an ilwerted task id translation
*/

/**
 * @brief Creates a new task 
 * 
 * Each task has its own task-local handle table.
 * 
 * Caller provides the addressSpace the task is to run in,
 * as well as the memorySet for memory allocation.
 * 
 * @note: Tasks create with addressSpace==kernelAddressSpace
 *        will be selected to run in S-mode not U-mode.
 *        Pre-emption still behaves as usual.  
 * 
 * @param priority 
 * @param addressSpace 
 * @param memorySet 
 * @param entryPoint 
 * @return LibosStatus 
 */
LibosStatus KernelTaskCreate(
    LibosPriority                priority, 
    AddressSpace               * addressSpace, 
    MemorySet                  * memorySet,
    LwU64                        entryPoint,
    LwBool                       createWaitingOnPort,
    Task                      ** outTask
)
{
    LibosStatus status = LibosOk;
    Task * task;

    // Allocate the task
    task = (Task *)KernelPoolAllocate(LIBOS_POOL_TASK);
    if (!task) 
        return LibosErrorOutOfMemory;
    
    KernelTrace("KernelTaskCreate: %p entry:%llx\n", task, entryPoint);
    // Initialize the task (without ASID)
    // @todo: user spawn fix
    status = KernelTaskConstruct(task, priority, entryPoint, addressSpace == kernelAddressSpace /* S mode */);
    if (status != LibosOk)
        return status;

    // Bind LibosDefaultMemorySet
    // @todo: Move into TaskInitialize
    if (memorySet) {
        status = KernelTaskRegisterObject(task, 0, memorySet, LibosGrantMemorySetAll, LibosDefaultMemorySet);  
        if (status != LibosOk)
        {
            KernelPoolRelease(task);
            return status;
        }
    }

    // Internal shuttles used for crash/replay are not entered in the handle table
    // @todo: Why can't we move this in?
    status = KernelShuttleAllocateAndRegister(task, LibosGrantShuttleAll, LibosShuttleInternalCrash, &task->crashSendShuttle);
    if (status != LibosOk)
    {
        KernelPoolRelease(task);
        return status;
    }

    status = KernelShuttleAllocateAndRegister(task, LibosGrantShuttleAll,  LibosShuttleInternalReplay, &task->replayWaitShuttle);
    if (status != LibosOk)
    {
        KernelPoolRelease(task);
        return status;
    }

    // Bind the address space to the task
    task->asid = addressSpace;
    KernelPoolAddref(addressSpace);
    
    task->state.radix = (LwU64)task->asid->pagetable;

    // Setup a stack
    // @todo: Caller provides size.
    // @todo: Move into Init
    AddressSpaceRegion * stackRegion = 0;
    LwU64                virtualAddress = 0;
    status = KernelAddressSpaceMap(
        addressSpace, 
        &stackRegion,         
        &virtualAddress,
        1ULL << 16,
        LibosMemoryReadable | LibosMemoryWriteable | (addressSpace==kernelAddressSpace ? LibosMemoryKernelPrivate : 0),
        LibosMemoryFlagPopulate,
        memorySet);

    if (status != LibosOk) {
        KernelPoolRelease(task);
        return status;
    }
    // @todo: We need to stuff this pointer away in the
    //        Task structure so that it's lifetime is tied to it.

    task->state.registers[RISCV_SP] = virtualAddress + (1ULL << 16) - 8;

    // @todo: Only grant bind access (not all access)
    status = KernelTaskRegisterObject(task, 0, addressSpace, LibosGrantAddressSpaceAll, LibosDefaultAddressSpace);  
    if (status != LibosOk) {
        KernelPoolRelease(task);
        return status;
    }

    // Set the flag to prevent the wakeup message from
    // writing to any of the register state
    task->taskState = TaskStateWaitTrapped;
    
    // Waiting on replay command
    // @todo: Why aren't we using bind shuttle?
    if (createWaitingOnPort)
    {
        task->syncRecvShuttle->state = ShuttleStateRecv;
        task->syncRecvShuttle->waitingOnPort = &task->base;
        task->syncRecvShuttle->grantedShuttle = 0;
        task->syncRecvShuttle->payloadAddress = (LwU64)&task->state.registers[RISCV_A0];
        task->syncRecvShuttle->payloadSize = 8 * sizeof(LwU64);  // A0..A7
        task->syncRecvShuttle->copyFlags = LIBOS_SHUTTLE_COPY_FLAG_KERNEL_BUFFER | LIBOS_SHUTTLE_COPY_FLAG_MAP_HANDLES;

        PortEnqueReceiver(&task->base, task->syncRecvShuttle);

        task->shuttleSleeping = task->syncRecvShuttle;        
    }

    KernelPrintf("Libos: Task spawned\n");
    *outTask = task;

    return LibosOk;
}

void KernelShuttleConstructInternal(Task * owner, Shuttle * shuttle, LibosShuttleHandle handle)
{
    shuttle->state = ShuttleStateReset;
    shuttle->owner = owner;
    shuttle->taskLocalHandle = handle;
}

static LibosStatus KernelShuttleAllocateAndRegister(
    Task            * owner,
    LibosGrantShuttle grant,
    LwU64             probeHandle,
    Shuttle       * * outShuttle)
{
    Shuttle * shuttle = KernelPoolAllocate(LIBOS_POOL_SHUTTLE);
    if (!shuttle)
        return LibosErrorOutOfMemory;

    LibosHandle handle;
    LibosStatus status = KernelTaskRegisterObject(owner, &handle, shuttle, grant, probeHandle);
    if (status != LibosOk) {
        KernelPoolRelease(shuttle);
        return status;
    }
    
    KernelShuttleConstructInternal(owner, shuttle, handle);
    *outShuttle = shuttle;
   
    return LibosOk;
}


__attribute__((used)) LibosStatus KernelSyscallShuttleCreate()
{
    Task * self = KernelSchedulerGetTask();

    // Allocate the shuttles
    Shuttle * shuttle;
    LibosStatus status = KernelShuttleAllocateAndRegister(self, LibosGrantShuttleAll, 0, &shuttle);
    if (status != LibosOk)
        return status;
    
    // Return task in register t0 
    self->state.registers[RISCV_T0] = shuttle->taskLocalHandle;

    // Release our handle (it will still be in the table)
    KernelPoolRelease(shuttle);

    return LibosOk;
}

void KernelLockConstruct(Lock *lock)
{
    KernelPortConstruct(&lock->port);
    lock->unlockShuttle.state = ShuttleStateReset;
    lock->holdCount = 1;
    lock->holder = KernelSchedulerGetTask();
    KernelLockRelease(lock);
}

LibosStatus KernelLockAllocate(Lock **lock)
{
    Lock *newLock = KernelPoolAllocate(LIBOS_POOL_LOCK);
    if (!newLock)
        return LibosErrorOutOfMemory;

    KernelLockConstruct(newLock);
    *lock = newLock;

    return LibosOk;
}

__attribute__((used)) LibosStatus KernelSyscallLockCreate()
{
    Task *self = KernelSchedulerGetTask();

    Lock *lock;
    LibosStatus status = KernelLockAllocate(&lock);
    if (status != LibosOk)
    {
        return status;
    }

    LibosHandle outLock;
    status = KernelTaskRegisterObject(self, &outLock, lock, LibosGrantLockAll, 0);
    if (status != LibosOk)
    {
        return status;
    }

    KernelPoolRelease(lock);

    self->state.registers[RISCV_T0] = outLock;

    return LibosOk;
}

#endif
