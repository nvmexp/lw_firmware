#define LW_RISCV_PA_DTCM_BASE 0x0000000000180000
#define LW_RISCV_PA_ITCM_BASE 0x0000000000100000


OUTPUT_ARCH( "riscv" )
ENTRY(startup)

MEMORY {
    dtcm (rw) : ORIGIN = LW_RISCV_PA_DTCM_BASE, LENGTH = 0x10000 // MK TODO: this should be passed by compiler
    itcm (rx) : ORIGIN = LW_RISCV_PA_ITCM_BASE, LENGTH = 0x10000 // MK TODO: this should be passed by compiler
}

SECTIONS
{
  .monitor_code :
  { 
      *(.text.startup)
      *(.text .text.*)
      *(.rodata .rodata.* .srodata .srodata.*)
      . = ALIGN(256);
  } > itcm

  .monitor_data :
  {
    *(.data .data.*)
    *(.bss .bss.*)
    . = ALIGN(256);
    _monitor_stack_bottom = . ;
    . += 8192;
    _monitor_stack_top = . ;
  } > dtcm
}
