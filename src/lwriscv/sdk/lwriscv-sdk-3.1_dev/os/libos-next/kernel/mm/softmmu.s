#include <libos-config.h>
#include <manifest_defines.h>
#include <peregrine-headers.h>
#include <libos_xcsr.h>

.global KernelTrapInstructionFault
.global KernelTrapStoreFault
.global KernelTrapLoadFault

#define RISCV_GP 3
#define RISCV_TP 4

// Ported from manuals due to GAS missing ternary operator
#define LW_RISCV_CSR_SATP_MODE_LWMPU_M        0xf000000000000000
#define LW_RISCV_CSR_SATP_MODE_HASH_M         0xe000000000000000
#define LW_RISCV_CSR_XMPUATTR_UR_M            0x01
#define LW_RISCV_CSR_XMPUATTR_UW_M            0x02
#define LW_RISCV_CSR_XMPUATTR_UX_M            0x04
#define LW_RISCV_CSR_XMPUATTR_SR_M            0x08
#define LW_RISCV_CSR_XMPUATTR_SW_M            0x10
#define LW_RISCV_CSR_XMPUATTR_SX_M            0x20
#define LW_RISCV_CSR_XMPUATTR_MR_M            0x200
#define LW_RISCV_CSR_XMPUATTR_MW_M            0x400
#define LW_RISCV_CSR_XMPUATTR_MX_M            0x800
#define LW_RISCV_CSR_XMPUATTR_CACHEABLE_M     0x40000       // 18:18
#define LW_RISCV_CSR_XMPUATTR_COHERENT_M      0x80000       // 19:19
// ____________________________________________
// | valid | address...... | xxx | attributes |
// --------------------------------------------
//
//  @todo: Where does page size fit in for 2mb regions
//         that are actually smaller?
//
.section .data

// @ : Support for multiple instances of LIBOS
//        requires that the kernel mapping to .saveArea
//        be accessed through a non-identity mapping
.saveArea:
    .long 0     /* a0 */
    .long 0     /* t0 */
    .long 0     /* t1 */
    .long 0     /* t2 */

.section .text
/*
           000 -> invalid
           001 -> read
           011 -> read/write
           101 -> read/execute
          1000 -> super

        Requires translation table of 32 entries. Wasteful.
*/

// @todo: non-cacheable mappings
.align 4
.attributesByKind:
        /* CACHE U-    */ .long 0
        /* CACHE UR    */ .long LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE UW    */ .long LW_RISCV_CSR_XMPUATTR_UW_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M; 
        /* CACHE URW   */ .long LW_RISCV_CSR_XMPUATTR_UW_M | LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE UX    */ .long LW_RISCV_CSR_XMPUATTR_UX_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE UXR   */ .long LW_RISCV_CSR_XMPUATTR_UX_M | LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE UXW   */ .long 0;
        /* CACHE UXRW  */ .long 0;
        /* CACHE S-    */ .long 0;
        /* CACHE SR    */ .long LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE SW    */ .long LW_RISCV_CSR_XMPUATTR_SW_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M; 
        /* CACHE SRW   */ .long LW_RISCV_CSR_XMPUATTR_SW_M | LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE SX    */ .long LW_RISCV_CSR_XMPUATTR_SX_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE SXR   */ .long LW_RISCV_CSR_XMPUATTR_SX_M | LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_CACHEABLE_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /* CACHE SXW   */ .long 0;
        /* CACHE SXRW  */ .long 0;
        /*       U-    */ .long 0
        /*       UR    */ .long LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       UW    */ .long LW_RISCV_CSR_XMPUATTR_UW_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M; 
        /*       URW   */ .long LW_RISCV_CSR_XMPUATTR_UW_M | LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       UX    */ .long LW_RISCV_CSR_XMPUATTR_UX_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       UXR   */ .long LW_RISCV_CSR_XMPUATTR_UX_M | LW_RISCV_CSR_XMPUATTR_UR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       UXW   */ .long 0;
        /*       UXRW  */ .long 0;
        /*       S-    */ .long LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       SR    */ .long LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       SW    */ .long LW_RISCV_CSR_XMPUATTR_SW_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M; 
        /*       SRW   */ .long LW_RISCV_CSR_XMPUATTR_SW_M | LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       SX    */ .long LW_RISCV_CSR_XMPUATTR_SX_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       SXR   */ .long LW_RISCV_CSR_XMPUATTR_SX_M | LW_RISCV_CSR_XMPUATTR_SR_M | LW_RISCV_CSR_XMPUATTR_COHERENT_M;
        /*       SXW   */ .long 0;
        /*       SXRW  */ .long 0;        


