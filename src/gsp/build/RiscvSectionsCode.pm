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
# Defines all known code RISC-V GSP sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#

my $riscvSectionsCodeRaw =
[
    ### Special memory carveout (must be first thing that's placed in imem!)
    HS_SWITCH_IMEM =>
    {
        INPUT_SECTIONS  =>
        [
            # Default section is enough
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'gsp-*', ],
    },

    ### Kernel mode sections
    KERNEL_CODE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_code .kernel_code.*'],
            ['*libSafeRTOS.a',  '.text .text.*'],
            ['*/kernel/*.o',    '.text .text.*'],
            ['*libDrivers.a',   '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'gsp-*', ],
    },

    KERNEL_CODE_FB =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_code_fb .kernel_code_fb.*'],
            ['*',               '.kernel_code_init .kernel_code_init.*'],
            ['*',               '.kernel_code_deinit .kernel_code_deinit.*'],
        ],
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'gsp-*', ],
    },

    ### Global sections
    GLOBAL_CODE_IMEM =>
    {
        PREFIX          => '__syscall_permitted_start = .',
        SUFFIX          => '__syscall_permitted_end = .',
        INPUT_SECTIONS  =>
        [
            ['*',               '.shared_code .shared_code.*'],
            ['*/vcast/*.o',     '.text .text.*'],
            ['*libShlib.a',     '.text .text.*'],

            # Added for future-proofing, not used right now:
            ['*libgcc.a',       '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    GLOBAL_CODE_FB =>
    {
        MAP_AT_INIT     => 1,
        INPUT_SECTIONS  =>
        [
            ['*',               '.syslib_code .syslib_code.*'],
            ['*',               '.imem_*'], # HDCP uses this, TODO: make it not use this.
            ['*libSyslib.a',    '.text .text.*'],
            ['*lib*.a',         '.text .text.* .imem*'],
        ],
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    ODP_TEST_CODE =>
    {
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    ### Task sections
    TASK_CODE_IDLE =>
    {
        MAP_AT_INIT     => 0,
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_RM_CMD =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_RM_MSG =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_DEBUGGER =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/debugger/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_TEST =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/test/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'fb' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_HDCP1X =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/hdcp1x/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_HDCP22WIRED =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/hdcp22/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

    TASK_CODE_SCHEDULER =>
    {
        MAP_AT_INIT     => 0,
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'imem_res' => [ 'gsp-t2*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'gsp-*', ],
    },

];

# return the raw data of Sections definition
return $riscvSectionsCodeRaw;
