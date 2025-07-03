-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

package Riscv_Core.Csb_Reg
is
    -- Csb_Reg_Hw_Model package
    package Csb_Reg_Hw_Model
    with
        Initializes    => Mmio_State,
        Abstract_State =>
            (Mmio_State with External => (Async_Readers, Effective_Writes))
    is
    end Csb_Reg_Hw_Model;
    -----------------------------------------------------------------------

    generic
        type Generic_Register is private;
    procedure Rd32_Generic (Addr : LwU32; Data : out Generic_Register)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Input => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    generic
        type Generic_Register is private;
    procedure Wr32_Generic (Addr : LwU32; Data : Generic_Register)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Output => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Input => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Output => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

end Riscv_Core.Csb_Reg;