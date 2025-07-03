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
# Defines all known builds.  Each Build has a mapped profile defined in
# *-config.cfg
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/Profiles.txt
#

my $profilesRaw =
[
    GA10X =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            EMEM                     => 0x2000,      # (bytes) [8 kB] Size of physical EMEM
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0x40000,     # (bytes)
            VMA_HOLE_SIZE            => 0x400,

            ODP_DATA_PAGE_MAX_COUNT  => 16,
            ODP_DATA_PAGE_SIZE       => 0x400,
            ODP_CODE_PAGE_MAX_COUNT  => 16,
            ODP_CODE_PAGE_SIZE       => 0x400,
            ODP_MPU_SHARED_COUNT     => 32,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE  => 0x6000,
            DMEM_START_CARVEOUT_SIZE  => 0x4000,
        ],
    },

    T23XD =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x20000,     # (bytes) [128 kB] Size of physical IMEM
            DMEM                     => 0x15000,     # (bytes) [84 kB] Size of physical DMEM
            EMEM                     => 0x2000,      # (bytes) [8 kB] Size of physical EMEM
        ],  #TODO 200586859: Need to revisit this after finalizing TSEC/SEC2's IMEM/DMEM/EMEM sizes

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0x2000,      # (bytes) [8 kB] T23XD needs all code/data to be resident. To accommodate reducing freezable heap.
            VMA_HOLE_SIZE            => 0x400,

            ODP_DATA_PAGE_MAX_COUNT  => 16,
            ODP_DATA_PAGE_SIZE       => 0x400,
            ODP_CODE_PAGE_MAX_COUNT  => 16,
            ODP_CODE_PAGE_SIZE       => 0x400,
            ODP_MPU_SHARED_COUNT     => 32,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE  => 0x6000,
            DMEM_START_CARVEOUT_SIZE  => 0x4000,
        ],
    },

    GH100 =>
    {
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical IMEM
            DMEM                     => 0x10000,     # (bytes) [64 kB] Size of physical DMEM
            EMEM                     => 0x2000,      # (bytes) [8 kB] Size of physical EMEM
        ],

        RISCV => [
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0x40000,     # (bytes)
            VMA_HOLE_SIZE            => 0x400,

            ODP_DATA_PAGE_MAX_COUNT  => 16,
            ODP_DATA_PAGE_SIZE       => 0x400,
            ODP_CODE_PAGE_MAX_COUNT  => 16,
            ODP_CODE_PAGE_SIZE       => 0x400,
            ODP_MPU_SHARED_COUNT     => 32,

            # SK carveout configuration
            IMEM_START_CARVEOUT_SIZE  => 0x1000,
            DMEM_START_CARVEOUT_SIZE  => 0x1000,
        ],
    },
];

# return the raw data of Profiles definition
return $profilesRaw;

