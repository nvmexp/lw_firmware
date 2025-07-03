OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

/*
 * There is not 128K of memory but the code space is still 128K thanks to Imem-
 * tagging. To make sure that it fits in the physical overlay, we have to make
 * sure that the tasks size do exceed the size left above the RTOS e.g between
 * __overlay_base and end of physical memory.
 */
MEMORY
{
    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 128K
    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 64K
    OVERLAYS  : ORIGIN = 0x20000000, LENGTH = 1M
    DESC (A)  : ORIGIN = 0x30000000, LENGTH = 1K
}

PROVIDE (__stack = LENGTH(DATA));
SECTIONS
{
    /*
     * RESIDENT CODE SECTION
     *
     * Define the components which comprise the Resident Code Section. The
     * Resident Code Section is a logical code section which physically maps
     * to the .text section of the image.
     */

    __acr_resident_code_start = .;
    .text :
    {
        *(.start)
    }

    __acr_code_start = ALIGN(256);
    OVERLAY ALIGN(256): NOCROSSREFS
    {
        .imem_acr { *.o(.imem_acrinit) *.o(.imem_acr.*) *.o(.imem_acr) . = ALIGN(256); }
        .imem_acr { *CCC.a:(.text.*)   . = ALIGN(256); }
        .imem_acr { *.o(.imem_acr_end) . = ALIGN(256); }
    } >CODE AT >OVERLAYS

    __acr_code_end = ALIGN(256);
    __acr_base     = __acr_code_start;

    _acrBaseFB     = ADDR(.imem_acr);
    _acrSize       = __acr_code_end - __acr_code_start;

    /*Making all the code resident*/
    __acr_resident_code_end = ALIGN(256);

    /*
     * RESIDENT DATA SECTION
     */

    .data   : { *(.dataEntry) *(.data) *(.rodata)}

    __bss_start = ALIGN(4);
    .bss    : { *(.bss) *(COMMON)       }
    __bss_end = ALIGN(4);

    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data);
    _dataBaseDMEM   = ADDR(.data);

    /*
     * OVERLAY LOAD START/STOP ADDRESSES
     */

    __load_stop_acr      = LOADADDR(.imem_acr)     + SIZEOF(.imem_acr);
    __load_start_acr     = LOADADDR(.imem_acr)     & ~(0xFF);

    __num_overlays = 1;

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
