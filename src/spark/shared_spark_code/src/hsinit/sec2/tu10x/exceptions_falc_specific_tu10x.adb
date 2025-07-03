with Exceptions; use Exceptions;

package body Exceptions_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   -- TODO : Use below once UC issue is resolved by adacore instead of writing each field with Init
   --pragma Assume(LwU32'Size = LW_CSEC_FALCON_IBRKPT1_REGISTER'Size);
   --function UC_Ibrkpt1 is new Ada.Unchecked_Colwersion(Source => LwU32,
   --                                                    Target => LW_CSEC_FALCON_IBRKPT1_REGISTER);

   procedure Stack_Underflow_Installer(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)

   is
      pragma Suppress(All_Checks);
      Stack_Cfg_Reg : LW_CSEC_FALCON_STACKCFG_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         --  Setup STACKCFG.
         Stack_Cfg_Reg.F_Bottom := LW_CSEC_FALCON_STACKCFG_BOTTOM_FIELD(Get_Stack_Bottom);
         Stack_Cfg_Reg.F_Spexc  := Lw_Csec_Falcon_Stackcfg_Spexc_Enable;

         Csb_Reg_Access_Stackcfg.Csb_Reg_Wr32_Generic_Inline(Reg    => Stack_Cfg_Reg,
                                                             Addr   => Lw_Csec_Falcon_Stackcfg,
                                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Update HS Init state in ghost variable for SPARK Prover
         Ghost_Hs_Init_States_Tracker := STACK_UNDERFLOW_INSTALLER;
      end loop Main_Block;
   end Stack_Underflow_Installer;

   procedure Set_Ibrkpt_Registers(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)

   is
      pragma Suppress(All_Checks);

      Ibrkpt1 : LW_CSEC_FALCON_IBRKPT1_REGISTER ;
      Ibrkpt2 : LW_CSEC_FALCON_IBRKPT2_REGISTER ;
      Ibrkpt3 : LW_CSEC_FALCON_IBRKPT3_REGISTER ;
      Ibrkpt4 : LW_CSEC_FALCON_IBRKPT4_REGISTER ;
      Ibrkpt5 : LW_CSEC_FALCON_IBRKPT5_REGISTER ;

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Ibrkpt1.F_Pc        := Lw_Csec_Falcon_Ibrkpt1_Pc_Init;
         Ibrkpt1.F_Suppress  := Lw_Csec_Falcon_Ibrkpt1_Suppress_Init;
         Ibrkpt1.F_Skip      := Lw_Csec_Falcon_Ibrkpt1_Skip_Init;
         Ibrkpt1.F_En        := Lw_Csec_Falcon_Ibrkpt1_En_Init;

         Csb_Reg_Access_Ibrkpt1.Csb_Reg_Wr32_Generic_Inline(Reg    => Ibrkpt1,
                                                            Addr   => Lw_Csec_Falcon_Ibrkpt1,
                                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ibrkpt2.F_Pc        := Lw_Csec_Falcon_Ibrkpt2_Pc_Init;
         Ibrkpt2.F_Suppress  := Lw_Csec_Falcon_Ibrkpt2_Suppress_Init;
         Ibrkpt2.F_Skip      := Lw_Csec_Falcon_Ibrkpt2_Skip_Init;
         Ibrkpt2.F_En        := Lw_Csec_Falcon_Ibrkpt2_En_Init;

         Csb_Reg_Access_Ibrkpt2.Csb_Reg_Wr32_Generic_Inline(Reg    => Ibrkpt2,
                                                            Addr   => Lw_Csec_Falcon_Ibrkpt1,
                                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ibrkpt3.F_Pc        := Lw_Csec_Falcon_Ibrkpt3_Pc_Init;
         Ibrkpt3.F_Suppress  := Lw_Csec_Falcon_Ibrkpt3_Suppress_Init;
         Ibrkpt3.F_Skip      := Lw_Csec_Falcon_Ibrkpt3_Skip_Init;
         Ibrkpt3.F_En        := Lw_Csec_Falcon_Ibrkpt3_En_Init;

         Csb_Reg_Access_Ibrkpt3.Csb_Reg_Wr32_Generic_Inline(Reg    => Ibrkpt3,
                                                            Addr   => Lw_Csec_Falcon_Ibrkpt1,
                                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ibrkpt4.F_Pc        := Lw_Csec_Falcon_Ibrkpt4_Pc_Init;
         Ibrkpt4.F_Suppress  := Lw_Csec_Falcon_Ibrkpt4_Suppress_Init;
         Ibrkpt4.F_Skip      := Lw_Csec_Falcon_Ibrkpt4_Skip_Init;
         Ibrkpt4.F_En        := Lw_Csec_Falcon_Ibrkpt4_En_Init;

         Csb_Reg_Access_Ibrkpt4.Csb_Reg_Wr32_Generic_Inline(Reg    => Ibrkpt4,
                                                            Addr   => Lw_Csec_Falcon_Ibrkpt1,
                                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ibrkpt5.F_Pc        := Lw_Csec_Falcon_Ibrkpt5_Pc_Init;
         Ibrkpt5.F_Suppress  := Lw_Csec_Falcon_Ibrkpt5_Suppress_Init;
         Ibrkpt5.F_Skip      := Lw_Csec_Falcon_Ibrkpt5_Skip_Init;
         Ibrkpt5.F_En        := Lw_Csec_Falcon_Ibrkpt5_En_Init;

         Csb_Reg_Access_Ibrkpt5.Csb_Reg_Wr32_Generic_Inline(Reg    => Ibrkpt5,
                                                            Addr   => Lw_Csec_Falcon_Ibrkpt1,
                                                            Status => Status);
      end loop Main_Block;

   end Set_Ibrkpt_Registers;

end Exceptions_Falc_Specific_Tu10x;
