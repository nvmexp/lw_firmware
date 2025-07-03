OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

/*
 * There is not 128K of memory but the code space is still 128K thanks to Imem-
 * tagging. To make sure that it fits in the physical overlay, we have to make
 * sure that the tasks size do exceed the size left above the RTOS e.g between
 * __overlay_base and end of physical memory.
 *
 * Reserve the first 8K of DMEM for HS-falcon usage. RISCV will use the DMEM area
 * after 8K.
 */
MEMORY
{
    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 128K
    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 8K
    OVERLAYS  : ORIGIN = 0x20000000, LENGTH = 1M
    DESC (A)  : ORIGIN = 0x30000000, LENGTH = 1K
}

PROVIDE (__stack = LENGTH(DATA));
SECTIONS
{
    .text :
    {
        *(.start)
        *entry.c.p.o(.text.*)
    } > CODE

    OVERLAY ALIGN(256): NOCROSSREFS
    {
        .ovl1 {
            *.o(.text.enterHs)
            *.o(.text.selwreBusFun .text.writeBar0 .text.bar0WaitIdle)
            *.o(.text.*)
            *.o(.imem*)
            . = ALIGN(256);
        }
    } >CODE AT >OVERLAYS

    PROVIDE(__hs_ovl_start = ADDR(.ovl1));
    PROVIDE(__hs_ovl_size  = ALIGN(SIZEOF(.ovl1), 256));

    .data   : { *(.dataEntry) *(.data) *(.data.*) }
    .rodata : { *(.rodata)              }
    .bss    : { *(.bss) *(COMMON)       }

    /* For siggen */
    _ovl1Size         = __hs_ovl_size;
    _ovl1BaseFB       = __hs_ovl_start;
    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data);
    _dataBaseDMEM   = ADDR(.data);

    /* This symbol is dereferenced in _isPtrValid() and has to be larger than all the possible input address */
    __end_physical_dmem = 64K;

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

NOCROSSREFS(.ovl);
