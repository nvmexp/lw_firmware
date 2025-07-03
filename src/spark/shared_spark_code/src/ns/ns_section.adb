
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;
with Utility; use Utility;
with Ucode_Post_Codes; use Ucode_Post_Codes;

package body Ns_Section
with
SPARK_Mode => On
is

   function To_Mailbox is new Ada.Unchecked_Colwersion(Source => LW_UCODE_UNIFIED_ERROR_TYPE,
                                                       Target => LW_CSEC_FALCON_MAILBOX0_DATA_FIELD);

   procedure Get_Selwre_Blk_Start_Addr( Selwre_Blk_Start_From_Linker : out LwU32 )
   is
      -- Populated at link time denoting the start of secure code.
      Selwre_Blk_Start : LwU32
        with
          Import     => True,
          Convention => C,
          External_Name => "_pub_code_start";
   begin
      Selwre_Blk_Start_From_Linker := UC_ADDRESS_TO_LwU32( Selwre_Blk_Start'Address );
   end Get_Selwre_Blk_Start_Addr;

   procedure Get_Pkc_Param_Addr( Pkc_Params_Addr : out LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD)
     with SPARK_Mode => Off
   is
   begin
      Pkc_Params_Addr := UC_SYSTEM_ADDRESS_PARAADDR( Pkc_Params'Address );
   end Get_Pkc_Param_Addr;

   procedure Stack_Check_Fail
   is
      Falcon_Mailbox0 : LW_CSEC_FALCON_MAILBOX0_REGISTER;

   begin
      Falcon_Mailbox0.F_Data := To_Mailbox(UCODE_ERROR_SSP_STACK_CHECK_FAILED);
      Csb_Reg_Wr32_Falcon_Mailbox0.Csb_Reg_Wr32_Generic_No_Peh(Reg => Falcon_Mailbox0,
                                                               Addr => Lw_Csec_Falcon_Mailbox0);
      Infinite_Loop;
   end Stack_Check_Fail;

   procedure Ns_Entry
   is
      pragma Suppress(All_Checks);

      Hs_Start : LwU32;
      Blk_Base                  : LW_FALC_SEC_SPR_SELWRE_BLK_BASE_Field;
      Falcon_Spr_Sec            : LW_FALC_SEC_SPR_Register;
      Falcon_Brom_Paraaddr      : LW_CSEC_FALCON_BROM_PARAADDR_REGISTER;
      Pkc_Params_Addr           : LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD;
      Falcon_Brom_Engidmask     : LW_CSEC_FALCON_BROM_ENGIDMASK_REGISTER;
      Falcon_Brom_Lwrr_Ucode_Id : LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_REGISTER;
      Falcon_Mod_Sel            : LW_CSEC_FALCON_MOD_SEL_REGISTER;
      Falcon_Mailbox0           : LW_CSEC_FALCON_MAILBOX0_REGISTER;
      Falcon_Csberrstat         : LW_CSEC_FALCON_CSBERRSTAT_REGISTER;

      Selwre_Blk_Cnt : LwU32
        with
          Import     => True,
          Convention => C,
          External_Name => "_pubBlkCnt";

   begin

      Ns_Canary_Setup;

      -- Initialize MAILBOX0
      Falcon_Mailbox0.F_Data := To_Mailbox(UCODE_ERROR_BIN_STARTED_BUT_NOT_FINISHED);
      Csb_Reg_Wr32_Falcon_Mailbox0.Csb_Reg_Wr32_Generic_No_Peh(Reg => Falcon_Mailbox0,
                            Addr => Lw_Csec_Falcon_Mailbox0);

      --  Populate PKC Params record.
      Pkc_Params.F_Pkc_Pk := 0;
      Pkc_Params.F_Hash_Saving := 0;

      --  Write address of PKC Params struct to Brom_Paraaddr_Addr register.
      Get_Pkc_Param_Addr( Pkc_Params_Addr => Pkc_Params_Addr);

      Falcon_Brom_Paraaddr.F_Val := Pkc_Params_Addr;

      Csb_Reg_Wr32_Falcon_Brom_Paraaddr.Csb_Reg_Wr32_Generic_No_Peh(Reg  => Falcon_Brom_Paraaddr,
                                 Addr => Lw_Csec_Falcon_Brom_Paraaddr_Addr(0));

      --  Write ENGID Mask.
      Falcon_Brom_Engidmask.F_Sec := Lw_Csec_Falcon_Brom_Engidmask_Sec_Enabled;
      Falcon_Brom_Engidmask.F_Dpu := Lw_Csec_Falcon_Brom_Engidmask_Dpu_Disabled;
      Falcon_Brom_Engidmask.F_Lwdec := Lw_Csec_Falcon_Brom_Engidmask_Lwdec_Disabled;
      Falcon_Brom_Engidmask.F_Pmu := Lw_Csec_Falcon_Brom_Engidmask_Pmu_Disabled;
      Falcon_Brom_Engidmask.F_Fbfalcon := Lw_Csec_Falcon_Brom_Engidmask_Fbfalcon_Disabled;
      Falcon_Brom_Engidmask.F_Lwenc := Lw_Csec_Falcon_Brom_Engidmask_Lwenc_Disabled;
      Falcon_Brom_Engidmask.F_Gpccs := Lw_Csec_Falcon_Brom_Engidmask_Gpccs_Disabled;
      Falcon_Brom_Engidmask.F_Fecs := Lw_Csec_Falcon_Brom_Engidmask_Fecs_Disabled;
      Falcon_Brom_Engidmask.F_Minion := Lw_Csec_Falcon_Brom_Engidmask_Minion_Disabled;
      Falcon_Brom_Engidmask.F_Xusb := Lw_Csec_Falcon_Brom_Engidmask_Xusb_Disabled;
      Falcon_Brom_Engidmask.F_Gsp := Lw_Csec_Falcon_Brom_Engidmask_Gsp_Disabled;
      Falcon_Brom_Engidmask.F_Lwjpg := Lw_Csec_Falcon_Brom_Engidmask_Lwjpg_Disabled;

      Csb_Reg_Wr32_Falcon_Engidmask.Csb_Reg_Wr32_Generic_No_Peh(Reg    => Falcon_Brom_Engidmask,
                          Addr   => Lw_Csec_Falcon_Brom_Engidmask);

      --  Write Ucode ID.
      Falcon_Brom_Lwrr_Ucode_Id.F_Val := Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id_Val_Pub;

      Csb_Reg_Wr32_Falcon_Brom_Ucode_Id.Csb_Reg_Wr32_Generic_No_Peh(Reg    => Falcon_Brom_Lwrr_Ucode_Id,
                            Addr   => Lw_Csec_Falcon_Brom_Lwrr_Ucode_Id);

      -- Select algorithm
      Falcon_Mod_Sel.F_Algo := Lw_Csec_Falcon_Mod_Sel_Algo_Rsa3K;

      Csb_Reg_Wr32_Falcon_Mod_Sel.Csb_Reg_Wr32_Generic_No_Peh(Reg    => Falcon_Mod_Sel,
                           Addr   => Lw_Csec_Falcon_Mod_Sel);

      -- Enable CSB Error reporting
      Falcon_Csberrstat.F_Enable := Lw_Csec_Falcon_Csberrstat_Enable_Init;
       Csb_Reg_Wr32_Falcon_Csberrstat.Csb_Reg_Wr32_Generic_No_Peh(Reg  => Falcon_Csberrstat,
                               Addr => Lw_Csec_Falcon_Csberrstat);

      Get_Selwre_Blk_Start_Addr (Hs_Start);


      Blk_Base := LW_FALC_SEC_SPR_SELWRE_BLK_BASE_Field( Safe_Shift_Right( Hs_Start, 8 ) );

      Falcon_Spr_Sec.Selwre_Blk_Base := Blk_Base;
      Falcon_Spr_Sec.Selwre_Blk_Cnt := LW_FALC_SEC_SPR_SELWRE_BLK_CNT_Field( Selwre_Blk_Cnt );
      Falcon_Spr_Sec.Secure := Clear;
      Falcon_Spr_Sec.Encrypted := Set;
      Falcon_Spr_Sec.Sig_Valid := Clear;
      Falcon_Spr_Sec.Disable_Exceptions := Clear;

      Write_Sec_Spr_C( To_Raw (Falcon_Spr_Sec) );

   end Ns_Entry;

end Ns_Section;
