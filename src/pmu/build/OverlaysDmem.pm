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
# Defines all known PMU DMEM overlays.
#
# Overlays:
#     An "overlay" is any generic piece of data that can be paged in and out of
#     DMEM at runtime. To gcc, this is simply a data section. That is, the
#     overlays used here are not considered by gcc as true overlays as each
#     section has its own unique data address-space.
#
#     NOTE: The ordering of the overlay definitions in this module is
#           significant. Overlays will be placed in the final image in the
#           order that they are defined in this file.
#
#     OVERLAY_NAME =>
#     {
#        DESCRIPTION     => 'Description of Overlay',
#        MAX_SIZE_BLOCKS => 4,
#        PROFILES        => [ 'pmu-gp10x...', ],
#        ENABLED_ON      => [ GP10X_and_later, ],
#    },
#
# Overlay Attributes:
#
#     DESCRIPTION
#         Optional description of the DMEM overlay.
#
#     NAME
#         Defines the name of the output-section that is created for the
#         overlay. By default, the overlay name is comes from the hash key used
#         below when defining the overlay. The key name is colwerted to
#         camel-case with the leading character in lower-case. This attribute
#         may be used as an override if the resulting name is undesirable.
#
#     SECTION_DESCRIPTION
#         This attribute may be used to specify the exact linker-script text
#         used to define the overlay. It overrides all other attributes (such
#         as the INPUT_SECTIONS) attribute. It should only be used under
#         special cirlwmstances where special syntax is required that is not
#         supported by the linker-script generation tool (ldgen).
#
#     MAX_SIZE_BLOCKS
#         The maximum size of this DMEM overlay in blocks (256 byte units)
#
#     PROFILES
#         Defines the list of profiles on which the overlay is enabled. If an
#         overlay is not used on a particular profile, that chip must be
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

