-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;
with Ada.Unchecked_Colwersion;

package body Falcon_Selwrebusdio_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   function UC_FALCON_PTIMER0 is new Ada.Unchecked_Colwersion (Source => LW_CSEC_FALCON_PTIMER0_REGISTER,
                                                               Target => LwU32);

   procedure Read_Ptimer0(Val : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Time : LW_CSEC_FALCON_PTIMER0_REGISTER;
   begin
      -- Initialization to please SPARK prover
      Val := 0;
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_Ptimer0(Reg    => Time,
                              Addr   => Lw_Csec_Falcon_Ptimer0,
                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Val := UC_FALCON_PTIMER0(Time);
      end loop Main_Block;
   end Read_Ptimer0;

   procedure Read_Doc_Ctrl_Error(Error : out Boolean; Error_Type : Type_Of_Errors;
                                 Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Data :  LW_CSEC_FALCON_DOC_CTRL_REGISTER;
   begin
      -- Initialization to please SPARK prover
      Error := False;
      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop

         Csb_Reg_Rd32_Doc_Ctrl(Reg    => Data,
                               Addr   => Lw_Csec_Falcon_Doc_Ctrl,
                               Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Error_Type = Error1 then
            Error := Data.F_Empty = Lw_Csec_Falcon_Doc_Ctrl_Empty_Init and then
              Data.F_Wr_Finished = Lw_Csec_Falcon_Doc_Ctrl_Wr_Finished_Init and then
              Data.F_Rd_Finished = Lw_Csec_Falcon_Doc_Ctrl_Rd_Finished_Init ;

         elsif Error_Type = Error2 then
            Error := Data.F_Empty = Lw_Csec_Falcon_Doc_Ctrl_Empty_Init and then
              Data.F_Wr_Finished = Lw_Csec_Falcon_Doc_Ctrl_Wr_Finished_Init and then
              Data.F_Count = FALC_DOC_CONTROL_MAX_FREE_ENTRIES ;
         elsif Error_Type = Error3 then
            Error := Data.F_Rd_Error = 1 or else
              Data.F_Protocol_Error = 1 ;
         end if;
      end loop Main_Block;

   end Read_Doc_Ctrl_Error;

   procedure Write_LwU32_Doc_D0(Val : LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

   begin
         Csb_Reg_Wr32_LwU32(Reg    => Val,
                            Addr   => Lw_Csec_Falcon_Doc_D0,
                            Status => Status);
   end Write_LwU32_Doc_D0;

   procedure Write_LwU32_Doc_D1(Val : LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

   begin
         Csb_Reg_Wr32_LwU32(Reg    => Val,
                            Addr   => Lw_Csec_Falcon_Doc_D1,
                            Status => Status);
   end Write_LwU32_Doc_D1;

   procedure Write_Dic_Ctrl(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

   begin
      Csb_Reg_Wr32_Dic_Ctrl(Reg    => (F_Count  => 0,
                                       F_Reset => 0,
                                       F_Valid    => 0,
                                       F_Pop => 1),
                            Addr   => Lw_Csec_Falcon_Dic_Ctrl,
                            Status => Status);
   end Write_Dic_Ctrl;

   procedure Read_LwU32_Dic_D0(Val : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

   begin
         Csb_Reg_Rd32_LwU32(Reg    => Val,
                            Addr   => Lw_Csec_Falcon_Dic_D0,
                            Status => Status);
   end Read_LwU32_Dic_D0;

   procedure Read_Count(Count : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Data : LW_CSEC_FALCON_DIC_CTRL_REGISTER;
   begin
      -- Initialization to please SPARK prover
      Count := 0;
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_Dic_Ctrl(Reg    => Data,
                               Addr   => Lw_Csec_Falcon_Dic_Ctrl,
                               Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Count := LwU32( Data.F_Count );
      end loop Main_Block;
   end Read_Count;

end Falcon_Selwrebusdio_Falc_Specific_Tu10x;
