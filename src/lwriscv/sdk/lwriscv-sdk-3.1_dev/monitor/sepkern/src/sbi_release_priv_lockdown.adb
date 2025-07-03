--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with Types;                      use Types;
with Dev_Prgnlcl;                use Dev_Prgnlcl;
with SBI_Types;                  use SBI_Types;

package body SBI_Release_Priv_Lockdown
is
    procedure Release_Priv_Lockdown (SBI_RC : out SBI_Return_Type)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register);
    begin
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Address,
                     Data => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register'(Lock => LOCK_UNLOCKED));
        SBI_RC.Error := SBI_SUCCESS;
        SBI_RC.Value := 0;
    end Release_Priv_Lockdown;

end SBI_Release_Priv_Lockdown;
