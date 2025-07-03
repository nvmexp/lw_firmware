OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

MEMORY
{
{{ EMIT_MEMORY_SECTIONS() }}
}

PROVIDE (__spMin = _spMin);
PROVIDE (__stack = LENGTH(DATA) - 0x1);
PROVIDE (__dataEnd = _dataEnd);

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
        /* adding new segements to make sure all code finishes in .text segment */
        *(.init.*)
        *(.memory.*)
        *(.resident.*)
    }
    
    /*Making all the code resident*/
    __resident_code_end = ALIGN(256);

    /*
     * RESIDENT DATA SECTION
     *  the start of the cmd queue section is aligned to 0x100 to have an alignement with
     *  eventual security masks
     */

    .data : {
       _dataBaseStart = .; 
       *(.rodata)
       *(.data)
     }
     
     /* use overlay for vbios table so the compiler is allowed to assign correct  */
     /* structure and it can also be overloaded with precompiled data             */
     /* Note: the bootime training and mclk swtich for the ate variant share the  */
     /*       same bios image so its important that it is placed at the same      */
     /*       location in memory. I would like to modify the lookup structure     */
     /*       such that all pointers are relative to the bios itself which would  */
     /*       remove that restriction.                                            */
     /* Note: there must be at least one access to vbiosTable inside  the ucode   */
     /*       for the compiler to build the full .vbios_dataset as listed below   */
     /*       as the reference of pointer to vbiosTable is loaded too there is no */
     /*       real requirement for that code to exist. */
     OVERLAY ALIGN(0x100):
     {
       .vbios_dump
       {
         _vbiosDumpStart = .;
         INCLUDE "../bios/gh100_ate_bios.ld"
         INCLUDE "../bios/gh100_ate_bios_references.ld"
         _vbiosDumpEnd = .;
        }
       .vbios_dataset
       {
         _vbiosTableStart = .;
         *(.vbiosTable.*)
         *(.vbiosTableReferences.static*)
         _vbiosTableEnd = .;
         ASSERT (SIZEOF(.vbios_dump) == SIZEOF(.vbios_dataset), "ERROR: vbios dump size not matching vbios data structure size");
       }
     } > DATA
      
     .data2 :
     {
       _dataEnd = .;
       _dataBaseEnd = .;
       
       . = ALIGN(256);
       *(.vbiosTableReferences.dynamic*)
       *(.bss)
       *(COMMON)
       
       . = ALIGN(256);   
       _trainingDataStart = .;
       *(.trainingData.*)
       _trainingDataEnd = .;

       . = ALIGN(256);
       *(.ppArea.*)

       _spMin = .;
    }
    
   
    
    _dataBaseFB     = ADDR(.data);
    _dataSize       = SIZEOF(.data) + SIZEOF(.data2);
    _dataBaseDMEM   = ADDR(.data);


    .desc ORIGIN( DESC ):
    {
        LONG(__resident_code_start);
        LONG(__resident_code_end);
        LONG(_dataBaseEnd - _dataBaseStart);
        LONG(0); /* __num_imem_overlays */
        LONG(0); /* __num_imem_hs_overlays */
        LONG(0); /* __num_dmem_overlays */
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

NOCROSSREFS(.ovl);
