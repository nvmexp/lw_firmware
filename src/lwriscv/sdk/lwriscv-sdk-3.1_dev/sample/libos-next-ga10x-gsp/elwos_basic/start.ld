OUTPUT_ARCH( "riscv" )
ENTRY(start)

PHDRS
{
  phdr_text     PT_LOAD;
  phdr_data     PT_LOAD;
}

SECTIONS
{
  . = 0x6060000000000000;
  .text : ALIGN(1024) 
  {
    text_start = . ;
    KEEP(*(.text.startup))
    KEEP(*(.got*))
    KEEP(*(.text*))
    KEEP(*(.cold.text*))
    . = ALIGN(4096);
  } :phdr_text

  .data : ALIGN(4096)
  {
    KEEP(*(.rodata*))
    KEEP(*(.srodata*))
    KEEP(*(.data*))
    KEEP(*(.cold.data*))
    KEEP(*(.sdata*))
    KEEP(*(.bss*))
    KEEP(*(.sbss*))
    . = ALIGN(4096);
  } :phdr_data

  image : ALIGN(4096)
  {
    image_start = . ;
    KEEP(*(.image*))
  } :phdr_data

  image_offset = ABSOLUTE(image_start - text_start);
  
}
