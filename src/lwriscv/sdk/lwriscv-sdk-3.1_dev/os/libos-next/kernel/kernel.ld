#include "libos-config.h"
#include "peregrine-headers.h"
#include "manifest_defines.h"
OUTPUT_ARCH( "riscv" )
ENTRY(_start)

PHDRS
{
    phdr_text     PT_LOAD FILEHDR PHDRS;
    phdr_data     PT_LOAD;
}

SECTIONS
{
    . = LIBOS_CONFIG_KERNEL_TEXT_BASE;
    .text : ALIGN(4096) 
    {
        taskall_code = . ;

        /* Processor bootstrap code must be first */
        KEEP(*(.text.startup))
        KEEP(*(.text.bootstrap))

        /* Trap handler and context save must be in the same page */
        KEEP(*(.text.trap))
        KEEP(*(.text.context))

        /* Code from all tasks must be together */
        KEEP(*(.hot.text*))
        KEEP(*(.cold.text*))
        KEEP(*(.text*))
        KEEP(*(.init.text*))

        /* Read-only data next */
        KEEP(*(.rodata*))
        KEEP(*(.srodata*))
        . = ALIGN(4096);
        taskall_code_size = ABSOLUTE(. - taskall_code);

        end_of_text = . ;
    } :phdr_text

    /* 
      Reserve spaces for the compiled manifest (e.g. resource hash tables) 
      @note declares taskinit_and_kernel_data  
    */
    . = LIBOS_CONFIG_KERNEL_DATA_BASE;
    .data :  {
        taskinit_and_kernel_data = . ;
        /* Kernel stack is first to ensure overflows trap */
        KEEP(*(.hot.data.KernelSchedulerStack))
        /* Declare kernel data */
  #   include "libos-data.ld"
  
        KEEP(*(.hot.data*))
        KEEP(*(.cold.data*))
        KEEP(*(.data*))
        KEEP(*(.sdata*))
        KEEP(*(.bss*))
        KEEP(*(.sbss*))
        . =  ALIGN(4096);
        taskinit_and_kernel_data_size = ABSOLUTE(. - taskinit_and_kernel_data);    
    } :phdr_data
}
