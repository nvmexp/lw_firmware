-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
-- 
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

with Ada.Unchecked_Colwersion;
with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;
with Lw_Ref_Dev_Riscv_Csr_64; use Lw_Ref_Dev_Riscv_Csr_64;
with System.Machine_Code; use System.Machine_Code;

package body Rv_Brom_Riscv_Core is
    package body Csb_Reg is

        package body Csb_Reg_Wr32_Hw_Model with
        Refined_State => (Mmio_State => null),
            SPARK_Mode    => On
        is
            procedure Write is null;
        end Csb_Reg_Wr32_Hw_Model;


        procedure Rd32_Addr32_Mmio (Addr : LwU32; Data : out LwU32) with
            Pre => (Addr mod 4) = 0,
            Inline_Always;

        procedure Rd32_Generic (Addr : LwU32; Data : out Generic_Register) is
            function Colwert_To_Reg is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                                     Target => Generic_Register);
            Value : LwU32;
        begin
            --vcast_dont_instrument_start
            Rd32_Addr32_Mmio (Addr => Addr, Data => Value);
            Data := Colwert_To_Reg (Value);
            --vcast_dont_instrument_end
        end Rd32_Generic;

        procedure Wr32_Generic (Addr : LwU32; Data : Generic_Register) is
            function Colwert_To_LwU32 is new Ada.Unchecked_Colwersion (Source => Generic_Register,
                                                                       Target => LwU32);
            Value : constant LwU32 := Colwert_To_LwU32 (Data);
        begin
            --vcast_dont_instrument_start
            Csb_Reg_Wr32_Hw_Model.Write;
            Wr32_Addr32_Mmio (Addr => Addr, Data => Value);
            --vcast_dont_instrument_end
        end Wr32_Generic;

        procedure Rd32_Addr32_Mmio (Addr : LwU32; Data : out LwU32)
            with SPARK_Mode => Off -- SPARK limitation: Can not use volatile on stack
        is
            Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
        begin
            --vcast_dont_instrument_start
            Data := Reg;
            --vcast_dont_instrument_end
        end Rd32_Addr32_Mmio;

        procedure Wr32_Addr32_Mmio (Addr : LwU32; Data : LwU32)
            with SPARK_Mode => Off -- SPARK limitation: Can not use volatile on stack
        is
            Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
        begin
            --vcast_dont_instrument_start
            Reg := Data;
            --vcast_dont_instrument_end
        end Wr32_Addr32_Mmio;

        procedure Rd32_Addr64_Mmio (Addr : LwU64; Data : out LwU32 )
            with SPARK_Mode => Off -- SPARK limitation: Can not use volatile on stack
        is
            Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
        begin
            Data := Reg;
        end Rd32_Addr64_Mmio;

        procedure Wr32_Addr64_Mmio_Safe (Addr : LwU64; Data : LwU32)
            with SPARK_Mode => Off -- SPARK limitation: Can not use volatile on stack
        is
            Reg : LwU32 with Address => System'To_Address (Addr), Volatile;
        begin
            Reg := Data;
            Inst.Fence_Io;
        end Wr32_Addr64_Mmio_Safe;

    end Csb_Reg;

    package body Rv_Csr is
        package body Csr_Reg_Wr64_Hw_Model with
        Refined_State => (Csr_State => null),
            SPARK_Mode    => On
        is
            procedure Write is null;
        end Csr_Reg_Wr64_Hw_Model;


        procedure Rd64_Generic (Addr : LwU32; Data : out Generic_Csr) is
            function Colwert_To_Csr is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                                     Target => Generic_Csr);
            Val : LwU64;
        begin
            --vcast_dont_instrument_start
            Inst.Csrr (Addr => Addr, Rd => Val);
            Data := Colwert_To_Csr (Val);
            --vcast_dont_instrument_end
        end Rd64_Generic;

        procedure Wr64_Generic (Addr : LwU32; Data : Generic_Csr) is
            function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => Generic_Csr,
                                                                       Target => LwU64);
            Val : constant LwU64 := Colwert_To_LwU64 (Data);
        begin
            --vcast_dont_instrument_start
            Csr_Reg_Wr64_Hw_Model.Write;
            Inst.Csrw (Addr => Addr, Rs => Val);
            --vcast_dont_instrument_end
        end Wr64_Generic;

        procedure Set64_Generic (Addr : LwU32; Data : Generic_Csr) is
            function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => Generic_Csr,
                                                                       Target => LwU64);
            Val : constant LwU64 := Colwert_To_LwU64 (Data);
        begin
            --vcast_dont_instrument_start
            Csr_Reg_Wr64_Hw_Model.Write;
            Inst.Csrs (Addr => Addr, Rs => Val);
            --vcast_dont_instrument_end

        end Set64_Generic;

        procedure Clr64_Generic (Addr : LwU32; Data : Generic_Csr) is
            function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => Generic_Csr,
                                                                       Target => LwU64);
            Val : constant LwU64 := Colwert_To_LwU64 (Data);
        begin
            --vcast_dont_instrument_start
            Csr_Reg_Wr64_Hw_Model.Write;
            Inst.Csrc (Addr => Addr, Rs => Val);
            --vcast_dont_instrument_end
        end Clr64_Generic;
    end Rv_Csr;

    procedure Csb_Reg_Rd32 is new Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_HWCFG2_Register);
    procedure Csb_Reg_Wr32 is new Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_Register);

    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);


    -- Check if the PL3 priviledge for BROM is enabled or not
    -- Note: the Riscv_Pl3_Disable is controlled by fuse.
    function Is_Pl3_Enabled return HS_BOOL with Inline_Always;

    function Is_Pl3_Enabled return HS_BOOL is
        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_HWCFG2_Address,
                      Data => Hwcfg2);
        return (if Hwcfg2.Riscv_Pl3_Disable = DISABLE_FALSE then HS_TRUE else HS_FALSE);
    end Is_Pl3_Enabled;

    procedure Lock_Priv_Level_And_Selwre_Mask (Selwre_Mode : HS_BOOL;
                                               Priv_Level  : LwU4) is
        Msecm : LW_RISCV_CSR_MSPM_MSECM_Field := (if Selwre_Mode = HS_TRUE then MSECM_SEC else MSECM_INSEC);
    begin
        if Is_Pl3_Enabled = HS_FALSE then
            Msecm := MSECM_INSEC;
        end if;

        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MSPM_Address,
                       Data => LW_RISCV_CSR_MSPM_Register' (Wpri3  => LW_RISCV_CSR_MSPM_WPRI3_RST,
                                                            Mmlock => MMLOCK_LOCK,
                                                            Wpri2  => LW_RISCV_CSR_MSPM_WPRI2_RST,
                                                            Msecm  => Msecm,
                                                            Wpri1  => LW_RISCV_CSR_MSPM_WPRI1_RST,
                                                            Ssecm  => SSECM_INSEC,
                                                            Usecm  => USECM_INSEC,
                                                            Mplm   => Priv_Level,
                                                            Wpri0  => LW_RISCV_CSR_MSPM_WPRI0_RST,
                                                            Splm   => LW_RISCV_CSR_MSPM_SPLM_LEVEL0,
                                                            Uplm   => LW_RISCV_CSR_MSPM_UPLM_LEVEL0));
    end Lock_Priv_Level_And_Selwre_Mask;

    procedure Change_Priv_Level_And_Selwre_Status (Selwre_Mode : HS_BOOL;
                                                   Priv_Level3 : HS_BOOL) is
        Requested_Priv_Lvl : LW_RISCV_CSR_MRSP_MRPL_Field;
    begin
        -- If any one of the parameters is TRUE
        if Selwre_Mode = HS_TRUE or else Priv_Level3 = HS_TRUE then
            -- 1. Write to MSPM to request the permission for using priv level1, level 2 and 3 and secure status
            Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MSPM_Address,
                          Data => LW_RISCV_CSR_MSPM_Register'(Wpri3     => 0,
                                                              Mmlock    => MMLOCK_UNLOCK,
                                                              Wpri2     => 0,
                                                              Msecm     => (if Selwre_Mode = HS_TRUE then MSECM_SEC else MSECM_INSEC),
                                                              Wpri1     => 0,
                                                              Ssecm     => SSECM_INSEC,
                                                              Usecm     => USECM_INSEC,
                                                              Mplm      => (if Priv_Level3 = HS_TRUE then LW_RISCV_CSR_MSPM_MPLM_LEVEL3 -- all levels
                                                                            else LW_RISCV_CSR_MSPM_MPLM_LEVEL0),
                                                              Wpri0     => 0,
                                                              Splm      => 0,
                                                              Uplm      => 0));


            -- 2. Write MRSP to set the priv level and secure

            if Priv_Level3 = HS_TRUE then
                -- Lets check if using PL3 is allowed
                if Is_Pl3_Enabled = HS_TRUE then
                    Requested_Priv_Lvl := MRPL_LEVEL3;
                else
                    Requested_Priv_Lvl := MRPL_LEVEL2;
                end if;
            else
                Requested_Priv_Lvl := MRPL_LEVEL0;
            end if;

            Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MRSP_Address,
                          Data => LW_RISCV_CSR_MRSP_Register'(Wpri2 => 0,
                                                              Mrsec => (if Selwre_Mode = HS_TRUE then MRSEC_SEC else MRSEC_INSEC),
                                                              Wpri1 => 0,
                                                              Srsec => SRSEC_INSEC,
                                                              Ursec => URSEC_INSEC,
                                                              Mrpl  => Requested_Priv_Lvl,
                                                              Wpri0 => 0,
                                                              Srpl  => SRPL_LEVEL0,
                                                              Urpl  => URPL_LEVEL0));
        else
            -- Both parameters are False, we will set the LW_RISCV_CSR_MSPM and LW_RISCV_CSR_MRSP_Address
            -- Write to MSPM to clear the permission, which will in turn clear the priv level and secure status in MRSP
            Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MSPM_Address,
                          Data => LW_RISCV_CSR_MSPM_Register'(Wpri3     => 0,
                                                              Mmlock    => MMLOCK_UNLOCK,
                                                              Wpri2     => 0,
                                                              Msecm     => MSECM_INSEC,
                                                              Wpri1     => 0,
                                                              Ssecm     => SSECM_INSEC,
                                                              Usecm     => USECM_INSEC,
                                                              Mplm      => LW_RISCV_CSR_MSPM_MPLM_LEVEL0, -- all levels
                                                              Wpri0     => 0,
                                                              Splm      => 0,
                                                              Uplm      => 0));
        end if;
    end Change_Priv_Level_And_Selwre_Status;

    procedure Lock_Machine_Mask is
    begin
        Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MSPM_Address,
                      Data => LW_RISCV_CSR_MSPM_Register'(Wpri3     => 0,
                                                          Mmlock    => MMLOCK_LOCK, -- lock
                                                          Wpri2     => 0,
                                                          Msecm     => MSECM_INSEC,
                                                          Wpri1     => 0,
                                                          Ssecm     => SSECM_INSEC,
                                                          Usecm     => USECM_INSEC,
                                                          Mplm      => 0,
                                                          Wpri0     => 0,
                                                          Splm      => 0,
                                                          Uplm      => 0));
    end Lock_Machine_Mask;

    procedure Halt is
    begin
        --vcast_dont_instrument_start
        loop
            Inst.Csrwi (Addr => LW_RISCV_CSR_MOPT_Address,
                        Imm  => LW_RISCV_CSR_MOPT_CMD_HALT);
        end loop;
        --vcast_dont_instrument_end
    end Halt;

    procedure Setup_Trap_Handler (Addr : LwU64) is
    begin
        --vcast_dont_instrument_start
        Inst.Csrw (Addr => LW_RISCV_CSR_MTVEC_Address,
                   Rs   => Addr);
        --vcast_dont_instrument_end
    end Setup_Trap_Handler;

    procedure Reset_Csrs is
    begin
        --vcast_dont_instrument_start
        -- Reset Mcycle
        Inst.Csrwi (Addr => LW_RISCV_CSR_MHPMCOUNTER_0_Address,
                    Imm  => 0);
        -- Reset Minstret
        Inst.Csrwi (Addr => LW_RISCV_CSR_MHPMCOUNTER_2_Address,
                    Imm  => 0);
        -- Reset MBPCFG
         Inst.Csrwi (Addr => LW_RISCV_CSR_MBPCFG_Address,
                     Imm  => 0);
        --vcast_dont_instrument_end
    end Reset_Csrs;

    procedure Clear_Gpr is
    begin
        --vcast_dont_instrument_start
        Asm ("li x1, 0", Volatile => True);
        Asm ("li x2, 0", Volatile => True);
        Asm ("li x3, 0", Volatile => True);
        Asm ("li x4, 0", Volatile => True);
        Asm ("li x5, 0", Volatile => True);
        Asm ("li x6, 0", Volatile => True);
        Asm ("li x7, 0", Volatile => True);
        Asm ("li x8, 0", Volatile => True);
        Asm ("li x9, 0", Volatile => True);
        Asm ("li x10, 0", Volatile => True);
        Asm ("li x11, 0", Volatile => True);
        Asm ("li x12, 0", Volatile => True);
        Asm ("li x13, 0", Volatile => True);
        Asm ("li x14, 0", Volatile => True);
        Asm ("li x15, 0", Volatile => True);
        Asm ("li x16, 0", Volatile => True);
        Asm ("li x17, 0", Volatile => True);
        Asm ("li x18, 0", Volatile => True);
        Asm ("li x19, 0", Volatile => True);
        Asm ("li x20, 0", Volatile => True);
        Asm ("li x21, 0", Volatile => True);
        Asm ("li x22, 0", Volatile => True);
        Asm ("li x23, 0", Volatile => True);
        Asm ("li x24, 0", Volatile => True);
        Asm ("li x25, 0", Volatile => True);
        Asm ("li x26, 0", Volatile => True);
        Asm ("li x27, 0", Volatile => True);
        Asm ("li x28, 0", Volatile => True);
        Asm ("li x29, 0", Volatile => True);
        Asm ("li x30, 0", Volatile => True);
        Asm ("li x31, 0", Volatile => True);
        --vcast_dont_instrument_end
    end Clear_Gpr;

    procedure Set_Mepc (Addr : LwU64) is
    begin
        Inst.Csrw (Addr => LW_RISCV_CSR_MEPC_Address,
                   Rs   => Addr);
    end Set_Mepc;

    procedure Remove_Pl0_Access_For_Cpuctl is
        type Write_Protection_Field is record
            Level0 : LwU1;
            Level1 : LwU1;
            Level2 : LwU1;
            Level3 : LwU1;
        end record with Size => 4;

        for Write_Protection_Field use record
            Level0 at 0 range  0 .. 0;
            Level1 at 0 range  1 .. 1;
            Level2 at 0 range  2 .. 2;
            Level3 at 0 range  3 .. 3;
        end record;
        function Write_Prot_To_LwU4 is new Ada.Unchecked_Colwersion (Source => Write_Protection_Field,
                                                                     Target => LwU4);
        Write_Prot : constant Write_Protection_Field := ( Level0 => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE,
                                                          Level1 => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL1_ENABLE,
                                                          Level2 => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_ENABLE,
                                                          Level3 => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL3_ENABLE);

    begin
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_Address,
                       Data => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_Register'(
                           Read_Protection      => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
                           Write_Protection     => Write_Prot_To_LwU4 (Write_Prot),
                           Read_Violation       => VIOLATION_REPORT_ERROR,
                           Write_Violation      => VIOLATION_REPORT_ERROR,
                           Source_Read_Control  => CONTROL_BLOCKED,
                           Source_Write_Control => CONTROL_BLOCKED,
                           Source_Enable        => LW_PRGNLCL_FALCON_CPUCTL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED));
    end Remove_Pl0_Access_For_Cpuctl;


    procedure Scrub_Imem_Block (Start : Imem_Offset_Byte; Size : Imem_Size_Byte) is
        Dw64_Size : constant Imem_Size_Byte := Size / 8;
    begin
        --vcast_dont_instrument_start
        pragma Assert (Dw64_Size mod 8 = 0);
        --vcast_dont_instrument_end

        if Dw64_Size = 0 then
            return;
        end if;

        declare
            type Memory is array (Imem_Size_Byte range 1 .. Dw64_Size) of LwU64;
            Mem : Memory with Address => Imem_Offset_To_Address (Start);
            Pos : Imem_Size_Byte := Mem'First;
            Num : Imem_Size_Byte := Mem'Last;
        begin
            while Num >= 8 loop
                --vcast_dont_instrument_start
                pragma Loop_Ilwariant ((Num mod 8 = 0) and then ((Pos - 1) mod 8 = 0) and then Pos in Mem'Range);
                --vcast_dont_instrument_end

                Mem (Pos) := 0;
                Mem (Pos + 1) := 0;
                Mem (Pos + 2) := 0;
                Mem (Pos + 3) := 0;
                Mem (Pos + 4) := 0;
                Mem (Pos + 5) := 0;
                Mem (Pos + 6) := 0;
                Mem (Pos + 7) := 0;
                exit when Pos + 7 = Mem'Last;
                Num := Num - 8;
                Pos := Pos + 8;
            end loop;
        end;
    end Scrub_Imem_Block;

    procedure Scrub_Dmem_Block (Start : Dmem_Offset_Byte; Size : Dmem_Size_Byte) is
        Dw64_Size : constant Dmem_Size_Byte := Size / 8;
    begin
        --vcast_dont_instrument_start
        pragma Assert (Dw64_Size mod 8 = 0);
        if Dw64_Size = 0 then
            return;
        end if;

        declare
            type Memory is array (Dmem_Size_Byte range 1 .. Dw64_Size) of LwU64;
            Mem : Memory with Address => Dmem_Offset_To_Address (Start);
            Pos : Dmem_Size_Byte := Mem'First;
            Num : Dmem_Size_Byte := Mem'Last;
        begin
            -- 64 bytes per loop
            -- Reading the generated ASM, the following way is very inefficient.
--              loop
--                  pragma Loop_Ilwariant ((Num mod 8 = 0) and then ((Pos - 1) mod 8 = 0) and then Pos in Mem'Range);
--                  Mem (Pos) := 0;
--                  Mem (Pos + 1) := 0;
--                  Mem (Pos + 2) := 0;
--                  Mem (Pos + 3) := 0;
--                  Mem (Pos + 4) := 0;
--                  Mem (Pos + 5) := 0;
--                  Mem (Pos + 6) := 0;
--                  Mem (Pos + 7) := 0;
--                  exit when Pos + 7 = Mem'Last;
--                  Pos := Pos + 8;
--              end loop;

            while Num >= 8 loop
                pragma Loop_Ilwariant ((Num mod 8 = 0) and then ((Pos - 1) mod 8 = 0) and then Pos in Mem'Range);
                Mem (Pos) := 0;
                Mem (Pos + 1) := 0;
                Mem (Pos + 2) := 0;
                Mem (Pos + 3) := 0;
                Mem (Pos + 4) := 0;
                Mem (Pos + 5) := 0;
                Mem (Pos + 6) := 0;
                Mem (Pos + 7) := 0;
                exit when Pos + 7 = Mem'Last;
                Num := Num - 8;
                Pos := Pos + 8;
            end loop;
        end;
        --vcast_dont_instrument_end
    end Scrub_Dmem_Block;

    package body Inst is

        procedure Csrrw (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrw %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs1)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrrw;

        procedure Csrrs (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
        --vcast_dont_instrument_start
            Asm ("csrrs %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs1)),
                 Volatile => True);
        --vcast_dont_instrument_end
        end Csrrs;

        procedure Csrrc (Addr : LwU32; Rs1 : LwU64; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrc %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs1)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrrc;

        procedure Csrrwi (Addr : LwU32; Imm : LwU5; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrwi %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrrwi;

        procedure Csrrsi (Addr : LwU32; Imm : LwU5; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrsi %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrrsi;

        procedure Csrrci (Addr : LwU32; Imm : LwU5; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrci %0, %1, %2",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrrci;

        procedure Csrr (Addr : LwU32; Rd : out LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrr %0, %1",
                 Outputs  => LwU64'Asm_Output ("=r", Rd),
                 Inputs   => LwU32'Asm_Input ("i", Addr),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrr;

        procedure Csrw (Addr : LwU32; Rs : LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrw %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrw;

        procedure Csrs (Addr : LwU32; Rs : LwU64)
            with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrs %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrs;

        procedure Csrc (Addr : LwU32; Rs : LwU64) with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrc %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU64'Asm_Input ("r", Rs)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrc;

        procedure Csrwi (Addr : LwU32; Imm : LwU5) with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrwi %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrwi;

        procedure Csrsi (Addr : LwU32; Imm : LwU5) with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrsi %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrsi;

        procedure Csrci (Addr : LwU32; Imm : LwU5) with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrci %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", Addr),
                              LwU5'Asm_Input ("i", Imm)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Csrci;

        procedure Ecall with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("ecall", Volatile => True);
            --vcast_dont_instrument_end
        end Ecall;

        procedure Nop with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("nop", Volatile => True);
            --vcast_dont_instrument_end
        end Nop;

        procedure Fence_Io with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("fence io,io", Volatile => True);
            --vcast_dont_instrument_end
        end Fence_Io;

        -- light weight fence
        procedure Lwfence_All with SPARK_Mode => Off -- Turning off SPARK_Mode on asm wrapper
        is
        begin
            --vcast_dont_instrument_start
            Asm ("csrrw x0, %0, %1",
                 Inputs   => (LwU32'Asm_Input ("i", LW_RISCV_CSR_LWFENCEALL_Address),
                              LwU64'Asm_Input ("r", 0)),
                 Volatile => True);
            --vcast_dont_instrument_end
        end Lwfence_All;
    end Inst;

end Rv_Brom_Riscv_Core;

