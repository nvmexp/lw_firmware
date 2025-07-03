/*
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

/*
 * Pictorial representation of data memory (not drawn to scale)
 * This picture is only valid for RTOS Falcons without DMEM VA and without EMEM
 * TODO-ssapre: Update this diagram once the code changes are done.
 * Please keep this diagram up to date with other code changes!
 *
 *     ___________________________________ addr = DMEM_MAX
 *    |                                   |   ^
 *    |     Init arguments (64 bytes)     |   |
 *    |___________________________________|   |
 *    |                                   |   | RM share for queues and RM
 *    |    Init stack (used until the     |   | managed DMEM heap (shared by
 *    |        scheduler starts)          |   | init arguments and init stack
 *    |___________________________________|   | since they are only used before
 *    |                                   |   | init msg is sent to RM).
 *    |                                   |   |
 *    |         Part of RM share          |   |
 *    |                                   |   |
 *    |___________________________________|   v
 *    |                                   |   ^
 *    |             OS heap               |   | Heap used for TCBs, task queues,
 *    |                                   |   | and other allocations.
 *    |___________________________________|   v
 *    |                                   |
 *    |            ISR stack              |     Interrupt Service Routine stack.
 *    |    (256 bytes, configurable)      |
 *    |___________________________________|
 *    |                                   |
 *    |            ESR stack              |     Exception Service Routine stack.
 *    |    (200 bytes, configurable)      |
 *    |___________________________________|
 *    |                                   |
 *    |                                   |
 *    |           Global data             |
 *    |                                   |
 *    |                                   |
 *    |___________________________________| addr = 0
 *
 * Notes:
 *
 * 1. The init arguments size is statically bound to 64 bytes, and if they ever
 * increase beyond that, we would need to reserve more space for them to avoid
 * clashing with the init stack.
 *
 * 2. The init stack grows downwards through the RM share, and that is okay
 * because the RM share will never be used until the init msg is sent. The
 * assumption is that the init stack is small enough that it will never flow
 * into the OS heap, which is used at the same time as the init stack. We don't
 * set a limit for the init stack, and never do bounds checking on it. All
 * stacks other than the init stack are subject to bounds checking (either SW-
 * based checking on older Falcon cores where HW isn't available, or HW limit
 * enforcement on the latest cores).
 *
 * 3. The ISR/ESR stacks has predefined size (see Profiles.pm), and are never
 * save-restored. Since we don't support nested interrupts, we would always
 * start each ISR/ESR with the stack pointer at the highest address reserved
 * for its stack and let it grow downwards.
 */

OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

/*
 * Profile Specific Data
 */
MEMORY
{
{{ EMIT_MEMORY_SECTIONS() }}
}

{{ EMIT_PROVIDE_MEMORY_SIZE_ITEMS() }}

/*
 * Common Part below
 */
