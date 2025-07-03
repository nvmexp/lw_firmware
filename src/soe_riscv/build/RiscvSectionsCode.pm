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
# Defines all known code RISC-V soe sections and overlays.
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
            ['*',               '.kernel_code_init .kernel_code_init.*'],
            ['*',               '.kernel_code_deinit .kernel_code_deinit.*'],
            ['*libSafeRTOS.a',  '.text .text.*'],
            ['*/kernel/*.o',    '.text .text.*'],
            ['*libDrivers.a',   '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'soe-*', ],
    },

    ### Global sections
    GLOBAL_CODE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.syslib_code .syslib_code.*'],
            ['*',               '.shared_code .shared_code.*'],
            ['*',               '.imem_resident .imem_resident.*'],
            ['*libSyslib.a',    '.text .text.*'],
            ['*libShlib.a',     '.text .text.*'],
            ['*libUprocLwos.a', '.text .text.*'],
            ['*libUprocCmn.a',  '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    ### Task sections
    TASK_CODE_IDLE =>
    {
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_RM_CMD =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/cmdmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_RM_MSG =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/msgmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx', #TODO: Check whether we really need read permission for here
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_CHNLMGMT =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/chnlmgmt/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_WORKLAUNCH =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/worklaunch/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_TEST =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/test/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_SCHEDULER =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/scheduler/*.o', '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_THERM =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/therm/*.o', '.text.*'],
            ['*/therm/*/*.o'     , '.text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_CORE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/core/*.o', '.text.*'],
            ['*/core/*/*.o'     , '.text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_SPI =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/spi/*.o', '.text.*'],
            ['*/spi/*/*.o'     , '.text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_BIF =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/bif/*.o', '.text.*'],
            ['*/bif/*/*.o'     , '.text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },

    TASK_CODE_SMBPBI =>
    {
        INPUT_SECTIONS  =>
        [
            ['*/tasks/smbpbi/*.o', '.text.*'],
            ['*/smbpbi/*/*.o'     , '.text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'soe-*', ],
    },
];

# return the raw data of Sections definition
return $riscvSectionsCodeRaw;
