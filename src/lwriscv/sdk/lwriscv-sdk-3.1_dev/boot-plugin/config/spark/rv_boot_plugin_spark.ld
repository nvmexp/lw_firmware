OUTPUT_ARCH("riscv")

ENTRY(rv_brom_entry);

/* Engine TCM spec */
_IMEM_PA_OFFSET = 0x00100000;
_DMEM_PA_OFFSET = 0x00180000;
_IMEM_SIZE = 0xc000; /* 48K */
_DMEM_SIZE = 0xa000; /* 40K */

/* Boot-plugin footprint info */
_TEXT_SIZE_LIMIT = 0x2000; /* 8K */
_PKCPARAM_SIZE = 0xa00; /* 2.5K */
_DATA_SIZE_LIMIT = 0x400; /* 1K */
_STACK_SIZE = 0x400; /* 1K */

_TEXT_OFFSET = _IMEM_PA_OFFSET + _IMEM_SIZE - _TEXT_SIZE_LIMIT;
_DATA_OFFSET = _DMEM_PA_OFFSET + _DMEM_SIZE - _DATA_SIZE_LIMIT - _PKCPARAM_SIZE;

MEMORY
{
    boot_plugin_text(rx): ORIGIN = _TEXT_OFFSET, LENGTH = _TEXT_SIZE_LIMIT
    boot_plugin_data(rw): ORIGIN = _DATA_OFFSET, LENGTH = _DATA_SIZE_LIMIT
}

SECTIONS
{
    .rv_boot_plugin_overwritable_code :
    {
        *(.text.rv_brom_entry)
        . = ALIGN(0x4);

        *(.text._ada_rv_brom);
        . = ALIGN(0x4);

        *(__init_device_phase)
        . = ALIGN(0x4);
        *(__load_pkc_boot_param_phase)
        . = ALIGN(0x4);
        *(__sanitize_manifest_phase)
        . = ALIGN(0x4);
        *(__load_fmc_phase1)
    } > boot_plugin_text

    .rv_boot_plugin_non_overwritable_code :
    {
        *(__load_fmc_phase2)
        . = ALIGN(0x4);
        *(__revoke_resource_phase)
        . = ALIGN(0x4);
        *(__configure_fmc_elw_phase)
        . = ALIGN(0x4);

        *(.text.rv_boot_plugin_error_handling__trap_handler)
        . = ALIGN(0x4);
        *(.text.rv_boot_plugin_error_handling__trap_handler_for_halt_only)
        . = ALIGN(0x4);
        *(.text.rv_boot_plugin_error_handling__last_chance_handler)
        . = ALIGN(0x4);
        *(*.__gnat_last_chance_handler)
        . = ALIGN(0x4);

        *(.text .text.*)
        . = ALIGN(0x100);
    } > boot_plugin_text

    /*
     * Push rv_boot_plugin_non_overwritable_code as far back in memory as possible to maximize the
     * space that FMC code can occupy. We do this by callwlating __imem_overwritable_region_end
     * based on the size of the rv_boot_plugin_non_overwritable_code region we just defined,
     * and then setting the start address of that region to __imem_overwritable_region_end.
     */
    __imem_overwritable_region_end = ABSOLUTE(ORIGIN(boot_plugin_text) + LENGTH(boot_plugin_text) -
        SIZEOF(.rv_boot_plugin_non_overwritable_code));

    .rv_boot_plugin_non_overwritable_code __imem_overwritable_region_end : { } > boot_plugin_text

    .rv_boot_plugin_data :
    {
        __stack_start = . ;
        . += _STACK_SIZE ;
        __stack_end = . ;

    } > boot_plugin_data
}