my $dmemOverlaysRaw =
[
    STACK_CMDMGMT =>
    {
        DESCRIPTION     => 'Stack for the CMDMGMT task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp10x...', ],
    },

    STACK_LPWR =>
    {
        DESCRIPTION     => 'Stack for the LPWR task.',
        MAX_SIZE_BLOCKS => [
                               3  => DEFAULT,
                               4  => [ 'pmu-tu10x...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', ],
    },

    STACK_LPWR_LP =>
    {
        DESCRIPTION     => 'Stack for the LPWR_LP task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-ga10b', '-pmu-gh100', '-pmu-g00x', ],
    },

    STACK_I2C =>
    {
        DESCRIPTION     => 'Stack for the I2C task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_SPI =>
    {
        DESCRIPTION     => 'Stack for the SPI task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_SEQ =>
    {
        DESCRIPTION     => 'Stack for the SEQ task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', '-pmu-ga100...', ],
    },

    STACK_PCMEVT =>
    {
        DESCRIPTION     => 'Stack for the PCMEVT task.',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_PMGR =>
    {
        DESCRIPTION     => 'Stack for the PMGR task.',
        MAX_SIZE_BLOCKS => [
                               7  => DEFAULT,
                               10  => [ 'pmu-gv*', 'pmu-tu10x...' ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_PERFMON =>
    {
        DESCRIPTION     => 'Stack for the PERFMON task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp10x', 'pmu-gv10x', 'pmu-gv11b', 'pmu-g*b*', ],
    },

    STACK_DISP =>
    {
        DESCRIPTION     => 'Stack for the DISP task.',
        MAX_SIZE_BLOCKS => 5,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_THERM =>
    {
        DESCRIPTION     => 'Stack for the THERM task.',
        MAX_SIZE_BLOCKS => [
                                3 => DEFAULT,
                                4 => [ 'pmu-tu10x...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_ACR =>
    {
        DESCRIPTION     => 'Stack for the ACR task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-gp*', '-pmu-gp100', 'pmu-gv*', 'pmu-ga10b', ],
    },

    STACK_PERF =>
    {
        DESCRIPTION     => 'Stack for the PERF task.',
        MAX_SIZE_BLOCKS => [
                               11 => DEFAULT,
                               8 => [ 'pmu-tu10x...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    STACK_PERF_DAEMON =>
    {
        DESCRIPTION     => 'Stack for the PERF DAEMON task.',
        MAX_SIZE_BLOCKS => 9,
        PROFILES        => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    STACK_WATCHDOG =>
    {
        DESCRIPTION     => 'Stack for the Watchdog task.',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ ],
    },

    CMDMGMT_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold CMDMGMT\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    CMDMGMT_SSMD =>
    {
        DESCRIPTION     => 'Super-surface member descriptors (LWGPU builds only).',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ ]
    },

    SEQ_RPC_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold SEQ RPC data',
        MAX_SIZE_BLOCKS => 10,
        PROFILES        => [ 'pmu-gp10x...pmu-tu10x', '-pmu-g*b*', ],
    },

    STACK_BIF =>
    {
        DESCRIPTION     => 'Stack for the BIF task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    STACK_PERF_CF =>
    {
        DESCRIPTION     => 'Stack for the PERF_CF task.',
        MAX_SIZE_BLOCKS => [
                                8   => DEFAULT,
                                17  => [ 'pmu-gh100...', ],
                           ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    STACK_LOWLATENCY =>
    {
        DESCRIPTION     => 'Stack for the LOWLATENCY task.',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b', ],
    },

    STACK_NNE =>
    {
        DESCRIPTION     => 'Stack for the Neural Net Engine (NNE) task.',
        MAX_SIZE_BLOCKS => [
                                3 => DEFAULT,
                                4 => [ ],
                           ],
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    I2C_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF_CF\'s commands',
        MAX_SIZE_BLOCKS => 16,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    I2C_DEVICE =>
    {
        DESCRIPTION     => 'Data overlay for I2C_DEVICE objects, shared between multiple tasks.',
        MAX_SIZE_BLOCKS => 10,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    ILLUM =>
    {
        DESCRIPTION     => 'Data overlay for ILLUM objects',
        MAX_SIZE_BLOCKS => 4,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR =>
    {
        DESCRIPTION     => 'Primary data overlay for the PMGR task',
        # MAX_SIZE_BLOCKS has been changed for additional PMGR DMEM requirement
        # for TURING and AMPERE_ GPUs.
        MAX_SIZE_BLOCKS => [
                                22 => DEFAULT,
                                24 => [ 'pmu-tu10x', 'pmu-ga100', ],
                                51 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    PMGR_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PMGR\'s commands',
        MAX_SIZE_BLOCKS => 128,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PMGR_PWR_POLICY_35_MULT_DATA =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_POLICY_35_MULT_DATA in PWR_POLICY_35 infrastructure',
        MAX_SIZE_BLOCKS => [
                                11 => DEFAULT,
                                19 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR_PWR_DEVICE_STATE_SYNC =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_CHANNELS_STATUS called from pwrDevStateSync',
        MAX_SIZE_BLOCKS => 11,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR_PWR_CHANNELS_STATUS =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_CHANNELS_STATUS in PWR_CHANNEL_35 infrastructure',
        MAX_SIZE_BLOCKS => 11,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PMGR_PWR_POLICIES_PERF_LIMITS =>
    {
        DESCRIPTION     => 'Overlay containing PERF_LIMITS_CLIENT for power policy',
        MAX_SIZE_BLOCKS => [
                               4 => DEFAULT,
                               6 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LIB_PMGR_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the PMGR test framework',
        MAX_SIZE_BLOCKS => 1,
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    THERM =>
    {
        DESCRIPTION     => 'Primary data overlay for the THERM task',
        # MAX_SIZE_BLOCKS has been changed from 4 -> 7 due to additional THERM
        # DMEM requirement for VOLTA+ GPUs. PASCAL GPUs work well with
        # MAX_SIZE_BLOCKS = 4.
        MAX_SIZE_BLOCKS => [
                                4 => DEFAULT,
                                7 => [ 'pmu-gv*', ],
                                8 => [ 'pmu-tu10x', 'pmu-ga100' ],
                                12 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    THERM_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold THERM\'s commands',
        MAX_SIZE_BLOCKS => 80,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    SMBPBI =>
    {
        DESCRIPTION     => 'Data overlay for OBJSMBPBI object',
        MAX_SIZE_BLOCKS => [
                                1 => DEFAULT,
                                2 => [ 'pmu-ga100...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    SMBPBI_LWLINK =>
    {
        DESCRIPTION     => 'Data overlay for the SMBPBI LWLink data',
        MAX_SIZE_BLOCKS => [
                                1 => DEFAULT,
                                3 => [ 'pmu-ga100', ],
                           ],
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    LIB_THERM_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the Therm test framework',
        MAX_SIZE_BLOCKS => [
                                2 => DEFAULT,
                                5 => [ 'pmu-gh100...', ],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    SPI =>
    {
        DESCRIPTION     => 'Primary data overlay for the SPI task',
        MAX_SIZE_BLOCKS => [
                                4 => DEFAULT,
                                12 => ['pmu-ga100...'],
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    SPI_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold SPI\'s commands',
        MAX_SIZE_BLOCKS => 12,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_LIMIT =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_LIMIT objects within the PERF task',
        # MAX_SIZE_BLOCKS here is callwlated by estimation followed by testing to determine how many blocks are actually used.
        # Once the number of used blocks has been determined, the value here adds a padding amount to derive the final value.
        # PERF_LIMIT:                                lwrSize = ??    maxSize = ??
        MAX_SIZE_BLOCKS => 76,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ], # dTURING_and_later, -TEGRA_DGPU,
    },

    PERF_LIMIT_CHANGE_SEQ_CHANGE =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_LIMIT Change Sequencer change buffer.',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ], # dTURING_and_later, -TEGRA_DGPU,
    },

    PERF_POLICY =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_POLICY objects within the PERF task',
        MAX_SIZE_BLOCKS => 4,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ], # dTURING_and_later, -TEGRA_DGPU,
    },

    PERF =>
    {
        DESCRIPTION     => 'Primary data overlay for the PERF task',
        # MAX_SIZE_BLOCKS here is callwlated by estimation followed by testing to determine how many blocks are actually used.
        # Once the number of used blocks has been determined, the value here adds a padding amount to derive the final value.
        # Current usage with VF 3.5 is 74 blocks on TU102
        MAX_SIZE_BLOCKS => [
                                82 => DEFAULT,
                                180 => [ TURING_and_later, ],   # PP-TODO : Correct this number after disabling TU10x_ampere
                           ],
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    PERF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF\'s commands',
        MAX_SIZE_BLOCKS => 256,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    LIB_VOLT_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the Volt test framework',
        MAX_SIZE_BLOCKS => 1,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    CLK_VF_POINT_SEC =>
    {
        DESCRIPTION     => 'Data overlay for the secondary VF lwrve.',
        MAX_SIZE_BLOCKS => 30,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ], # dTURING_and_later, -TEGRA_DGPU,
    },

    CLK_CONTROLLERS =>
    {
        DESCRIPTION     => 'Data overlay for the clock controllers - CLFC and CLVC',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b', ],
    },

    CLK_3X =>
    {
        DESCRIPTION     => 'Data overlay for the clocks 3.x objects within the PERF_DAEMON task',
        # Much of this overlay is statically allocated, with such things as the
        # schematic dag (type ClkSchematicDag), vtables (types ClkFreqDomain_Virtual
        # and ClkFreqSrc_Virtual), etc.  See source files in pmu_sw/prod_app/clk/clk3.
        # The static size is shown as 'lwrSize' in the generated header file.
        # 'maxSize' is whatever number we put here colwerted from blocks to bytes.
        # There are 256 bytes to a block.  If the number is too small, we get
        # failure during initialization.  If it's too large, build scripts complain
        # about running short.  It's okay to pad it a little.
        MAX_SIZE_BLOCKS => 24,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    CLK_ADCS =>
    {
        DESCRIPTION     => 'Data overlay for the clocks ADC device objects',
        MAX_SIZE_BLOCKS => 4,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    CLK_ACCESS_CONTROL =>
    {
        DESCRIPTION     => 'Data overlay for allocations needed by clock access control code',
        MAX_SIZE_BLOCKS => 10, #TODO akshatam - evaluate if this needs to be increased/decreased
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_DAEMON =>
    {
        DESCRIPTION     => 'Primary data overlay of the PERF DAEMON task. Permanently attached to the task.',
        MAX_SIZE_BLOCKS => 22,
        PROFILES        => [ 'pmu-gv10x...', '-pmu-g*b*', ],
    },

    PERF_DAEMON_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF_DAEMON\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CHANGE_SEQ_CHANGE_NEXT =>
    {
        DESCRIPTION     => 'Scratch overlay to store next pending change.',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CHANGE_SEQ_CLIENT =>
    {
        DESCRIPTION     => 'Primary data overlay of the PERF CHANGE SEQUENCER CLIENTs.',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CHANGE_SEQ_LPWR =>
    {
        DESCRIPTION     => 'Scratch overlay to store LPWR perf change Sequencer scripts.',
        MAX_SIZE_BLOCKS => 50, # PP-TODO to correct based on silicon observations.
        PROFILES        => [ 'pmu-tu10x', 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    CLK_VOLT_CONTROLLER =>
    {
        DESCRIPTION     => 'Data overlay for the Voltage Controllers',
        MAX_SIZE_BLOCKS => 4,
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b', ],
    },

    PERF_VFE =>
    {
        DESCRIPTION     => 'Data overlay for VFE Framework.',
        MAX_SIZE_BLOCKS => 63,
        PROFILES        => [ 'pmu-gp10x...', '-pmu-g*b*', ],
    },

    VBIOS =>
    {
        DESCRIPTION     => 'Primary data overlay of the VBIOS parsing task. Attached when required by any task',
        MAX_SIZE_BLOCKS =>  [
                                3 => DEFAULT,
                                9 => [ 'pmu-gh20x', ],
                            ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    VBIOS_FRTS_ACCESS_DMA_BUFFERS =>
    {
        DESCRIPTION     => 'Section to hold Firmware Runtime Security DMA buffers',
        MAX_SIZE_BLOCKS =>  54,
        PROFILES        => [ 'pmu-ga10x_selfinit_riscv...', '-pmu-gh100*', '-pmu-ga10b*', '-pmu-ad10b_riscv', ],
    },  

    BIF =>
    {
        DESCRIPTION     => 'Primary overlay of the BIF task. Permanently attached to the task.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    BIF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold BIF\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    BIF_PCIE_LIMIT_CLIENT =>
    {
        DESCRIPTION     => 'Data overlay for BIF_PCIE limit client',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    LPWR =>
    {
        DESCRIPTION     => 'Primary overlay of the LPWR task. Permanently attached to the task.',
        # The total size of data structures we are planning to move here is ~2.5 KB. For BA State
        # Save/restore, we are using another 420 bytes of memory. So total size is now ~3KB. For
        # safety we will be having a buffer of 512 bytes. So the MAX_SIZE becomes 3.5 KB (i.e. 14 blocks)
        MAX_SIZE_BLOCKS => 14,
        PROFILES        => [ 'pmu-gp10x...', ],
    },

    LPWR_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LPWR\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    LPWR_LP =>
    {
        DESCRIPTION     => 'Primary overlay of the LPWR_LP task. Permanently attached to the task.',
        # 2 block needed for this overlay. We will update the size as per need basis
        MAX_SIZE_BLOCKS => 2,
        PROFILES    => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-ga10b', '-pmu-gh100', '-pmu-g00x', ],
    },

    LPWR_LP_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LPWR_LP\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },


    LPWR_TEST =>
    {
        DESCRIPTION     => 'Overlay for LPWR test.',
        # 112 blocks accounting to maximum DMEM size of 28K, to trigger more page faults.
        MAX_SIZE_BLOCKS => 112,
        PROFILES    => [ ],
    },

    LPWR_PRIV_BLOCKER_INIT_DATA =>
    {
        DESCRIPTION     => 'Data Overlay to keep various data to initialize Priv Blockers',
        # 4 block needed for this overlay. We will update the size as per need basis
        MAX_SIZE_BLOCKS => 4,
        PROFILES    => [ 'pmu-tu10x...', '-pmu-ga100', '-pmu-gh100', '-pmu-g00x', ],
    },

    DLPPC_1X_METRICS_SCRATCH =>
    {
        DESCRIPTION     => 'Scratch data overlay for intermediate DLPPC_1X metrics results.',
        MAX_SIZE_BLOCKS => 40,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF common.',
        MAX_SIZE_BLOCKS => 4,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF_CF\'s commands',
        MAX_SIZE_BLOCKS => 140,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF_PM_SENSORS =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF PM Sensors',
        MAX_SIZE_BLOCKS => 32,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF_TOPOLOGY =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF Sensors and Topologys.',
        MAX_SIZE_BLOCKS => [
                                15 => DEFAULT,
                                20 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_MODEL =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF PwrModels.',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_PWR_MODEL_SCALE_RPC_BUF_ODP =>
    {
        DESCRIPTION     => 'Data overlay holding data for the scale RPC on profiles with ODP.',
        MAX_SIZE_BLOCKS => 92,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF_PWR_MODEL_SCALE_RPC_BUF_NON_ODP =>
    {
        DESCRIPTION     => 'Data overlay holding data for the scale RPC on profiles without ODP.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-ga100', '-pmu-g*b*', ],
    },

    DLPPM_1X_RUNTIME_SCRATCH =>
    {
        DESCRIPTION     => 'Data overlay for DLPPM_1X to use at runtime as a scratch buffer.',
        MAX_SIZE_BLOCKS => 274,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    PERF_CF_PWR_MODEL_TGP_1X_RUNTIME_SCRATCH =>
    {
        DESCRIPTION     => 'Data overlay for TGP_1X to use at runtime as a scratch buffer.',
        MAX_SIZE_BLOCKS => 5,
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC =>
    {
        DESCRIPTION     => 'Data overlay holding data for the Client PwrModel Profile scale RPC.',
        MAX_SIZE_BLOCKS => 3,
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b*', ],
    },

    PERF_CF_CONTROLLER =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF Controllers and Policys.',
        MAX_SIZE_BLOCKS => [
                                38 => DEFAULT,
                                2220 => [ 'pmu-ga10x_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    PERF_CF_LIMIT_CLIENT =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF limit client.',
        MAX_SIZE_BLOCKS => 10,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b*', ],
    },

    NNE =>
    {
        DESCRIPTION     => 'Data overlay for the Neural Net Engine (NNE) task.',
        MAX_SIZE_BLOCKS => 24,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    NNE_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold NNE\'s commands',
        MAX_SIZE_BLOCKS => 128,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    NNE_DESC_INFERENCE_RPC_BUF =>
    {
        DESCRIPTION     => 'Data overlay holding the staging buffer for an inference RPC.',
        MAX_SIZE_BLOCKS => 312,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    BIF_LANE_MARGINING =>
    {
        DESCRIPTION     => 'Data overlay for lane margining data',
        MAX_SIZE_BLOCKS => 1,
        PROFILES        => [ 'pmu-tu10x', 'pmu-ga100', ],
    },

    LOWLATENCY =>
    {
        DESCRIPTION     => 'Data overlay for the LOWLATENCY task. Permanently attached to the task',
        MAX_SIZE_BLOCKS => 2,
        PROFILES        => [ 'pmu-tu10x...', '-pmu-g*b', ],
    },

    LOWLATENCY_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LOWLATENCY\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b', ],
    },

    LIB_AVG =>
    {
        DESCRIPTION     => 'Data overlay for the average counters library',
        MAX_SIZE_BLOCKS => 1,
        PROFILES        => [ 'pmu-ga100...', '-pmu-g*b', ],
    },

    DISP_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold DISP\'s commands',
        MAX_SIZE_BLOCKS => 8,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-g*b*', ],
    },

    #
    # This overlay MUST be last!!!
    #
    GC6_DMEM_COPY =>
    {
        DESCRIPTION     => 'Artificially increase DMEM footprint making room for the original DMEM copy (of the compile time allocated data).',
        MAX_SIZE_BLOCKS =>  [
                                54 => DEFAULT,
                                186 => [ 'pmu-ga10x_riscv...', ],
                            ],
        PROFILES        => [ 'pmu-tu10x', ],
    },
];

# return the raw data of Overlays definition
return $dmemOverlaysRaw;
