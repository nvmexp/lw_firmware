/*
    The user has direct control over which features are compiled in
    if they elect the LIBOS_TINY model.

    Otherwise, we enable most features by default.
*/

//
// LIBOS uses MUSLC's printf() routines.  Our tiny profile
// removes padding, positional arguments and half-integer types.
// Size is < 512 bytes.
//
// @note: We always use this even for the full kernel as we
//        want to minimize kernel stack usage.  We also
//        expect clients of the full kernel to use a ported
//        instance of the full MUSLC.
#define LIBOS_CONFIG_PRINTF_UNICODE 0

//
//  RISC-V 2.0 and beyond only supports the OS in S-Mode
//  and supports compresed ISA
//
#if LIBOS_LWRISCV >= 220
#   define LIBOS_CONFIG_MPU_HASHED       1
#   define LIBOS_CONFIG_RISCV_COMPRESSED 1
#   define LIBOS_CONFIG_RISCV_S_MODE     1
#   define LIBOS_CONFIG_GDMA_SUPPORT     1
#elif LIBOS_LWRISCV >= 200
#   define LIBOS_CONFIG_MPU_HASHED       0
#   define LIBOS_CONFIG_RISCV_COMPRESSED 1
#   define LIBOS_CONFIG_RISCV_S_MODE     1
#   define LIBOS_CONFIG_GDMA_SUPPORT     0
#else
#   define LIBOS_CONFIG_MPU_HASHED       0
#   define LIBOS_CONFIG_RISCV_COMPRESSED 0
#   define LIBOS_CONFIG_RISCV_S_MODE     0
#   define LIBOS_CONFIG_GDMA_SUPPORT     0
#endif

#if !LIBOS_TINY
#   define LIBOS_CONFIG_KERNEL_STACK_SIZE   16384
#   define LIBOS_FEATURE_SHUTDOWN           1
#   define LIBOS_FEATURE_PRIORITY_SCHEDULER 1
#   define LIBOS_CONFIG_ID_SIZE                4

// @todo: Remove once dynamic API is fully present
#   define LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT   2


// Disabled lwrrently
#   define LIBOS_CONFIG_PROFILER                 0
#   define LIBOS_FEATURE_PAGING                  0
#   define LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND 0
#   define LIBOS_CONFIG_INTEGRITY_MEMERR         0
#else

// Tiny profile
#ifndef LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT
#   define LIBOS_CONFIG_MAX_TABLE_DISPLACEMENT   2
#endif

#if LIBOS_TINY
#define LIBOS_CONFIG_ID_SIZE                     1
#else
#define LIBOS_CONFIG_ID_SIZE                     4
#endif

// Set defaults for the kernel stack
#if LIBOS_CONFIG_FEATURE_MM
#   define LIBOS_CONFIG_KERNEL_STACK_SIZE 16384
#elif !LIBOS_CONFIG_KERNEL_STACK_SIZE && LIBOS_CONFIG_KERNEL_LOG
#   define LIBOS_CONFIG_KERNEL_STACK_SIZE 2000
#elif !LIBOS_CONFIG_KERNEL_STACK_SIZE && !LIBOS_CONFIG_KERNEL_LOG
#   define LIBOS_CONFIG_KERNEL_STACK_SIZE 512
#endif

#define LIBOS_FEATURE_PAGING      0

#define LIBOS_CONFIG_PROFILER_SAVE_ON_SUSPEND 0
#define LIBOS_CONFIG_PROFILER 0
#endif