/****************************************************************************************
                        PAGETABLE LOOKUP AND DISPATCH
****************************************************************************************/
        
.KernelAddress: /* (a0:fault-addr, tp=permissions-required) */        
        csrr  t0, sstatus                /* mpp:8 = isSupervisor */
        andi  t0, t0, 256
        beq   t0, x0, KernelTaskPanic    /* User tasks cannot access kernel address */
        la    t0, kernelGlobalPagetable  /* Use the global kernel pagetable */
        addi  tp, tp, 32                 /* Mask in LibosMemoryKernelPrivate */
        j .Continue

KernelTrapInstructionFault:  /* (tp:4) @see trap-ulwectored */
        addi  tp, tp, 8

KernelTrapStoreFault:                  
        addi  tp, tp, 4

KernelTrapLoadFault: /* (tp:required privilege mask) */
        /* At this point tp is LibosMemoryReadable, LibosMemoryWriteable, or LibosMemoryExelwtable. */

        /* Save working registers t0, t1, t2 and the fault address register a0. */
        la    gp, .saveArea
        sd    a0, 0(gp)
        sd    t0, 8(gp)
        sd    t1, 16(gp)
        sd    t2, 24(gp)

        /* Set a0 to small page the fault olwrred in */
        csrr  a0, stval                 
        li    t1, ~(LIBOS_CONFIG_PAGESIZE-1)
        and   a0, a0, t1                /* Round to page boundary */

        /* Handle address space lookup for kernel addresses
           Returns control at .Continue
           
           @note This subroutine ensures we're running in kernel */
        blt   a0, x0, .KernelAddress    

.UserAddress:
        csrr  gp, sscratch                  /* sscratch is pointer to Task structure */
        ld    t0, OFFSETOF_TASK_RADIX(gp)   /* Locate the radix table root */
        
.Continue: /* (a0:fault address page, t0: root page directory, tp: required-permissions) */

        /*
                Ensure that the address is within range for a 39 bit signed
                address space.

                This isn't technically required, as security features like
                TBI (Top Byte Ignore) opt into tiling the address space.

                Based on this, we've only enabled it in debug builds.
        */
        slli  t1, a0, (64-LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE)
        srai  t1, t1, (64-LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE)
        bne   t1, a0, .IlwalidPageAccess  

        /* Resolve the address in the global page directory */
        srli  t1, a0, LIBOS_CONFIG_PAGESIZE_HUGE_LOG2      
        andi  t1, t1, (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)        
        slli  t1, t1, LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2
        add   t0, t1, t0
        ld    t0, 0(t0)         /* Never null, may point to zeroMiddle
                                   which is a valid but empty PDE */

        /* Resolve the address in the middle page directory */
        srli  t1, a0, LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2                 
        andi  t1, t1, (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)
        slli  t1, t1, LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2
        add   t0, t1, t0
        ld    t0, 0(t0)                  
        bge   t0, x0, .isVirtualMpuEntryOrNull 
                /* (t0:PDE, tp:required-privileges)
        
                @note: The null case is handled by .isVirtualMpuEntryOrNull

                @see PageTableMiddle 
                Positive addresses are 2mb virtual pages 
                Negative addresses are always valid PDE addresses
                */

        /* Resolve address in the leaf page directory */
        srli  t1, a0, LIBOS_CONFIG_PAGESIZE_LOG2     
        andi  t1, t1, (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)
        slli  t1, t1, LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2                
        add   t0, t1, t0
        ld    t1, 0(t0)

        /* Ensure the PTE grants the required permissions we've aclwmulated in tp  */
        and   t0, t1, tp                        /* Lower bits in PTE match LibosMemory* @see PageTableLeaf */
        bne   t0, tp, .IlwalidPageAccess

