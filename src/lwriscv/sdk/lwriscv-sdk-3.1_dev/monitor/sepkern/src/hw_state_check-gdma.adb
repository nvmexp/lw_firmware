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

package body Hw_State_Check.GDMA
is
    procedure Check_GDMA (Err_Code : in out Error_Codes)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_HWCFG2_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_GDMA_CHAN_STATUS_Register);

        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
        Chan0_Status : LW_PRGNLCL_GDMA_CHAN_STATUS_Register;
        Chan1_Status : LW_PRGNLCL_GDMA_CHAN_STATUS_Register;
    begin

        Csb_Reg_Rd32(Addr => LW_PRGNLCL_FALCON_HWCFG2_Address, Data => Hwcfg2);

        -- check only if GDMA exist
        if Hwcfg2.Gdma = GDMA_ENABLE then

            -- make sure GDMA engine is idle and clean
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_GDMA_CHAN_STATUS_0_Address, Data => Chan0_Status);
            Csb_Reg_Rd32(Addr => LW_PRGNLCL_GDMA_CHAN_STATUS_1_Address, Data => Chan1_Status);

            if Chan0_Status.Busy = BUSY_TRUE or else
               Chan1_Status.Busy = BUSY_TRUE
            then
                Err_Code := HW_CHECK_GDMA_NOT_IDLE;
                return;
            end if;

            if Chan0_Status.Error_Vld = VLD_TRUE or else
               Chan1_Status.Error_Vld = VLD_TRUE
            then
                Err_Code := HW_CHECK_GDMA_ERROR;
                return;
            end if;

        end if;

    end Check_GDMA;

end Hw_State_Check.GDMA;
