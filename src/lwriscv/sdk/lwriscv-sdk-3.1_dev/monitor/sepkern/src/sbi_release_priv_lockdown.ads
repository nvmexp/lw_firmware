--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with SBI_Types;         use SBI_Types;

package SBI_Release_Priv_Lockdown
is

    procedure Release_Priv_Lockdown (SBI_RC : out SBI_Return_Type)
    with
        Global => (Output => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

end SBI_Release_Priv_Lockdown;
