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
    CODE (X)  : ORIGIN = 0x00000000, LENGTH = 64K
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

    __pub_resident_code_start = .;
    .text :
    {
        *(.start)
        *(.text.*)
    }

    __pub_code_start = ALIGN(256);
    OVERLAY ALIGN(256): NOCROSSREFS
    {
        .imem_pub { *.o(.imem_pubinit) *.o(.imem_pub.*) *.o(.imem_pub) . = ALIGN(256); }
        .imem_pub { *.o(.imem_pub_end)             . = ALIGN(256); }
    } >CODE AT >OVERLAYS

    __pub_code_end = ALIGN(256);
    __pub_base     = __pub_code_start;

    _pubBaseFB     = ADDR(.imem_pub);
    _pubSize       = __pub_code_end - __pub_code_start;
    __pubBlkCnt     = (_pubSize / 256 );

    /*Making all the code resident*/
    __pub_resident_code_end = ALIGN(256);

    /*
     * RESIDENT DATA SECTION
     */

    .data   : { *(.dataEntry) *(.data)  }

    __bss_begin = .;

    .bss    :
    {
        *(.bss) *(COMMON)
    }

    __bss_end = .;

    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data);
    _dataBaseDMEM   = ADDR(.data);

    /*
     * OVERLAY LOAD START/STOP ADDRESSES
     */

    __load_stop_pub      = LOADADDR(.imem_pub)     + SIZEOF(.imem_pub);
    __load_start_pub     = LOADADDR(.imem_pub)     & ~(0xFF);

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
