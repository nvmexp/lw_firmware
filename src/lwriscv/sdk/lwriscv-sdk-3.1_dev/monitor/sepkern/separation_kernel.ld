OUTPUT_ARCH( "riscv" )
ENTRY(startup)
MEMORY {
    dtcm (rw) : ORIGIN = _DMEM_START_ADDRESS, LENGTH = _DMEM_LIMIT
    itcm (rx) : ORIGIN = _IMEM_START_ADDRESS, LENGTH = _IMEM_LIMIT
}
SECTIONS
{
  .sepkern_code :
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
      . = ALIGN(0x1000);
  } > itcm
  __sepkern_code_size = SIZEOF(.sepkern_code);

  ASSERT(__sepkern_code_size % 1024 == 0, "__sepkern_code_size is not 1024B aligned") 

  .sepkern_data :
  {
    _sepkern_stack_bottom = . ;
    . += 1024 ;
    _sepkern_stack_top = . ;

    . = ALIGN(256);

    *(.rodata .rodata.* .srodata .srodata.*)
    *(.data .data.* .sdata .sdata.*)
    *(.sdata2 .sdata2.*)
    *(.bss .bss.* .sbss*)
    . = ALIGN(0x1000);

  } > dtcm

}

