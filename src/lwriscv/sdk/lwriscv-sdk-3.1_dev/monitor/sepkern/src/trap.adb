-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with SBI;
with Ada.Unchecked_Colwersion;
with Lw_Types.Shift_Left_Op;   use Lw_Types.Shift_Left_Op;
with Riscv_Core;               use Riscv_Core;
with Riscv_Core.Inst;          use Riscv_Core.Inst;
with Riscv_Core.Rv_Gpr;        use Riscv_Core.Rv_Gpr;
with Dev_Riscv_Csr_64;         use Dev_Riscv_Csr_64;
with Types;                    use Types;
with Error_Handling;           use Error_Handling;
with Trace_Buffer;             use Trace_Buffer;

package body Trap
is

    procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);
    procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);
    procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);

    procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);
    procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);

    procedure Handle_Timer_Interrupt
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        Inline_Always
    is
        -- Set corresponding bit of CSR
        procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MIP_Register);

        -- Clear corresponding bit of CSR
        procedure Csr_Reg_Clr64 is new Riscv_Core.Rv_Csr.Clr64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
    begin
        -- Set Supervisor timer interrupt pending bit
        Csr_Reg_Set64(Addr => LW_RISCV_CSR_MIP_Address,
                      Data => LW_RISCV_CSR_MIP_Register'(Wiri3 => LW_RISCV_CSR_MIP_WIRI3_RST,
                                                         Meip  => LW_RISCV_CSR_MIP_MEIP_RST,
                                                         Wiri2 => LW_RISCV_CSR_MIP_WIRI2_RST,
                                                         Seip  => LW_RISCV_CSR_MIP_SEIP_RST,
                                                         Ueip  => LW_RISCV_CSR_MIP_UEIP_RST,
                                                         Mtip  => LW_RISCV_CSR_MIP_MTIP_RST,
                                                         Wiri1 => LW_RISCV_CSR_MIP_WIRI1_RST,
                                                         Stip  => 1, -- Set
                                                         Utip  => LW_RISCV_CSR_MIP_UTIP_RST,
                                                         Msip  => LW_RISCV_CSR_MIP_MSIP_RST,
                                                         Wiri0 => LW_RISCV_CSR_MIP_WIRI0_RST,
                                                         Ssip  => LW_RISCV_CSR_MIP_SSIP_RST,
                                                         Usip  => LW_RISCV_CSR_MIP_USIP_RST));

        -- Disable interrupt for M-mode; Clear MTIE
        Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIE_Address,
                      Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => LW_RISCV_CSR_MIE_WPRI3_RST,
                                                         Meie  => LW_RISCV_CSR_MIE_MEIE_RST,
                                                         Wpri2 => LW_RISCV_CSR_MIE_WPRI2_RST,
                                                         Seie  => LW_RISCV_CSR_MIE_SEIE_RST,
                                                         Ueie  => LW_RISCV_CSR_MIE_UEIE_RST,
                                                         Mtie  => 1, -- Clear
                                                         Wpri1 => LW_RISCV_CSR_MIE_WPRI1_RST,
                                                         Stie  => LW_RISCV_CSR_MIE_STIE_RST,
                                                         Utie  => LW_RISCV_CSR_MIE_UTIE_RST,
                                                         Msie  => LW_RISCV_CSR_MIE_MSIE_RST,
                                                         Wpri0 => LW_RISCV_CSR_MIE_WPRI0_RST,
                                                         Ssie  => LW_RISCV_CSR_MIE_SSIE_RST,
                                                         Usie  => LW_RISCV_CSR_MIE_USIE_RST));
    end Handle_Timer_Interrupt;

    -- -----------------------------------------------------------------------------
    -- *** Porting the WAR from C SepKern
    -- -----------------------------------------------------------------------------
    procedure PassThroughEcall
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MTVAL_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE2_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_STVEC_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MSTATUS_Register);

        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_STVAL_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCAUSE_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCAUSE2_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SEPC_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSTATUS_Register);

        Mtval   : LW_RISCV_CSR_MTVAL_Register;
        Mcause  : LW_RISCV_CSR_MCAUSE_Register;
        Mcause2 : LW_RISCV_CSR_MCAUSE2_Register;
        Stvec   : LW_RISCV_CSR_STVEC_Register;
        Mepc    : LW_RISCV_CSR_MEPC_Register;
        Mstatus : LW_RISCV_CSR_MSTATUS_Register;
    begin

        -- // Copy trap information to S-mode
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MTVAL_Address,
                     Data => Mtval);
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_STVAL_Address,
                     Data => LW_RISCV_CSR_STVAL_Register'(Value => Mtval.Value));

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEPC_Address,
                     Data => Mepc);
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SEPC_Address,
                     Data => LW_RISCV_CSR_SEPC_Register'(Epc => Mepc.Epc));

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                     Data => Mcause);

        --// We cannot write EXCODE_SCALL to scause, so we must also fake it as an ECALL from U-mode to S-mode.
        Mcause.Excode := LW_RISCV_CSR_MCAUSE_EXCODE_UCALL;

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                     Data => Mcause);
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCAUSE_Address,
                     Data => LW_RISCV_CSR_SCAUSE_Register'(Int    => Mcause.Int,
                                                           Wlrl   => Mcause.Wlrl,
                                                           Excode => Mcause.Excode));

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE2_Address,
                     Data => Mcause2);
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCAUSE2_Address,
                     Data => LW_RISCV_CSR_SCAUSE2_Register'(Wpri  => Mcause2.Wpri,
                                                            Cause => Mcause2.Cause,
                                                            Error => Mcause2.Error));
        -- // Return to the S-mode trap vector
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_STVEC_Address,
                     Data => Stvec);
        -- It's ok just to shift left since MODE is tied off to 0x0 sinze we don't support vectored interrupts (RISCV_2_0_IAS - 6.1.2.2 stvec)
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEPC_Address,
                     Data => LW_RISCV_CSR_MEPC_Register'(Epc => LwU64 (Lw_Shift_Left (Value => LwU64 (Stvec.Base), Amount => 2))));

        -- // Simulate trap entry to S-mode:
        -- // 1) Clear MSTATUS.SIE
        -- // 2) Copy old MSTATUS.SIE to MSTATUS.SPIE
        -- // 3) Move MSTATUS.MPP to MSTATUS.SPP
        -- // 4) Set MSTATUS.MPP to S mode

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MSTATUS_Address,
                    Data => Mstatus);

        Mstatus.Spie := (if Mstatus.Sie = SIE_ENABLE then SPIE_ENABLE else SPIE_DISABLE);
        Mstatus.Sie  := SIE_DISABLE;
        Mstatus.Spp  := (if Mstatus.Mpp = MPP_USER then SPP_USER else SPP_SUPERVISOR);
        Mstatus.Mpp  := MPP_SUPERVISOR;

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSTATUS_Address,
                     Data => Mstatus);

    end PassThroughEcall;

    -- -----------------------------------------------------------------------------
    -- *** FMC/SK - SK entry point. Handler for Non-delegated interrupts/exceptions
    -- -----------------------------------------------------------------------------
    procedure Trap_Handler
    is
        Mcause : LW_RISCV_CSR_MCAUSE_Register;
        Mepc   : LW_RISCV_CSR_MEPC_Register;
        p      : Pointer_Registers_Array;
        t      : Temp_Registers_Array;
        a      : Argument_Registers_Array;
        s      : Saved_Registers_Array;

        -- TODO: Hack to allow M-mode to redirect syscalls back to S-mode
        -- This is still used by PMU. Sync with PMU team and only remove
        -- after the fix has been established in PMU first
        LW_SYSCALL_FLAG_SMODE : constant := 16#8000_0000_0000_0000#;
    begin

        Riscv_Core.Ghost_Switch_To_SK_Stack; -- Just writting to Ghost var; Actually done in 'trapEntry'

        Riscv_Core.Rv_Gpr.Save_Pointer_Registers_To(p);
        Riscv_Core.Rv_Gpr.Save_Temp_Registers_To(t);
        Riscv_Core.Rv_Gpr.Save_Argument_Registers_To(a);

        Disable_Trace_Buffer;

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                     Data => Mcause);

        -- All interrupts/exceptions are delegated to the partition except ECALL for SBI implementation and Timer interrupt
        case Mcause.Int is
        when 0 =>
            if Mcause.Excode = LW_RISCV_CSR_MCAUSE_EXCODE_SCALL and then a(7) >= LW_SYSCALL_FLAG_SMODE
            then
                PassThroughEcall;

            elsif Mcause.Excode = LW_RISCV_CSR_MCAUSE_EXCODE_SCALL
            then
                Riscv_Core.Rv_Gpr.Save_Saved_Registers_To(s);

                -- Skip 'ecall' instruction
                Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEPC_Address,
                             Data => Mepc);

                -- In case of Switch_To SBI this would be ok, but we fail all such SBIs as this isn't a realistic valid use-case.
                if Mepc.Epc > (LwU64'Last - 4) then
                    Error_Handling.Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                                        Err_Code => ILWALID_MEPC);
                end if;

                Mepc.Epc := Mepc.Epc + 4;
                Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEPC_Address,
                             Data => Mepc);

                SBI.Dispatch(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7));
                Riscv_Core.Rv_Gpr.Restore_Saved_Registers_From(s);
            else
                -- Will Halt the core in this case. No need to restore GPRs
                Error_Handling.Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                                    Err_Code => UNEXPECTED_EXCEPTION_CAUSE);

            end if;

        when 1 =>
            if Mcause.Excode /= LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT then
                Error_Handling.Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                                    Err_Code => UNEXPECTED_INTERRUPT_CAUSE);
            end if;

            Handle_Timer_Interrupt;

        end case;

        Restore_Trace_Buffer;

        Riscv_Core.Rv_Gpr.Restore_Pointer_Registers_From(p);
        Riscv_Core.Rv_Gpr.Restore_Temp_Registers_From(t);
        Riscv_Core.Rv_Gpr.Restore_Argument_Registers_From(a);

        Riscv_Core.Rv_Gpr.Switch_To_Old_Stack(Load_Old_SP_Value_From => LwU32(LW_RISCV_CSR_MSCRATCH_Address));

        Riscv_Core.Inst.Mret;

    end Trap_Handler;

    function Get_Address_Of_Trap_Entry return LwU64
    with
        SPARK_Mode => Off
    is
        Trap_Entry : LwU64
        with
            Import        => True,
            Convention    => C,
            External_Name => "trap_entry";

        Trap_Entry_Address : aliased constant System.Address := Trap_Entry'Address;

        -- System.Address to LwU64 colwertor function
        function System_Address_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => System.Address,
            Target => LwU64)
        with
            Inline_Always;
    begin
        return System_Address_To_LwU64(Trap_Entry_Address);
    end Get_Address_Of_Trap_Entry;

end Trap;
