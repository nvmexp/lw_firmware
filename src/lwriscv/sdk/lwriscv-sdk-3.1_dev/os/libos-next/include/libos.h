/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBOS_H_
#define LIBOS_H_

#include "libos-config.h"
#include "lwmisc.h"
#include "lwtypes.h"


#define CONTAINER_OF(p, type, field) ((type *)((LwU64)(p)-offsetof(type, field)))

#if LIBOS_TINY
  typedef LwU8                            LibosPortHandle;
  typedef LwU8                            LibosShuttleHandle;
  typedef LwU8                            LibosTaskHandle;
  typedef LwU8                            LibosLockHandle;
#else
  typedef LwU32                           LibosHandle;
  typedef LwU32                           LibosPortHandle;
  typedef LwU32                           LibosShuttleHandle;
  typedef LwU32                           LibosTaskHandle;
  typedef LwU32                           LibosElfHandle;
  typedef LwU32                           LibosAddressSpaceHandle;
  typedef LwU32                           LibosPartitionHandle;
  typedef LwU32                           LibosMemoryHandle;
  typedef LwU32                           LibosLockHandle;
  typedef LwU32                           LibosRegionHandle;
  typedef LwU32                           LibosMemoryPoolHandle;
  typedef LwU32                           LibosMemorySetHandle;
  typedef LwU32                           LibosMappableBufferHandle;
#endif

#include "libos-config.h"
#include "libos_syscalls.h"
#include "libos_task_state.h"
#include "libos_status.h"
#include "libos_manifest.h"
#include "libos_port.h"
#include "libos_lock.h"
#include "libos_time.h"
#include "libos_assert.h"
#include "libos_log.h"
#include "libos_memory.h"
#if !LIBOS_TINY
#   include "libos_ipc.h"
#endif // !LIBOS_TINY
#include "libos_mpu_tiny.h"
#include <stddef.h> // offsetof

#if LIBOS_TINY
#   include "libos_mpu_tiny.h"
#endif

/**
 * @brief Flushes a buffer from any data caches.
 * 
 * @param base
 *     Start of memory region to ilwalidate
 * @param size
 *     Total length of the region to ilwalidate in bytes
 */
void LibosCacheIlwalidateData(void *base, LwU64 size);

/**
 * @brief Flushes all data caches.
 */
#if !LIBOS_TINY
void LibosCacheIlwalidateDataAll();
#endif

/**
 * @brief Yield the GSP to next KernelSchedulerRunnable task
 */
void LibosTaskYield(void);

/**
 * @brief Put the GSP into the suspended state
 *
 * This call issues a writeback and ilwalidate to the calling task's
 * page entries prior to notifying the host CPU that the GSP is
 * entering the suspended state.
 */
#define LIBOS_PARTITION_SWITCH_ARG_COUNT 5

void LibosPartitionSwitch
(
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0  
    LwU64 partitionId,
    const LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT],
    LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT]
#endif
);


#if LIBOS_CONFIG_GDMA_SUPPORT

/**
 * @brief Use GDMA to transfer data.
 */
LibosStatus LibosGdmaTransfer(LwU64 dstVa, LwU64 srcVa, LwU32 size, LwU64 flags);

/**
 * @brief Issue a GDMA flush operation.
 */
LibosStatus LibosGdmaFlush(void);

#endif

/**
 * @brief Enable GSP profiler and set up the hardware profiling counters.
 */
#if LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND
void LibosProfilerEnable(void);
#endif


/**
 * @brief Exit the current task such that it will no longer run.
 *
 * The exit code will be available to TASK_INIT.
 * This syscall does not return.
 *
 * Note: this syscall was made with voluntary task termination in mind
 * although it lwrrently has the same effect as LibosPanic (and is
 * handled by the trap handling in TASK_INIT).
 *
 * @param[in] exitCode   Exit code
 */

#if !LIBOS_TINY
__attribute__((noreturn)) void LibosTaskExit(LwU64 exitCode);
#endif

#define LIBOS_TASK_EXIT_NAKED()                                      \
    __asm__ __volatile__(                                             \
        "li t0, %[IMM_SYSCALL_TASK_EXIT];"                           \
        "ecall;"                                                      \
        :: [IMM_SYSCALL_TASK_EXIT] "i"(LIBOS_SYSCALL_TASK_EXIT)     \
    )

/**
 * @brief Panic the current task such that it will no longer run.
 *
 * The panic reason will be available to TASK_INIT's task trap handling.
 * This syscall does not return.
 *
 * Note: this syscall is also available via a function-like macro wrapper
 * (that only uses basic asm) to allow for its use in attribute((naked))
 * functions.
 *
 * @param[in] reason   Reason for panic
 */

// function-like macro wrapper for LibosPanic
#define LIBOS_TASK_PANIC_NAKED()                                      \
    __asm__ __volatile__(                                             \
        "li t0, %[IMM_SYSCALL_TASK_PANIC];"                           \
        "ecall;"                                                      \
        :: [IMM_SYSCALL_TASK_PANIC] "i"(LIBOS_SYSCALL_TASK_PANIC)     \
    )

