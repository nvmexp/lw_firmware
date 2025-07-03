-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;
with Lw_Types;         use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;      use Dev_Prgnlcl;

package body Debug_Control
is

    procedure Configure_ICD
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MDBGCTL_Register);
    begin

        -- Refer to following page for ICD settings:
        -- https://confluence.lwpu.com/display/LW/LWWATCH+Debugging+and+Security+-+GA10X+POR
        -- Debug settings
        -- Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MDBGCTL_Address,
        --              Data => LW_RISCV_CSR_MDBGCTL_Register'(Wiri                 => LW_RISCV_CSR_MDBGCTL_WIRI_RST,
        --                                                     Micd_Dis             => DIS_FALSE,
        --                                                     Icdcsrprv_S_Override => OVERRIDE_FALSE,
        --                                                     Icdmemprv            => ICDMEMPRV_M,
        --                                                     Icdmemprv_Override   => OVERRIDE_FALSE,
        --                                                     Icdpl                => ICDPL_USE_LWR));

        -- Production setting
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MDBGCTL_Address,
                     Data => LW_RISCV_CSR_MDBGCTL_Register'(Wiri                 => LW_RISCV_CSR_MDBGCTL_WIRI_RST,
                                                            Micd_Dis             => DIS_TRUE,           -- ICD stop at M-mode is not allowed
                                                            Icdcsrprv_S_Override => OVERRIDE_TRUE,      -- ICD access CSR has S-mode privilege
                                                            Icdmemprv            => ICDMEMPRV_S,        -- ICD load/store has S-mode privilege
                                                            Icdmemprv_Override   => OVERRIDE_TRUE,      -- Use ICDMEMPRV as ICD memory privilege (S-mode)
                                                            Icdpl                => ICDPL_USE_PL0));    -- ICD load/store will use dGPU PRIV level 0
    end Configure_ICD;

    procedure Program_Debug_Control(Debug_Control : Plcy_Debug_Control)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DBGCTL_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DBGCTL_LOCK_Register);
    begin

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_DBGCTL_Address,
                     Data => Debug_Control.Debug_Ctrl);

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_DBGCTL_LOCK_Address,
                     Data => Debug_Control.Debug_Ctrl_Lock);

    end Program_Debug_Control;

end Debug_Control;
