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
# Defines all known PMU IMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of code that can be paged in and out of
#     IMEM at runtime. To gcc, this is simply a code section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique code address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     INPUT_SECTIONS
#         Defines a two-dimensional array where each top-level array element
#         defines a filename,section pair. The filename may refer to a specific
#         object file or a file glob. Combined with the section, the pair
#         represents an input code-section that will be mapped into the overlay
#         (output-section) by the linker.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     ALIGNMENT
#         Defines the starting alignment of the overlay. By default, overlays
#         are not aligned. Each overlay starts where the previous overlay
#         ended. This attribute may be used if a specific alignment is required
#         (e.g. 256-byte alignment).
#
#     PROFILES
#         Defines the list of profiles on which the overlay is enabled. If an
#         overlay is not used on a particular profile, that profile must be
#         excluded from this list. We generate symbols in DMEM for every overlay
#         that is enabled, and that oclwpies (limited) DMEM resources. Hence, it
#         is important to make sure that we only enable the overlay only on
#         profiles where they are used.
#         Every overlay must specify either PROFILES or ENABLED_ON attribute.
#
#     ENABLED_ON
#         Defines the list of chips on which the overlay is enabled. If an
#         overlay is not used on a particular chip, that chip must be excluded
#         from this list. We generate symbols in DMEM for every overlay that is
#         enabled, and that oclwpies (limited) DMEM resources. Hence, it is
#         important to make sure that we only enable the overlay only on chips
#         where they are used.
#         Every overlay must specify either PROFILES or ENABLED_ON attribute.
#

