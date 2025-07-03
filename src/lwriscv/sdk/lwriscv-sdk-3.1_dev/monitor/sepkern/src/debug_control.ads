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
with Policy_Types;       use Policy_Types;

package Debug_Control
is

    procedure Configure_ICD
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    procedure Program_Debug_Control(Debug_Control : Plcy_Debug_Control)
    with
        Global => (Output => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

end Debug_Control;
