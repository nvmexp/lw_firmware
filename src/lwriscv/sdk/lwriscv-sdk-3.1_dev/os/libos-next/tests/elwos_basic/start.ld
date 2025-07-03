OUTPUT_ARCH( "riscv" )
ENTRY(start)

PHDRS
{
  phdr_text     PT_LOAD;
}

SECTIONS
{
  . = 0x7400000000000000;
  .text : ALIGN(1024) 
  {
    KEEP(*(.text.startup))
    KEEP(*(.got*))
    KEEP(*(.text*))
    KEEP(*(.cold.text*))
    KEEP(*(.rodata*))
    KEEP(*(.srodata*))
    KEEP(*(.data*))
    KEEP(*(.cold.data*))
    KEEP(*(.sdata*))
    KEEP(*(.bss*))
    KEEP(*(.sbss*))
    . = ALIGN(4096);
  } :phdr_text

  image : ALIGN(4096) 
  {
    image_start = . ;
    KEEP(*(.image*))
  } :phdr_text
}