my $imemOverlaysRaw =
[
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
        ALIGNMENT   => 256,
        DESCRIPTION => 'Initialization overlay for one-time code.',
        INPUT_SECTIONS =>
        [
            ['*', '.imem_instInit.*'],
            ['*', '.imem_ustreamerInit.*'],
            ['*', '.imem_osTmrCallbackInit.*'],
        ],
        PROFILES    => [ 'pmu-*', ],
    },

    #
    # Section .libInit regroups all functions that will get exelwted during PMU
    # boot process and RM based initialization of task.
    #
    LIB_INIT =>
    {
        PROFILES    => [ 'pmu-*', ],
    },

    LIB_VC => {
        DESCRIPTION => "Contains code for vectorcast coverage",
        PROFILES    => [ ],
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
        PROFILES    => [ 'pmu-*', ],
    },

    CMDMGMT_MISC => {
        DESCRIPTION => "Contains init/teardown code plus features that ended dumped here due to a lack of better location.",
        PROFILES    => [ 'pmu-*', ],
    },

    CMDMGMT_RPC =>
    {
        DESCRIPTION => "Contains code to handle CMDMGMT specific RPCs.",
        PROFILES    => [ 'pmu-*', ],
    },

    THERM_LIB_FAN_COMMON_CONSTRUCT =>
    {
        NAME        => 'thermLibFanCommonConstruct',
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
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
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM =>
    {
        INPUT_SECTIONS =>
        [
            ['*/fan/lw/objfan.o' , '.text.*'],
            ['*/task_therm.o'    , '.text.*'],
            ['*/therm/*/*.o'     , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM_LIB_POLICY =>
    {
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    THERM_LIB_SENSOR_2X =>
    {
        NAME           => 'thermLibSensor2X', # 2X not 2x
        INPUT_SECTIONS =>
        [
            ['*/therm/lw/thrmdevice*.o'  , '.text.*'],
            ['*/therm/lw/thrmchannel*.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM_LIB_CONSTRUCT =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM_LIB_MONITOR =>
    {
        DESCRIPTION => "Thermal Monitor related routines",
        INPUT_SECTIONS =>
        [
            ['*/therm/lw/thrmmon*.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    SMBPBI =>
    {
        INPUT_SECTIONS =>
        [
            ['*/smbpbi/*/*.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    SMBPBI_LWLINK => {
        DESCRIPTION => "Contains set of functions required to get LWlink information for smbpbi.",
        PROFILES    => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    THERM_LIB_FAN => # (FAN30_TODO: Rename to THERM_LIB_FAN_1X)
    {
        INPUT_SECTIONS =>
        [
            ['*/fan/lw/pmu_fanctrl.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM_LIB_RPC =>
    {
        DESCRIPTION => "Implementation of thermal RPC calls",
        NAME        => 'thermLibRPC', # instead of 'thermLibRpc'
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    THERM_LIB_TEST =>
    {
        INPUT_SECTIONS =>
        [
            ['*/therm/lw/thrmtest.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LPWR_LOADIN =>
    {
        DESCRIPTION => "Handles LPWR_LOADIN RPCs",
        PROFILES    => [ 'pmu-*', ],
    },

    LIB_FIFO =>
    {
        DESCRIPTION => "Channel and FIFO related functionality",
        PROFILES    => [ 'pmu-*', ],
    },

    LIB_GR =>
    {
        DESCRIPTION    => "GR-ELPG entry and exit sequence",
        INPUT_SECTIONS =>
        [
            ['*/gr/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gm*x', ],
    },

    LIB_RM_ONESHOT =>
    {
        DESCRIPTION => "Library containing RM Disp oneshot routines.",
        PROFILES    => [ 'pmu-*', '-pmu-gp100', '-pmu-g*b*', ],
    },

    LIB_PSI =>
    {
        DESCRIPTION => "Power Feature Coupled PSI entry and exit sequence",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_PSI_CALLBACK =>
    {
        DESCRIPTION => "PSI callbacks for sleep current callwlation",
        PROFILES    => [ 'pmu-gp10x...', '-pmu-g*b*', ],
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
        PROFILES    => [ 'pmu-*', ],
    },

    LPWR_LP =>
    {
        DESCRIPTION    => "Low Priority task handled by LPWR_LP",
        INPUT_SECTIONS =>
        [
            ['*/task_lpwr_lp.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-ga10b', '-pmu-gh100', '-pmu-g00x', ],
    },

    LIB_LPWR =>
    {
        DESCRIPTION => "LPWR task level APIs",
        PROFILES    => [ 'pmu-*', ],
    },

    LIB_MS =>
    {
        DESCRIPTION    => "MSCG entry and exit sequence",
        INPUT_SECTIONS =>
        [
            ['*/ms/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gp100', '-pmu-g*b*', 'pmu-ga10b', 'pmu-gv11b', ],
    },

    LPWR_TEST =>
    {
        DESCRIPTION => "lpwr test related APIs",
        PROFILES    => [ ],
    },

    LIB_DI =>
    {
        DESCRIPTION    => "Manages Deep Idle entry/exit sequence",
        INPUT_SECTIONS =>
        [
            ['*/di/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gp100', '-pmu-g*b*', ],
    },

    GCX =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_gcx.o'        , '.text.*'],
            ['*/gcx/*/pmu_didle*.o', '.text.*'],
            ['*/gcx/lw/*.o'        , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gp100', '-pmu-g*b*', ],
    },

    LIB_GC6 =>
    {
        NAME           => 'libGC6', # GC6 not Gc6
        INPUT_SECTIONS =>
        [
            ['*/gcx/*/pmu_gc6*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    RTD3_ENTRY =>
    {
        DESCRIPTION => "RTD3/GC6 enetry specific section",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_SYNC_GPIO =>
    {
        DESCRIPTION => "SCI and PMGR GPIOs synchronizing mechanism",
        PROFILES    => [ 'pmu-*', '-pmu-gp100', '-pmu-g*b*', ],
    },

    LIB_AP =>
    {
        DESCRIPTION => "Adaptive Power algorithm",
        PROFILES    => [ 'pmu-*', '-pmu-gp100', ],
    },

    LIB_ACR =>
    {
        DESCRIPTION => "Contains functions of ACR required in GCX task, to avoid loading complete ACR overlay",
        PROFILES    => [ 'pmu-gm2*', 'pmu-gp*', 'pmu-gv*', 'pmu-ga10b', ],
    },

    SEQ =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_sequencer.o', '.text.*'],
            ['*/seq/*/*.o',        '.text.*'],
            ['*/fb/*/*.o' ,        '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', '-pmu-ga100...', ],
    },

    I2C =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_i2c.o' , '.text.*'],
            ['*/i2c/*/*.o'  , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    SPI =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_spi.o', '.text.*'],
            ['*/spi/*/*.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    SPI_LIB_CONSTRUCT => {
        DESCRIPTION => "Library containing SPI routines triggered by RM's SET_OBJECT command.",
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    SPI_LIB_ROM_INIT =>
    {
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    PMGR_LIB_ILLUM_CONSTRUCT => {
        DESCRIPTION => "Contains set of functions required during construction of ILLUM object.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR_LIB_PFF => {
        DESCRIPTION => "Contains set of functions required during piecewise frequency floor lwrve evaluation.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PMGR_INIT => {
        DESCRIPTION => "Library containing pmgr routines that are exlwted when the pmgr task scheduled for the first time.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_CONSTRUCT =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_POLICY_CONSTRUCT =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_LOAD =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_ILLUM => {
        DESCRIPTION => "Set of functions required for operation for Illumination control feature.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PWR_EQUATION => {
        DESCRIPTION => "PWR_EQUATION related interfaces",
        PROFILES    => [ 'pmu-gv10x...', ],
    },

    PMGR_LIB_PWR_POLICY =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_POLICY_WORKLOAD_MULTIRAIL =>
    {
        DESCRIPTION => "Set of functions required for operation of PWR_POLICY_WORKLOAD_MULTIRAIL class.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_POLICY_WORKLOAD_SHARED =>
    {
        DESCRIPTION => "Set of shared functions required for operation of Workload PWR_POLICY classes.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_POLICY_CLIENT => {
        DESCRIPTION => "Contains generic set of thread safe client interfaces which other tasks can request.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_PWR_POLICY_PERF_CF_PWR_MODEL => {
        DESCRIPTION => "Set of functions required for operation for PWR_POLICY_PERF_CF_PWR_MODEL interface management.",
        PROFILES    => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_SCALE => {
        DESCRIPTION => "Set of functions required for operation for PWR_POLICY_PERF_CF_PWR_MODEL scale interfaces.",
        PROFILES    => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    PMGR_PWR_POLICY_CALLBACK => {
        DESCRIPTION => "Callback containing the PWR_POLICY evaluation callbacks.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', '-pmu-*0a', ],
    },

    PMGR_PWR_CHANNELS_CALLBACK =>
    {
        PROFILES    => [ 'pmu-g*05', 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_pmgr.o', '.text.*'],
            ['*/pmgr/*/*.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_BOARD_OBJ =>
    {
        DESCRIPTION => "Library of Board Object routing routines in PMGR",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_BOARD_OBJ =>
    {
        DESCRIPTION => "Library of Board Object [ Group [ Mask ]] routines.",
        INPUT_SECTIONS =>
        [
            ['*/boardobj*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_QUERY =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_MONITOR =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_PWR_DEVICE_STATE_SYNC =>
    {
        PROFILES    => ['pmu-*', '-pmu-g*b*', ]
    },

    LIB_FXP_BASIC => {
        DESCRIPTION => "Library containing basic FXP math routines.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_FXP_EXTENDED => {
        DESCRIPTION => "Library containing extended FXP math routines. Added to reduce amount of attached code when only basic functionaly is required.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_LEAKAGE   =>
    {
        DESCRIPTION => "Library containing leakage evaluation routines.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIBI2C =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_PWM_CONSTRUCT => {
        DESCRIPTION => "Library containing PWM routines required only during init.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_PWM => {
        DESCRIPTION => "Library containing PWM routines required at runtime.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_LW64 => {
        DESCRIPTION    => "Unsigned 64-bit math. Most common operations (add/sub/cmp) are kept resident.",
        INPUT_SECTIONS =>
        [
            ['*_udivdi3.*', '.text'],
            ['*_lshrdi3.*', '.text'],
            ['*_ashldi3.*', '.text'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gm20x', '-pmu-g*b*', ],
    },

    LIB_LW64_UMUL => {
        DESCRIPTION    => "Unsigned 64-bit multiplication used by both .libLw64 and .libLwF32",
        INPUT_SECTIONS =>
        [
            ['*_muldi3.*' , '.text'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-gm20x', '-pmu-g*b*', ],
    },

    LIB_LW_F32 => {
        DESCRIPTION    => "Single precision (IEEE-754 binary32) floating point math. Mul/Div depends on 64-bit integer multiplication (.libLw64)",
        INPUT_SECTIONS =>
        [
            ['*sf.o' , '.text'],
            ['*si.o' , '.text'],
            ['*si2.o', '.text'],
        ],
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_VFE_BOARD_OBJ => {
        DESCRIPTION => "VFE routines triggered by RM's SET & GET_STATUS commands.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_VFE => {
        DESCRIPTION => "Overlay containing VFE routines.",
        INPUT_SECTIONS =>
        [
            ['*/perf/lw/3x/vfe*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_VFE_EQU_MONITOR => {
        DESCRIPTION => "Overlay containing VFE_EQU_MONITOR routines that need to be used both by VFE code and by PMU clients of VFE_EQU_MONITOR.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_PERF_BOARD_OBJ => {
        DESCRIPTION => "Library of PERF Board Object Group SET/GET_STATUS handlers.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_PSTATE_BOARD_OBJ => {
        DESCRIPTION => "Library containint PSTATE routines triggered by RM's SET_OBJECT or GET_STATUS command (or legacy communications).",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_VPSTATE_CONSTRUCT => {
        DESCRIPTION => "Library containing VPSTATE routines triggered by RM's SET_OBJECT command.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLOCK_MON  => {
        DESCRIPTION => "Library containing clock routines for clock monitors",
        PROFILES    => [ 'pmu-ga100', ],
        INPUT_SECTIONS =>
        [
            ['*/clk/lw/clk_clockmon*.o', '.text.*'],
        ],
    },

    LIB_CLK_VOLT  => {
        DESCRIPTION => "Library containing clock routines to get effective voltage from ADCs",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_CLK_AVFS_INIT => {
        DESCRIPTION => "AVFS clock initialization routines that are loaded by perf task.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_CLK_AVFS => {
        DESCRIPTION => "AVFS clock routines that are loaded by perf task.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CHANGE_SEQ => {
        DESCRIPTION => "Library containing perf change sequence routines.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_PERF_INIT => {
        DESCRIPTION => "Library containing perf routines that are exlwted when the perf task scheduled for the first time.",
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_PERF_LIMIT_BOARDOBJ => {
        DESCRIPTION => "Library containing PERF_LIMIT routines implementing BOARDOBJ RM_PMU interfaces.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PERF_LIMIT => {
        DESCRIPTION => "Library containing base PERF_LIMIT routines.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_LIMIT_ARBITRATE => {
        DESCRIPTION => "Library containing PERF_LIMIT perfLimitsArbitrate() routines.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_LIMIT_VF => {
        DESCRIPTION => "Library containing PERF_LIMIT VF routines.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PERF_LIMIT_CLIENT => {
        DESCRIPTION => "Library containing PERF_LIMIT client routines.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PERF_POLICY_BOARD_OBJ => {
        DESCRIPTION => "Library containing PERF_POLICY routines implementing BOARDOBJ RM_PMU intefaces.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PERF_POLICY => {
        DESCRIPTION => "Library containing PERF_POLICY routines.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_VF => {
        DESCRIPTION    => "Library containing all PERF VF/POR routines.  Shareable client overlay with other TASKs for any shareable PERF VF/POR routines.",
        INPUT_SECTIONS =>
        [
            ['*/perf/*/lib_perf.o', '.text.*'],
            ['*/perf/*/perfps20.o', '.text.*'],
            ['*/perf/*/perfps30.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PERF =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_perf.o'          , '.text.*'],
            ['*/perf/*/pmu_objperf.o' , '.text.*'],
            ['*/perf/*/perfrmcmd.o'   , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_ILWALIDATION => {
        DESCRIPTION => "Library containing PERF ilwalidation interfaces.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_VF_ILWALIDATION => {
        DESCRIPTION => "Library containing PERF lwrve ilwalidation interfaces.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    VF_LOAD => {
        DESCRIPTION => "Library containing VF lwrve load interfaces.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
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
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_VOLT_API =>
    {
        DESCRIPTION    => "Library containing client volt API-s.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_GPIO =>
    {
        DESCRIPTION => "Library containing GPIO routines required to configure GPIOs.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_AVG =>
    {
        DESCRIPTION => "Library containing counter avg routines.",
        PROFILES    => [ 'pmu-ga100...', '-pmu-g*b', ],
    },

    LIB_PMUMON =>
    {
        DESCRIPTION => "Library containing PMUMON runtime routines.",
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    LIB_PMUMON_CONSTRUCT =>
    {
        DESCRIPTION => "Library containing PMUMON construction routines.",
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    LIB_VOLT_CONSTRUCT =>
    {
        DESCRIPTION => "Library containing volt device construct routines.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLK_CONSTRUCT => {
        DESCRIPTION => "Library containing CLK BOARDOBJ SET routines.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLK_READ_ANY_TASK => {
        DESCRIPTION => "Library containing Clocks read routines which may be called from any task.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_CLK_READ_PERF_DAEMON => {
        DESCRIPTION => "Library containing Clocks read routines to be called only within the PERF_DAEMON task.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLK_CONFIG => {
        DESCRIPTION => "Library containing Clocks config routines to be called only within the PERF_DAEMON task.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLK_PROGRAM => {
        DESCRIPTION => "Library containing Clocks program/cleanup routines to be called only within the PERF_DAEMON task.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    LIB_CLK_STATUS => {
        DESCRIPTION => "Library containing CLK BOARDOBJ GET_STATUS routines.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    PERF_DAEMON =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_perf_daemon.o'  , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    PERF_DAEMON_CHANGE_SEQ => {
        DESCRIPTION => "Library containing perf change sequence daemon routines.",
        PROFILES    => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    PERF_DAEMON_CHANGE_SEQ_LPWR => {
        DESCRIPTION => "Library containing perf change sequence daemon LPWR routines.",
        PROFILES    => [ 'pmu-ga10x_riscv...', 'pmu-tu10x', '-pmu-g*b*', ],
    },

    CHANGE_SEQ_LOAD => {
        DESCRIPTION => "Library containing perf change sequence load routines.",
        PROFILES    => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    PERFMON =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_perfmon.o', '.text.*'],
        ],
        PROFILES    => [ '...pmu-gv10x', 'pmu-gv11b', 'pmu-g*b*', ],
    },

    LIB_GPUMON =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LIB_CLK_EFF_AVG => {
        DESCRIPTION => "Library containing routines for CLK_FREQ_EFFECTIVE_AVG feature.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b*', ],
    },

    CLK_LIB_CNTR =>
    {
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    CLK_LIB_CNTR_SW_SYNC =>
    {
        DESCRIPTION => "Library containing clock counter routines required to sync the SW cntr with HW cntr (Client ex: LPWR)",
        PROFILES    => [ 'pmu-gm20x...', '-pmu-g*b*', ],
    },

    LIB_CLK_FREQ_COUNTED_AVG => {
        DESCRIPTION => "Library containing routines for CLK_FREQ_COUNTED_AVG feature.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b', ],
    },

    LIB_CLK_CONTROLLER => {
        DESCRIPTION => "Library containing routines for all clock controllers.",
        PROFILES    => [ 'pmu-ga100...', '-pmu-g*b', ],
    },

    LIB_CLK_FREQ_CONTROLLER => {
        DESCRIPTION => "Library containing routines for CLK_FREQ_CONTROLLER feature.",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b', ],
    },

    LIB_CLK_VOLT_CONTROLLER => {
        DESCRIPTION => "Library containing routines for CLK_VOLT_CONTROLLER feature.",
        PROFILES    => [ 'pmu-ga100...', '-pmu-g*b', ],
    },

    CLK_LPWR => {
        DESCRIPTION => "Library containing clock routines needed for diferent LPWR features.",
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', 'pmu-tu10x', ],
    },

    PCMEVT =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_pcmEvent.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gm20x', ],
    },

    DISP =>
    {
       INPUT_SECTIONS =>
        [
            ['*/task_disp.o' , '.text.*'],
            ['*/disp/*/*.o'  , '.text.*'],
            ['*/psr/*/*.o'   , '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    DISP_IMP =>
    {
       DESCRIPTION => "Contains display IMP related functions",
       PROFILES   => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100', '-pmu-g*b*', ],
    },

    HDCP =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_hdcp.o' , '.text.*'],
            ['*/hdcp/*.o'    , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gm10x', ],
    },

    SHA1 =>
    {
        INPUT_SECTIONS =>
        [
            ['*/sha1/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    GID =>
    {
        INPUT_SECTIONS =>
        [
            ['*/gid/*.o'   , '.text.*'],
            ['*/ecid/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    LOGGER_WRITE =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    PMGR_LIB_PWR_MONITOR_GPUMON =>
    {
        PROFILES    => [ 'pmu-*', '-pmu-g*b*', ],
    },

    ACR =>
    {
        INPUT_SECTIONS =>
        [
            ['*/task_acr.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-gm2*', 'pmu-gp*', 'pmu-gv*', 'pmu-ga10b', ],
    },

    BIF =>
    {
        DESCRIPTION => "BIF related code",
        INPUT_SECTIONS =>
        [
            ['*/task_bif.o', '.text.*'],
            ['*/bif/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_BIF_INIT =>
    {
        DESCRIPTION => "Library containing bif routine that is exlwted when the bif task scheduled for the first time.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_BIF =>
    {
        DESCRIPTION => "Library containing bif routines required in PERF change task, to avoid loading complete BIF overlay",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_INIT => {
        DESCRIPTION => "PERF_CF code used only in the post-init paths.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_PM_SENSORS => {
        DESCRIPTION => "PERF_CF PM Sensor code.",
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF_BOARD_OBJ => {
        DESCRIPTION => "PERF_CF Board Object Group SET/GET_STATUS handlers.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_LOAD => {
        DESCRIPTION => "PERF_CF LOAD functions.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_DEV_INFO => {
        DESCRIPTION => "Device info querying library.",
        PROFILES    => [ 'pmu-ga10x_riscv...', ],
    },

    PERF_CF => {
        DESCRIPTION => "PERF_CF common and Controller/Policy code.",
        INPUT_SECTIONS =>
        [
            ['*/perf/lw/cf/*.o', '.text.*'],
            ['*/task_perf_cf.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_ENG_UTIL =>
    {
        DESCRIPTION => "Computes utilization of Graphics engine, FB engine, Video (enc/dec) engine, and PCIE.",
        PROFILES    => [ 'pmu-*', ],
    },

    PERF_CF_TOPOLOGY => {
        DESCRIPTION => "PERF_CF Sensor and Topology code.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_MODEL => {
        DESCRIPTION => "PERF_CF PwrModel code.",
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    NNE =>
    {
        DESCRIPTION    => "NNE task imem overlay",
        INPUT_SECTIONS =>
        [
            [ '*/task_nne.o', '.text.*', ],
            [ '*/objnne.o', '.text.*', ],
        ],
        PROFILES       => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    NNE_INFERENCE_CLIENT =>
    {
        DESCRIPTION  => "NNE client-side inference library",
        PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    NNE_BOARD_OBJ =>
    {
        DESCRIPTION  => "NNE BOARDOBJGRP_CMD RPC handling",
        PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    NNE_INIT =>
    {
        DESCRIPTION  => "NNE init imem overlay",
        PROFILES     => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
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
        PROFILES    => [ 'pmu-gv10x...', ],
    },

    VBIOS =>
    {
        DESCRIPTION => "VBIOS related code",
        INPUT_SECTIONS =>
        [
            ['*/vbios/lw/objvbios.o'  , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp10x...', '-pmu-g*b*', ],
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
        PROFILES    => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    DISP_PMS =>
    {
        DESCRIPTION => "DISPLAY PMS related code",
        INPUT_SECTIONS =>
        [
            ['*/disp/pascal/*.o'  , '.text.*'],
            ['*/disp/turing/*.o'  , '.text.*'],
        ],
        PROFILES    => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    LIB_VBIOS_IMAGE =>
    {
        DESCRIPTION => "Contains the code for reading data out of the VBIOS image",
        PROFILES    => [ 'pmu-ga10x_selfinit_riscv...', '-pmu-gh100*', '-pmu-ga10b*', '-pmu-ad10b_riscv', ],
    },

    LIB_VOLT_TEST =>
    {
        INPUT_SECTIONS =>
        [
            ['*/volt/lw/volttest.o' , '.text.*'],
        ],
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PRIV_SEC=>
    {
        DESCRIPTION => "PMU PRIV level settings related",
        PROFILES    => [ 'pmu-gp10x...', '-pmu-g*b', ],
    },

    LIB_SEMAPHORE_R_W =>
    {
        DESCRIPTION => "Reader-writer semaphore library",
        PROFILES    => [ 'pmu-gp100...', '-pmu-g*b', ],
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
        PROFILES    => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    LIB_LWLINK =>
    {
        DESCRIPTION => "Update HSHUB registers for LWLink/PCIe traffic",
        PROFILES    => [ 'pmu-gp100', 'pmu-gv10x', ],
    },

    LIB_BIF_MARGINING =>
    {
        DESCRIPTION => "Library containing bif routines required to handle margining requests",
        PROFILES    => [ 'pmu-tu10x', 'pmu-ga100', ],
    },

    LOWLATENCY =>
    {
        DESCRIPTION    => "Contains LOWLATENCY task related code",
        INPUT_SECTIONS =>
        [
            ['*/task_lowlatency.o', '.text.*'],
            ['*/lowlatency/*/*.o', '.text.*'],
        ],
        PROFILES    => [ 'pmu-tu10x...', '-pmu-g*b', ],
    },

    LIB_PMGR_TEST =>
    {
        DESCRIPTION => "Contains code related to PMGR test infrastructure",
        INPUT_SECTIONS =>
        [
            ['*/pmgr/lw/pmgrtest.o' , '.text.*'],
        ],
        PROFILES => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    LIB_SCP_RAND_HS =>
    {
        DESCRIPTION => "Contains code for interacting with SCP's true random number generator",
        PROFILES => [ 'pmu-ga100...', ],
    },

#*********************************  Cold Code  ********************************#
#                                                                              #
# For cold code definition please see @ref RiscvSectionsCode.pm                #
#                                                                              #
# This is temporary overlay / section required until Falcon ODP is used for    #
# GA10X+ testing / validation. Please remove when deprecating those builds.    #
#                                                                              #
#******************************************************************************#

    ERROR_HANDLING_COLD =>
    {
        DESCRIPTION => "Error handling cold code",
        PROFILES => [ 'pmu-ga10x_riscv...', ],
    },
];

# return the raw data of Overlays definition
return $imemOverlaysRaw;
