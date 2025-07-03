OUTPUT_ARCH( "riscv" )
ENTRY(bridge_entry)

PHDRS
{
  phdr_text     PT_LOAD;
  phdr_data     PT_LOAD;
}

SECTIONS
{
  . = 0x103000;
  .text : ALIGN(1024) 
  {
    KEEP(*(.text.init_partition_bridge))
    KEEP(*(.text*))
    . = ALIGN(4096);
    /* fmc code includes both SK and Bridge contents, it's kinda hacky here that
     * it assumes bridge being the last content in fmc. Though it probably won't
     * change for this sample app */
    fmc_code_size = ABSOLUTE(. - 0x100000);
  } :phdr_text

  . = 0x182000;
  .data : ALIGN(4096)
  {
    KEEP(*(.rodata*))
    KEEP(*(.srodata*))
    KEEP(*(.data*))
    KEEP(*(.sdata*))
    KEEP(*(.bss*))
    KEEP(*(.sbss*))
    . = ALIGN(4096);
  } :phdr_data
}