/****************************************************************************************
                                POPULATE 4KB PAGE
****************************************************************************************/
.isSmallPage: /* (a0:fault-addr, t1:PTE) */

#if LIBOS_CONFIG_MPU_HASHED
        /* Hash the page address to bucket cluster */
        srli  t0, a0, LIBOS_CONFIG_HASHED_MPU_SMALL_S0-2        
        srli  t2, a0, LIBOS_CONFIG_HASHED_MPU_SMALL_S1-2
        xor   t0, t0, t2
        andi  t0, t0, (LIBOS_CONFIG_MPU_INDEX_COUNT-1) &~ 3     /* t0 is now bucket group */

        /* Pick a 'random' bucket */
        csrr  t2, time            
        andi  t2, t2, 3
        xor   t0, t0, t2                                        /* t0 is now an mpu index */

        /*      
            Buckets 0, 4, and 8 are pinned
            If the value is 8 or lower, we or 1 in.
        */
        li   t2, 8
        bgt  t0, t2, .ContinueMpuPopulate
        ori  t0, t0, 1                                          /* t0 is now a free bucket that isn't pinned */
#else

        /* (a0:fault-addr, t1:PTE) */
.Retry:
        csrr  t0, time            
        andi  t0, t0, LIBOS_CONFIG_MPU_INDEX_COUNT-1
        li    t2, LIBOS_CONFIG_MPU_DATA
        ble   t0, t2, .Retry                                    /* t0 is now random unpinned bucket */
#endif


 .ContinueMpuPopulate: /* (a0: fault page address, t0:mpu-index, t1: PTE) */

        csrw LW_RISCV_CSR_XMPUIDX, t0   /* Bind the MPU index */
        
        ori   t0, a0, 1                 
        csrw  LW_RISCV_CSR_XMPUVA,  t0  /* Program VA with valid bit set */

        li    t2, ~(LIBOS_CONFIG_PAGESIZE-1)
        and   t0, t2, t1                /* Extract PA from PTE */
        csrw  LW_RISCV_CSR_XMPUPA,  t0  

        li    t0, LIBOS_CONFIG_PAGESIZE
        csrw  LW_RISCV_CSR_XMPURNG, t0

        andi  t0, t1, 4|8|16|32          /* Extract attributes from PTE */

        /* Translate attributes to XMPUATTR */
.attribute.relocation:
        auipc t1, %pcrel_hi(.attributesByKind)       
        add   t0, t0, t1
        lw    t0, %pcrel_lo(.attribute.relocation)(t0)

        csrw  LW_RISCV_CSR_XMPUATTR, t0 /* Program attributes */


/****************************************************************************************
                                TASK RETURN
****************************************************************************************/

.returnToTask:
        /* Restore registers and exit */
        la    gp, .saveArea
        ld    a0, 0(gp)
        ld    t0, 8(gp)
        ld    t1, 16(gp)
        ld    t2, 24(gp)

        /* Restore task reference tp/gp */
        csrr  gp, sscratch              
        ld    tp, RISCV_TP*8+OFFSETOF_TASK_REGISTERS(gp)
        ld    gp, RISCV_TP*8+OFFSETOF_TASK_REGISTERS(gp)
        sret


/****************************************************************************************
                             POPULATE VIRTUAL MPU ENTRY
****************************************************************************************/

