-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;

package SBI_Shutdown
is

    procedure Shutdown
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        Inline_Always,
        No_Return;

end SBI_Shutdown;
