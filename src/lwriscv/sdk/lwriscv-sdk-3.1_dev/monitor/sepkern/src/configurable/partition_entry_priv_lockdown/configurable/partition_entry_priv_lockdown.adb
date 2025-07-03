--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Policy_Types;               use Policy_Types;
with Dev_Prgnlcl;                use Dev_Prgnlcl;
with Riscv_Core.Csb_Reg;         use Riscv_Core.Csb_Reg;
with Separation_Kernel.Policies; use Separation_Kernel.Policies;
with Policy;                     use Policy;

package body Partition_Entry_Priv_Lockdown
is
    procedure Enforce_Priv_Lockdown (Partition_Policy : Policy.Policy)
    is

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register);
    begin
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Address,
                     Data => Partition_Policy.Priv_Lockdown);
    end Enforce_Priv_Lockdown;
end Partition_Entry_Priv_Lockdown;
