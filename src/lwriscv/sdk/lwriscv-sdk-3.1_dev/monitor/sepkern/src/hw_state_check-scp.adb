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

package body Hw_State_Check.SCP
is
    procedure Check_SCP (Err_Code : in out Error_Codes)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_HWCFG2_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_SCP_STATUS_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_SCPDMAPOLL_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_SCP_CTL_P2PRX_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_SCP_INTR_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);

        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
        Status : LW_PRGNLCL_SCP_STATUS_Register;
        ScpDma : LW_PRGNLCL_RISCV_SCPDMAPOLL_Register;
        P2prx : LW_PRGNLCL_SCP_CTL_P2PRX_Register;
        Intr : LW_PRGNLCL_SCP_INTR_Register;
        MmodeReg : LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register;
        MmodeScp : MMODE_GROUP;
    begin

        Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_HWCFG2_Address, Data => Hwcfg2);

        -- check only if SCP exist
        if Hwcfg2.Scp = SCP_ENABLE then

            -- read SCP enable/disable
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_2_Address,
                         Data => MmodeReg);
            MmodeScp := LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_2(MmodeReg).SCP;

            -- only check if SCP is enabled in device map
            if MmodeScp.Readable = MMODE_READ_ENABLE and then
               MmodeScp.Writable = MMODE_WRITE_ENABLE
            then
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_SCP_STATUS_Address, Data => Status);
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_SCPDMAPOLL_Address, Data => ScpDma);
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_SCP_CTL_P2PRX_Address, Data => P2prx);

                -- make sure SCP engine is idle
                if Status.Scp_Active = ACTIVE_TRUE or else
                   ScpDma.Dma_Active = ACTIVE_ACTIVE or else
                   P2prx.Sfk_Loaded = LOADED_FALSE
                then
                    Err_Code := HW_CHECK_SCP_NOT_IDLE;
                    return;
                end if;

                -- make sure SCP engine is clean
                Csb_Reg_Rd32(Addr => LW_PRGNLCL_SCP_INTR_Address, Data => Intr);
                if Intr.Acl_Vio = LW_PRGNLCL_SCP_INTR_ACL_VIO_PENDING or else
                   Intr.Selwrity_Vio = LW_PRGNLCL_SCP_INTR_SELWRITY_VIO_PENDING or else
                   Intr.Cmd_Error = LW_PRGNLCL_SCP_INTR_CMD_ERROR_PENDING
                then
                    Err_Code := HW_CHECK_SCP_ERROR;
                    return;
                end if;
            end if;
        end if;

    end Check_SCP;

end Hw_State_Check.SCP;
