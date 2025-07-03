-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;


package body Hs_Init_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Is_Valid_Falcon_Engine(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Reg_Val : LW_CSEC_FALCON_ENGID_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_EngineId(Reg    => Reg_Val,
                               Addr   => Lw_Csec_Falcon_Engid,
                               Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Reg_Val.F_Familyid /= Lw_Csec_Falcon_Engid_Familyid_Sec then
            -- Ghost variable Ghost_Valid_Engine_State is updated in case of error
            Ghost_Valid_Engine_State := ILWALID_ENGINE;
            Status := UNEXPECTED_FALCON_ENGID;
         end if;

      end loop Main_Block;

   end Is_Valid_Falcon_Engine;

   procedure Enable_Csb_Err_Reporting( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      pragma Suppress(All_Checks);

      Reg_Val : LW_CSEC_FALCON_CSBERRSTAT_REGISTER;
   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Exempt the write which enables csb err reporting
         -- Update CSB Err Reporting State in ghost variable for SPARK prover
         Ghost_Csb_Err_Reporting_State := CSB_ERR_REPORTING_ENABLED;

         Reg_Val.F_Enable := Lw_Csec_Falcon_Csberrstat_Enable_True;

         -- Not Writtable by SW and are dummy updates to please SPARK Prover.
         Reg_Val.F_Valid := Lw_Csec_Falcon_Csberrstat_Valid_True;
         Reg_Val.F_Pc := 0;

         -- Exempt the write which enables csb err reporting
         -- Ghost_Skip_Permission_Csb_Err_Reporting := SKIP_ALLOWED;

         Csb_Reg_Access_Csberrstat.Csb_Reg_Wr32_Generic_Inline(Reg    => Reg_Val,
                                                               Addr   => Lw_Csec_Falcon_Csberrstat,
                                                               Status => Status);

      end loop Main_Block;

      -- Reset permission to default not allowed
      --Ghost_Skip_Permission_Csb_Err_Reporting := SKIP_NOT_ALLOWED;

      -- Update HS Init State in ghost variable for SPARK prover
      Ghost_Hs_Init_States_Tracker := CSB_ERROR_REPORTING_ENABLED;

   end Enable_Csb_Err_Reporting;

   procedure Set_Bar0_Timeout( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      Reg_Val : LW_CSEC_BAR0_TMOUT_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Reg_Val.F_Cycles := Lw_Csec_Bar0_Tmout_Cycles_Prod;
         Csb_Reg_Wr32_Bar0_Tmout(Reg  => Reg_Val,
                                 Addr => Lw_Csec_Bar0_Tmout,
                                 Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Update HS Init State in ghost variable for SPARK Prover
         Ghost_Hs_Init_States_Tracker := BAR0_TMOUT_SET;
         Ghost_Bar0_Tmout_State := BAR0_TIMOUT_SET;
      end loop Main_Block;

   end Set_Bar0_Timeout;

   procedure Prevent_Halted_Cpu_From_Being_Restarted(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      pragma Suppress(All_Checks);

      Reg_Val : LW_CSEC_FALCON_CPUCTL_REGISTER;

   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Access_Cpuctl.Csb_Reg_Rd32_Generic_Inline(Reg    => Reg_Val,
                                                           Addr   => Lw_Csec_Falcon_Cpuctl,
                                                           Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Reg_Val.F_Alias_En := Lw_Csec_Falcon_Cpuctl_Alias_En_False;


         Csb_Reg_Access_Cpuctl.Csb_Reg_Wr32_Generic_Inline(Reg    => Reg_Val,
                                                           Addr   => Lw_Csec_Falcon_Cpuctl,
                                                           Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Update HS Init state in ghost variables for SPARK Prover
         Ghost_Hs_Init_States_Tracker := HS_RESTART_PREVENTED;

      end loop Main_Block;


   end Prevent_Halted_Cpu_From_Being_Restarted;

   procedure Disable_Cmembase_Aperture(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Cmembase_Reg : LW_CSEC_FALCON_CMEMBASE_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Cmembase_Reg.F_Val := Lw_Csec_Falcon_Cmembase_Val_Init;

         Csb_Reg_Access_Cmembase.Csb_Reg_Wr32_Generic_Inline(Reg    => Cmembase_Reg,
                                                             Addr   => Lw_Csec_Falcon_Cmembase,
                                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         -- Update HS Init state in ghost variable for SPARK prover
         Ghost_Hs_Init_States_Tracker := CMEMBASE_APERTURE_DISABLED;
      end loop Main_Block;

   end Disable_Cmembase_Aperture;

   procedure Disable_Trace_Pc_Buffer(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      pragma Suppress(All_Checks);

      Reg_Val : LW_CSEC_FALCON_HSCTL_REGISTER;

   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Access_Hsctl.Csb_Reg_Rd32_Generic_Inline(Reg    => Reg_Val,
                            Addr   => Lw_Csec_Falcon_Hsctl,
                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Reg_Val.F_Tracepc := Lw_Csec_Falcon_Hsctl_Tracepc_Disable;

         Csb_Reg_Access_Hsctl.Csb_Reg_Wr32_Generic_Inline(Reg    => Reg_Val,
                            Addr   => Lw_Csec_Falcon_Hsctl,
                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Update HS Init state in ghost variable for SPARK prover
         Ghost_Hs_Init_States_Tracker := TRACE_PC_BUFFER_DISABLED;

      end loop Main_Block;

   end Disable_Trace_Pc_Buffer;

   procedure Callwlate_Imem_Size_In_256B(Imem_Size : out LwU32;
                                         Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress (All_Checks);
      Reg_Val         : LW_CSEC_FALCON_HWCFG_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Default value for Imem_Size is 0 which is also case of error (ILWALID_IMEM_BLOCK_COUNT)
         Imem_Size := 0;

         Csb_Reg_Rd32_Hwcfg(Reg    => Reg_Val,
                            Addr   => Lw_Csec_Falcon_Hwcfg,
                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Imem_Size := LwU32 (Reg_Val.F_Imem_Size);

         if Imem_Size = 0
         then
            Status := ILWALID_IMEM_BLOCK_COUNT;
         end if;

      end loop Main_Block;
   end Callwlate_Imem_Size_In_256B;

end Hs_Init_Falc_Specific_Tu10x;
