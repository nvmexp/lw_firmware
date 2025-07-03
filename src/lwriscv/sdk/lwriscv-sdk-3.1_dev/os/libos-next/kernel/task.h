/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBOS_TASK_H
#define LIBOS_TASK_H

#include "../include/libos_task_state.h"
#include "kernel.h"
#include "sched/port.h"
#include "sched/timer.h"
#include "rootfs.h"
#if !LIBOS_TINY
#   include "mm/address_space.h"
#   include "mm/objectpool.h"
#endif

typedef enum __attribute__ ((__packed__)) {
    TaskStateReady,
    TaskStateWaitTimeout,
    TaskStateWait,
    TaskStateWaitTrapped
} TaskState;


typedef enum __attribute__ ((__packed__)) {
    TaskContinuationNone,
    TaskContinuationRecv,
    TaskContinuationWait
} TaskContinuation;

typedef struct ObjectTableEntry
{
#if LIBOS_TINY
    // LIBOS_TINY uses a single hash-table for all
    // handles.  These are the keys
    LwU8                   owner;
    LibosPortHandle        id;      
#endif

    // LIBOS_TINY uses a compressed pointer here 
    // to reduce the table size
#if LIBOS_TINY
    LwU16                  offset;  // relative to portArray
    LwU8                   typeMask;
#else
    void                 * target;  // Inside slab and supports DynamicCast
#endif

    LibosGrantUntyped      grant;
} ObjectTableEntry;


struct Task
{
#if !LIBOS_TINY
    Port                      base;
    BORROWED_PTR Shuttle                 *  crashSendShuttle;
    BORROWED_PTR Shuttle                 *  replayWaitShuttle;
    BORROWED_PTR Shuttle                 *  syncSendShuttle;
    BORROWED_PTR Shuttle                 *  syncRecvShuttle;
#endif

    LwU64                     timestamp;
    ListHeader                schedulerHeader;
    ListHeader                timerHeader;
    ListHeader                shuttlesCompleted;

    TaskState                 taskState;

    LibosTaskHandle           id;

#if LIBOS_FEATURE_PRIORITY_SCHEDULER
    LibosPriority             priority;
#endif

#if !LIBOS_TINY
    OWNED_PTR AddressSpace            * asid;
#endif

    LibosTaskState            state;

#if LIBOS_TINY
    Port                      portSynchronousReply;
#else
    OWNED_PTR Port          * portSynchronousReply;
#endif

    // @todo: Make this specific to LIBOS_LIGHT when kernel VMA is enabled
    
#if LIBOS_TINY
    LwU64                     mpuEnables[2 /*(DRF_MASK(LW_RISCV_CSR_MMPUIDX_INDEX) + 1 + 63)/64*/];
#endif

#if LIBOS_TINY
    LwU64                     tableKey;
#endif

    // WAIT
    OWNED_PTR LIBOS_PTR32(Shuttle)      shuttleSleeping;

#if !LIBOS_TINY
    ObjectTableEntry          table[128];
    LwU64                     tableFilled;
    LwU64                     tableSize;
#endif

};

extern LwBool  criticalSection;

#if LIBOS_TINY
   extern Task TaskInit;
#else
   extern Task * TaskInit;
#endif

LIBOS_NORETURN void KernelTaskPanic(void);

void LIBOS_NORETURN                         KernelSyscallReturn(LibosStatus status);

void                                        KernelInitTask(void);
LibosStatus                                 ShuttleSyscallReset(LibosShuttleHandle reset_shuttle_id);
void                                        KernelPortTimeout(Task *owner);
void                                        kernel_task_replay(LwU32 task_index);
void                                        ListLink(ListHeader *id);
void                                        ListUnlink(ListHeader *id);
void LIBOS_NORETURN                         kernel_port_signal();

LIBOS_NORETURN void KernelSyscallPartitionSwitch();
LIBOS_NORETURN void KernelSyscallTaskCriticalSectionEnter();
LIBOS_NORETURN void KernelSyscallTaskCriticalSectionLeave();

extern LwU64 LIBOS_SECTION_DMEM_PINNED(userXStatus);

LIBOS_NORETURN LIBOS_SECTION_IMEM_PINNED void KernelTaskReturn();

#if LIBOS_TINY
void * KernelObjectResolveInternal(Task * self, LwU64 id, LwU8 typeMask, LwU64 grant);
#else
void * KernelTaskHandleEntry(Task * self, LwU64 handle, LwU64 * grant);
#endif

#define TinyTypeShuttle 1
#define TinyTypePort    2
#define TinyTypeTimer   4 
#define TinyTypeLock    8

// @note: This macro allows type safe lookups to the object table
//        Ideally, this would be a template in C++
#if LIBOS_TINY
#define KernelObjectResolve(self, handle, T, requestedGrant)                        \
    KernelObjectResolveInternal(self, handle, TinyType##T, requestedGrant)

#else
#define KernelObjectResolve(self, handle, T, requestedGrant)                        \
    ({                                                                              \
        /* Lookup the handle */                                                     \
        LwU64 grantContained;                                                       \
        void * object = KernelTaskHandleEntry(self, handle, &grantContained);       \
        /* Verify we have the permissions we need before casting */                 \
        T * addr = 0;                                                               \
        if (object && (grantContained & requestedGrant) == requestedGrant)          \
            /* Perform a type safe cast */                                          \
            addr = DynamicCast(T, object);                                          \
        if (addr) KernelPoolAddref(addr);                                           \
        addr;                                                                       \
    })
#endif
#if !LIBOS_TINY



// used in init.c
LibosStatus KernelTaskConstruct(
    Task * task, 
    LibosPriority priority, 
    LwU64 entryPoint,
    LwBool supervisorMode);

LibosStatus KernelPortAllocate(Port * * port);
void KernelPortConstruct(Port * port);

// Also init.c
LibosStatus KernelTaskBindAddressSpace(Task * task, LwU64 asid);
LibosStatus KernelCreateAddressSpace(struct Task * task, LibosHandle * handle);
LibosStatus KernelSyscallAddressSpaceMapPhysical(LwU64 asid, LwU64 va, LwU64 pa, LwU64 size, LwU64 flags);
LibosStatus KernelTaskRegisterObject(Task * target, LibosHandle * outHandle, void * object, LwU64 grants, LwU64 probeHandle);

LibosStatus KernelTaskCreate(
    LibosPriority                priority, 
    AddressSpace               * addressSpace, 
    MemorySet                  * memorySet,
    LwU64                        entryPoint,
    LwBool                       waitingOnPort,
    Task                     * * outTask
);
#endif

#if LIBOS_TINY
extern Task                 taskTable[];
extern ObjectTableEntry     objectTable[];
#define LIBOS_HASH_MULTIPLIER     0x72777af862a1faeeULL
#endif

#if !LIBOS_TINY
void KernelTaskDestroy(void * ptr);
LibosStatus KernelSyscallHandleClose(LibosHandle handle);
#endif

#endif
