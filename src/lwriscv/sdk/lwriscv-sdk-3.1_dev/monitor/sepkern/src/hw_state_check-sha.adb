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
with Ada.Unchecked_Colwersion;
with Device_Map_Policy_Types;  use Device_Map_Policy_Types;
with Policy_External_To_Internal_Colwersions; use Policy_External_To_Internal_Colwersions;

package body Hw_State_Check.SHA
is
    procedure Check_SHA (Err_Code : in out Error_Codes)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_HWCFG2_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_SHA_STATUS_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_SHA_ERR_STATUS_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);

        function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
            (Source => LW_PRGNLCL_FALCON_SHA_ERR_STATUS_Register,
             Target => LwU32);

        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
        Status : LW_PRGNLCL_FALCON_SHA_STATUS_Register;
        Err : LW_PRGNLCL_FALCON_SHA_ERR_STATUS_Register;
        MmodeReg : LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register;
        MmodeSha : MMODE_GROUP;
    begin

        Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_HWCFG2_Address, Data => Hwcfg2);

        -- only run checks if SHA exists
        if Hwcfg2.Sha = SHA_ENABLE then

            -- read Sha enable/disable
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_1_Address,
                         Data => MmodeReg);
            MmodeSha := LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_1(MmodeReg).SHA;

            -- only check if SHA is enabled in device map
            if MmodeSha.Readable = MMODE_READ_ENABLE and then
               MmodeSha.Writable = MMODE_WRITE_ENABLE
            then
                -- make sure SHA engine is idle
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_SHA_STATUS_Address, Data => Status);
                if Status.State = LW_PRGNLCL_FALCON_SHA_STATUS_STATE_BUSY then
                    Err_Code := HW_CHECK_SHA_NOT_IDLE;
                    return;
                end if;

                -- make sure SHA engine is clean
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_SHA_ERR_STATUS_Address, Data => Err);
                if Colwert_To_LwU32(Err) /= 0 then
                    Err_Code := HW_CHECK_SHA_ERROR;
                    return;
                end if;
            end if;
        end if;

    end Check_SHA;

end Hw_State_Check.SHA;
