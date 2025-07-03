-- @summary
-- This file contains the information that will be used by BROM
package Project is
    pragma Pure;
    -- The following definitions are colwerted from the project.py directly.
    LW_PROJECT                             : constant String := "ghlit1_lwdecghlit1";
    LW_FALCON_UC_PREFIX                    : constant String := "LWDEC";
    LW_FALCON_IMEM_SIZE                    : constant        := 192;
    LW_FALCON_DMEM_SIZE                    : constant        := 160;
    LW_HAS_MODULE_tfbif                    : constant        := 0;
    LW_CHIP_EXTENDED_PHYSICAL_ADDRESS_BITS : constant        := 52;
    LW_CHIP_EXTENDED_VIRTUAL_ADDRESS_BITS  : constant        := 57;

    LW_LWDEC_GDMA_CHAN_NUM          : constant := 0;
    LW_LWDEC_FBIF_NACK_AS_ACK       : constant := 0;
    LW_LWDEC_RISCV_PA_IROM_END      : constant := 16#10_0000#;
    LW_LWDEC_RISCV_PA_IROM_START    : constant := 16#8_0000#;
    LW_LWDEC_RISCV_PA_IMEM_END      : constant := 16#18_0000#;
    LW_LWDEC_RISCV_PA_IMEM_START    : constant := 16#10_0000#;
    LW_LWDEC_RISCV_PA_DMEM_END      : constant := 16#20_0000#;
    LW_LWDEC_RISCV_PA_DMEM_START    : constant := 16#18_0000#;
    LW_LWDEC_RISCV_PA_INTIO_END     : constant := 16#160_0000#;
    LW_LWDEC_RISCV_PA_INTIO_START   : constant := 16#140_0000#;
    LW_LWDEC_RISCV_PA_EXTIO1_END    : constant := 16#2000_0000_0400_0000#;
    LW_LWDEC_RISCV_PA_EXTIO1_START  : constant := 16#2000_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTIO2_END    : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTIO2_START  : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTIO3_END    : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTIO3_START  : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTIO4_END    : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTIO4_START  : constant := 16#0#;
    LW_LWDEC_RISCV_PA_EXTMEM1_END   : constant := 16#7800_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM1_START : constant := 16#7400_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM2_END   : constant := 16#9800_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM2_START : constant := 16#9400_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM3_END   : constant := 16#a200_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM3_START : constant := 16#a000_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM4_END   : constant := 16#6010_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM4_START : constant := 16#6000_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM5_END   : constant := 16#8010_0000_0000_0000#;
    LW_LWDEC_RISCV_PA_EXTMEM5_START : constant := 16#8000_0000_0000_0000#;

    -- Rename the definitions which will be used in Ada
    LW_XXX_FALCON_IMEM_SIZE : constant := LW_FALCON_IMEM_SIZE;
    LW_XXX_FALCON_DMEM_SIZE : constant := LW_FALCON_DMEM_SIZE;

    LW_XXX_RISCV_PA_IROM_END    : constant         := LW_LWDEC_RISCV_PA_IROM_END;
    LW_XXX_RISCV_PA_IROM_START  : constant         := LW_LWDEC_RISCV_PA_IROM_START;
    LW_XXX_RISCV_PA_IROM_EXISTS : constant Boolean := LW_XXX_RISCV_PA_IROM_START < LW_XXX_RISCV_PA_IROM_END;

    LW_XXX_RISCV_PA_IMEM_END    : constant         := LW_LWDEC_RISCV_PA_IMEM_END;
    LW_XXX_RISCV_PA_IMEM_START  : constant         := LW_LWDEC_RISCV_PA_IMEM_START;
    LW_XXX_RISCV_PA_IMEM_EXISTS : constant Boolean := LW_XXX_RISCV_PA_IMEM_START < LW_XXX_RISCV_PA_IMEM_END;

    LW_XXX_RISCV_PA_DMEM_END    : constant         := LW_LWDEC_RISCV_PA_DMEM_END;
    LW_XXX_RISCV_PA_DMEM_START  : constant         := LW_LWDEC_RISCV_PA_DMEM_START;
    LW_XXX_RISCV_PA_DMEM_EXISTS : constant Boolean := LW_XXX_RISCV_PA_DMEM_START < LW_XXX_RISCV_PA_DMEM_END;

    LW_XXX_RISCV_PA_INTIO_END    : constant         := LW_LWDEC_RISCV_PA_INTIO_END;
    LW_XXX_RISCV_PA_INTIO_START  : constant         := LW_LWDEC_RISCV_PA_INTIO_START;
    LW_XXX_RISCV_PA_INTIO_EXISTS : constant Boolean := LW_XXX_RISCV_PA_INTIO_START < LW_XXX_RISCV_PA_INTIO_END;

    LW_XXX_RISCV_PA_EXTIO1_END    : constant         := LW_LWDEC_RISCV_PA_EXTIO1_END;
    LW_XXX_RISCV_PA_EXTIO1_START  : constant         := LW_LWDEC_RISCV_PA_EXTIO1_START;
    LW_XXX_RISCV_PA_EXTIO1_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTIO1_START < LW_XXX_RISCV_PA_EXTIO1_END;

    LW_XXX_RISCV_PA_EXTIO2_END    : constant         := LW_LWDEC_RISCV_PA_EXTIO2_END;
    LW_XXX_RISCV_PA_EXTIO2_START  : constant         := LW_LWDEC_RISCV_PA_EXTIO2_START;
    LW_XXX_RISCV_PA_EXTIO2_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTIO2_START < LW_XXX_RISCV_PA_EXTIO2_END;

    LW_XXX_RISCV_PA_EXTIO3_END    : constant         := LW_LWDEC_RISCV_PA_EXTIO3_END;
    LW_XXX_RISCV_PA_EXTIO3_START  : constant         := LW_LWDEC_RISCV_PA_EXTIO3_START;
    LW_XXX_RISCV_PA_EXTIO3_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTIO3_START < LW_XXX_RISCV_PA_EXTIO3_END;

    LW_XXX_RISCV_PA_EXTIO4_END    : constant         := LW_LWDEC_RISCV_PA_EXTIO4_END;
    LW_XXX_RISCV_PA_EXTIO4_START  : constant         := LW_LWDEC_RISCV_PA_EXTIO4_START;
    LW_XXX_RISCV_PA_EXTIO4_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTIO4_START < LW_XXX_RISCV_PA_EXTIO4_END;

    LW_XXX_RISCV_PA_EXTMEM1_END    : constant         := LW_LWDEC_RISCV_PA_EXTMEM1_END;
    LW_XXX_RISCV_PA_EXTMEM1_START  : constant         := LW_LWDEC_RISCV_PA_EXTMEM1_START;
    LW_XXX_RISCV_PA_EXTMEM1_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTMEM1_START < LW_XXX_RISCV_PA_EXTMEM1_END;

    LW_XXX_RISCV_PA_EXTMEM2_END    : constant         := LW_LWDEC_RISCV_PA_EXTMEM2_END;
    LW_XXX_RISCV_PA_EXTMEM2_START  : constant         := LW_LWDEC_RISCV_PA_EXTMEM2_START;
    LW_XXX_RISCV_PA_EXTMEM2_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTMEM2_START < LW_XXX_RISCV_PA_EXTMEM2_END;

    LW_XXX_RISCV_PA_EXTMEM3_END    : constant         := LW_LWDEC_RISCV_PA_EXTMEM3_END;
    LW_XXX_RISCV_PA_EXTMEM3_START  : constant         := LW_LWDEC_RISCV_PA_EXTMEM3_START;
    LW_XXX_RISCV_PA_EXTMEM3_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTMEM3_START < LW_XXX_RISCV_PA_EXTMEM3_END;

    LW_XXX_RISCV_PA_EXTMEM4_END    : constant         := LW_LWDEC_RISCV_PA_EXTMEM4_END;
    LW_XXX_RISCV_PA_EXTMEM4_START  : constant         := LW_LWDEC_RISCV_PA_EXTMEM4_START;
    LW_XXX_RISCV_PA_EXTMEM4_EXISTS : constant Boolean := LW_XXX_RISCV_PA_EXTMEM4_START < LW_XXX_RISCV_PA_EXTMEM4_END;

    LW_XXX_HAS_MODULE_TFBIF : constant Boolean := LW_HAS_MODULE_tfbif = 1;
    LW_XXX_FBIF_NACK_AS_ACK : constant Boolean := LW_LWDEC_FBIF_NACK_AS_ACK = 1;

    LW_XXX_GDMA_CHAN_NUM : constant := LW_LWDEC_GDMA_CHAN_NUM;

end Project;
