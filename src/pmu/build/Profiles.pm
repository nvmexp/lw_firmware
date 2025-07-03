#************************ BEGIN COPYRIGHT NOTICE ***************************#
#                                                                           #
#          Copyright (c) LWPU Corporation.  All rights reserved.          #
#                                                                           #
# All information contained herein is proprietary and confidential to       #
# LWPU Corporation.  Any use, reproduction, or disclosure without the     #
# written permission of LWPU Corporation is prohibited.                   #
#                                                                           #
#************************** END COPYRIGHT NOTICE ***************************#
#
# Defines all known PMU builds.  Each Build has a mapped profile defined in pmu-config.cfg
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/Profiles.txt
#

my $profilesRaw =
[
    GM10X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x8000,      # (bytes) [32 kB] Size of physical IMEM
            DMEM                     => 0x6000,      # (bytes) [24 kB] Size of physical DMEM
            DESC                     => 0x400,       # (bytes) [1 kB] Size of descriptor area
            RM_SHARE                 => 0x980,       # (bytes) RM share size
        ],

        FALCON => [
            IMEM_VA                  => 0x20000,     # (bytes) [128 kB] Size of virtually addressed IMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xC8,        # (bytes) Exception stack size
        ],
    },

    GM20X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0xC000,      # (bytes) [48 kB] Size of physical IMEM
            DMEM                     => 0xA000,      # (bytes) [40 kB] Size of physical DMEM
            DESC                     => 0x400,       # (bytes) [1 kB] Size of descriptor area
            RM_SHARE                 => 0xA00,       # (bytes) RM share size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xC8,        # (bytes) Exception stack size
        ],
    },

    GP100 =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0xC000,      # (bytes) [48 kB] Size of physical IMEM
            DMEM                     => 0xA000,      # (bytes) [40 kB] Size of physical DMEM
            DESC                     => 0x800,       # (bytes) [2 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xC8,        # (bytes) Exception stack size
        ],
    },

    GP10X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0x800,       # (bytes) [2 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x6100,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xE0,        # (bytes) Exception stack size
        ],
    },

    GV10X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0x800,       # (bytes) [2 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x6100,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xD0,        # (bytes) Exception stack size
        ],
    },

    GV11B =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x6000,      # (bytes) [24 kB] Size of physical IMEM
            DMEM                     => 0x6000,      # (bytes) [24 kB] Size of physical DMEM
            DESC                     => 0x800,       # (bytes) [2 kB] Size of descriptor area
            RM_SHARE                 => 0x100,       # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size

            # Queue information (DMEM queues)
            QUEUE_COUNT_CMD          => 0x3,         # Number of queues
            QUEUE_LENGTH_CMD_HPQ     => 0x0,         # (bytes) Size of high-priority queue
            QUEUE_LENGTH_CMD_LPQ     => 0x0,         # (bytes) Size of low-priority queue
            QUEUE_LENGTH_CMD_FBFLCN  => 0x0,         # (bytes) Size of queue for FB Falcon -> PMU communication
            QUEUE_LENGTH_MESSAGE     => 0x0,          # (bytes) Size of message queue
        ],

        FALCON => [
            IMEM_VA                  => 0x20000,     # (bytes) [128 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x5B00,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0xD0,        # (bytes) Exception stack size
        ],
    },

    TU10X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x6000,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0x100,       # (bytes) Exception stack size
        ],
    },

    GA100 =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x6000,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0x100,       # (bytes) Exception stack size
        ],
    },

    GA10X_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x24000,     # (bytes) [144 kB] Size of physical IMEM
            DMEM                     => 0x2C000,     # (bytes) [176 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    GA10X_SELFINIT_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x24000,     # (bytes) [144 kB] Size of physical IMEM
            DMEM                     => 0x2C000,     # (bytes) [176 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    # MMINTS-TODO: remove this when possible!
    GA10B =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x6000,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0x100,       # (bytes) Exception stack size
        ],
    },

    GA10B_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x3400,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x4000,      # (bytes) [16 kB] This accounts for SK + liblwriscv code + ACR partition code
            DMEM_START_CARVEOUT_SIZE => 0x2000,      # (bytes) [8 kB]  This accounts for SK + liblwriscv data + ACR partition data/stack
        ],
    },

    GA10F_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x3400,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x4000,      # (bytes) [16 kB] This accounts for SK + liblwriscv code + ACR partition code
            DMEM_START_CARVEOUT_SIZE => 0x2000,      # (bytes) [8 kB]  This accounts for SK + liblwriscv data + ACR partition data/stack
        ],
    },

    GA11B_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x3400,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x400,       # (bytes) [1 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x4000,      # (bytes) [16 kB] This accounts for SK + liblwriscv code + ACR partition code
            DMEM_START_CARVEOUT_SIZE => 0x2000,      # (bytes) [8 kB]  This accounts for SK + liblwriscv data + ACR partition data/stack
        ],
    },

    AD10X_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x24000,     # (bytes) [144 kB] Size of physical IMEM
            DMEM                     => 0x2C000,     # (bytes) [176 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    AD10B_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x2A000,     # (bytes) [168 kB] Size of physical IMEM
            DMEM                     => 0x2C800,     # (bytes) [178 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0x800,       # (bytes) RM managed heap size
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x3400,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    # MMINTS-TODO: remove this when possible!
    GH100 =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0x1000,      # (bytes) [4 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x5F00,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0x100,       # (bytes) Exception stack size
        ],
    },

    GH100_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x2A000,     # (bytes) [168 kB] Size of physical IMEM
            DMEM                     => 0x2C800,     # (bytes) [178 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    # MMINTS-TODO: remove this when possible!
    GH20X =>
    {
        ARCH => 'FALCON',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            DESC                     => 0x1000,      # (bytes) [4 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        FALCON => [
            IMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed IMEM
            DMEM_VA                  => 0x1000000,   # (bytes) [16384 kB] Size of virtually addressed DMEM
            DMEM_VA_BOUND            => 0x5F00,      # (bytes) Start address of virtually addressable DMEM
            ISR_STACK                => 0x100,       # (bytes) Interrupt stack size
            ESR_STACK                => 0x100,       # (bytes) Exception stack size
        ],
    },

    GH20X_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x2A000,     # (bytes) [168 kB] Size of physical IMEM
            DMEM                     => 0x2C800,     # (bytes) [178 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    GB100_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x2A000,     # (bytes) [168 kB] Size of physical IMEM
            DMEM                     => 0x2C800,     # (bytes) [178 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

    G00X_RISCV =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x2A000,     # (bytes) [168 kB] Size of physical IMEM
            DMEM                     => 0x2C800,     # (bytes) [178 kB] Size of physical DMEM
            DESC                     => 0xC00,       # (bytes) [3 kB] Size of descriptor area
            RM_SHARE                 => 0x0,         # (bytes) RM share size
            RM_MANAGED_HEAP          => 0,           # (bytes) RM managed heap size
            FRTS_MAX_SIZE            => 0x100000,    # (bytes) Maximum expected size of the FRTS region
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0,           # PMU has no freeable heap
            OS_HEAP                  => 0x7800,      # (bytes) Heap size
            VMA_HOLE_SIZE            => 0x400,       # (bytes) [1 kB] Gap between sections in virtual memory
            CODE_SECTION_PREFIX      => 'imem_',     # Prepend to code section names to avoid name clashes
            DATA_SECTION_PREFIX      => 'dmem_',     # Prepend to data section names to avoid name clashes
            NO_FB_SECTIONS           => 1,           # Forbid sections from being mapped into FBGPA

            # ODP configuration
            ODP_CODE_PAGE_MAX_COUNT  => 32,          # Max number of pages in ITCM
            ODP_CODE_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of code pages
            ODP_DATA_PAGE_MAX_COUNT  => 32,          # Max number of pages in DTCM
            ODP_DATA_PAGE_SIZE       => 0x1000,      # (bytes) [4 kB] Size of data pages
            ODP_MPU_SHARED_COUNT     => 64,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
            DMEM_START_CARVEOUT_SIZE => 0x1000,      # (bytes) [4 kB]
        ],
    },

];

# return the raw data of Profiles definition
return $profilesRaw;

