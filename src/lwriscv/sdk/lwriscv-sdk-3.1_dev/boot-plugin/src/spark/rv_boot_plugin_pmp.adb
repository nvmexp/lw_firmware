-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;
with Lw_Ref_Dev_Riscv_Csr_64; use Lw_Ref_Dev_Riscv_Csr_64;
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;
with Lw_Types.Shift_Left_Op;use Lw_Types.Shift_Left_Op;
with Ada.Unchecked_Colwersion;
with System;
with Rv_Brom_Riscv_Core;
with Lw_Types;

package body Rv_Boot_Plugin_Pmp is

    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG2_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPADDR_Register);

    procedure Csr_Reg_Rd64 is new Rv_Brom_Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG2_Register);
    procedure Csr_Reg_Rd64 is new Rv_Brom_Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPADDR_Register);

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_INDEX_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_MODE_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_CFG_Register);

    procedure Initialize is
    begin
        -- Set up Core-PMP
        -- backgound
        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address + (LW_RISCV_CSR_MEXTPMPADDR_1_Address - LW_RISCV_CSR_MEXTPMPADDR_0_Address) * (COREPMP_INDEX_BACKGROUND - 16),
                       Data => LW_RISCV_CSR_MEXTPMPADDR_Register'(Addr => LwU62 (IOPMP_ADDR_BACKGROUND),
                                                                  Wiri => 0));
        -- Enable all
        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address + (LW_RISCV_CSR_MEXTPMPADDR_1_Address - LW_RISCV_CSR_MEXTPMPADDR_0_Address) * (COREPMP_INDEX_BROM - 16),
                       Data => LW_RISCV_CSR_MEXTPMPADDR_Register'(Addr => LwU62 (IOPMP_ADDR_BACKGROUND),
                                                                  Wiri => 0));


        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG2_Address,
                       Data => LW_RISCV_CSR_MEXTPMPCFG2_Register'(Pmp31l => PMP31L_LOCK,
                                                                  Pmp31s => 0,
                                                                  Pmp31a => PMP31A_NAPOT,   -- Disable all
                                                                  Pmp31x => PMP31X_DENIED,
                                                                  Pmp31w => PMP31W_DENIED,
                                                                  Pmp31r => PMP31R_DENIED,
                                                                  Pmp30l => PMP30L_LOCK,
                                                                  Pmp30s => 0,
                                                                  Pmp30a => PMP30A_NAPOT,   -- Enable all
                                                                  Pmp30x => PMP30X_PERMITTED,
                                                                  Pmp30w => PMP30W_PERMITTED,
                                                                  Pmp30r => PMP30R_PERMITTED,
                                                                  Pmp29l => PMP29L_UNLOCK,
                                                                  Pmp29s => 0,
                                                                  Pmp29a => PMP29A_OFF,
                                                                  Pmp29x => PMP29X_DENIED,
                                                                  Pmp29w => PMP29W_DENIED,
                                                                  Pmp29r => PMP29R_DENIED,
                                                                  Pmp28l => PMP28L_UNLOCK,
                                                                  Pmp28s => 0,
                                                                  Pmp28a => PMP28A_OFF,
                                                                  Pmp28x => PMP28X_DENIED,
                                                                  Pmp28w => PMP28W_DENIED,
                                                                  Pmp28r => PMP28R_DENIED,
                                                                  Pmp27l => PMP27L_UNLOCK,
                                                                  Pmp27s => 0,
                                                                  Pmp27a => PMP27A_OFF,
                                                                  Pmp27x => PMP27X_DENIED,
                                                                  Pmp27w => PMP27W_DENIED,
                                                                  Pmp27r => PMP27R_DENIED,
                                                                  Pmp26l => PMP26L_UNLOCK,
                                                                  Pmp26s => 0,
                                                                  Pmp26a => PMP26A_OFF,
                                                                  Pmp26x => PMP26X_DENIED,
                                                                  Pmp26w => PMP26W_DENIED,
                                                                  Pmp26r => PMP26R_DENIED,
                                                                  Pmp25l => PMP25L_UNLOCK,
                                                                  Pmp25s => 0,
                                                                  Pmp25a => PMP25A_OFF,
                                                                  Pmp25x => PMP25X_DENIED,
                                                                  Pmp25w => PMP25W_DENIED,
                                                                  Pmp25r => PMP25R_DENIED,
                                                                  Pmp24l => PMP24L_UNLOCK,
                                                                  Pmp24s => 0,
                                                                  Pmp24a => PMP24A_OFF,
                                                                  Pmp24x => PMP24X_DENIED,
                                                                  Pmp24w => PMP24W_DENIED,
                                                                  Pmp24r => PMP24R_DENIED));


        -- Clear the address value for 26, 25 and 24

        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address + (LW_RISCV_CSR_MEXTPMPADDR_1_Address - LW_RISCV_CSR_MEXTPMPADDR_0_Address) * (COREPMP_INDEX_GLBLIO - 16),
                       Data => LW_RISCV_CSR_MEXTPMPADDR_Register'(Addr => LwU62 (0),
                                                                  Wiri => 0));
        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address + (LW_RISCV_CSR_MEXTPMPADDR_1_Address - LW_RISCV_CSR_MEXTPMPADDR_0_Address) * (COREPMP_INDEX_25 - 16),
                       Data => LW_RISCV_CSR_MEXTPMPADDR_Register'(Addr => LwU62 (0),
                                                                  Wiri => 0));
        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address + (LW_RISCV_CSR_MEXTPMPADDR_1_Address - LW_RISCV_CSR_MEXTPMPADDR_0_Address) * (COREPMP_INDEX_24 - 16),
                       Data => LW_RISCV_CSR_MEXTPMPADDR_Register'(Addr => LwU62 (0),
                                                                  Wiri => 0));
    end Initialize;

end Rv_Boot_Plugin_Pmp;
