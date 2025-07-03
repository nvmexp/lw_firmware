--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;                 use Lw_Types;
with Dev_Prgnlcl;              use Dev_Prgnlcl;
with Riscv_Core.Csb_Reg;       use Riscv_Core.Csb_Reg;
with Error_Handling;           use Error_Handling;
with Device_Map_Policy_Types;  use Device_Map_Policy_Types;
with Policy_External_To_Internal_Colwersions; use Policy_External_To_Internal_Colwersions;

package body Hw_State_Check.IOPMP
is
    procedure Check_IOPMP (Err_Code : in out Error_Codes)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);
        State : LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_Register;
        MmodeReg : LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register;
        MmodeIopmp : MMODE_GROUP;
    begin

        -- read IOPMP enable/disable
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_1_Address,
                     Data => MmodeReg);
        MmodeIopmp := LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_1(MmodeReg).IOPMP;

        -- only check if IOPMP is enabled in device map
        if MmodeIopmp.Readable = MMODE_READ_ENABLE and then
           MmodeIopmp.Writable = MMODE_WRITE_ENABLE
        then
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_Address, Data => State);

            -- make sure there's no iopmp violation
            if State.Valid = VALID_TURE then
                Err_Code := HW_CHECK_IOPMP_ERROR;
            end if;
        end if;

    end Check_IOPMP;

end Hw_State_Check.IOPMP;
