-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with System.Machine_Code; use System.Machine_Code;
with Riscv_Core.Inst;

package body Riscv_Core.Csb_Reg
is

    -- Csb_Reg_Hw_Model
    package body Csb_Reg_Hw_Model
    with
        Refined_State => (Mmio_State => null),
        SPARK_Mode    => On
    is
    end Csb_Reg_Hw_Model;
    -----------------------------------------------------------------------

    procedure Rd32_Addr32_Mmio (Addr : LwU32; Data : out LwU32)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Input => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Wr32_Addr32_Mmio (Addr : LwU32; Data : LwU32)
    with
        Pre => (Addr mod 4) = 0,
        Global => (Output => Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Rd32_Generic (Addr : LwU32; Data : out Generic_Register)
    is
        function Colwert_To_Reg is new Ada.Unchecked_Colwersion
            (Source => LwU32,
                Target => Generic_Register);

        Value : LwU32;
    begin
        Rd32_Addr32_Mmio (Addr => Addr, Data => Value);
        Data := Colwert_To_Reg (Value);
    end Rd32_Generic;

    procedure Wr32_Generic (Addr : LwU32; Data : Generic_Register)
    is
        function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion
            (Source => Generic_Register,
                Target => LwU32);

        Value : constant LwU32 := Colwert_To_LwU32 (Data);
    begin
        Wr32_Addr32_Mmio (Addr => Addr, Data => Value);
    end Wr32_Generic;

    procedure Rd32_Addr32_Mmio (Addr : LwU32; Data : out LwU32)
    with
        SPARK_Mode => Off
    is
        Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
    begin
        Data := Reg;
    end Rd32_Addr32_Mmio;

    procedure Wr32_Addr32_Mmio (Addr : LwU32; Data : LwU32)
        with SPARK_Mode => Off
    is
        Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
    begin
        Reg := Data;
    end Wr32_Addr32_Mmio;

    procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32 )
    with
        SPARK_Mode => Off
    is
        Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
    begin
        Data := Reg;
    end Rd32_Addr64_Mmio;

    procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32)
    with
        SPARK_Mode => Off
    is
        Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
    begin
        Reg := Data;
        Inst.Fence_Io;
    end Wr32_Addr64_Mmio_Safe;

end Riscv_Core.Csb_Reg;