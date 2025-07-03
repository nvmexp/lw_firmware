-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;


package body Reset_Plm_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Update_Falcon_Reset_PLM(isRaise : LwBool; Status : out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Reg_Val : LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_REGISTER;

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_Flcn_Reset_PLM(Reg    => Reg_Val,
                                     Addr   => Lw_Csec_Falcon_Reset_Priv_Level_Mask,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if isRaise = LW_TRUE then
            Reg_Val.F_Write_Protection := Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Disabled;
            Reg_Val.F_Write_Violation  := Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Report_Error;
         else
            Reg_Val.F_Write_Protection := Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;
            Reg_Val.F_Write_Violation  := Lw_Csec_Falcon_Reset_Priv_Level_Mask_Write_Violation_Report_Error;
         end if;

         Csb_Reg_Wr32_Flcn_Reset_PLM(Reg    => Reg_Val,
                                     Addr   => Lw_Csec_Falcon_Reset_Priv_Level_Mask,
                                     Status => Status);
      end loop Main_Block;

      -- Update HS Init state in ghost variable for SPARK Prover
      Ghost_Hs_Init_States_Tracker := RESET_PLM_PROTECTED;

   end Update_Falcon_Reset_PLM;

end Reset_Plm_Falc_Specific_Tu10x;
