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
# Defines all known data RISC-V soe sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#

my $riscvSectionsDataRaw =
[
    RM_QUEUE =>
    {
        MAP_AT_INIT     => 1,
        NO_LOAD         => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.rm_rtos_queues .rm_rtos_queues.*'],
        ],
        LOCATION        => 'emem',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    ### Kernel mode sections
    KERNEL_RODATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_rodata .kernel_rodata.*'],
            ['*libSafeRTOS.a',  ' .rodata  .rodata.* .got*'],
            ['*libSafeRTOS.a',  '.srodata .srodata.*'],
            ['*/kernel/*.o',    ' .rodata  .rodata.* .got*'],
            ['*/kernel/*.o',    '.srodata .srodata.*'],
            ['*libDrivers.a',   ' .rodata  .rodata.* .got*'],
            ['*libDrivers.a',   '.srodata .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'R',
        PROFILES        => [ 'soe-*', ],
    },

    KERNEL_DATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_data .kernel_data.*'],
            ['*libSafeRTOS.a',  '.data .data.* .sdata .sdata.*'],
            ['*/kernel/*.o',    '.data .data.* .sdata .sdata.*'],
            ['*libDrivers.a',   '.data .data.* .sdata .sdata.*'],
            ['*libSafeRTOS.a',  '.bss .bss.* .sbss .sbss.* COMMON'],
            ['*/kernel/*.o',    '.bss .bss.* .sbss .sbss.* COMMON'],
            ['*libDrivers.a',   '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'RW',
        PROFILES        => [ 'soe-*', ],
    },

    ### Global sections
    GLOBAL_RODATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.shared_rodata .shared_rodata.*'],
            ['*',               '.syslib_rodata .syslib_rodata.*'],
            # include also task RO data, we don't care if kernel can read it
            ['*',               '.task_rodata .task_rodata.*'],
            ['*libShlib.a',     '.rodata  .rodata.* .got*'],
            ['*libshlib.a',     '.srodata .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'r',
        PROFILES        => [ 'soe-*', ],
    },

    GLOBAL_DATA =>
    {
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    FREEABLE_HEAP =>
    {
        INPUT_SECTIONS  =>
        [
            '. += __freeable_heap_profile_size',
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_SHARED_DATA_DMEM_RES =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.shared_data .shared_data.*'],
            ['*',               '.task_data .task_data.*'],
            ['*.a',             '.data .data.* .sdata .sdata.*'],
            ['*.a',             '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_SHARED_SCP =>
    {
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    ### Task sections
    TASK_DATA_IDLE =>
    {
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_RM_CMD =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/cmdmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/cmdmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_RM_MSG =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/msgmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/msgmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_WORKLAUNCH =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/worklaunch/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/worklaunch/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/worklaunch/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_TEST =>
    {
        # MAP_AT_INIT is 1 for _res sections by default
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/test/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/test/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/test/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_SCHEDULER =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/scheduler/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/scheduler/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    GLOBAL_RODATA_CMN =>
    {
        INPUT_SECTIONS  => [
            # Catch-all for the remaining .rodata.
            # This has to be in a section that goes after the task sections
            # because LD pattern-matching picks the first fit encountered,
            # and we don't want to override the task .rodata placements.
            ['*',               '.rodata .rodata.* .got* .srodata* .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_THERM =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/therm/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/therm/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/therm/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        DESCRIPTION     => 'Primary overlay of the THERM task.',
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_CORE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/core/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/core/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/core/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        DESCRIPTION     => 'Primary overlay of the CORE task.',
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_SPI =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/spi/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/spi/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/spi/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        DESCRIPTION     => 'Primary overlay of the SPI task.',
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_BIF =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/bif/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/bif/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/bif/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        DESCRIPTION     => 'Primary overlay of the BIF task.',
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_DATA_SMBPBI =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/smbpbi/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/smbpbi/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/smbpbi/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        DESCRIPTION     => 'Primary overlay of the SMBPBI task.',
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'soe-*', ],
    },
];

# return the raw data of Sections definition
return $riscvSectionsDataRaw;
