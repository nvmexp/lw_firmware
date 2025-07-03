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
with Dev_Prgnlcl;                use Dev_Prgnlcl;
with SBI_Types;                  use SBI_Types;
with Type_Colwersion;            use Type_Colwersion;

package body SBI_Update_Tracectl
is
    procedure Update_Tracectl (Param : LwU64; SBI_RC : out SBI_Return_Type)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_TRACECTL_Register);

        function Colwert_To_Tracectl is new Ada.Unchecked_Colwersion
            (Source => LwU32,
             Target => LW_PRGNLCL_RISCV_TRACECTL_Register);

        Input_LwU32 : LwU32;
        Input_In_Range : Boolean;

        VALUE_PARAM_OUT_OF_RANGE       : constant LwU64 := 1;
        VALUE_PARAM_ILWALID_MODE       : constant LwU64 := 2;

    begin
        -- param must be a valid 32bit value
        -- This is for both correctness check and spark flow check
        Colwert_LwU64_To_LwU32(Param, Input_LwU32, Input_In_Range);
        if Input_In_Range = False then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_OUT_OF_RANGE;
            return;
        end if;

        -- Make sure the Mode field is valid, 0x3 is not allowed
        if ((Shift_Right (Input_LwU32, 24)) and 16#3#) = 16#3# then
            SBI_RC.Error := SBI_ERR_ILWALID_PARAM;
            SBI_RC.Value := VALUE_PARAM_ILWALID_MODE;
            return;
        end if;

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_TRACECTL_Address,
                     Data => Colwert_To_Tracectl(Input_LwU32));

        SBI_RC.Error := SBI_SUCCESS;
        SBI_RC.Value := 0;

    end Update_Tracectl;

end SBI_Update_Tracectl;
