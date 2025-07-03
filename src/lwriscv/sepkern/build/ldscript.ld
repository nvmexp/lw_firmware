OUTPUT_ARCH( "riscv" )
ENTRY(startup)

MEMORY {
    dtcm (rw) : ORIGIN = 0x0000000000180000, LENGTH = 0x600
    itcm (rx) : ORIGIN = 0x0000000000100000, LENGTH = 0x1000
}

SECTIONS
{
    .monitor_code :
    { 
        *(.text.startup)
        *(.text .text.*)
        . = ALIGN(256);
    } >itcm
    __monitorCodeSize = SIZEOF(.monitor_code);

    .monitor_data :
    {
        *(.rodata .rodata.* .srodata .srodata.*)
        *(.data .data.*)
        . = ALIGN(256);
    } >dtcm
    __monitorDataSize = SIZEOF(.monitor_data);

    .monitor_bss :
    {
        *(.bss .bss.*)
        . = ALIGN(256);
    } >dtcm
    /* No need to export this since .bss does not live in elf file */
}
