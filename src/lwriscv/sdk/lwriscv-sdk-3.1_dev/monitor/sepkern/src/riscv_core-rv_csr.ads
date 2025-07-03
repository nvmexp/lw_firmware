-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

package Riscv_Core.Rv_Csr
is

    -- Csr_Reg_Hw_Model
    package Csr_Reg_Hw_Model
    with
        Abstract_State =>
            (Csr_State with External => (Async_Readers, Effective_Writes))
    is
    end Csr_Reg_Hw_Model;
    -----------------------------------------------------------------------

    generic
        type Generic_Csr is private;
    procedure Rd64_Generic (Addr : Csr_Addr; Data : out Generic_Csr)
    with
        Global => (Input => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

    generic
        type Generic_Csr is private;
    procedure Wr64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    with
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

    -- Set corresponding bit of CSR
    generic
        type Generic_Csr is private;
    procedure Set64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    with
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

    -- Clear corresponding bit of CSR
    generic
        type Generic_Csr is private;
    procedure Clr64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    with
        Global => (Output => Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

end Riscv_Core.Rv_Csr;