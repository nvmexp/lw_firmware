/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <libos-config.h>
#include "lwmisc.h"
#include "lwtypes.h"
#include "libelf.h"
#include "extintr.h"
#include "kernel.h"
#include "profiler.h"
#include "libriscv.h"
#include "task.h"
#include "sched/timer.h"
#include "scheduler.h"
#include "trap.h"
#include "diagnostics.h"
#include "mm/pagetable.h"
#include "mm/memorypool.h"
#include "task.h"
#include "sched/global_port.h"
#include "mm/objectpool.h"
#if LIBOS_LWRISCV >= LIBOS_LWRISCV_2_0
#include "../root/root.h"
#include "lwriscv-2.0/sbi.h"
#endif
#if LIBOS_CONFIG_GDMA_SUPPORT
#include "gdma.h"
#endif
#include "mm/address_space.h"
#include "mm/softmmu.h"
#include "server/server.h"
#include "context.h"

/*!
 * @file init.c
 * @brief This module is responsible for configuring and booting the kernel.
 *        
 *   The _start entry point is called directly from the separation kernel when
 *   another partition sends  partition switch is requested to our partition.
 * 
 *   Generally, our partition will receive a verb in register a0.  This is a command
 *   such as INIT, RUN, GLOBAL_MESSAGE_PERFORM_SEND/RECV, or a NOTIFICATION.
 * 
 *   Unless we're given the RUN verb, we're expected to quickly process the RPC
 *   and return to the calling partition.
 * 
 *   Scenario. "cold" boot flow:
 *      _start - Enables the MPU with four pinned translations (code, data, pri, identity)
 *      KernelInit
 *          \-> KernelFirstBoot - One time initialize of the kernel data structures
 *
 *   Scenario. SeparationKernelPartitionCall
 *      @note This is used from the scheduler stack to synchronously call another partition.
 *            When the other partition is done, the separation kernel will switch back to
 *            our _start method.
 *      _start - Restores MPU mappings
 *      KernelInit - Restores trap handlers, and timer programming
 *          \-> SeparationKernelPartitionContinuation - Return control back to the scheduler stack.
 * 
 *   Scenario. SeparationKernelPartitionSwitch
 *      @note If called from the scheduler stack, all scheduler stack state is lost. 
 *            Our partition will not run code again until we receive the "RUN" verb.
 *              
 *   @note: SeparationKernelPartitionSwitch is used to 
 */

/**
 * @brief Bootstraps the pri mapping through manual MPU Injection.
 * 
 * This is done to enable printf() before the MM is fully up and running.
 * 
 */
static void KernelInitBootstrapPri()
{
    // Map a single 2mb page that contains the debuginfo register
    // the full mapping will be done in kernel
    LwU64 pageOffset = (LW_PRGNLCL_FALCON_DEBUGINFO - LW_RISCV_AMAP_INTIO_START) &~ (0x1fffff);

#if LIBOS_CONFIG_MPU_HASHED
    LwU64 idx = KernelBootstrapHashMedium(LIBOS_CONFIG_PRI_MAPPING_BASE+pageOffset)+1;
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, idx);
#else
    KernelCsrWrite(LW_RISCV_CSR_XMPUIDX, LIBOS_CONFIG_MPU_BOOTSTRAP + 1);
#endif
    KernelCsrWrite(
        LW_RISCV_CSR_XMPUATTR,
        REF_NUM64(LW_RISCV_CSR_XMPUATTR_XR, 1ULL) | REF_NUM64(LW_RISCV_CSR_XMPUATTR_XW, 1ULL));
    KernelCsrWrite(LW_RISCV_CSR_XMPUPA,  LW_RISCV_AMAP_INTIO_START + pageOffset);
    KernelCsrWrite(LW_RISCV_CSR_XMPURNG, LIBOS_CONFIG_PRI_MAPPING_RANGE);
    KernelCsrWrite(LW_RISCV_CSR_XMPUVA, (LIBOS_CONFIG_PRI_MAPPING_BASE + pageOffset) | 1ULL /* VALID */);

    KernelTrace("Libos: Bootstrapped Printf\n");
}

OWNED_PTR Task * TaskInit = 0;

/**
 * @brief Sets up the kernel for first boot.
 * 
 * The sequence of operations is critical.
 * @see KernelPoolBootstrap for how object allocation is supported
 *      before the main memory allocator is online
 * 
 * @param memoryStart 
 * @param memorySize 
 * @param globalPageStart 
 * @param globalPageSize 
 */
