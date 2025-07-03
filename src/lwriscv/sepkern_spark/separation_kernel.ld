OUTPUT_ARCH( "riscv" )
ENTRY(startup)
MEMORY {
    dtcm (rw) : ORIGIN = _DMEM_START_ADDRESS, LENGTH = _DMEM_LIMIT
    itcm (rx) : ORIGIN = _IMEM_START_ADDRESS, LENGTH = _IMEM_LIMIT
}
SECTIONS
{
  .monitor_code :
  {
      *(.text.startup)
      . = ALIGN(4);
      *(.text.trapEntry)
      . = ALIGN(4);
      *(.text.trap_handler)
      . = ALIGN(4);
      *(.text.temp_s_mode_trap_handler)
      . = ALIGN(4);
      *(.text._ada_sk_init)
      . = ALIGN(4);
      *(.text .text.*)
      . = ALIGN(256);
  } > itcm
  __monitor_code_size = SIZEOF(.monitor_code);

  ASSERT(__monitor_code_size % 256 == 0, "__monitor_code_size is not 256B aligned") 

  .monitor_data :
  {
    _monitor_stack_bottom = . ;
    . += 512 ;
    _monitor_stack_top = . ;

    . = ALIGN(256);

    *(.rodata .rodata.* .srodata .srodata.*)
    *(.data .data.*)
    *(.bss .bss.*)
    . = ALIGN(256);

  } > dtcm

}

