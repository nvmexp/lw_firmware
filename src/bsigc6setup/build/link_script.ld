OUTPUT_FORMAT("elf32-falcon", "elf32-falcon", "elf32-falcon")
OUTPUT_ARCH(falcon)

ENTRY(_start)

MEMORY
{
    CODE (X) : ORIGIN = 0x00000000, LENGTH = 1k
}

PROVIDE (__stack = 0x1000);

SECTIONS
{
  .start (0x0) : AT (0x0)
   {
      *(.start)            
   }
  .text . : AT (SIZEOF(.start))
   { 
     *(.text.*) 
   }

  /* DWARF2 debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
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