void KernelFirstBoot(LwU64 memoryStart, LwU64 memorySize, LwU64 globalPageStart, LwU64 globalPageSize)
{
    // Force our trap handler to fail if we crash
    // before we finish booting the kernel.
    KernelCsrWrite(LW_RISCV_CSR_XSCRATCH, 0);

    // One time injection of pri mapping into TLB
    // not required on later boots as we will create
    // pagetable mappings later
    KernelInitBootstrapPri();
    KernelTrace("Libos: Bootstrapped MPU for printf().\n");       

    // Bring up the physical memory allocator
    KernelInitMemoryPool();
    KernelInitMemorySet();

    // Register the physical memory
    OWNED_PTR MemoryPool * node;
    KernelTrace("Libos: Registering physical memory node %010llx-%010llx().\n", memoryStart, memoryStart+memorySize);  
    KernelPanilwnless(KernelAllocateMemoryPool(memoryStart, memorySize, &node) == LibosOk);

    // Create a physical memory node set for the kernel
    KernelPanilwnless(KernelMemorySetAllocate(&KernelPoolMemorySet) == LibosOk);
    
    // Add the newly created physical memory node to the kernel set
    KernelPanilwnless(KernelMemorySetInsert(KernelPoolMemorySet, node, 0) == LibosOk);
    node = 0;

    // Bring up the pagetables
    KernelInitPagetable();

    // Create the global kernel pagetable 
    KernelInitAddressSpace();

    // Bootstrap SOFT-TLB to a safe null task until we're 
    // fully up.
    static Task null = {0};
    KernelCsrWrite(LW_RISCV_CSR_XSCRATCH, (LwU64)&null);

    // Create pri mapping in main pagetables
    // @todo: Shouldn't we need to create a memory node for pri?
    //        This would remove some hacks related to not checkin for failed pins.
    KernelPanilwnless(LibosOk==KernelAddressSpaceMapContiguous(
        kernelAddressSpace, 
        kernelPrivMapping,
        0,
        LW_RISCV_AMAP_INTIO_START, 
        LIBOS_CONFIG_PRI_MAPPING_RANGE, 
        LibosMemoryReadable | LibosMemoryWriteable));

    // Clear out the bootstrapped pri entry created in KernelPagingBootstrap
    KernelSoftmmuFlushAll();
    KernelTrace("Libos: Bootstrap pri mapping replaced.\n");  
    
    // Initialize the scheduler subsystem
    KernelInitScheduler();

    // Initialize task subsystem (this early to ensure the address pool slab is up)
    KernelInitTask();

    // Initialize partition subsystem
    KernelInitPartition(globalPageStart, globalPageSize);

    // Create the server port
    KernelInitServer();

    // Create the address space for the init task
    AddressSpace * addressSpace;
    KernelPanilwnless(LibosOk == KernelAllocateAddressSpace(LW_TRUE, &addressSpace));

    // Mount the root filesystem
    imagePhysical = KernelGlobalPage()->args.libosInit.imageStart;
    KernelPanilwnless(LibosOk == KernelRootfsMountFilesystem(imagePhysical));     

    // Setup the loader
    KernelInitElfLoader();          

    // Open the init.elf
    LibosBootfsRecord * initElf = KernelFileOpen("init.elf");
    KernelPanilwnless(initElf);
    KernelPanilwnless(LibosOk == KernelElfMap(addressSpace, initElf));

    // Create the init task
    KernelPanilwnless(LibosOk == KernelTaskCreate(
        LIBOS_PRIORITY_NORMAL,
        addressSpace,
        KernelPoolMemorySet,
        LibosBootFindElfHeader(rootFs, initElf)->entry,
        LW_FALSE,               // not waiting on a port
        &TaskInit));

    // Pass arguments to the init task
    TaskInit->state.registers[RISCV_A0] = selfPartitionId;
    if (KernelPartitionParentPort)
    {
        LibosPortHandle outHandle;
        KernelPanilwnless(LibosOk == KernelTaskRegisterObject(TaskInit, &outHandle, KernelPartitionParentPort, LibosGrantWait, 0));
        TaskInit->state.registers[RISCV_A1] = outHandle;
    }
    else
        TaskInit->state.registers[RISCV_A1] = KernelRiscvRegisterEncodeLwU32(LibosPortHandleNone);

    // Initialize timer subsystem
    KernelInitTimer();               
    KernelTrace("Libos: Timer subsystem initialized.\n");        

    // Start the kernel server
    KernelServerRun();

    // Schedule the init task
    KernelSpinlockAcquire(&KernelSchedulerLock);
    KernelSchedulerRun(TaskInit);
    KernelSpinlockRelease(&KernelSchedulerLock);

    KernelTrace("Libos: Scheduled init task.\n");        
}

