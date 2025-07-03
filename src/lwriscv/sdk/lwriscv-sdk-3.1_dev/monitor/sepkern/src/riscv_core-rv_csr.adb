-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Riscv_Core.Inst;

package body Riscv_Core.Rv_Csr
is

    -- Csr_Reg_Hw_Model
    package body Csr_Reg_Hw_Model
    with
        Refined_State => (Csr_State => null),
        SPARK_Mode    => On
    is
    end Csr_Reg_Hw_Model;
    -----------------------------------------------------------------------

    procedure Rd64_Generic (Addr : Csr_Addr; Data : out Generic_Csr)
    is
        function Colwert_To_Csr is new Ada.Unchecked_Colwersion
            (Source => LwU64,
             Target => Generic_Csr);

        Val : LwU64;
    begin
        Inst.Csrr (Addr => Addr, Rd => Val);
        Data := Colwert_To_Csr (Val);
    end Rd64_Generic;

    procedure Wr64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    is
        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => Generic_Csr,
             Target => LwU64);

        Val : constant LwU64 := Colwert_To_LwU64 (Data);
    begin
        Inst.Csrw (Addr => Addr, Rs => Val);
    end Wr64_Generic;

    procedure Set64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    is
        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => Generic_Csr,
             Target => LwU64);

        Val : constant LwU64 := Colwert_To_LwU64 (Data);
    begin
        Inst.Csrs (Addr => Addr, Rs => Val);

    end Set64_Generic;

    procedure Clr64_Generic (Addr : Csr_Addr; Data : Generic_Csr)
    is
        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => Generic_Csr,
             Target => LwU64);

        Val : constant LwU64 := Colwert_To_LwU64 (Data);
    begin
        Inst.Csrc (Addr => Addr, Rs => Val);
    end Clr64_Generic;

end Riscv_Core.Rv_Csr;