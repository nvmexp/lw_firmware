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
# Defines all known data RISC-V GSP sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#

my $riscvSectionsDataRaw =
[
    ### Special memory carveout (must be first thing that's placed in dmem!)
    HS_SWITCH_DMEM =>
    {
        INPUT_SECTIONS  =>
        [
            # Enforces some ordering constraints
            ['*',               '.hsSwitchDmemCmd .hsSwitchDmemCmd.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    RM_QUEUE =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.rm_rtos_queues .rm_rtos_queues.*'],
        ],
        LOCATION        => 'emem',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
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
        PROFILES        => [ 'gsp-*', ],
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
        PROFILES        => [ 'gsp-*', ],
    },

    # Necessary for ODP support (temporarily)
    DMEM_HEAP =>
    {
        HEAP_SIZE       =>
        [
            0x2000      => DEFAULT,
            0x0         => [ 'gsp-t2*', ],
        ],

        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'RW',
        PROFILES        => [ 'gsp-*', ],
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
        PROFILES        => [ 'gsp-*', ],
    },

    GLOBAL_DATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/vcast/*.o',     ' .rodata  .rodata.* .got* .data*'],
            ['*/vcast/*.o',     '.srodata .srodata.* .data*'],
            ['*/vcast/*.o',     '.bss .bss.* .sbss .sbss.* COMMON'],
            ['*', '.dmem_*'], # HDCP uses this, TODO: make it not use this.
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    FREEABLE_HEAP =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            '. += __freeable_heap_profile_size',
        ],
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
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
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_SHARED_DATA_DMEM_RES =>
    {
        INPUT_SECTIONS  => [
            ['*',               '.shared_data .shared_data.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_SHARED_SCP =>
    {
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    ODP_TEST_DATA =>
    {
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'fb' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    ### Task sections
    TASK_DATA_IDLE =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            '. += 0x1000',
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_RM_CMD =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/cmdmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/cmdmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_RM_MSG =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/msgmgmt/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/msgmgmt/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_DEBUGGER =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/debugger/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/debugger/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/debugger/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
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
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_HDCP1X =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/hdcp1x/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/hdcp1x/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/hdcp1x/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_HDCP22WIRED =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/hdcp22/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/hdcp22/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/hdcp22/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_DATA_SCHEDULER =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.data .data.* .sdata .sdata.*'],
            ['*/tasks/scheduler/*.o', '.rodata .rodata.* .srodata .srodata.*'],
            ['*/tasks/scheduler/*.o', '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        =>
        [
            'dmem_odp' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
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
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'dmem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    }
];

# return the raw data of Sections definition
return $riscvSectionsDataRaw;