// Disable hashed MPU if MM is disabled
// @note: Eventually this will be controlled directly by tiny
#if LIBOS_TINY
#   undef LIBOS_CONFIG_MPU_HASHED
#   define LIBOS_CONFIG_MPU_HASHED 0
#else
#define LIBOS_CONFIG_SOFTTLB_MAPPING_BASE  0x8000000000000000
#define LIBOS_CONFIG_SOFTTLB_MAPPING_RANGE 0x1000000000000000
#define LIBOS_CONFIG_PRI_MAPPING_BASE 0xFFFFFFFF60000000
#define LIBOS_CONFIG_PRI_MAPPING_RANGE LW_RISCV_AMAP_EXTIO1_END - LW_RISCV_AMAP_EXTIO1_START
#define LIBOS_CONFIG_KERNEL_TEXT_BASE 0xffffffff93000000
#define LIBOS_CONFIG_KERNEL_TEXT_RANGE 0x1000000
#define LIBOS_CONFIG_KERNEL_DATA_BASE 0xffffffffa3000000
#define LIBOS_CONFIG_KERNEL_DATA_RANGE 0x1000000
#define LIBOS_CONFIG_GLOBAL_PAGE_MAPPING_BASE 0xffffffff70000000
#define LIBOS_CONFIG_GLOBAL_PAGE_MAPPING_RANGE 0x4000
#define LIBOS_CONFIG_PORT_APERTURE_SIZE 0x1000000
#define LIBOS_CONFIG_PORT_APERTURE_0 0xFFFFFFFFF4000000
#define LIBOS_CONFIG_PORT_APERTURE_1 0xFFFFFFFFF5000000
#define LIBOS_CONFIG_PORT_APERTURE_2 0xFFFFFFFFF6000000
#define LIBOS_CONFIG_PORT_APERTURE_3 0xFFFFFFFFF7000000
#endif

#if LIBOS_CONFIG_MPU_HASHED
// Do not change without reviewing softmmu.s comment 'Buckets 0, 4, and 8 are pinned'
// These values must correspond to the hashes of the corresponding addresses.
// The addresses were chosen to ensure we could quickly locate the pinned bucket(s)
// in the fault handler.
#   define LIBOS_CONFIG_MPU_IDENTITY  0
#   define LIBOS_CONFIG_MPU_TEXT      4
#   define LIBOS_CONFIG_MPU_DATA      8
#else
#   define LIBOS_CONFIG_MPU_IDENTITY  0
#   define LIBOS_CONFIG_MPU_TEXT      1
#   define LIBOS_CONFIG_MPU_DATA      2
#   define LIBOS_CONFIG_MPU_BOOTSTRAP 3 /* This MPU entry is free once bootstrap has completed */
#endif 

// Page sizes
#define LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2 3
#define LIBOS_CONFIG_PAGETABLE_ENTRIES    (LIBOS_CONFIG_PAGESIZE >> LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2)
#define LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2 (2 * LIBOS_CONFIG_PAGESIZE_LOG2 - LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2)
#define LIBOS_CONFIG_PAGESIZE_MEDIUM      LIBOS_CONFIG_PAGESIZE

#define LIBOS_CONFIG_PAGESIZE_HUGE_LOG2   (3 * LIBOS_CONFIG_PAGESIZE_LOG2 - 2 * LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2)
#define LIBOS_CONFIG_PAGESIZE_HUGE        LIBOS_CONFIG_PAGESIZE

// @see pagetable.c as we use a 3 level pagetable
#define LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE (4 * LIBOS_CONFIG_PAGESIZE_LOG2 - 3 * LIBOS_CONFIG_PAGETABLE_ENTRY_LOG2)

// Set defaults for hashed MPU
#if !LIBOS_TINY
#   define LIBOS_CONFIG_HASHED_MPU_SMALL_S0  LIBOS_CONFIG_PAGESIZE_LOG2
#   define LIBOS_CONFIG_HASHED_MPU_SMALL_S1  (LIBOS_CONFIG_PAGESIZE_LOG2+7)
#   define LIBOS_CONFIG_HASHED_MPU_MEDIUM_S0 LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2
#   define LIBOS_CONFIG_HASHED_MPU_MEDIUM_S1 (LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2+7)

#   define LIBOS_CONFIG_HASHED_MPU_1PB_S0    50
#   define LIBOS_CONFIG_HASHED_MPU_1PB_S1    (50+7)
#endif