__attribute__((noreturn))
static inline void LibosPanic()
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_PANIC;
    __asm__ __volatile__(LIBOS_SYSCALL_INSTRUCTION : : "r"(t0) : "memory");
    __builtin_unreachable();
}

// @todo: This should require an object so that we can do access control (Interrupt object)
LibosStatus LibosInterruptAsyncRecv(LibosShuttleHandle interruptShuttle, LibosPortHandle interruptPort, LwU32 interruptWaitingMask);
LibosStatus LibosInterruptBind(LibosPortHandle designatedPort);


void LibosProcessorShutdown(void);

#if !LIBOS_TINY
/**
 * Calls the root partition to destroy our partition. The root partition will
 * also reclaim any memory and global port IDs owned by us.
 * 
 * For security reasons, this function may only be called by the init task,
 * otherwise it will return LibosErrorAccess.
 */
LibosStatus LibosPartitionExit(void);
#endif // !LIBOS_TINY

// Handles present in all tasks
#if LIBOS_TINY
#   define LibosShuttleSyncSend ID(shuttleSyncSend)
#   define LibosShuttleSyncRecv ID(shuttleSyncRecv)
#   define LibosShuttleAny ID(shuttleAny)
#else
// @todo: These names don't conform to LIBOS naming patterns
#   define LibosDefaultAddressSpace       1
#   define LibosShuttleAny                2
#   define LibosShuttleSyncSend           3
#   define LibosShuttleSyncRecv           4
#   define LibosShuttleInternalCrash      5
#   define LibosShuttleInternalReplay     6
#   define LibosDefaultMemorySet          7
#   define LibosKernelServer              8
#   define LibosTaskSelf                  9
#   define LibosHandleFirstUser           10
#endif

#if LIBOS_TINY
    typedef LwU8 LibosGrantUntyped;
#else
    typedef LwU64 LibosGrantUntyped;
#endif

typedef enum {
    LibosGrantWait            = 1,
    LibosGrantPortSend        = 2,
    LibosGrantPortAll         = LibosGrantPortSend | LibosGrantWait,
} LibosGrantPort;

// Timer inherits from Port 
typedef enum __attribute__ ((__packed__)) {
    LibosGrantTimerWait       = LibosGrantWait,
    LibosGrantTimerSet        = 4,
    LibosGrantTimerAll        = LibosGrantTimerSet | LibosGrantTimerWait,
} LibosGrantTimer;

typedef enum {
    LibosGrantShuttleAll      = 1,
} LibosGrantShuttle;

typedef enum {
    LibosGrantAddressSpaceAll = 1,
} LibosGrantAddressSpace;

typedef enum {
    LibosGrantMemorySetAll = 1
} LibosGrantMemorySet;

typedef enum {
    LibosGrantMemoryPoolAll = 1
} LibosGrantMemoryPool;

typedef enum {
    LibosGrantTaskAll         = LibosGrantPortAll,
} LibosGrantTask;

typedef enum {
    LibosGrantElfAll          = 1,
} LibosGrantElf;

typedef enum {
    LibosGrantPartitionAll    = LibosGrantPortAll,
} LibosGrantPartition;

typedef enum {
    LibosGrantRegionAll = 1
} LibosGrantRegion;

typedef enum {
    LibosGrantLockAcquire     = LibosGrantWait,
    LibosGrantLockRelease     = LibosGrantPortSend | 8,
    LibosGrantLockAll         = LibosGrantLockAcquire | LibosGrantLockRelease,
} LibosGrantLock;


#define LibosMaxPath 128

#if !LIBOS_TINY

#define LibosCommandLoaderElfOpen                 0x30
#define LibosCommandLoaderElfMap                  0x31

typedef struct
{   
    // @out: ELF handle
    LIBOS_MESSAGE_HEADER(handles, handleCount, 1);

    // Inputs
    char          filename[LibosMaxPath];

    // Outputs
    LibosStatus   status;
} LibosLoaderElfOpen;

typedef struct
{   
    // @in: AddressSpace, ELF
    LIBOS_MESSAGE_HEADER(handles, handleCount, 2);

    // Outputs
    LwU64         entryPoint;
    LibosStatus   status;
} LibosLoaderElfMap;


// @todo Timer creation API

LibosStatus LibosElfOpen(
    const char  * elfName,
    LibosHandle * handle
);

LibosStatus LibosElfMap(LibosAddressSpaceHandle asid, LibosElfHandle elf, LwU64 * entry);


/**
 * @brief Closes the handle 
 *
 * If this is the last task holding a handle to the object, it will be
 * destroyed.  
 *     
 * Port:: A destroyed port will release LibosWarnPortLost
 *        if any shuttles were actively waiting on the result *  
 *
 *
 * @param[in] handle   Handle for the task, partition, port, shuttle, etc.
 */

LibosStatus LibosHandleClose(
    LibosHandle handle
);

#endif

#endif
