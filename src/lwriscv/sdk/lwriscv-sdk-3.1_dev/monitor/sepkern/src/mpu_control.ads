-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Rv_Csr;
with Mpu_Policy_Types; use Mpu_Policy_Types;

package Mpu_Control
is

    procedure Program_Mmpu_Ctl(Plcy_Mpu_Ctl : Plcy_Mpu_Control)
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

end Mpu_Control;
