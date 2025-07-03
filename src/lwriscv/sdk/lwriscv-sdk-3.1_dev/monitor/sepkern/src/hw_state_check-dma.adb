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

package body Hw_State_Check.DMA
is
    procedure Check_DMA (Err_Code : in out Error_Codes)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_DMATRFCMD_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);
        State : LW_PRGNLCL_FALCON_DMATRFCMD_Register;
        MmodeReg : LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register;
        MmodeDma : MMODE_GROUP;
    begin

        -- read DMA enable/disable
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_0_Address,
                     Data => MmodeReg);
        MmodeDma := LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_0(MmodeReg).DMA;

        -- only check if DMA is enabled in device map
        if MmodeDma.Readable = MMODE_READ_ENABLE and then
           MmodeDma.Writable = MMODE_WRITE_ENABLE
        then
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_DMATRFCMD_Address, Data => State);

            -- make sure DMA engine is idle
            if State.Idle = IDLE_FALSE then
                Err_Code := HW_CHECK_DMA_NOT_IDLE;
                return;
            end if;

            -- make sure DMA engine is clean
            if State.Error = ERROR_TRUE then
                Err_Code := HW_CHECK_DMA_ERROR;
                return;
            end if;
        end if;

    end Check_DMA;

end Hw_State_Check.DMA;
