--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with Ada.Unchecked_Conversion;
with Dev_Prgnlcl; use Dev_Prgnlcl;
with Nv_Types;    use Nv_Types;

package body FBIF_Cfg
is
    procedure FBIF_Write_Transcfg (Region : NvU32;
                                   Value  : NvU32)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_TRANSCFG_Register);
        function Convert_To_Transcfg is new Ada.Unchecked_Conversion
            (Source => NvU32,
             Target => LW_PRGNLCL_FBIF_TRANSCFG_Register);
        ADDR_STRIDE : constant := LW_PRGNLCL_FBIF_TRANSCFG_1_Address - LW_PRGNLCL_FBIF_TRANSCFG_0_Address;

    begin
        Csb_Reg_Wr32(Addr => (LW_PRGNLCL_FBIF_TRANSCFG_0_Address + (ADDR_STRIDE * Region)),
                             Data => Convert_To_Transcfg(Value));

    end FBIF_Write_Transcfg;

    procedure FBIF_Write_Regioncfg (Region : NvU32;
                                    Value  : NvU4)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FBIF_REGIONCFG_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FBIF_REGIONCFG_Register);
        RegionCfg : LW_PRGNLCL_FBIF_REGIONCFG_Register;

    begin
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_FBIF_REGIONCFG_Address,
                     Data => RegionCfg);

        case Region is
            when 0 =>
                RegionCfg.T0 := Value;
            when 1 =>
                RegionCfg.T1 := Value;
            when 2 =>
                RegionCfg.T2 := Value;
            when 3 =>
                RegionCfg.T3 := Value;
            when 4 =>
                RegionCfg.T4 := Value;
            when 5 =>
                RegionCfg.T5 := Value;
            when 6 =>
                RegionCfg.T6 := Value;
            when 7 =>
                RegionCfg.T7 := Value;
            when others =>
                null;
        end case;

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_FBIF_REGIONCFG_Address,
                     Data => RegionCfg);

    end FBIF_Write_Regioncfg;
end FBIF_Cfg;
