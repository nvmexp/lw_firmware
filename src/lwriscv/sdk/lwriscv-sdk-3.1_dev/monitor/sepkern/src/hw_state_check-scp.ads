--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with Error_Handling;           use Error_Handling;

package Hw_State_Check.SCP
is
    procedure Check_SCP (Err_Code : in out Error_Codes)
    with
        Pre    => Err_Code = OK,
        Global => (Input => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

end Hw_State_Check.SCP;
