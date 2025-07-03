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
    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 6K
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

    __resident_code_start = .;
    .text :
    {
        *(.start)
        *(.text.*)
    }

    __ovl1_code_start = ALIGN(256);
    OVERLAY ALIGN(256): NOCROSSREFS
    {
        .ovl1 { *.o(.ovl1init) *.o(.ovl1*) . = ALIGN(256); }
    } >CODE AT >OVERLAYS

    __ovl1_code_end = ALIGN(256);
    __ovl1_base     = __ovl1_code_start;

    _ovl1BaseFB     = ADDR(.ovl1);
    _ovl1Size       = __ovl1_code_end - __ovl1_code_start;

    /*Making all the code resident*/
    __resident_code_end = ALIGN(256);

    /*
     * RESIDENT DATA SECTION
     */

    .data   : { *(.dataEntry) *(.data)  }
    .rodata : { *(.rodata)              }
    .bss    : { *(.bss) *(COMMON)       }

    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data);
    _dataBaseDMEM   = ADDR(.data);

    /*
     * OVERLAY LOAD START/STOP ADDRESSES
     */

    __load_stop_ovl1      = LOADADDR(.ovl1)     + SIZEOF(.ovl1);
    __load_start_ovl1     = LOADADDR(.ovl1)     & ~(0xFF);

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
