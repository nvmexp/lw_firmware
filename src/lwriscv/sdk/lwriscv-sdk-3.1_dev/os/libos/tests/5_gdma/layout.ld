#include "libos-config.h"
#include "peregrine-headers.h"
#include "manifest_defines.h"
OUTPUT_ARCH( "riscv" )
ENTRY(KernelBootstrap)

MEMORY
{
  imem      (rx)  : ORIGIN = LIBOS_CONFIG_IMEM_FIRST, LENGTH = LIBOS_CONFIG_IMEM_SIZE
  dmem      (rw)  : ORIGIN = LIBOS_CONFIG_DMEM_FIRST, LENGTH = LIBOS_CONFIG_DMEM_SIZE
  manifest  (rx)  : ORIGIN = 0xFFFFFFFF00000000, LENGTH = 0x100000000
}

PHDRS
{
  phdr_text     PT_LOAD;
  phdr_data     PT_LOAD;
  phdr_manifest 0x1000;
}

SECTIONS
{
  /* Define the manifest sections (e.g. task/port declarations)
     These are placed in phdr_manifest and may be dropped from the final image
    after running the manifest compiler. */
  #include "libos-manifest.ld"

  . = 0x40000;
  .text : ALIGN(1024) 
  {
    /* Kernel mapping include code for init task */
    taskall_code = . ;
    KEEP(*(.kernel.startup))
    KEEP(*(.kernel.hot.text*))
    KEEP(*(.kernel.cold.text*))
    KEEP(*(.rodata*))
    KEEP(*(.srodata*))

    /* User code */
    KEEP(*(.text*))
    . = ALIGN(1024);
    taskall_code_size = ABSOLUTE(. - taskall_code);
  } >imem :phdr_text

  /* 
    Reserve spaces for the compiled manifest (e.g. resource hash tables) 
    @note declares taskinit_and_kernel_data  
  */
  #include "libos-compiled-manifest.ld"

  .data :  {
    /* Declare kernel data */
#   include "libos-data.ld"
    KEEP(*(.kernel.hot.data*))
    KEEP(*(.kernel.cold.data*))
    KEEP(*(.init.data*))
    . =  ALIGN(1024);
    taskinit_and_kernel_data_size = ABSOLUTE(. - taskinit_and_kernel_data);    
    
    /* Worker task */
    taskworker_data = . ;
    KEEP(*(.worker.data*))
    . = ALIGN(1024);
    taskworker_data_size = ABSOLUTE(. - taskworker_data);

    /* Catch-all to ensure all variables were tagged */
    must_be_empty = . ;
    KEEP(*(.data))
    KEEP(*(.sdata))
    KEEP(*(.bss))
    KEEP(*(.sbss))
    ASSERT(ABSOLUTE(.) == ABSOLUTE(must_be_empty), "One or more variables or functions were not assigned a __attribute__((section(...)))" );*/
  } >dmem :phdr_data
}
