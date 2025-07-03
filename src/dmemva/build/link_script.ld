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
    DATA (A)  : ORIGIN = 0x10000000, LENGTH = 1792
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
        .ovl1
        {
            *.o(.ovl1Entry) *.o(.ovl1) . = ALIGN(256);
            PROVIDE (__selwreSharedStart = .); 
            *.o(.ovl1Shared) . = ALIGN(256);
            PROVIDE (__selwreSharedEnd = .); 
        }
    } >CODE AT >OVERLAYS

    __ovl1_code_end = ALIGN(256);
    __ovl1_base     = __ovl1_code_start;

    _ovl1BaseFB     = ADDR(.ovl1);
    _ovl1Size       = __ovl1_code_end - __ovl1_code_start;

    __ovl2_code_start = ALIGN(256);
    OVERLAY ALIGN(256): NOCROSSREFS
    {
        .ovl2
        {
            PROVIDE (__tc1s  = .); *.o(.tc1 ) . = ALIGN(256); PROVIDE (__tc1e  = .);
            PROVIDE (__tc2s  = .); *.o(.tc2 ) . = ALIGN(256); PROVIDE (__tc2e  = .);
            PROVIDE (__tc3s  = .); *.o(.tc3 ) . = ALIGN(256); PROVIDE (__tc3e  = .);
            PROVIDE (__tc4s  = .); *.o(.tc4 ) . = ALIGN(256); PROVIDE (__tc4e  = .);
            PROVIDE (__tc5s  = .); *.o(.tc5 ) . = ALIGN(256); PROVIDE (__tc5e  = .);
            PROVIDE (__tc6s  = .); *.o(.tc6 ) . = ALIGN(256); PROVIDE (__tc6e  = .);
            PROVIDE (__tc7s  = .); *.o(.tc7 ) . = ALIGN(256); PROVIDE (__tc7e  = .);
            PROVIDE (__tc8s  = .); *.o(.tc8 ) . = ALIGN(256); PROVIDE (__tc8e  = .);
            PROVIDE (__tc9s  = .); *.o(.tc9 ) . = ALIGN(256); PROVIDE (__tc9e  = .);
            PROVIDE (__tc10s = .); *.o(.tc10) . = ALIGN(256); PROVIDE (__tc10e = .);
            PROVIDE (__tc11s = .); *.o(.tc11) . = ALIGN(256); PROVIDE (__tc11e = .);
            PROVIDE (__tc12s = .); *.o(.tc12) . = ALIGN(256); PROVIDE (__tc12e = .);
            PROVIDE (__tc13s = .); *.o(.tc13) . = ALIGN(256); PROVIDE (__tc13e = .);
            PROVIDE (__tc14s = .); *.o(.tc14) . = ALIGN(256); PROVIDE (__tc14e = .);
            PROVIDE (__tc15s = .); *.o(.tc15) . = ALIGN(256); PROVIDE (__tc15e = .);
            PROVIDE (__tc16s = .); *.o(.tc16) . = ALIGN(256); PROVIDE (__tc16e = .);
            PROVIDE (__tc17s = .); *.o(.tc17) . = ALIGN(256); PROVIDE (__tc17e = .);
            PROVIDE (__tc18s = .); *.o(.tc18) . = ALIGN(256); PROVIDE (__tc18e = .);
            PROVIDE (__tc19s = .); *.o(.tc19) . = ALIGN(256); PROVIDE (__tc19e = .);
            PROVIDE (__tc20s = .); *.o(.tc20) . = ALIGN(256); PROVIDE (__tc20e = .);
            PROVIDE (__tc21s = .); *.o(.tc21) . = ALIGN(256); PROVIDE (__tc21e = .);
            PROVIDE (__tc22s = .); *.o(.tc22) . = ALIGN(256); PROVIDE (__tc22e = .);
            PROVIDE (__tc23s = .); *.o(.tc23) . = ALIGN(256); PROVIDE (__tc23e = .);
            PROVIDE (__tc24s = .); *.o(.tc24) . = ALIGN(256); PROVIDE (__tc24e = .);
            PROVIDE (__tc25s = .); *.o(.tc25) . = ALIGN(256); PROVIDE (__tc25e = .);
            PROVIDE (__tc26s = .); *.o(.tc26) . = ALIGN(256); PROVIDE (__tc26e = .);
            PROVIDE (__tc27s = .); *.o(.tc27) . = ALIGN(256); PROVIDE (__tc27e = .);
            PROVIDE (__tc28s = .); *.o(.tc28) . = ALIGN(256); PROVIDE (__tc28e = .);
            PROVIDE (__tc29s = .); *.o(.tc29) . = ALIGN(256); PROVIDE (__tc29e = .);
            PROVIDE (__tc30s = .); *.o(.tc30) . = ALIGN(256); PROVIDE (__tc30e = .);
            PROVIDE (__tc31s = .); *.o(.tc31) . = ALIGN(256); PROVIDE (__tc31e = .);
            PROVIDE (__tc32s = .); *.o(.tc32) . = ALIGN(256); PROVIDE (__tc32e = .);
        }
    } >CODE AT >OVERLAYS

    __ovl2_code_end = ALIGN(256);
    __ovl2_base     = __ovl2_code_start;

    _ovl2BaseFB     = ADDR(.ovl2);
    _ovl2Size       = __ovl2_code_end - __ovl2_code_start;

    /*Making all the code resident*/
    __resident_code_end = ALIGN(256);

    /*
     * RESIDENT DATA SECTION
     */

    .data : { *(.rodata) *(.bss) *(COMMON) *(.data)  *(.dataEnd) }
    _dataOvlEnd     = ALIGN(256);

    _dataBaseFB     = ADDR(.data);
    __dataSize       = _dataOvlEnd - _dataBaseFB;
    _dataBaseDMEM   = ADDR(.data);
    __ovl1_data_end = __dataSize;

    /*
     * OVERLAY LOAD START/STOP ADDRESSES
     */

    __load_stop_ovl1      = LOADADDR(.ovl1)     + SIZEOF(.ovl1);
    __load_start_ovl1     = LOADADDR(.ovl1)     & ~(0xFF);

    __load_stop_ovl2      = LOADADDR(.ovl2)     + SIZEOF(.ovl2);
    __load_start_ovl2     = LOADADDR(.ovl2)     & ~(0xFF);

    __num_overlays = 2;

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
