--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Riscv_Core.Csb_Reg;
with Lw_Types;                   use Lw_Types;
with SBI_Types;                  use SBI_Types;
with Type_Colwersion;            use Type_Colwersion;
with FBIF_Cfg;                   use FBIF_Cfg;

package body SBI_FBIF_Cfg
is
    procedure FBIF_Transcfg (Region : LwU64;
                             Value  : LwU64;
                             SBI_RC : out SBI_Return_Type)
    is
        RegInput_LwU32 : LwU32;
        RegInput_In_Range : Boolean;

        NUM_REGIONS : constant LwU64 := 8;

        VALUE_PARAM_ILWALID_REGION         : constant LwU64 := 1;
        VALUE_PARAM_REG_VALUE_OUT_OF_RANGE : constant LwU64 := 2;

    begin
        -- region number must be in range
        if Region >= NUM_REGIONS then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_ILWALID_REGION;
            return;
        end if;

        -- param must be a valid 32bit value
        -- This is for both correctness check and spark flow check
        Colwert_LwU64_To_LwU32(Value, RegInput_LwU32, RegInput_In_Range);
        if RegInput_In_Range = False then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_REG_VALUE_OUT_OF_RANGE;
            return;
        end if;

        FBIF_Write_Transcfg(LwU32(Region), RegInput_LwU32);

        SBI_RC.Error := SBI_SUCCESS;
        SBI_RC.Value := 0;

    end FBIF_Transcfg;

    procedure FBIF_Regioncfg (Region : LwU64;
                              Value  : LwU64;
                              SBI_RC : out SBI_Return_Type)
    is
        RegInput_LwU4 : LwU4;
        RegInput_In_Range : Boolean;

        NUM_REGIONS : constant LwU64 := 8;

        VALUE_PARAM_ILWALID_REGION         : constant LwU64 := 1;
        VALUE_PARAM_REG_VALUE_OUT_OF_RANGE : constant LwU64 := 2;

    begin

        -- region number must be in range
        if Region >= NUM_REGIONS then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_ILWALID_REGION;
            return;
        end if;

        -- param must be a valid regioncfg value (LwU4)
        -- This is for both correctness check and spark flow check
        Colwert_LwU64_To_LwU4(Value, RegInput_LwU4, RegInput_In_Range);
        if RegInput_In_Range = False then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_REG_VALUE_OUT_OF_RANGE;
            return;
        end if;

        FBIF_Write_Regioncfg(LwU32(Region), RegInput_LwU4);

        SBI_RC.Error := SBI_SUCCESS;
        SBI_RC.Value := 0;

    end FBIF_Regioncfg;

end SBI_FBIF_Cfg;
