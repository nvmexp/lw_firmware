-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core; use Riscv_Core;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Types; use Types;
with SBI;
with Error_Handling; use Error_Handling;
with System;
with Ada.Unchecked_Colwersion;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;
with Typecast; use Typecast;

package body Trap is

   procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);
   procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);
   procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);

   procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);
   procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);

   procedure HandleTimerInterrupt with 
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
                    Data => LW_RISCV_CSR_MIP_Register'(Wiri3 => 0,
                                                       Meip  => 0,
                                                       Wiri2 => 0,
                                                       Seip  => 0,
                                                       Ueip  => 0,
                                                       Mtip  => 0,
                                                       Wiri1 => 0,
                                                       Stip  => 1, -- Set
                                                       Utip  => 0,
                                                       Msip  => 0,
                                                       Wiri0 => 0,
                                                       Ssip  => 0,
                                                       Usip  => 0));

      -- Disable interrupt for M-mode; Clear MTIE
      Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIE_Address,
                    Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => 0,
                                                       Meie  => 0,
                                                       Wpri2 => 0,
                                                       Seie  => 0,
                                                       Ueie  => 0,
                                                       Mtie  => 1, -- Clear
                                                       Wpri1 => 0,
                                                       Stie  => 0,
                                                       Utie  => 0,
                                                       Msie  => 0,
                                                       Wpri0 => 0,
                                                       Ssie  => 0,
                                                       Usie  => 0));
   end HandleTimerInterrupt; 


   -- -----------------------------------------------------------------------------
   -- *** Porting the WAR from C SepKern
   -- -----------------------------------------------------------------------------
   procedure PassThroughEcall with 
     Inline_Always
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

      Mtval : LW_RISCV_CSR_MTVAL_Register;
      Mcause : LW_RISCV_CSR_MCAUSE_Register;
      Mcause2 : LW_RISCV_CSR_MCAUSE2_Register;
      Stvec : LW_RISCV_CSR_STVEC_Register;
      Mepc : LW_RISCV_CSR_MEPC_Register;
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
                   Data => LW_RISCV_CSR_SCAUSE_Register'(Int => Mcause.Int,
                                                         Wlrl => Mcause.Wlrl,
                                                         Excode => Mcause.Excode));

      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE2_Address,
                   Data => Mcause2);
      Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCAUSE2_Address,
                   Data => LW_RISCV_CSR_SCAUSE2_Register'(Wpri => Mcause2.Wpri,
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
      Mstatus.Sie := SIE_DISABLE;
      Mstatus.Spp := (if Mstatus.Mpp = MPP_USER then SPP_USER else SPP_SUPERVISOR);
      Mstatus.Mpp := MPP_SUPERVISOR;

      Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSTATUS_Address,
                   Data => Mstatus);

   end PassThroughEcall;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - SK entry point. Handler for Non-delegated interrupts/exceptions
-- -----------------------------------------------------------------------------
   procedure Trap_Handler 
   is
      Mcause : LW_RISCV_CSR_MCAUSE_Register;
      Mepc : LW_RISCV_CSR_MEPC_Register;
      s : Callee_Saved_Registers;
      a : Argument_Registers;  

      -- TODO: Hack to allow M-mode to redirect syscalls back to S-mode
      LW_SYSCALL_FLAG_SMODE : constant := 16#8000_0000_0000_0000#;
   begin
      pragma Assume(Riscv_Core.Ghost_Lwrrent_Stack = Riscv_Core.SK_Stack); -- Done in 'trapEntry'
      Riscv_Core.Rv_Gpr.Load_From_Argument_Registers(a);

      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                   Data => Mcause);

      Operation_Done:
      for Unused_Loop_Var in 1..1 loop
         -- All interrupts/exceptions are delegated to the partition except ECALL for SBI implementation and Timer interrupt
         -- Halt the core if not
         case Mcause.Int is
         when 0 =>
            if Mcause.Excode = LW_RISCV_CSR_MCAUSE_EXCODE_SCALL and then a(7) >= LW_SYSCALL_FLAG_SMODE then
               PassThroughEcall;

            elsif Mcause.Excode = LW_RISCV_CSR_MCAUSE_EXCODE_SCALL then
               Riscv_Core.Rv_Gpr.Save_Callee_Saved_Registers(s);

               -- Skip 'ecall' instruction
               Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEPC_Address,
                            Data => Mepc);

               exit Operation_Done when Mepc.Epc > (LwU64'Last - 4);

               Mepc.Epc := Mepc.Epc + 4;
               Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEPC_Address,
                            Data => Mepc);

               SBI.SBI_Dispatch(a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7));
               Riscv_Core.Rv_Gpr.Restore_Callee_Saved_Registers(s);

            else
               -- Will Halt the core in this case. No need to restore the stack/registers
               exit Operation_Done;

            end if;

         when 1 =>
            exit Operation_Done when Mcause.Excode /= LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT;

            HandleTimerInterrupt;

         end case;

         Riscv_Core.Rv_Gpr.Save_To_Argument_Registers(a); -- Restoring a-registers or loading values from SBI call
         Riscv_Core.Rv_Gpr.Switch_To_Old_Stack(Load_Old_SP_Value_From => LwU32(LW_RISCV_CSR_MSCRATCH2_Address));
         Riscv_Core.Inst.Mret;

      end loop Operation_Done;

      Error_Handling.Throw_Critical_Error(Pz_Code => TRAP_HANDLER_PHASE,
                                              Err_Code => CRITICAL_ERROR);

   end Trap_Handler;

   function Get_Address_Of_Trap_Entry return LwU64 with SPARK_Mode => Off
   is
      Trap_Entry : LwU64 with
        Import        => True,
        Convention    => C,
        External_Name => "trap_entry";

      Trap_Entry_Address : aliased constant System.Address := Trap_Entry'Address;

      function System_Addr_To_LwU64 is new Ada.Unchecked_Colwersion(Source => System.Address,
                                                                    Target => LwU64);
   begin
      return Address_To_LwU64(Trap_Entry_Address);
   end Get_Address_Of_Trap_Entry;

-- -----------------------------------------------------------------------------
-- *** Temporary Trap Handler for S-Mode - until partition loads its own
-- !!! Address leaked to S-Mode !!!
-- -----------------------------------------------------------------------------
   procedure Temp_S_Mode_Trap_Handler is
   begin
      Riscv_Core.Halt;
   end Temp_S_Mode_Trap_Handler; 

   function Get_Address_Of_Temp_S_Mode_Trap_Handler return LwU64  with SPARK_Mode => Off
   is
   begin
      return Address_To_LwU64 (Temp_S_Mode_Trap_Handler'Address);
   end Get_Address_Of_Temp_S_Mode_Trap_Handler;

end Trap;
