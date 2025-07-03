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
# Defines all known code RISC-V sec2 sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#

my $riscvSectionsCodeRaw =
[
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
        PROFILES        => [ 'sec2-*', ],
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
        LOCATION        => 'fb',
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'sec2-*', ],
    },

    ### Global sections
    GLOBAL_CODE_IMEM =>
    {
        PREFIX          => '__syscall_permitted_start = .',
        SUFFIX          => '__syscall_permitted_end = .',
        INPUT_SECTIONS  =>
        [
            ['*',               '.shared_code .shared_code.*'],
            ['*libShlib.a',     '.text .text.*'],

            # Added for future-proofing, not used right now:
            ['*libgcc.a',       '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
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
        LOCATION        => 'fb',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

    ### Task sections
    TASK_CODE_IDLE =>
    {
        LOCATION        => 'fb',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_CODE_RM_CMD =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_CODE_RM_MSG =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rx', #TODO: Check whether we really need read permission for here
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_CODE_CHNLMGMT =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/chnlmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_CODE_WORKLAUNCH =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/worklaunch/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },


    TASK_CODE_TEST =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/test/*.o', '.text .text.*'],
        ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

    TASK_CODE_SCHEDULER =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.text .text.*'],
        ],
        LOCATION        =>
        [
            'imem_odp' => DEFAULT,
            'fb' => [ 'sec2-*', ],
        ],

        PERMISSIONS     => 'rx',
        PROFILES        => [ 'sec2-*', ],
    },

];

# return the raw data of Sections definition
return $riscvSectionsCodeRaw;
