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
# Defines all known code RISC-V PMU sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#
my $riscvSectionsCodeRaw =
[
    # Code that is Kernel RX only
    KERNEL_CODE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',                         '.kernel_code .kernel_code.*'],
            ['*libSafeRTOS.a',            '.text .text.*'],
            ['*libDrivers.a',             '.text .text.*'],
            ['*falcon_intrinsics_sim.o',  '.text .text.*'],
            ['*pmu_riscv.o',              '.text .text.*'],
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Code that is U+Kernel RX residing in imem
    GLOBAL_CODE_IMEM_RES =>
    {
        PREFIX          => '__syscall_permitted_start = .',
        INPUT_SECTIONS  =>
        [
            ['*',                         '.shared_code .shared_code.*'],
            ['*',                         '.imem_resident .imem_resident.*'],
            ['*',                         '.imem_instRuntime .imem_instRuntime.*'],
            ['*',                         '.imem_ustreamerRuntime.*'],
            ['*libgcc.a',                 '.text .text.*'],
            ['*libShlib.a',               '.text .text.*'],
            ['*libUprocLwos.a',           '.text .text.*'],
            ['*libUprocCmn.a',            '.text .text.*'],
            ['*pmu_oslayer.o',            '.text .text.*'],
            ['*',                         '.syslib_code .syslib_code.*'],
            ['*libSyslib.a',              '.text .text.*'],
        ],
        SUFFIX          => '__syscall_permitted_end   = .',
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Code that is U+Kernel RX in paged imem
    GLOBAL_CODE_IMEM =>
    {
        INPUT_SECTIONS  =>
        [
            ['*libSCP.a',                 '.text .text.*'],
            ['*libMutex.a',               '.text .text.*'],
            ['*c_cover_io_wrapper.o',     '.text .text.*'],
        ],
        LOCATION        => 'imem_odp',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Code that is Kernel RX only in paged imem
    KERNEL_CODE_IMEM =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',                         '.kernel_code_init .kernel_code_init.*'],
            ['*',                         '.kernel_code_deinit .kernel_code_deinit.*'],
            ['*',                         '.imem_init.*'],
        ],
        LOCATION        => 'imem_odp',
        PERMISSIONS     => 'RX',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Usermode code
    USER_CODE =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',                         '.task_code .task_code.*'],
        ],
        CHILD_SECTIONS =>
        [
            LIB_VC => {
                DESCRIPTION => "Contains code for vectorcast coverage",
                PROFILES    => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', ],
            },

            CMDMGMT =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/cmdmgmt/lw/*.o' , '.text.*'],
                    ['*/task_cmdmgmt.o' , '.text.*'],
                    ['*/pbi/lw/pbi.o'   , '.text.*'],
                    ['*'                , '.imem_osTCBDispatcher.*'],
                    ['*'                , '.imem_instCtrl.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            CMDMGMT_MISC => {
                DESCRIPTION => "Contains init/teardown code plus features that ended dumped here due to a lack of better location.",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            CMDMGMT_RPC =>
            {
                DESCRIPTION => "Contains code to handle CMDMGMT specific RPCs.",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            THERM_LIB_FAN_COMMON =>
            {
                NAME           => 'thermLibFanCommon',
                INPUT_SECTIONS =>
                [
                    ['*/fan/lw/objfan.o'        , '.text.*'],
                    ['*/fan/lw/fancooler*.o'    , '.text.*'],
                    ['*/fan/lw/fanpolicy*.o'    , '.text.*'],
                    ['*/fan/lw/*/fanpolicy*.o'  , '.text.*'],
                    ['*/fan/lw/*/fanarbiter*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/fan/lw/objfan.o' , '.text.*'],
                    ['*/task_therm.o'    , '.text.*'],
                    ['*/therm/*/*.o'     , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_POLICY =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_SENSOR_2X =>
            {
                NAME           => 'thermLibSensor2X', # 2X not 2x
                INPUT_SECTIONS =>
                [
                    ['*/therm/lw/thrmdevice*.o'  , '.text.*'],
                    ['*/therm/lw/thrmchannel*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_MONITOR =>
            {
                DESCRIPTION => "Thermal Monitor related routines",
                INPUT_SECTIONS =>
                [
                    ['*/therm/lw/thrmmon*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_FSP_RPC =>
            {
                INPUT_SECTIONS =>
                [
                    ['*libFspRpc.a' , '.text.*'],
                ],
                DESCRIPTION => "Contains common code for RPC communication with FSP",
                PROFILES => [ 'pmu-gh100_riscv', ],
            },

            SMBPBI =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/smbpbi/*/*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            SMBPBI_LWLINK => {
                DESCRIPTION => "Contains set of functions required in get LWlink information for smbpbi.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_FAN => # (FAN30_TODO: Rename to THERM_LIB_FAN_1X)
            {
                INPUT_SECTIONS =>
                [
                    ['*/fan/lw/pmu_fanctrl.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_RPC =>
            {
                DESCRIPTION => "Implementation of thermal RPC calls",
                NAME        => 'thermLibRPC', # instead of 'thermLibRpc'
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_TEST =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/therm/lw/thrmtest.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_FIFO =>
            {
                DESCRIPTION => "Channel and FIFO related functionality",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_GR =>
            {
                DESCRIPTION    => "GR-ELPG entry and exit sequence",
                INPUT_SECTIONS =>
                [
                    ['*/gr/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_MS_LTC =>
            {
                DESCRIPTION    => "L2-RPPG entry and exit sequence",
                INPUT_SECTIONS =>
                [
                    ['*/ms/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
            },

            LIB_PSI_CALLBACK =>
            {
                DESCRIPTION => "PSI callbacks for sleep current callwlation",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LPWR_LP =>
            {
                DESCRIPTION    => "Low Priority task handled by LPWR_LP",
                INPUT_SECTIONS =>
                [
                    ['*/task_lpwr_lp.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-g00x', ],
            },

            LIB_DI =>
            {
                DESCRIPTION    => "Manages Deep Idle entry/exit sequence",
                INPUT_SECTIONS =>
                [
                    ['*/di/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            GCX =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_gcx.o'        , '.text.*'],
                    ['*/gcx/*/pmu_didle*.o', '.text.*'],
                    ['*/gcx/lw/*.o'        , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_GC6 =>
            {
                NAME           => 'libGC6', # GC6 not Gc6
                INPUT_SECTIONS =>
                [
                    ['*/gcx/*/pmu_gc6*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_SYNC_GPIO =>
            {
                DESCRIPTION => "SCI and PMGR GPIOs synchronizing mechanism",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_ACR =>
            {
                DESCRIPTION => "Contains functions of ACR required in GCX task, to avoid loading complete ACR overlay",
                PROFILES    => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
            },

            LIB_SCP_CRYPT_HS =>
            {
                DESCRIPTION => "Contains functions for SCP Crypt  Lib usage on RISCV and Falcon",
                PROFILES => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_SCP_RAND_HS =>
            {
                DESCRIPTION => "Contains functions for SCP RAnd  Lib usage on RISCV and Falcon",
                PROFILES => [ 'pmu-ga10x_riscv...', ],
            },

            I2C =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_i2c.o' , '.text.*'],
                    ['*/i2c/*/*.o'  , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            SPI =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_spi.o', '.text.*'],
                    ['*/spi/*/*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PFF => {
                DESCRIPTION => "Contains set of functions required during piecewise frequency floor lwrve evaluation.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_LOAD =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_ILLUM => {
                DESCRIPTION => "Set of functions required for operation for Illumination control feature.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PWR_EQUATION => {
                DESCRIPTION => "PWR_EQUATION related interfaces",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            PMGR_LIB_PWR_POLICY =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_POLICY_WORKLOAD_MULTIRAIL =>
            {
                DESCRIPTION => "Set of functions required for operation of PWR_POLICY_WORKLOAD_MULTIRAIL class.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_POLICY_WORKLOAD_SHARED =>
            {
                DESCRIPTION => "Set of shared functions required for operation of Workload PWR_POLICY classes.",
                PROFILES    => [ 'pmu-gp100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_POLICY_CLIENT => {
                DESCRIPTION => "Contains generic set of thread safe client interfaces which other tasks can request.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_PWR_POLICY_PERF_CF_PWR_MODEL => {
                DESCRIPTION => "Set of functions required for operation for PWR_POLICY_PERF_CF_PWR_MODEL interface management.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_SCALE => {
                DESCRIPTION => "Set of functions required for operation for PWR_POLICY_PERF_CF_PWR_MODEL scale interfaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_PWR_POLICY_CALLBACK => {
                DESCRIPTION => "Callback containing the PWR_POLICY evaluation callbacks.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_PWR_CHANNELS_CALLBACK =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_pmgr.o', '.text.*'],
                    ['*/pmgr/*/*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_BOARD_OBJ =>
            {
                DESCRIPTION => "Library of Board Object routing routines in PMGR",
                PROFILES    => [ 'pmu-*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_BOARD_OBJ =>
            {
                DESCRIPTION => "Library of Board Object [ Group [ Mask ]] routines.",
                INPUT_SECTIONS =>
                [
                    ['*/boardobj*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_QUERY =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_MONITOR =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_PWR_DEVICE_STATE_SYNC =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ]
            },

            LIB_FXP_BASIC => {
                DESCRIPTION => "Library containing basic FXP math routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_FXP_EXTENDED => {
                DESCRIPTION => "Library containing extended FXP math routines. Added to reduce amount of attached code when only basic functionaly is required.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_LEAKAGE   =>
            {
                DESCRIPTION => "Library containing leakage evaluation routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIBI2C =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PWM => {
                DESCRIPTION => "Library containing PWM routines required at runtime.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_LW64 => {
                DESCRIPTION    => "Unsigned 64-bit math. Most common operations (add/sub/cmp) are kept resident.",
                INPUT_SECTIONS =>
                [
                    ['*_udivdi3.*', '.text'],
                    ['*_lshrdi3.*', '.text'],
                    ['*_ashldi3.*', '.text'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_LW64_UMUL => {
                DESCRIPTION    => "Unsigned 64-bit multiplication used by both .libLw64 and .libLwF32",
                INPUT_SECTIONS =>
                [
                    ['*_muldi3.*' , '.text'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_LW_F32 => {
                DESCRIPTION    => "Single precision (IEEE-754 binary32) floating point math. Mul/Div depends on 64-bit integer multiplication (.libLw64)",
                INPUT_SECTIONS =>
                [
                    ['*sf.o' , '.text'],
                    ['*si.o' , '.text'],
                    ['*si2.o', '.text'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_VFE_BOARD_OBJ => {
                DESCRIPTION => "VFE routines triggered by RM's SET & GET_STATUS commands.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_VFE => {
                DESCRIPTION => "Overlay containing VFE routines.",
                INPUT_SECTIONS =>
                [
                    ['*/perf/lw/3x/vfe*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_VFE_EQU_MONITOR => {
                DESCRIPTION => "Overlay containing VFE_EQU_MONITOR routines that need to be used both by VFE code and by PMU clients of VFE_EQU_MONITOR.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_BOARD_OBJ => {
                DESCRIPTION => "Library of PERF Board Object Group SET/GET_STATUS handlers.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PSTATE_BOARD_OBJ => {
                DESCRIPTION => "Library containint PSTATE routines triggered by RM's SET_OBJECT or GET_STATUS command (or legacy communications).",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLOCK_MON  => {
                DESCRIPTION => "Library containing clock routines for clock monitors",
                PROFILES    => [ ],
                INPUT_SECTIONS =>
                [
                    ['*/clk/lw/clk_clockmon*.o', '.text.*'],
                ],
            },

            PERF_CLK_AVFS => {
                DESCRIPTION => "AVFS clock routines that are loaded by perf task.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CHANGE_SEQ => {
                DESCRIPTION => "Library containing perf change sequence routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_LIMIT_BOARDOBJ => {
                DESCRIPTION => "Library containing PERF_LIMIT routines implementing BOARDOBJ RM_PMU interfaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_LIMIT => {
                DESCRIPTION => "Library containing base PERF_LIMIT routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_LIMIT_ARBITRATE => {
                DESCRIPTION => "Library containing PERF_LIMIT perfLimitsArbitrate() routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_LIMIT_VF => {
                DESCRIPTION => "Library containing PERF_LIMIT VF routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_LIMIT_CLIENT => {
                DESCRIPTION => "Library containing PERF_LIMIT client routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_POLICY_BOARD_OBJ => {
                DESCRIPTION => "Library containing PERF_POLICY routines implementing BOARDOBJ RM_PMU intefaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_POLICY => {
                DESCRIPTION => "Library containing PERF_POLICY routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_VF => {
                DESCRIPTION    => "Library containing all PERF VF/POR routines.  Shareable client overlay with other TASKs for any shareable PERF VF/POR routines.",
                INPUT_SECTIONS =>
                [
                    ['*/perf/*/lib_perf.o', '.text.*'],
                    ['*/perf/*/perfps20.o', '.text.*'],
                    ['*/perf/*/perfps30.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_perf.o'          , '.text.*'],
                    ['*/perf/*/pmu_objperf.o' , '.text.*'],
                    ['*/perf/*/perfrmcmd.o'   , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_ILWALIDATION => {
                DESCRIPTION => "Library containing PERF ilwalidation interfaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_VF_ILWALIDATION => {
                DESCRIPTION => "Library containing PERF lwrve ilwalidation interfaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            VF_LOAD => {
                DESCRIPTION => "Library containing VF lwrve load interfaces.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VOLT =>
            {
                DESCRIPTION    => "Library containing volt routines.",
                INPUT_SECTIONS =>
                [
                    ['*/volt/*/objvolt.o'     , '.text.*'],
                    ['*/volt/*/voltrail*.o'   , '.text.*'],
                    ['*/volt/*/voltdev*.o'    , '.text.*'],
                    ['*/volt/*/voltpolicy*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VOLT_API =>
            {
                DESCRIPTION    => "Library containing client volt API-s.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_GPIO =>
            {
                DESCRIPTION => "Library containing GPIO routines required to configure GPIOs.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_AVG =>
            {
                DESCRIPTION => "Library containing counter avg routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PMUMON =>
            {
                DESCRIPTION => "Library containing PMUMON runtime routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PMUMON_CONSTRUCT =>
            {
                DESCRIPTION => "Library containing PMUMON construction routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_READ_ANY_TASK => {
                DESCRIPTION => "Library containing Clocks read routines which may be called from any task.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_READ_PERF_DAEMON => {
                DESCRIPTION => "Library containing Clocks read routines to be called only within the PERF_DAEMON task.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_CONFIG => {
                DESCRIPTION => "Library containing Clocks config routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_PROGRAM => {
                DESCRIPTION => "Library containing Clocks program/cleanup routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_STATUS => {
                DESCRIPTION => "Library containing CLK BOARDOBJ GET_STATUS routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_DAEMON =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_perf_daemon.o'  , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_DAEMON_CHANGE_SEQ => {
                DESCRIPTION => "Library containing perf change sequence daemon routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_DAEMON_CHANGE_SEQ_LPWR => {
                DESCRIPTION => "Library containing perf change sequence daemon LPWR routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            CHANGE_SEQ_LOAD => {
                DESCRIPTION => "Library containing perf change sequence load routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERFMON =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_perfmon.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
            },

            LIB_GPUMON =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_EFF_AVG => {
                DESCRIPTION => "Library containing routines for CLK_FREQ_EFFECTIVE_AVG feature.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            CLK_LIB_CNTR =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_FREQ_COUNTED_AVG => {
                DESCRIPTION => "Library containing routines for CLK_FREQ_COUNTED_AVG feature.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            CLK_LPWR => {
                DESCRIPTION => "Library containing clock routines needed for diferent LPWR features.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_CONTROLLER => {
                DESCRIPTION => "Library containing routines for all clock controllers.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_FREQ_CONTROLLER => {
                DESCRIPTION => "Library containing routines for CLK_FREQ_CONTROLLER feature.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ]
            },

            LIB_CLK_VOLT_CONTROLLER => {
                DESCRIPTION => "Library containing routines for CLK_VOLT_CONTROLLER feature.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            DISP =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_disp.o' , '.text.*'],
                    ['*/disp/*/*.o'  , '.text.*'],
                    ['*/psr/*/*.o'   , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            DISP_IMP =>
            {
                DESCRIPTION => "Contains display IMP related functions",
                PROFILES   => [ 'pmu-ga10x_riscv...', '-pmu-gh100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            SHA1 =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/sha1/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            GID =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/gid/*.o'   , '.text.*'],
                    ['*/ecid/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LOGGER_WRITE =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_MONITOR_GPUMON =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            ACR =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_acr.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
            },

            BIF =>
            {
                DESCRIPTION => "BIF related code",
                INPUT_SECTIONS =>
                [
                    ['*/task_bif.o', '.text.*'],
                    ['*/bif/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_BIF =>
            {
                DESCRIPTION => "Library containing bif routines required in PERF change task, to avoid loading complete BIF overlay",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF_BOARD_OBJ => {
                DESCRIPTION => "PERF_CF Board Object Group SET/GET_STATUS handlers.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF_LOAD => {
                DESCRIPTION => "PERF_CF LOAD functions.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_DEV_INFO => {
                DESCRIPTION => "Device info query library.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF => {
                DESCRIPTION => "PERF_CF common and Controller/Policy code.",
                INPUT_SECTIONS =>
                [
                    ['*/perf/lw/cf/*.o', '.text.*'],
                    ['*/task_perf_cf.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_ENG_UTIL =>
            {
                DESCRIPTION => "Computes utilization of Graphics engine, FB engine, Video (enc/dec) engine, and PCIE.",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            PERF_CF_PM_SENSORS => {
                DESCRIPTION => "PERF_CF PM Sensor code.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF_TOPOLOGY => {
                DESCRIPTION => "PERF_CF Sensor and Topology code.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF_MODEL => {
                DESCRIPTION => "PERF_CF PwrModel code.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            NNE =>
            {
                DESCRIPTION    => "NNE task imem overlay",
                INPUT_SECTIONS =>
                [
                    [ '*/task_nne.o', '.text.*', ],
                    [ '*/objnne.o', '.text.*', ],
                ],
                PROFILES       => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            NNE_INFERENCE_CLIENT =>
            {
                DESCRIPTION  => "NNE client-side inference library",
                PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            NNE_BOARD_OBJ =>
            {
                DESCRIPTION  => "NNE BOARDOBJGRP_CMD RPC handling",
                PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            NNE_INIT =>
            {
                DESCRIPTION  => "NNE init imem overlay",
                PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            CLK_LIB_CLK3 => {
                NAME            =>  "clkLibClk3",
                DESCRIPTION     =>  "Clocks3.0 programming routines that are loaded by perf task.",
                INPUT_SECTIONS  =>
                [
                    ['*/clk3/fs/clkxtal.o', '.text.*'],
                    ['*/clk3/fs/clkwire.o', '.text.*'],
                    ['*/clk3/fs/clkapll.o', '.text.*'],
                    ['*/clk3/fs/clkmultiplier.o', '.text.*'],
                    ['*/clk3/fs/clkdivider.o', '.text.*'],
                    ['*/clk3/fs/clkldivunit.o', '.text.*'],
                    ['*/clk3/fs/clkmux.o', '.text.*'],
                    ['*/clk3/fs/clkreadonly.o', '.text.*'],
                    ['*/clk3/fs/clkbif.o', '.text.*'],
                    ['*/clk3/fs/clknafll.o', '.text.*'],
                    ['*/clk3/fs/clkadynramp.o', '.text.*'],
                    ['*/clk3/fs/clkasdm.o', '.text.*'],
                    ['*/clk3/sd/clksdgv100.o','.text.*'],
                    ['*/clk3/clkfieldvalue.o', '.text.*'],
                    ['*/clk3/clksignal.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            VBIOS =>
            {
                DESCRIPTION => "VBIOS related code",
                INPUT_SECTIONS =>
                [
                    ['*/vbios/lw/objvbios.o'  , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VBIOS =>
            {
                DESCRIPTION => "VBIOS library related code",
                INPUT_SECTIONS =>
                [
                    ['*/vbios/lw/vbios_ied.o'  , '.text.*'],
                    ['*/vbios/lw/vbios_opcodes.o'  , '.text.*'],
                    ['*/vbios/pascal/pmu_vbiosgp10x.o'  , '.text.*'],
                    ['*/vbios/turing/pmu_vbiostu10x.o'  , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            DISP_PMS =>
            {
                DESCRIPTION => "DISPLAY PMS related code",
                INPUT_SECTIONS =>
                [
                    ['*/disp/pascal/*.o'  , '.text.*'],
                    ['*/disp/turing/*.o'  , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VBIOS_IMAGE =>
            {
                DESCRIPTION => "Contains the code for reading data out of the VBIOS image",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VOLT_TEST =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/volt/lw/volttest.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PRIV_SEC=>
            {
                DESCRIPTION => "PMU PRIV level settings related",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_SEMAPHORE_R_W =>
            {
                DESCRIPTION => "Reader-writer semaphore library",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            WATCHDOG =>
            {
                INPUT_SECTIONS =>
                [
                    ['*/task_watchdog.o' , '.text.*'],
                    ['*/watchdog/*/*.o', '.text.*'],
                ],
                PROFILES    => [ ],
            },

            WATCHDOG_CLIENT =>
            {
                PROFILES    => [ ],
            },

            PMGR_LIB_FACTOR_A =>
            {
                DESCRIPTION => "Function to compute factor A",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_LWLINK =>
            {
                DESCRIPTION => "Update HSHUB registers for LWLink/PCIe traffic",
                PROFILES    => [ ],
            },

            LIB_BIF_MARGINING =>
            {
                DESCRIPTION => "Library containing bif routines required to handle margining requests",
                PROFILES    => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gh100_riscv', 'pmu-gb100_riscv', ],
            },

            LOWLATENCY =>
            {
                DESCRIPTION    => "Contains LOWLATENCY task related code",
                INPUT_SECTIONS =>
                [
                    ['*/task_lowlatency.o', '.text.*'],
                    ['*/lowlatency/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

#*********************************  Cold Code  ********************************#
#                                                                              #
# Cold code is code that runs:                                                 #
#   * never (i.e. error handling paths for errors that should not occur)       #
#   * once (i.e. all init code exelwted before starting task scheduler, ...)   #
#   * handfull of times (typically helpers used in init / construct code, ...) #
#   * does not run at production (extending the "definition" at code that is   #
#     ilwoked only during in-house testing and should never run on customer    #
#     systems / in production environment)                                     #
#                                                                              #
# Having cold code interleaved with code exelwted at runtime increases memory  #
# footprint and decreases the perfromance of ODP and overall code exelwteion.  #
#                                                                              #
# Place sections that satisfy above criteria below this point assuring that    #
# cold code is not penalizing frequently / periodically running code.          #
# Reach to core-PMU team in case of any questions and avoid guessing.          #
#                                                                              #
#******************************************************************************#

            ERROR_HANDLING_COLD =>
            {
                DESCRIPTION => "Error handling cold code",
                PROFILES => [ 'pmu-*riscv', ],
            },

            LIB_SANITIZER_COV => {
                DESCRIPTION => "Contains code for SanitizerCoverage (GCC coverage-guided fuzzing instrumentation).",
                PROFILES    => [ 'pmu-*_riscv', '-pmu-ga1*b_riscv', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-g00x_riscv', ],
            },

            LIB_PMGR_TEST =>
            {
                DESCRIPTION => "Contains code related to PMGR test infrastructure",
                INPUT_SECTIONS =>
                [
                    ['*/pmgr/lw/pmgrtest.o' , '.text.*'],
                ],
                PROFILES => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LPWR_TEST =>
            {
                DESCRIPTION => "lpwr test related APIs",
                PROFILES    => [ ],
            },

            THERM_LIB_FAN_COMMON_CONSTRUCT =>
            {
                NAME        => 'thermLibFanCommonConstruct',
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            THERM_LIB_CONSTRUCT =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            SPI_LIB_CONSTRUCT => {
                DESCRIPTION => "Library containing SPI routines triggered by RM's SET_OBJECT command.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_ILLUM_CONSTRUCT => {
                DESCRIPTION => "Contains set of functions required during construction of ILLUM object.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_CONSTRUCT =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PMGR_LIB_PWR_POLICY_CONSTRUCT =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PWM_CONSTRUCT => {
                DESCRIPTION => "Library containing PWM routines required only during init.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VPSTATE_CONSTRUCT => {
                DESCRIPTION => "Library containing VPSTATE routines triggered by RM's SET_OBJECT command.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_VOLT_CONSTRUCT =>
            {
                DESCRIPTION => "Library containing volt device construct routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_CONSTRUCT => {
                DESCRIPTION => "Library containing CLK BOARDOBJ SET routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_PRINT => {
                DESCRIPTION => "Library containing Clocks print routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LPWR_LOADIN =>
            {
                DESCRIPTION => "Handles LPWR_LOADIN RPCs",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            #
            # By convention, the section .init regroups all initialization functions
            # that will be exelwted once at boot up. We create a special overlay for
            # that in order to save a significant amount of code space.
            #
            # .init and .libInit (below) overlays are loaded at start-up, exelwted and
            # will be overwritten by the other overlays. After this point, .init
            # overlay should not be loaded but any task can load .libInit overlay
            # during RM based initialization.
            #
            # As the first overlay defined, this overlay must start on 256-byte aligned
            # address.
            #
            INIT =>
            {
                DESCRIPTION => 'Initialization overlay for one-time code.',
                INPUT_SECTIONS =>
                [
                    '. = ALIGN(256)',
                    ['*', '.imem_instInit.*'],
                    ['*', '.imem_ustreamerInit.*'],
                    ['*', '.imem_osTmrCallbackInit.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_INIT =>
            {
                DESCRIPTION => "Section .libInit regroups all functions that will get exelwted during PMU boot process and RM based initialization of task.",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            SPI_LIB_ROM_INIT =>
            {
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PMGR_INIT =>
            {
                DESCRIPTION => "Library containing pmgr routines that are exlwted when the pmgr task scheduled for the first time.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CLK_AVFS_INIT =>
            {
                DESCRIPTION => "AVFS clock initialization routines that are loaded by perf task.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PERF_INIT =>
            {
                DESCRIPTION => "Library containing perf routines that are exlwted when the perf task scheduled for the first time.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PSTATE_BOARD_OBJ_INIT => {
                DESCRIPTION => "Library containing PSTATE BOARDOBJ routines used during construction and initialization.",
                PROFILES    => [ 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gb100_riscv...', ],
            },

            LIB_BIF_INIT =>
            {
                DESCRIPTION => "Library containing bif routine that is exlwted when the bif task scheduled for the first time.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            PERF_CF_INIT =>
            {
                DESCRIPTION => "PERF_CF code used only in the post-init paths.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_FSP_RPC_INIT =>
            {
                DESCRIPTION => "FSP RPC code used only in post-init paths.",
                PROFILES    => [ 'pmu-gh100_riscv' ],
            },

            LIB_VGPU =>
            {
                DESCRIPTION => "Update per-VF BAR1 size",
                PROFILES    => [ 'pmu-ga10x_riscv', ],
            },

#*********************************  Cold Code  ********************************#
#                                                                              #
# Place cold code sections above this point.                                   #
#                                                                              #
#******************************************************************************#

        ],
        LOCATION        => 'imem_odp',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Usermode code resident
    USER_CODE_RES =>
    {
        CHILD_SECTIONS  =>
        [
            LPWR_RESIDENT =>
            {
                DESCRIPTION => "Functionalities handled by LPWR which has to be resident",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            RTD3_ENTRY =>
            {
                DESCRIPTION => "RTD3/GC6 enetry specific section",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            #
            # Note: this has to be resident for now because code in it is called
            # from the shutdown sequence code, which does not support paging
            # code in. If this code can be pinned before the shutdown sequence,
            # then this can be made non-resident.
            #
            LIB_SCP_COMMON =>
            {
                DESCRIPTION => "Contains common SCP library functionality.",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },
        ],
        LOCATION        => 'imem_res',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    LPWR_PINNED_CODE =>
    {
        CHILD_SECTIONS  =>
        [
            LIB_RM_ONESHOT =>
            {
                DESCRIPTION => "Library containing RM Disp oneshot routines.",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_PSI =>
            {
                DESCRIPTION => "Power Feature Coupled PSI entry and exit sequence",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LPWR =>
            {
                DESCRIPTION    => "Common functionalities handled by LPWR",
                INPUT_SECTIONS =>
                [
                    ['*/task_lpwr.o', '.text.*'],
                    ['*/pg/*/*.o'   , '.text.*'],
                    ['*/lpwr/*/*.o' , '.text.*'],
                    ['*/fifo/*/*.o' , '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_LPWR =>
            {
                DESCRIPTION => "LPWR task level APIs",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            LIB_MS =>
            {
                DESCRIPTION    => "MSCG entry and exit sequence",
                INPUT_SECTIONS =>
                [
                    ['*/ms/*/*.o', '.text.*'],
                ],
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', 'pmu-g*b', ],
            },

            LIB_AP =>
            {
                DESCRIPTION => "Adaptive Power algorithm",
                PROFILES    => [ 'pmu-ga10x_riscv...', ],
            },

            CLK_LIB_CNTR_SW_SYNC =>
            {
                DESCRIPTION => "Library containing clock counter routines required to sync the SW cntr with HW cntr (Client ex: LPWR)",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },

            LIB_CLK_VOLT  => {
                DESCRIPTION => "Library containing clock routines to get effective voltage from ADCs",
                PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
            },
        ],
        LOCATION        => 'imem_odp',
        PERMISSIONS     => 'rx',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },
];

# return the raw data of Sections definition
return $riscvSectionsCodeRaw;
