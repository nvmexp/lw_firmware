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
# Defines all known SOE builds.  Each Build has a mapped profile defined in soe-config.cfg
#
#   A Build contains the following information:
#

my $profilesRaw =
[
    LS10 =>
    {
        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)
        ARCH => 'RISCV',
        COMMON => [
            IMEM                     => 0x30000,     # (bytes) [192 kB] Size of physical IMEM
            DMEM                     => 0x20000,     # (bytes) [128 kB] Size of physical DMEM
            EMEM                     => 0x2000,      # (bytes) [8 kB] Size of physical EMEM
        ],

        RISCV => [
            HAS_MONITOR              => 1,          # TODO: PJAMBHLEKAR; Cleanup after removing Vlad's monitor
            PARTITION_BOOT           => 1,
            SYS_STACK                => 0x1000,      # (bytes) [4 kB] Size of interrupt+exception+syscall stack
            MPU_GRANULARITY          => 0x400,       # (bytes) [1 kB] MPU page size
            FREEABLE_HEAP            => 0x4000,     # (bytes)
            VMA_HOLE_SIZE            => 0x400,

            PRE_EXTRACTED            => 1,          # Place the MPU descriptor in DMEM rather than FB and disable .bss hacks.

            ODP_DATA_PAGE_MAX_COUNT  => 1024,       # ODP not used, silence warnings
            ODP_DATA_PAGE_SIZE       => 0x400,
            ODP_CODE_PAGE_MAX_COUNT  => 1024,       # ODP not used, silence warnings
            ODP_CODE_PAGE_SIZE       => 0x400,
            ODP_MPU_SHARED_COUNT     => 32,
        ],

        FREEABLE_HEAP    => 70,       # (blocks) size of freeable heap in physically addressable DMEM space

        ISR_STACK        => 512,      # (bytes)
        ESR_STACK        => 512,      # (bytes)

        # queue length information
        CMD_QUEUE_LENGTH => 0x80,     # (bytes)
        MSG_QUEUE_LENGTH => 0x80,     # (bytes)

        # channel interface method ID information
        APP_METHOD_ARRAY_SIZE => 0x50,  # 80 distinct app methods
        APP_METHOD_MIN        => 0x100, # (0x400 >> 2)
        APP_METHOD_MAX        => 0x1CF, # (0x73C >> 2)

        # SK+bootstub carveout configuration
        IMEM_START_CARVEOUT_SIZE  => 0x2000, # (bytes) SK code: 4K, bootstub code: 4K
        DMEM_START_CARVEOUT_SIZE  => 0x1000, # (bytes) SK data: 4K, bootstub has no data
    },

];

# return the raw data of Profiles definition
return $profilesRaw;
