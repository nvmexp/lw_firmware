--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;

package Trace_Buffer
    with Abstract_State => Trace_Buffer_State
is
    procedure Disable_Trace_Buffer
    with
        Global => (Output => Trace_Buffer_State,
                   In_Out => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

    procedure Restore_Trace_Buffer
    with
        Global => (Input  => Trace_Buffer_State,
                   Output => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

end Trace_Buffer;
