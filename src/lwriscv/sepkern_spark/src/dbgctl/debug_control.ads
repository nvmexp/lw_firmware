-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;

package Debug_Control is

   procedure Configure_ICD with
     Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always;

end Debug_Control;