SECTIONS
{
    /*
     * HEAVY SECURE OVERLAY CODE SECTIONS
     */

{{  EMIT_IMEM_OVERLAY_OUTPUT_SECTIONS('hs_overlay', '    ') }}

    /*
     * RESIDENT CODE SECTION
     *
     * Define the components which comprise the Resident Code Section. The
     * Resident Code Section is a logical code section which physically maps
     * to the .text section of the image.
     *
     * The .text section contains the .text section of every object file
     * excluding task/overlay modules (taskn_*.o).
     */

    /*
     * __hs_overlay_code_end is ALIGN(256)
     */
    __resident_code_start = __hs_overlay_code_end;
    .text : ALIGN(256)
    {
{{  EMIT_IMEM_RESIDENT_SECTION('        ') }}
    }
    __resident_code_end         = ALIGN(4);
    __resident_code_end_aligned = ALIGN(256);

    __ls_resident_end_imem = ALIGN(256) - __resident_code_start;
    __overlay_base_imem = __ls_resident_end_imem + {{EMIT_IMEM_HS_RESIDENT_OVERLAY_SIZE()}}
    __overlay_base_imem_idx = __overlay_base_imem >> 8;
    /*
     * NON SECURE/LIGHT SECURE OVERLAY CODE SECTIONS
     */

{{  EMIT_IMEM_OVERLAY_OUTPUT_SECTIONS('non_hs_overlay', '    ') }}

    /*
     * Patch locations for encrypted HS overlays.
     * Debug and prod encrypted HS overlays will be output when
     * BOOT_FROM_HS is not enabled.  When BOOT_FROM_HS is enabled
     * one one set of HS overlays are output for each debug and 
     * prod binary.
     */
{{  EMIT_CODE_PATCH_HOLES('patch_dbg_', '    ') }}
{{  EMIT_CODE_PATCH_HOLES('patch_prd_', '    ') }}
{{  EMIT_CODE_PATCH_HOLES('patch_', '    ') }}

    /*
     * LS SIGNATURE GROUP SECTION
     * NOTE: This section should always be the end of CODE
     */
{{  EMIT_IMEM_LS_SIG_GROUP_SECTIONS('    ',__resident_code_start,__resident_code_end_aligned) }}

    /*
     * RESIDENT DATA SECTION
     */
    .data : {
{{  EMIT_DMEM_RESIDENT_SECTION('        ') }}
    }

{{  EMIT_DMEM_OVERLAY_OUTPUT_SECTIONS('resident_overlay', '    ') }}

    /*
     * The following two offsets specify the base of the data section in the
     * uncompressed image. They are in a format that siggen requries in order
     * to sign HS overlays and patch signatures into the data section of the
     * built image.
     */
    _dataBaseFB = ADDR(.data);
    _dataBaseDMEM = _dataBaseFB;

    /*
     * Fill a dummy data at 256 aligned address which creates a hole between
     * .data and .imemOvlTag below. This is only needed when there is any HS
     * overlay so that any change to .data won't cause rearrangement of the
     * OS resident data which requires resigning.
     */
    .dmemHole : ALIGN(256)
    {
{{      EMIT_DMEM_HOLE('        ') }}
    }
    PROVIDE (__dmemHole = ADDR(.dmemHole));
    .imemOvlTag : ALIGN(2)
    {
{{      EMIT_IMEM_OVERLAY_TAG_LIST('        ') }}
    }
    PROVIDE (__imem_ovl_tag = ADDR(.imemOvlTag));
{{      EMIT_PATCHED_IMEM_OVERLAY_TAG_LIST('patch_dbg_', '        ', '__patched_dbg_imem_ovl_tag', 'patchedDbgImemOvlTag'), }}
{{      EMIT_PATCHED_IMEM_OVERLAY_TAG_LIST('patch_prd_', '        ', '__patched_prd_imem_ovl_tag', 'patchedPrdImemOvlTag' ) }}
{{      EMIT_PATCHED_IMEM_OVERLAY_TAG_LIST('patch_', '        '    , '__patched_imem_ovl_tag'    , 'patchedImemOvlTag' ) }}
    .imemOvlSize :
    {
{{      EMIT_IMEM_OVERLAY_SIZE_LIST('        ') }}
    }
    PROVIDE (__imem_ovl_size = ADDR(.imemOvlSize));

    /*
     * Note: IMEM Overlay End Address list is a special case, as we only want
     * to emit symbols when necessary. Omit from linker script otherwise.
     */
{{      EMIT_IMEM_OVERLAY_END_ADDR_LD_ENTRY_AND_LIST('    ') }}
    .imemOvlMruPrev :
    {
{{      EMIT_OVERLAY_MRU_LIST('        ', 'imem', 'prev') }}
    }
    PROVIDE (__imem_ovl_mru_prev = ADDR(.imemOvlMruPrev));
    .imemOvlMruNext :
    {
{{      EMIT_OVERLAY_MRU_LIST('        ', 'imem', 'next') }}
    }
    PROVIDE (__imem_ovl_mru_next = ADDR(.imemOvlMruNext));

    PROVIDE (__num_imem_hs_overlays = {{ EMIT_NUM_HS_OVERLAYS() }});

{{      EMIT_IMEM_HS_SIGNATURES_LIST('        ', 'dbg', '__imem_hs_signatures_dbg', 'imem_hs_signatures_dbg') }}
{{      EMIT_IMEM_HS_SIGNATURES_LIST('        ', 'prd', '__imem_hs_signatures_prd', 'imem_hs_signatures_prd') }}
{{      EMIT_IMEM_HS_SIGNATURES_LIST('        ', ''   , '__imem_hs_signatures'   , 'imem_hs_signatures') }}

    __num_dmem_overlays = {{ EMIT_NUM_DMEM_OVERLAYS() }};

    .imem_hs_sig_metadata :
    {
{{      EMIT_IMEM_HS_SIGNATURE_UCODE_ID('        ') }}
{{      EMIT_IMEM_HS_SIGNATURE_FUSE_VER('        ') }}
    }
    PROVIDE (__imem_hs_sig_metadata = ADDR(.imem_hs_sig_metadata));

    .dmemOvlStartAddress  : ALIGN(4)
    {
{{  EMIT_DMEM_OVERLAY_START_ADDRESS('        ') }}
    }
    PROVIDE (__dmem_ovl_start_address = ADDR(.dmemOvlStartAddress));

    .dmemOvlSizeLwrrent  : ALIGN(2)
    {
{{  EMIT_DMEM_OVERLAY_SIZE_LWRRENT('        ') }}
    }
    PROVIDE (__dmem_ovl_size_lwrrent = ADDR(.dmemOvlSizeLwrrent));

    .dmemOvlSizeMax  : ALIGN(2)
    {
{{  EMIT_DMEM_OVERLAY_SIZE_MAX('        ') }}
    }
    PROVIDE (__dmem_ovl_size_max = ADDR(.dmemOvlSizeMax));

    .dmemOvlMruPrev :
    {
{{      EMIT_OVERLAY_MRU_LIST('        ', 'dmem', 'prev') }}
    }
    PROVIDE (__dmem_ovl_mru_prev = ADDR(.dmemOvlMruPrev));

    .dmemOvlMruNext :
    {
{{      EMIT_OVERLAY_MRU_LIST('        ', 'dmem', 'next') }}
    }
    PROVIDE (__dmem_ovl_mru_next = ADDR(.dmemOvlMruNext));

{{  EMIT_DMEM_OVERLAY_ID('    ') }}

    .imemOvlAttrResident :
    {
{{      EMIT_IMEM_OVERLAY_ATTRIBUTE_RESIDENT('        ') }}
    }
    PROVIDE (__imem_ovl_attr_resident = ADDR(.imemOvlAttrResident));

    .dmemOvlAttrResident :
    {
{{      EMIT_DMEM_OVERLAY_ATTRIBUTE_RESIDENT('        ') }}
    }
    PROVIDE (__dmem_ovl_attr_resident = ADDR(.dmemOvlAttrResident));

    /*
     * __heap, __heap_end, __esr_stack_start, __esr_stack_end, __isr_stack_start,
     * and __isr_stack_end are defined as offset from the
     * beginning DMEM because they are used in OpenRTOS.
     */
    PROVIDE (__esr_stack_start   = ALIGN(4) - ORIGIN(DATA));
    PROVIDE (__esr_stack_end     = __esr_stack_start + {{ EMIT_ESR_STACK_SIZE() }});

    PROVIDE (__isr_stack_start   = __esr_stack_end);
    PROVIDE (__isr_stack_end     = __isr_stack_start + {{ EMIT_ISR_STACK_SIZE() }});

{{  EMIT_BUILD_SPECIFIC_REGIONS('    ') }}

    PROVIDE (__stack             = __end_physical_dmem - 64);

    /*
     * Place the Data section of DMEM shared with RM at the end. This section is
     * used by RTOS to define the communication queue as well as the DMEM part
     * managed by RM to pass/get payload.
     */
    .rm_rtos_share ORIGIN(DATA) + __rm_share_start : { *(.rmrtos) *(.rmrtos.*) } > DATA

    /*
     * __end_rmqueue defines the end of the queues allocation : the space between
     * __end_rmqueue and end_physical_dmem is reserved for RM as a memory pool.
     */
    __end_rmqueue = . - ORIGIN(DATA);


{{  EMIT_DMEM_OVERLAY_OUTPUT_SECTIONS('non_resident_overlay', '    ') }}

{{  EMIT_IMEM_OVERLAY_SYMBOLS('    ') }}

    __num_imem_overlays = {{ EMIT_NUM_IMEM_OVERLAYS() }};

    .desc ORIGIN( DESC ) :
    {
        LONG(__resident_code_start);
        LONG(__resident_code_end);
        LONG(__resident_data_end);
        LONG(__num_imem_overlays);
        LONG(__num_imem_hs_overlays);
        LONG(__num_dmem_overlays);

        __imem_overlay_desc_start = .;

{{      EMIT_IMEM_OVERLAY_DESCRIPTOR_TABLE('        ') }}

        __imem_overlay_desc_end = .;

        __dmem_overlay_desc_start = .;

{{      EMIT_DMEM_OVERLAY_DESCRIPTOR_TABLE('        ') }}

        __dmem_overlay_desc_end = .;

    } > DESC

    /*
     * DWARF2 debug sections. Symbols in the DWARF debugging sections are
     * relative to the beginning of the section so we begin them at 0.
     */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /DISCARD/ : { *(.note.GNU-stack) }
}

