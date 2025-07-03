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

#include "lwmisc.h"
#include "lwtypes.h"

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

#define CONTAINER_OF(p, type, field) ((type *)((LwU64)(p)-offsetof(type, field)))


/**
 * @brief Flushes GSP's DCACHE
 *
 * @note This call has no effect on PAGED_TCM mappings.
 *
 * @param base
 *     Start of memory region to ilwalidate
 * @param size
 *     Total length of the region to ilwalidate in bytes
 */
void LibosCacheIlwalidate(void *base, LwU64 size);


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
#if LIBOS_FEATURE_PARTITION_SWITCH
    #define LIBOS_PARTITION_SWITCH_ARG_COUNT 5

    void LibosPartitionSwitch
    (
#   if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
        LwU64 partitionId,
        const LwU64 inArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT],
        LwU64 outArgs[LIBOS_PARTITION_SWITCH_ARG_COUNT]
#   endif
    );
#endif


/*
 * @brief This is a helper function for use by the init task to handle
 *         child task termination/crash, partition switching, and other
 *         core daemon functionality.
 *
 *   This function will call 
 *       LibosInitHookTaskTrap on child task exit/crash
 *       
 *   On tiny builds, you'll also receive
 *       LibosInitHookMpu when the conents of MPU has been reset.  
 *           The user is expected to call LibosBootstrapMmap.
 */

LwU32 LibosInitDaemon();

/*
 * @brief This function is called in tiny builds when the contents of the MPU 
 *        have been lost (due to a partition switch).  The user is expect to
 *        reload the MPU with calls to LibosInitHookMpu.
 * 
 */
void LibosInitHookMpu();

/*
 * @brief This function is called when a child task crashes or exits.
 *        You may optionally re-schedule the task by replying on the 
 *        shuttleTrap with a new LibosTaskState structure [includes registers]
 */
#if LIBOS_FEATURE_USERSPACE_TRAPS
void LibosInitHookTaskTrap(LibosTaskState * taskState, LibosShuttleId taskInitShuttleTrap);
#endif

/**
 * @brief Use DMA to transfer data from a DMA buffer to/from a TCM buffer.
 *
 * The DMA buffer should be declared with the DMA memory region attribute.
 * The TCM buffer should be declared with the PAGED_TCM memory region attribute.
 *
 * @param[in] tcmVa    Virtual address of TCM buffer
 * @param[in] dmaVA    Virtual address of DMA buffer
 * @param[in] size     Number of bytes to transfer
 * @param[in] from_tcm LW_FALSE - transfer from DMA buffer to TCM buffer
 *                     LW_TRUE  - transfer from TCM buffer to DMA buffer
 *
 * @note The tcmVa, dmaVA and size arguments must all be 4-byte aligned.
 *
 * @returns
 *    LibosErrorAccess
 *      One of the input arguments is not 4-byte aligned.
 *    LibosErrorArgument
 *      dmaSrc argument does not represent a valid memory region.
 *      The underlying physical memory of the DMA buffer is not in a valid sysmem or fbmem aperture.
 *    LibosErrorFailed
 *      The destination TCM buffer could not be accessed.
 *    LibosOk
 *      The DMA transfer was successful.
 */
LibosStatus LibosDmaCopy(LwU64 tcmVa, LwU64 dmaVA, LwU64 size, LwBool fromTcm);

/**
 * @brief Issue a DMA flush operation.
 *
 * This call waits for all pending DMA operations to complete before returning.
 */
void LibosDmaFlush(void);

#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0

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
 * @brief Register ilwerted page-table (init task only)
 *
 * This call waits for all pending DMA operations to complete before returning.
 */
#if LIBOS_FEATURE_PAGING
void LibosInitRegisterPagetables(libos_pagetable_entry_t *hash, LwU64 lengthPow2, LwU64 probeCount);
#endif

/**
 * @brief Enable GSP profiler and set up the hardware profiling counters.
 */
#if LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND
void LibosProfilerEnable(void);
#endif

/**
 * @brief KernelBootstrap a mapping into the MPU/TLB
 *
 * @note This call may only be used in init before the scheduler
 *       and paging subsystems have been enabled.
 *
 *       All addresses and sizes must be a multiple of
 *       LIBOS_CONFIG_MPU_PAGESIZE.
 *
 * @param va
 *     Virtual address of start of mapping. No collision detection
 *     will be performed.
 * @param pa
 *     Physical address to map
 * @param size
 *     Length of mapping.
 * @param attributes
 *     Processor specific MPU attributes or TLB attributes for mapping.
 */

#if !LIBOS_FEATURE_PAGING
#   define LIBOS_MPU_CODE                       0
#   define LIBOS_MPU_DATA_INIT_AND_KERNEL       1
#   define LIBOS_MPU_MMIO_KERNEL                2
#   define LIBOS_MPU_USER_INDEX                 3
#endif
void LibosBootstrapMmap(LwU64 index, LwU64 va, LwU64 pa, LwU64 size, LwU64 attributes);


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

__attribute__((noreturn))
static inline void LibosTaskExit(LwU64 exitCode)
{
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_TASK_EXIT;
    register LwU64 a0 __asm__("a0") = exitCode;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0) : "memory");
    __builtin_unreachable();
}

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
    __asm__ __volatile__("ecall" : : "r"(t0) : "memory");
    __builtin_unreachable();
}

#if LIBOS_CONFIG_EXTERNAL_INTERRUPT
static inline LibosStatus LibosInterruptAsyncRecv(LibosShuttleId interruptShuttle, LibosPortId interruptPort, LwU32 interruptWaitingMask)
{
    // Queue a receive on the interrupt port
    LibosStatus status = LibosPortAsyncRecv(interruptShuttle, interruptPort, 0, 0);
    if (status != LibosOk)
        return status;

    // Reset and clear any prior interrupts to avoid race where we miss the interrupt
    register LwU64 t0 __asm__("t0") = LIBOS_SYSCALL_INTERRUPT_ARM_EXTERNAL;
    register LwU64 a0 __asm__("a0") = interruptWaitingMask;
    register LwU64 a1 __asm__("a1") = (LwU64)interruptPort;
    __asm__ __volatile__("ecall" : : "r"(t0), "r"(a0), "r"(a1) : "memory");  

    return LibosOk;
}
#endif



/**
 * @brief Enters a scheduler critical section
 * 
 * Once in a scheduler critical section the kernel will not context switch
 * to another task on end of timeslice.  The kernel interrupt ISR is still 
 * enabled but all user-space interrupt servicing is disabled.
 * 
 * Ports:: Waits on shuttle completion are forbidden.  Asynchronous send/recv
 *         is allowed.
 * 
 * 
 * @param[in] behavior   Controls whether the critical section is released on 
 *                       task crash.
 */
typedef enum {
    LibosCriticalSectionNone = 0,
    LibosCriticalSectionPanicOnTrap = 1,
    LibosCriticalSectionReleaseOnTrap = 2
} LibosCriticalSectionBehavior;

void LibosCriticalSectionEnter(LibosCriticalSectionBehavior behavior);

/**
 * @brief Leaves a scheduler critical section
 */
void LibosCriticalSectionLeave(void);

void LibosProcessorShutdown(void);

#endif
