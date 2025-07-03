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
# Defines all known data RISC-V sec2 sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#

my $riscvSectionsDataRaw =
[
    RM_QUEUE =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.rm_rtos_queues .rm_rtos_queues.*'],
        ],
        LOCATION        => 'emem',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
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
        PROFILES        => [ 'sec2-*', ],
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
        PROFILES        => [ 'sec2-*', ],
    },

    # Necessary for ODP support (temporarily)
    DMEM_HEAP =>
    {
        HEAP_SIZE       => 0x2000,
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'RW',
        PROFILES        => [ 'sec2-*', ],
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
            ['*libShlib.a',     ' .rodata  .rodata.* .got*'],
            ['*libshlib.a',     '.srodata .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'r',
        PROFILES        => [ 'sec2-*', ],
    },

    GLOBAL_DATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*', '.dmem_*'], # HDCP uses this, TODO: make it not use this.
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    FREEABLE_HEAP =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            '. += __freeable_heap_profile_size',
        ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_SHARED_DATA_FB =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.task_data .task_data.*'],
            ['*.a',             '.data .data.* .sdata .sdata.*'],
            ['*.a',             '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_SHARED_DATA_DMEM_RES =>
    {
        INPUT_SECTIONS  => [
            ['*',               '.shared_data .shared_data.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_SHARED_SCP =>
    {
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    ODP_TEST_DATA =>
    {
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    ### Task sections
    TASK_DATA_IDLE =>
    {
        INPUT_SECTIONS  =>
        [
            '. += 0x1000',
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_DATA_RM_CMD =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/cmdmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/cmdmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_DATA_RM_MSG =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/msgmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/msgmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_DATA_CHNLMGMT =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/chnlmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/chnlmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/chnlmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_DATA_WORKLAUNCH =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/worklaunch/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/worklaunch/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/worklaunch/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
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
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_DATA_SCHEDULER =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/scheduler/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/scheduler/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    },

    GLOBAL_RODATA_FB =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  => [
            # Catch-all for the remaining .rodata.
            # This has to be in a section that goes after the task sections
            # because LD pattern-matching picks the first fit encountered,
            # and we don't want to override the task .rodata placements.
            ['*',               '.rodata .rodata.* .got* .srodata* .srodata.*'],
        ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'sec2-*', ],
    }
];

# return the raw data of Sections definition
return $riscvSectionsDataRaw;
