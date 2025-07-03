--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with Nv_Types;    use Nv_Types;
with Dev_Prgnlcl; use Dev_Prgnlcl;
with Bit_Ops;     use Bit_Ops;
with Ada.Unchecked_Conversion;

package body TFBIF_Cfg
is
    procedure TFBIF_Write_Transcfg (Region : NvU32;
                                    Swid   : NvU2)
    is
        function Convert_To_NvU32 is new Ada.Unchecked_Conversion
            (Source => LW_PRGNLCL_TFBIF_TRANSCFG_Register,
             Target => NvU32);

        function Convert_To_Reg is new Ada.Unchecked_Conversion
            (Source => NvU32,
             Target => LW_PRGNLCL_TFBIF_TRANSCFG_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_TRANSCFG_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_TRANSCFG_Register);

        TransCfg : LW_PRGNLCL_TFBIF_TRANSCFG_Register;
        Raw      : NvU32;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_TFBIF_TRANSCFG_Address,
                      Data => TransCfg);

        -- RMW the field for the region
        Raw := Convert_To_NvU32(TransCfg);
        Modify_Field(Raw, (Region * 4), 2, NvU32(Swid));
        TransCfg := Convert_To_Reg(Raw);

        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_TFBIF_TRANSCFG_Address,
                      Data => TransCfg);

    end TFBIF_Write_Transcfg;

    procedure TFBIF_Write_Regioncfg (Region : NvU32;
                                     Vpr    : NvU1;
                                     Apert  : NvU5)
    is
        function Convert_RegionCfg_To_NvU32 is new Ada.Unchecked_Conversion
            (Source => LW_PRGNLCL_TFBIF_REGIONCFG_Register,
             Target => NvU32);

        function Convert_NvU32_To_RegionCfg is new Ada.Unchecked_Conversion
            (Source => NvU32,
             Target => LW_PRGNLCL_TFBIF_REGIONCFG_Register);

        function Convert_RegionCfg1_To_NvU32 is new Ada.Unchecked_Conversion
            (Source => LW_PRGNLCL_TFBIF_REGIONCFG1_Register,
             Target => NvU32);

        function Convert_NvU32_To_RegionCfg1 is new Ada.Unchecked_Conversion
            (Source => NvU32,
             Target => LW_PRGNLCL_TFBIF_REGIONCFG1_Register);

        function Convert_RegionCfg2_To_NvU32 is new Ada.Unchecked_Conversion
            (Source => LW_PRGNLCL_TFBIF_REGIONCFG2_Register,
             Target => NvU32);

        function Convert_NvU32_To_RegionCfg2 is new Ada.Unchecked_Conversion
            (Source => NvU32,
             Target => LW_PRGNLCL_TFBIF_REGIONCFG2_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG1_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG1_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG2_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_TFBIF_REGIONCFG2_Register);

        RegionCfg  : LW_PRGNLCL_TFBIF_REGIONCFG_Register;
        RegionCfg1 : LW_PRGNLCL_TFBIF_REGIONCFG1_Register;
        RegionCfg2 : LW_PRGNLCL_TFBIF_REGIONCFG2_Register;
        RawRegionCfg  : NvU32;
        RawRegionCfg1 : NvU32;
        RawRegionCfg2 : NvU32;

    begin
        -- RMW vpr field for the region
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG_Address,
                      Data => RegionCfg);

        RawRegionCfg := Convert_RegionCfg_To_NvU32(RegionCfg);
        Modify_Field(RawRegionCfg, 3 + (Region * 4), 1, NvU32(Vpr));
        RegionCfg := Convert_NvU32_To_RegionCfg(RawRegionCfg);
        Csb_Reg_Wr32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG_Address,
                      Data => RegionCfg);

        -- RMW apert field for the region
        if Region < 4 then
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG1_Address,
                          Data => RegionCfg1);

            RawRegionCfg1 := Convert_RegionCfg1_To_NvU32(RegionCfg1);
            Modify_Field(RawRegionCfg1, (Region * 8), 5, NvU32(Apert));
            RegionCfg1 := Convert_NvU32_To_RegionCfg1(RawRegionCfg1);
            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG1_Address,
                          Data => RegionCfg1);
        else
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG2_Address,
                          Data => RegionCfg2);

            RawRegionCfg2 := Convert_RegionCfg2_To_NvU32(RegionCfg2);
            Modify_Field(RawRegionCfg2, ((Region - 4) * 8), 5, NvU32(Apert));
            RegionCfg2 := Convert_NvU32_To_RegionCfg2(RawRegionCfg2);
            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_TFBIF_REGIONCFG2_Address,
                          Data => RegionCfg2);
        end if;
    end TFBIF_Write_Regioncfg;
end TFBIF_Cfg;