void LIBOS_NORETURN KernelDispatchVerb(LwU64 verb, LwU64 callingPartition, LwU64 verbAndInitArg0, LwU64 initArg1, LwU64 initArg2, LwU64 resvd1, LwU64 resvd2)
{
    /**
     * @brief Process an kernel<->kernel RPC from another partition
     * 
     */
    switch(verb)
    {
        case LWOS_VERB_LIBOS_RUN:
            // @todo: We really ought to be running on the scheduler stack at this point
            KernelTrace("Libos: Partition Run Verb.\n");      
            KernelTaskReturn();   
            break;

        case LWOS_VERB_LIBOS_INIT:
            KernelTrace("Libos: Partition Init Verb. caller=%lld\n", callingPartition);      
            // Security: Only the root partition can trigger this event
            //           Return to the calling partition with an error status otherwise
            if (callingPartition != LWOS_ROOT_PARTITION)
                SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorAccess), 0, 0, 0, 0, callingPartition);

            LwU64 physicalEntryPoint = KernelGlobalPage()->args.libosInit.libosEntrypoint;
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_ROOTP_INITIALIZE_COMPLETED,0), physicalEntryPoint, 0, 0, 0, LWOS_ROOT_PARTITION);
            break;
        
        case LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_SEND:
            KernelTrace("Libos: Global Message Perform Send Verb. caller=%lld\n", callingPartition);
            KernelHandleGlobalMessagePerformSend(initArg1, initArg2, callingPartition);
            break;

        case LWOS_VERB_LIBOS_GLOBAL_MESSAGE_PERFORM_RECV:
            KernelTrace("Libos: Global Message Perform Receive Verb. caller=%lld\n", callingPartition);
            KernelHandleGlobalMessagePerformRecv(callingPartition);
            break;
        
        case LWOS_VERB_LIBOS_PARTITION_EXIT_NOTIFICATION:
            KernelTrace("Libos: Partition Exit Notification Verb. caller=%lld\n", callingPartition);
            if (callingPartition != LWOS_ROOT_PARTITION)
            {
                SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorAccess), 0, 0, 0, 0, callingPartition);
            }
            KernelHandlePartitionExit(LWOS_A0_ARG(verbAndInitArg0));
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_ROOTP_PARTITION_EXIT_HANDLE_COMPLETE, 0), 0, 0, 0, 0, LWOS_ROOT_PARTITION);
            break;

        default:
            // Unknown verb
            SeparationKernelPartitionSwitch(LWOS_A0(LWOS_VERB_RETURN, LibosErrorArgument), 0, 0, 0, 0, callingPartition);
            break;            
    }
}
/**
 * @brief Exelwtes verb requests on behalf of other partitions
 * 
 * @note The LWOS root partition is responsible for issuing
 *       the init verb.
 * 
 * @param verbAndInitArg0 - Verb and first argument
 * @param initArg1 
 * @param initArg2 
 * @param initArg3 
 * @param initArg4 
 * @param callingPartition - Secure register from seperation kernel. Trusted to be calling partition.
 * @param reserved1 
 * @param dataStart 
 * @return LIBOS_NORETURN 
 */
LIBOS_NORETURN LIBOS_SECTION_TEXT_COLD void KernelPartitionEntry(
    LwU64 verbAndInitArg0, 
    LwU64 initArg1, 
    LwU64 initArg2, 
    LwU64 initArg3, 
    LwU64 initArg4, 
    LwU64 callingPartition,
    LwU64 reserved1,
    LwU64 dataStart)  
{
    LwU64 verb     = LWOS_A0_VERB(verbAndInitArg0);

    // Load trap handler 
    KernelTrapLoad();

    /**
     * @brief RISC-V does not grant access to the user timer or cycle registers
     *        by default.  We need to explicitly enable them.
     * 
     */
    KernelCsrSet(
        LW_RISCV_CSR_XCOUNTEREN,
        REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_TM, 1u) | REF_NUM64(LW_RISCV_CSR_XCOUNTEREN_CY, 1u));

    /**
     * @brief The initialization verb must be handled before loading the scheduler.
     * 
     */
    if (verb == LWOS_VERB_LIBOS_INIT)
        KernelFirstBoot(initArg1, initArg2, initArg3, initArg4);
    else
        KernelCsrWrite(LW_RISCV_CSR_XSCRATCH, (LwU64)TaskInit);

    /**
     * @brief The scheduler needs to know that we've reloaded the hardware.
     * 
     * The state set here causes the next SchedulerExit to reprogram all
     * the quanta timers in hardware.
     */
    KernelSchedulerLoad();

    /**
     * @brief Enables the timer interrupt in the interrupt tree
     * 
     */
    KernelTimerLoad();
    KernelTrace("Libos: Timer subsystem loaded.\n");      

    /**
     * @brief Enable external interrupts and memory integrity interrupts
     * 
     */
    KernelExternalInterruptLoad();
    KernelTrace("Libos: Interrupt subsystem loaded.\n");      

    
    /**
     * @brief Was the scheduler in the middle of a synchronous RPC 
     *        to another partition?
     * 
     *        If so, it will have been run on the scheduler stack.
     *        Let's resume it.
     */
    if (StartInContinuation) {
        KernelTrace("Libos: Returning from SeparationKernelPartitionCall.\n");      
        SeparationKernelPartitionContinuation(
            verbAndInitArg0,
            initArg1,
            initArg2,
            initArg3,
            initArg4,
            callingPartition,
            reserved1,
            dataStart
        );
    }

    // Enter the scheduler lock
    KernelSchedulerSwitchToStack(verb, callingPartition, verbAndInitArg0, initArg1, initArg2, 0, 0, KernelDispatchVerb);

}