.isVirtualMpuEntryOrNull: /* (a0:fault page address, t0:PDE1, tp:permissions-required) */
        
        /* 
           Validate the current access type type is permissible 
           @note: This also handles the null case, since tp is guaranteed to be non-zero
        */
        and   t1, t0, tp                
        bne   t1, tp, .IlwalidPageAccess

        /* Extract the physical address of the descriptor */
        srli  t0, t0, 10   // @see LIBOS_PAGETABLE_PTE_VIRTUAL_MPU_ADDRESS_SHIFT

        /* Mask in the FBGPA aperture mapping */
        li    t1, LIBOS_CONFIG_SOFTTLB_MAPPING_BASE
        add   t0, t0, t1

        /* t0 is now a VirtualMpuEntry pointer */

#if LIBOS_CONFIG_MPU_HASHED
        /* Hash the page address to bucket cluster */
        srli  t2, a0, LIBOS_CONFIG_HASHED_MPU_MEDIUM_S0-2       
        srli  t1, a0, LIBOS_CONFIG_HASHED_MPU_MEDIUM_S1-2
        xor   t2, t2, t1        
        andi  t2, t2, (LIBOS_CONFIG_MPU_INDEX_COUNT-1) &~ 3     /* t2 is now the bucket group */

        /* Pick a 'random' bucket */
        csrr  t1, time            
        andi  t1, t1, 3
        xor   t2, t2, t1                                        /* t2 is now a desired bucket */


        /*      
            Buckets 0, 4, and 8 are pinned
            If the value is 8 or lower, we or 1 in.
        */
        li   t1, 8
        bgt  t2, t1, .ContinueMpuPopulate2
        ori  t2, t2, 1                                          /* t2 is now a valid unpinned bucket */
.ContinueMpuPopulate2:
#else
        /* Load VirtualMpuEntry::desiredMpuIndex*/
        ld    t2, 0(t0)

        /* Ensure the index is valid */
.Retry2:
        li     t1, LIBOS_CONFIG_MPU_DATA
        bgt    t2, t1, .ValidIndex2

        /* Pick a random MPU index */
        csrr  t2, time            
        andi  t2, t2, LIBOS_CONFIG_MPU_INDEX_COUNT-1
        j .Retry2

.ValidIndex2:
#endif
        /* Bind the MPU index */
        csrw  LW_RISCV_CSR_XMPUIDX, t2

        /* Program the virtual address */
        ld    t1, 8(t0)                         /* Load VirtualMpuEntry::virtualAddress */
        csrw  LW_RISCV_CSR_XMPUVA, t1

        /* Program the physical address */
        ld    t1, 16(t0)                         /* Load VirtualMpuEntry::physicalAddress */
        csrw  LW_RISCV_CSR_XMPUPA, t1

        /* Program the size */
        ld    t1, 24(t0)                         /* Load VirtualMpuEntry::size */
        csrw  LW_RISCV_CSR_XMPURNG, t1

        /* Program the attributes */
        ld    t1, 32(t0)                         /* Load VirtualMpuEntry::attributes */
        csrw  LW_RISCV_CSR_XMPUATTR, t1

        /* Return to task */
        j .returnToTask

/****************************************************************************************
                                TASK PANIC
****************************************************************************************/

.IlwalidPageAccess:
        csrr  t0, sstatus                      // mpp:8 = 0 -> umode or 1 -> smode
        andi  t0, t0, 256
        beq   t0, x0, .panicTask               // Panic the task if we were running in user
        j     KernelPanic                      // Otherwise panic the kernel

.panicTask:
        la    gp, .saveArea
        ld    a0, 0(gp)           // Restore context
        ld    t0, 8(gp)
        ld    t1, 16(gp)
        ld    t2, 24(gp)
        jal   gp, KernelContextSave
        jal   x0, KernelTaskPanic

