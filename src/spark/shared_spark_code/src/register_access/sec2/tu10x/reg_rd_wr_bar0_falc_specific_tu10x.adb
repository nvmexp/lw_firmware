-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;

package body Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Bar0_Wait_Idle(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      bDone : Boolean := False;
      csecBar0Csr : LW_CSEC_BAR0_CSR_REGISTER;
   begin

      Loop_Until:
      while bDone = False loop
         pragma Loop_Ilwariant(Status = UCODE_ERR_CODE_NOERROR and then
                               Ghost_Bar0_Status = BAR0_SUCCESSFUL and then
                               Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE);

         Csb_Reg_Rd32_Bar0Csr(Reg  => csecBar0Csr,
                              Addr => Lw_Csec_Bar0_Csr,
                              Status => Status);
         exit Loop_Until when Status /= UCODE_ERR_CODE_NOERROR;

         case csecBar0Csr.F_Status is
            when Lw_Csec_Bar0_Csr_Status_Idle =>
               bDone := True;
               Status := UCODE_ERR_CODE_NOERROR;
            when Lw_Csec_Bar0_Csr_Status_Busy =>
               null;

            when others =>
               Status := FLCN_BAR0_WAIT;
               Ghost_Bar0_Status := BAR0_WAITING;
               exit Loop_Until;
         end case;

      end loop Loop_Until;

   end Bar0_Wait_Idle;

   procedure Bar0_Reg_Rd32_Generic( Reg  : out GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Val : LwU32;
      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
      function To_Input_Type is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                              Target => GENERIC_REGISTER);
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Private (Val  => Val,
                                Addr => Addr,
                                Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Reg := To_Input_Type (Val);
      end loop Main_Block;

   end Bar0_Reg_Rd32_Generic;

   procedure Bar0_Reg_Wr32_Generic( Reg  : GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
      function To_Input_Type is new Ada.Unchecked_Colwersion (Source => GENERIC_REGISTER,
                                                              Target => LwU32);
      Val : constant LwU32 := To_Input_Type (Reg);
   begin
      Bar0_Reg_Wr32_Private (Val  => Val,
                             Addr => Addr,
                             Status => Status);
   end Bar0_Reg_Wr32_Generic;

   procedure Bar0_Reg_Wr32_Readback_Generic( Reg  : GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
      function To_Input_Type is new Ada.Unchecked_Colwersion (Source => GENERIC_REGISTER,
                                                              Target => LwU32);
      Val : constant LwU32 := To_Input_Type (Reg);
      Ret : LwU32 := 0;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Bar0_Reg_Wr32_Private (Val  => Val,
                                Addr => Addr,
                                Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Private (Val  => Ret,
                                Addr => Addr,
                                Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Ret /= Val
         then
            Status := UCODE_PUB_ERR_READBACK_FAILED;
         end if;
      end loop Main_Block;
   end Bar0_Reg_Wr32_Readback_Generic;


   procedure Bar0_Reg_Rd32_Private( Val  : out LwU32;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
       csecBar0Csr : LW_CSEC_BAR0_CSR_REGISTER;
   begin
      Main_Block:
      --
      -- To satisfy prover as Val may not be initialized if Pri Err oclwrs and any of the below
      -- csb read/write fails and returns early
      --

      for Unused_Loop_Var in 1 .. 1 loop
         Val := 0;

         Bar0_Wait_Idle(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Csb_Reg_Wr32_LwU32 ( Reg => LwU32(Addr),
                              Addr => Lw_Csec_Bar0_Addr,
                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         csecBar0Csr := ( F_Cmd            => Lw_Csec_Bar0_Csr_Cmd_Read,
                          F_Byte_Enable    => Lw_Csec_Bar0_Csr_Byte_Enable_Val_For_Bar0_Read,
                          F_Xact_Check     => Lw_Csec_Bar0_Csr_Xact_Check_False,
                          F_Xact_Stalled   => Lw_Csec_Bar0_Csr_Xact_Stalled_False,
                          F_Xact_Finished  => Lw_Csec_Bar0_Csr_Xact_Finished_False,
                          F_Status         => Lw_Csec_Bar0_Csr_Status_Idle,
                          F_Level          => 0,
                          F_Warn           => Lw_Csec_Bar0_Csr_Warn_Clean,
                          F_Halted         => Lw_Csec_Bar0_Csr_Halted_False,
                          F_Trig           => Lw_Csec_Bar0_Csr_Trig_True
                         );

         Csb_Reg_Wr32_Stall_Bar0Csr ( Reg => csecBar0Csr,
                                      Addr => Lw_Csec_Bar0_Csr,
                                      Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         pragma Warnings (Off, "unused assignment to ""csecBar0Csr""");
         pragma Warnings(Off, """csecBar0Csr"" modified by call, but value might not be referenced");
         Csb_Reg_Rd32_Bar0Csr(Reg  => csecBar0Csr,
                              Addr => Lw_Csec_Bar0_Csr,
                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         pragma Warnings (Off, "statement has no effect");
         pragma Warnings (On, "unused assignment to ""csecBar0Csr""");

         Bar0_Wait_Idle(Status =>  Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Csb_Reg_Rd32_LwU32 ( Reg => Val,
                              Addr => Lw_Csec_Bar0_Data,
                              Status => Status);
      end loop Main_Block;

   end Bar0_Reg_Rd32_Private;

   procedure Bar0_Reg_Wr32_Private( Val  : LwU32;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      csecBar0Csr : LW_CSEC_BAR0_CSR_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Wait_Idle(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Csb_Reg_Wr32_LwU32( Reg => LwU32(Addr),
                             Addr => Lw_Csec_Bar0_Addr,
                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Csb_Reg_Wr32_LwU32( Reg => Val,
                             Addr => Lw_Csec_Bar0_Data,
                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         --
         -- Can do this """ csecBar0Csr := GenericToLwU32(initVal), initVal := 0 """ and only write
         -- required fields (Cmd, Byte_Enable, Trig) with appropriate values,
         -- instead of below initializations once adacore provides CCG update on error regarding
         -- Unchecked Colwersions , tracked here : SB08-020
         --

         csecBar0Csr := ( F_Cmd            => Lw_Csec_Bar0_Csr_Cmd_Write,
                          F_Byte_Enable    => Lw_Csec_Bar0_Csr_Byte_Enable_Val_For_Bar0_Read,
                          F_Xact_Check     => Lw_Csec_Bar0_Csr_Xact_Check_False,
                          F_Xact_Stalled   => Lw_Csec_Bar0_Csr_Xact_Stalled_False,
                          F_Xact_Finished  => Lw_Csec_Bar0_Csr_Xact_Finished_False,
                          F_Status         => Lw_Csec_Bar0_Csr_Status_Idle,
                          F_Level          => 0,
                          F_Warn           => Lw_Csec_Bar0_Csr_Warn_Clean,
                          F_Halted         => Lw_Csec_Bar0_Csr_Halted_False,
                          F_Trig           => Lw_Csec_Bar0_Csr_Trig_True
                         );

         Csb_Reg_Wr32_Stall_Bar0Csr( Reg => csecBar0Csr,
                                     Addr => Lw_Csec_Bar0_Csr,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         pragma Warnings (Off, "unused assignment to ""csecBar0Csr""");
         Csb_Reg_Rd32_Bar0Csr(Reg  => csecBar0Csr,
                              Addr => Lw_Csec_Bar0_Csr,
                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         pragma Warnings (On, "unused assignment to ""csecBar0Csr""");

         Bar0_Wait_Idle(Status => Status);

      end loop Main_Block;

   end Bar0_Reg_Wr32_Private;

end Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
