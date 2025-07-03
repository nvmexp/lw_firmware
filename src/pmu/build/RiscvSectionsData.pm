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
# Defines all known data RISC-V PMU sections and overlays.
#
# For details on the contents of this file, please refer to:
#     <branch>/uproc/build/scripts/docs/RiscvSections.txt
#
my $riscvSectionsDataRaw =
[
    # This is the LEGACY kernel heap. Accessed with lwosMalloc.
    OS_HEAP =>
    {
        HEAP_SIZE       => '__os_heap_profile_size',
        INPUT_SECTIONS  => [ ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    DUMMY_SECTION_A =>
    {
        DESCRIPTION     => 'Dummy section used as temporary WAR for http://lwbugs/200581831',
        HEAP_SIZE       => 0x1,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
    },

    DUMMY_SECTION_B =>
    {
        DESCRIPTION     => 'Dummy section used as temporary WAR for http://lwbugs/200581831',
        HEAP_SIZE       => 0x1,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
    },

    DUMMY_SECTION_C =>
    {
        DESCRIPTION     => 'Dummy section used as temporary WAR for http://lwbugs/200581831',
        HEAP_SIZE       => 0x1,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
    },

    DUMMY_SECTION_D =>
    {
        DESCRIPTION     => 'Dummy section used as temporary WAR for http://lwbugs/200581831',
        HEAP_SIZE       => 0x1,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'fb',
        PERMISSIONS     => 'rw',
    },


    # Kernel rodata
    KERNEL_RODATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_rodata .kernel_rodata.*'],
            ['*libSafeRTOS.a',  '.rodata .rodata.* .got* .srodata* .srodata.*'],
            ['*libDrivers.a',   '.rodata .rodata.* .got* .srodata* .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'R',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Shared rodata (we put all in dmem)
    GLOBAL_RODATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.shared_rodata .shared_rodata.*'],
            ['*',               '.syslib_rodata .syslib_rodata.*'],
            # Also include task RO data, we don't care if kernel can read it
            ['*',               '.task_rodata .task_rodata.*'],
            ['*libShlib.a',     '.rodata .rodata.* .got* .srodata* .srodata.*'],
            ['*libUprocLwos.a', '.rodata .rodata.* .got* .srodata* .srodata.*'],
            ['*libSCP.a',       '.rodata .rodata.* .got* .srodata* .srodata.*'],
            ['*libMutex.a',     '.rodata .rodata.* .got* .srodata* .srodata.*'],
            ['*libUprocCmn.a',  '.rodata .rodata.* .got* .srodata* .srodata.*'],

            # Catch-all just in case:
            ['*',               '.rodata .rodata.* .got* .srodata* .srodata.*'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'r',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    # Kernel *writable* data, bss, etc.
    KERNEL_DATA =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.kernel_data .kernel_data.*'],
            ['*libSafeRTOS.a',  '.data .data.* .sdata .sdata.*'],
            ['*libKernel.a',    '.data .data.* .sdata .sdata.*'],
            ['*libDrivers.a',   '.data .data.* .sdata .sdata.*'],
            '. = ALIGN(8)',            # Align so we can use fast clean
            '__machine_bss_start = .',
            ['*libSafeRTOS.a',  '.bss .bss.*  .sbss .sbss.* COMMON'],
            ['*libKernel.a',    '.bss .bss.*  .sbss .sbss.* COMMON'],
            ['*libDrivers.a',   '.bss .bss.*  .sbss .sbss.* COMMON'],
            '. = ALIGN(8)',
            '__machine_bss_end = .',
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'RW',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    #
    # Shared (between tasks) *writable* resident data
    #
    # DO NOT rename this section without a matching change to
    # UPROC_SHARED_DMEM_SECTION_NAME / SHARED_DMEM_SECTION_NAME!
    # It's used by LWOS as a reference point for
    # fast VA<->PA translations for resident data.
    #
    TASK_SHARED_DATA_DMEM_RES =>
    {
        INPUT_SECTIONS  =>
        [
            '. = ALIGN(256)',
            ['*',               '.alignedData256 .alignedData256.*'],
            '. = ALIGN(128)',
            ['*',               '.alignedData128 .alignedData128.*'],
            '. = ALIGN( 64)',
            ['*',               '.alignedData64 .alignedData64.*'],
            '. = ALIGN( 32)',
            ['*',               '.alignedData32 .alignedData32.*'],
            '. = ALIGN( 16)',
            ['*',               '.alignedData16 .alignedData16.*'],
            ['*',               '.shared_data .shared_data.*'],
            ['*',               '.dmem_resident .dmem_resident.*'],
            ['*',               '.dmem_residentLwos .dmem_residentLwos.*'],
            ['*',               '.dmem_residentRtos .dmem_residentRtos.*'],
            ['*',               '.rmrtos .rmrtos.*'],
            ['*',               '.data .data.* .sdata .sdata.*'],
            ['EXCLUDE_FILE(*c_cover_io_wrapper.o) *',   '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    #
    # DO NOT rename this section without a matching change to
    # applicable LWOS_SECTION_* uses in lwos!
    # It's used by LWOS as a reference point for
    # fast VA<->PA translations for resident data.
    #
    LPWR =>
    {
        DESCRIPTION     => 'Primary overlay of the LPWR task. Permanently attached to the task.',
        HEAP_SIZE       => 0x1000,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
    },

    LPWR_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LPWR\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_LPWR =>
    {
        DESCRIPTION     => 'Stack for the LPWR task.',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
    },

    STACK_IDLE =>
    {
        DESCRIPTION     => 'Stack for the IDLE task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
    },


    # Shared (between tasks) *writable* pageable data
    TASK_SHARED_DATA_DMEM =>
    {
        INPUT_SECTIONS  =>
        [
            ['*',               '.task_data .task_data.*'],
            ['*',               '.task_bss'],
            ['*c_cover_io_wrapper.o',  '.bss .bss.* .sbss .sbss.* COMMON'],
        ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
    },

    #
    # TODO: all the HEAP_SIZE values at the moment are TENTATIVE.
    # The value of HEAP_SIZE is supposed to be max_size - lwrr_size, but
    # the current tentative values were all obtained by multiplying the
    # corresponding MAX_SIZE_BLOCKS from OverlaysDmem.pm by 256.,
    # and then by 2 to compensate for pointer size increasing
    # from 32 to 64 bit.
    #
    # The current tentative HEAP_SIZE values should, in most cases,
    # be overestimating the max_size.
    #

    STACK_CMDMGMT =>
    {
        DESCRIPTION     => 'Stack for the CMDMGMT task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_LPWR_LP =>
    {
        DESCRIPTION     => 'Stack for the LPWR_LP task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-g00x', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_I2C =>
    {
        DESCRIPTION     => 'Stack for the I2C task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_SPI =>
    {
        DESCRIPTION     => 'Stack for the SPI task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_PMGR =>
    {
        DESCRIPTION     => 'Stack for the PMGR task.',
        HEAP_SIZE       => 0x1000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_PERFMON =>
    {
        DESCRIPTION     => 'Stack for the PERFMON task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_DISP =>
    {
        DESCRIPTION     => 'Stack for the DISP task.',
        HEAP_SIZE       => 0x1000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_THERM =>
    {
        DESCRIPTION     => 'Stack for the THERM task.',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_ACR =>
    {
        DESCRIPTION     => 'Stack for the ACR task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga1*b*', 'pmu-ga10f_riscv', 'pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_PERF =>
    {
        DESCRIPTION     => 'Stack for the PERF task.',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_PERF_DAEMON =>
    {
        DESCRIPTION     => 'Stack for the PERF DAEMON task.',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_BIF =>
    {
        DESCRIPTION     => 'Stack for the BIF task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_PERF_CF =>
    {
        DESCRIPTION     => 'Stack for the PERF_CF task.',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_LOWLATENCY =>
    {
        DESCRIPTION     => 'Stack for the LOWLATENCY task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_NNE =>
    {
        DESCRIPTION     => 'Stack for the Neural Net Engine (NNE) task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    STACK_WATCHDOG =>
    {
        DESCRIPTION     => 'Stack for the Watchdog task.',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CMDMGMT_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold CMDMGMT\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CMDMGMT_SSMD =>
    {
        DESCRIPTION     => 'Super-surface member descriptors (LWGPU builds only).',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    I2C_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold I2C\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    I2C_DEVICE =>
    {
        DESCRIPTION     => 'Data overlay for I2C_DEVICE objects, shared between multiple tasks.',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    ILLUM =>
    {
        DESCRIPTION     => 'Data overlay for ILLUM objects',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR =>
    {
        DESCRIPTION     => 'Primary data overlay for the PMGR task',
        HEAP_SIZE       => 0x5200,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PMGR\'s commands',
        HEAP_SIZE       => 0x1000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR_PWR_POLICY_35_MULT_DATA =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_POLICY_35_MULT_DATA in PWR_POLICY_35 infrastructure',
        HEAP_SIZE       => 0x2600,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR_PWR_DEVICE_STATE_SYNC =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_CHANNELS_STATUS called from pwrDevStateSync',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR_PWR_CHANNELS_STATUS =>
    {
        DESCRIPTION     => 'Data overlay for the PWR_CHANNELS_STATUS in PWR_CHANNEL_35 infrastructure',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PMGR_PWR_POLICIES_PERF_LIMITS =>
    {
        DESCRIPTION     => 'Overlay containing PERF_LIMITS_CLIENT for power policy',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LIB_PMGR_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the PMGR test framework',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    THERM =>
    {
        DESCRIPTION     => 'Primary data overlay for the THERM task',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    THERM_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold THERM\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    SMBPBI =>
    {
        DESCRIPTION     => 'Data overlay for OBJSMBPBI object',
        HEAP_SIZE       => 0x0,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    SMBPBI_LWLINK =>
    {
        DESCRIPTION     => 'Data overlay for SMBPBI LWLink data',
        HEAP_SIZE       => 0x0,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LIB_THERM_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the Therm test framework',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    SPI =>
    {
        DESCRIPTION     => 'Primary data overlay for the SPI task',
        HEAP_SIZE       => 0x2400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    SPI_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold SPI\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_LIMIT =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_LIMIT objects within the PERF task',
        # HEAP_SIZE here should be callwlated by estimation followed by testing to determine how many blocks are actually used.
        # Once the number of used blocks has been determined, the value here adds a padding amount to derive the final value.
        # PERF_LIMIT:                                lwrSize = ??    maxSize = ??
        HEAP_SIZE       => 0x9C00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_LIMIT_CHANGE_SEQ_CHANGE =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_LIMIT Change Sequencer change buffer.',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_POLICY =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_POLICY objects within the PERF task',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF =>
    {
        DESCRIPTION     => 'Primary data overlay for the PERF task',
        # HEAP_SIZE here should be callwlated by estimation followed by testing to determine how many blocks are actually used.
        HEAP_SIZE       => 0xFF00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LIB_VOLT_TEST =>
    {
        DESCRIPTION     => 'Primary data overlay for the Volt test framework',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLK_VF_POINT_SEC =>
    {
        DESCRIPTION     => 'Data overlay for the secondary VF lwrve.',
        HEAP_SIZE       => 0x8000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLK_CONTROLLERS =>
    {
        DESCRIPTION     => 'Data overlay for the clock controllers - CLFC and CLVC',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLK_3X =>
    {
        DESCRIPTION     => 'Data overlay for the clocks 3.x objects within the PERF_DAEMON task',
        # Much of this overlay is statically allocated, with such things as the
        # schematic dag (type ClkSchematicDag), vtables (types ClkFreqDomain_Virtual
        # and ClkFreqSrc_Virtual), etc.  See source files in pmu_sw/prod_app/clk/clk3.
        HEAP_SIZE       => 0x3400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLK_ADCS =>
    {
        DESCRIPTION     => 'Data overlay for the clocks ADC device objects',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_res',
        PERMISSIONS     => 'rw',
    },

    CLK_ACCESS_CONTROL =>
    {
        DESCRIPTION     => 'Data overlay for allocations needed by clock access control code',
        HEAP_SIZE       => 0xA00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_DAEMON =>
    {
        DESCRIPTION     => 'Primary data overlay of the PERF DAEMON task. Permanently attached to the task.',
        HEAP_SIZE       => 0x4000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_DAEMON_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold PERF_DAEMON\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CHANGE_SEQ_CHANGE_NEXT =>
    {
        DESCRIPTION     => 'Scratch overlay to store next pending change.',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CHANGE_SEQ_CLIENT =>
    {
        DESCRIPTION     => 'Primary data overlay of the PERF CHANGE SEQUENCER CLIENTs.',
        HEAP_SIZE       => 0x1400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CHANGE_SEQ_LPWR =>
    {
        DESCRIPTION     => 'Scratch overlay to store LPWR perf change Sequencer scripts.',
        HEAP_SIZE       => 0x6800, # PP-TODO to correct based on silicon observations.
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLK_VOLT_CONTROLLER =>
    {
        DESCRIPTION     => 'Data overlay for the Voltage Controllers',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_VFE =>
    {
        DESCRIPTION     => 'Data overlay for VFE Framework.',
        HEAP_SIZE       => 0x4C00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    VBIOS =>
    {
        DESCRIPTION     => 'Primary data overlay of the VBIOS parsing task. Attached when required by any task',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    VBIOS_FRTS_ACCESS_DMA_BUFFERS =>
    {
        DESCRIPTION     => 'Section to hold Firmware Runtime Security DMA buffers',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-gh100*', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    BIF =>
    {
        DESCRIPTION     => 'Primary overlay of the BIF task. Permanently attached to the task.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    BIF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold BIF\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    BIF_PCIE_LIMIT_CLIENT =>
    {
        DESCRIPTION     => 'Data overlay for BIF_PCIE limit client',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LPWR_LP =>
    {
        DESCRIPTION     => 'Primary overlay of the LPWR_LP task. Permanently attached to the task.',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', '-pmu-gh100', '-pmu-g00x', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LPWR_LP_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LPWR_LP\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LPWR_TEST =>
    {
        DESCRIPTION     => 'Overlay for LPWR test.',
        #
        # Large DMEM size buffers allocated, to trigger more page faults.
        # This size have to recallwlated, if ODP mapping ilwalidate syscall
        # based pagefault test is implemented.
        #
        HEAP_SIZE       => 0x22000,
        PROFILES        => [ ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LPWR_PRIV_BLOCKER_INIT_DATA =>
    {
        DESCRIPTION     => 'Data overlay for LPWR List caches',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga100', '-pmu-gh100', '-pmu-g00x', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    DLPPC_1X_METRICS_SCRATCH =>
    {
        DESCRIPTION     => 'Scratch data overlay for intermediate DLPPC_1X metrics results.',
        HEAP_SIZE       => 0x2800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF common.',
        HEAP_SIZE       => 0x800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Section for the buffer to hold PERF_CF\'s commands.',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_PM_SENSORS =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF PM Sensors.',
        HEAP_SIZE       => [
                                0x4400 => DEFAULT,
                                0xE400 => [ 'pmu-gh100_riscv...', ],
                           ],
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_TOPOLOGY =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF Sensors and Topologies.',
        HEAP_SIZE       => 0x2500,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_MODEL =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF PwrModels.',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_PWR_MODEL_SCALE_RPC_BUF_ODP =>
    {
        DESCRIPTION     => 'Data overlay holding data for the scale RPC.',
        HEAP_SIZE       => 0x4900,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_PWR_MODEL_SCALE_RPC_NON_ODP =>
    {
        DESCRIPTION     => 'Data overlay holding data for the scale RPC on profiles without ODP.',
        HEAP_SIZE       => 0x300,
        PROFILES        => [ 'pmu-ga100', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    DLPPM_1X_RUNTIME_SCRATCH =>
    {
        DESCRIPTION     => 'Data overlay for DLPPM_1X to use at runtime as a scratch buffer.',
        HEAP_SIZE       => 0x3300,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_PWR_MODEL_TGP_1X_RUNTIME_SCRATCH =>
    {
        DESCRIPTION     => 'Data overlay for TGP_1X to use at runtime as a scratch buffer.',
        HEAP_SIZE       => 0x500,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    CLIENT_PERF_CF_PWR_MODEL_PROFILE_SCALE_RPC =>
    {
        DESCRIPTION     => 'Data overlay holding data for the Client PwrModel Profile scale RPC.',
        HEAP_SIZE       => 0x300,
        PROFILES        => [ 'pmu-ga100...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_CONTROLLER =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF Controllers and Policies.',
        HEAP_SIZE       => 0xC9000,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    PERF_CF_LIMIT_CLIENT =>
    {
        DESCRIPTION     => 'Data overlay for the PERF_CF limit client.',
        HEAP_SIZE       => 0xC00,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    NNE =>
    {
        DESCRIPTION     => 'Data overlay for the Neural Net Engine (NNE) task.',
        HEAP_SIZE       => 0x1800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    NNE_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold NNE\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    NNE_DESC_INFERENCE_RPC_BUF =>
    {
        DESCRIPTION     => 'Data overlay holding the staging buffer for an inference RPC.',
        HEAP_SIZE       => 0x13800,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    BIF_LANE_MARGINING =>
    {
        DESCRIPTION     => 'Data overlay for lane margining data',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv', 'pmu-ga10x_selfinit_riscv', 'pmu-ad10x_riscv', 'pmu-gh100_riscv', 'pmu-gb100_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LOWLATENCY =>
    {
        DESCRIPTION     => 'Data overlay for the LOWLATENCY task. Permanently attached to the task',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LOWLATENCY_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold LOWLATENCY\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    LIB_AVG =>
    {
        DESCRIPTION     => 'Data overlay for the average counters library',
        HEAP_SIZE       => 0x400,
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    DISP_CMD_BUFFER =>
    {
        DESCRIPTION     => 'Overlay for the buffer to hold DISP\'s commands',
        PROFILES        => [ 'pmu-ga10x_riscv...', '-pmu-ga1*b*', '-pmu-ga10f_riscv', '-pmu-ad10b_riscv', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    INIT_DATA =>
    {
        DESCRIPTION     => 'Overlay for the data used only in pre-init code',
        HEAP_SIZE       => 0x0,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

    USTREAMER_DATA =>
    {
        DESCRIPTION     => 'Overlay for uStreamer non-resident queues. Lwrrently used by PMUMon/LwTopps data',
        HEAP_SIZE       => 0x1000,
        PROFILES        => [ 'pmu-ga10x_riscv...', ],
        LOCATION        => 'dmem_odp',
        PERMISSIONS     => 'rw',
    },

];
# return the raw data of Sections definition
return $riscvSectionsDataRaw;
