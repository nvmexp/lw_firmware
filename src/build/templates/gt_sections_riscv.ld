/*
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 */

OUTPUT_ARCH( "riscv" )

/* Specify the entry point to the program
 *
 * On a RISCV image, this is _start
 */
{{ EMIT_ENTRYPOINT() }}

/*****************************************************************************
 * Define memory layout
 ****************************************************************************/
MEMORY
{
{{ EMIT_MEMORY_SECTIONS() }}
}

{{ EMIT_PROVIDE_MEMORY_SIZE_ITEMS() }}
{{ EMIT_HIDDEN_PA_POINTERS() }}
{{ EMIT_HEAP_SYMBOL('freeable') }}
{{ EMIT_HEAP_SYMBOL('os') }}

/*****************************************************************************
 * Define the sections, and where they are mapped in memory
 ****************************************************************************/
SECTIONS
{
/* This is generated from SectionsRiscvCode.pm and SectionsRiscvData.pm
 *
 * If the section has an alignment parameter, the greater of the two is used
 * for the placement alignment.
 *
 * Overlays get placed into FBGPA, as that's where they're stored physically.
 */

/* How VMA holes work:
 *
 * Virtual memory          Physical Memory
 * +-----------------+-----+-----------------+
 * | PAGE 0          |     | PAGE 0          |
 * +-----------------+-----+-----------------+
 * + VMA hole        +   / | PAGE 1          |
 * +-----------------+--/  +-----------------+
 * | PAGE 1          |    /
 * +-----------------+---/
 *
 * Only used blocks are physically present, holes are gaps in just virtual memory.
 * This means the VMA holes do not use any actual memory. However, the extra space
 * used may require larger instructions to address other sections.
 *
 * Trying to access (read/write/execute) memory in the hole causes an exception.
 *
 * Each region definition here is responsible for adding a VMA hole after itself.
 */

{{  EMIT_IDENTITY_SECTIONS() }}

{{  EMIT_LOGGING_SECTION() }}

    . = ORIGIN(vma);

/* Create VMA hole at the beginning */
{{  EMIT_RISCV_VMA_HOLE('null') }}

{{  EMIT_RISCV_DATA_SECTIONS() }}
{{  EMIT_RISCV_CODE_SECTIONS() }}

/* Fall-back sections */
{{  EMIT_RISCV_SECTION('text', 'fb', 0, 'rx', 0, [
        '*(.text) *(.text.*)',
    ], 1);
    EMIT_RISCV_VMA_HOLE('text') }}

{{  EMIT_RISCV_SECTION('rodata', 'fb', 0, 'r', 0, [
        '*(.rodata) *(.rodata.*)',
        '*(.srodata) *(.srodata.*)',
        '*(.got) *(.got.*)',
    ], 1);
    EMIT_RISCV_VMA_HOLE('rodata') }}

{{  EMIT_RISCV_SECTION('data', 'fb', 0, 'rw', 0, [
        '*(.data) *(.data.*)',
        '*(.bss) *(.bss.*)',

        '*(.sdata) *(.sdata.*)',
        '*(.sbss) *(.sbss.*)',
        '*(COMMON)',
    ], 1);
    EMIT_RISCV_VMA_HOLE('data') }}

{{  EMIT_SYSTEM_STACK('dmem_res') }}
    __stack_start = ADDR(.sysStack);
    __stack_size  = SIZEOF(.sysStack);
    __stack_end   = ADDR(.sysStack) + SIZEOF(.sysStack);

{{  EMIT_DMEM_END_CARVEOUT_SYMBOLS() }}

{{  EMIT_HEAP_CONSTANTS() }}

{{  EMIT_ODP_SYMBOLS() }}

{{  EMIT_MPU_DESCRIPTOR() }}

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

{{  EMIT_ASSERTIONS() }}

/* Make sure that fall-back sections are empty */
ASSERT (((__section_text_max_size) == 0),   "ERROR: Fall-back output section .text must be empty!{{
                                          }} Make sure that all code in .text (default code section) is accounted for in RiscvSectionsCode.pm{{
                                          }} (Hint: check that sections in RiscvSectionsCode.pm match the ones in OverlaysImem.pm)");

ASSERT (((__section_rodata_max_size) == 0), "ERROR: Fall-back output section .rodata must be empty!{{
                                          }} Make sure all string literals, jump tables and all other .rodata are accounted for in RiscvSectionsData.pm{{
                                          }} (Hint: check that sections in RiscvSectionsData.pm match the ones in OverlaysDmem.pm)");

ASSERT (((__section_data_max_size) == 0),   "ERROR: Fall-back output section .data must be empty!{{
                                          }} Make sure all global variables and other .data are accounted for in RiscvSectionsData.pm{{
                                          }} (Hint: check that sections in RiscvSectionsData.pm match the ones in OverlaysDmem.pm)");
