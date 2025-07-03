-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;       use Riscv_Core;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Policy_External;  use Policy_External;

package body Mpu_Control
is

    procedure Program_Mmpu_Ctl(Plcy_Mpu_Ctl : Plcy_Mpu_Control)
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MMPUCTL_Register);
    begin
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MMPUCTL_Address,
                     Data => Plcy_Mpu_Control_to_LW_RISCV_CSR_MMPUCTL_Register(Plcy_Mpu_Ctl));
    end Program_Mmpu_Ctl;

end Mpu_Control;
