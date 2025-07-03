-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System.Machine_Code; use System.Machine_Code;
with Dev_Riscv_Csr_64;    use Dev_Riscv_Csr_64;

package body Riscv_Core.Inst
is

    procedure Csrrw (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrw %0, %1, %2",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
        Volatile => True);
    end Csrrw;

    procedure Csrrs (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrs %0, %1, %2",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
        Volatile => True);
    end Csrrs;

    procedure Csrrc (Addr : Csr_Addr; Rs1 : LwU64; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrc %0, %1, %2",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs1)),
        Volatile => True);
    end Csrrc;

    procedure Csrrwi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrwi %0, %1, %2", Outputs => LwU64'Asm_Output ("=r", Rd),
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile          => True);
    end Csrrwi;

    procedure Csrrsi (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrsi %0, %1, %2",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile => True);
    end Csrrsi;

    procedure Csrrci (Addr : Csr_Addr; Imm : LwU5; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrrci %0, %1, %2",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile => True);
    end Csrrci;

    procedure Csrr (Addr : Csr_Addr; Rd : out LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrr %0, %1",
            Outputs => LwU64'Asm_Output ("=r", Rd),
            Inputs => Csr_Addr'Asm_Input ("i", Addr),
        Volatile => True);
    end Csrr;

    procedure Csrw (Addr : Csr_Addr; Rs : LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrw %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
        Volatile => True);
    end Csrw;

    procedure Csrs (Addr : Csr_Addr; Rs : LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrs %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
        Volatile => True);
    end Csrs;

    procedure Csrc (Addr : Csr_Addr; Rs : LwU64)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrc %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU64'Asm_Input ("r", Rs)),
        Volatile => True);
    end Csrc;

    procedure Csrwi (Addr : Csr_Addr; Imm : LwU5)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrwi %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile => True);
    end Csrwi;

    procedure Csrsi (Addr : Csr_Addr; Imm : LwU5)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrsi %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile => True);
    end Csrsi;

    procedure Csrci (Addr : Csr_Addr; Imm : LwU5)
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm
        ("csrci %0, %1",
        Inputs => (Csr_Addr'Asm_Input ("i", Addr), LwU5'Asm_Input ("i", Imm)),
        Volatile => True);
    end Csrci;

    procedure Mret
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        loop
            Asm ("mret", Volatile => True);
        end loop;
    end Mret;

    procedure Nop
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm ("nop", Volatile => True);
    end Nop;

    procedure Fence_Io
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm ("fence io,io", Volatile => True);
    end Fence_Io;

    procedure Fence_All
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm ("fence iorw,iorw", Volatile => True);
    end Fence_All;

    procedure Fence_VMA
    with
        SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
    is
    begin
        Asm ("sfence.vma", Volatile => True);
    end Fence_VMA;

    procedure Halt
    with
        SPARK_Mode => Off
    is
    begin
        loop
            Inst.Csrwi
                (Addr => LW_RISCV_CSR_MOPT_Address,
                Imm  => LW_RISCV_CSR_MOPT_CMD_HALT);
        end loop;
    end Halt;

end Riscv_Core.Inst;
