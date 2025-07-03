-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Pwr_Pri_Ada; use Dev_Pwr_Pri_Ada;
with Dev_Therm_Ada; use Dev_Therm_Ada;
with Pub_Pmu_Bar0_Reg_Rd_Wr_Instances; use Pub_Pmu_Bar0_Reg_Rd_Wr_Instances;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;

package body Update_Pmu_Plms_Tu10x
with SPARK_Mode => On
is

   procedure Lower_Pmu_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Lower_Ppwr_Exe_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Irqtmr_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Dmem_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Sctl_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Wdtmr_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Fbif_Regioncfg_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Pmu_Msgq_Head_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Ppwr_Pmu_Msgq_Tail_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Therm_Smartfan_One_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Therm_Vidctrl_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- New PLMs for regs requested by RM to PMU ucode

         Lower_Fuse_Iddqinfo_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Fuse_Sram_Vmin_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Fuse_Speedoinfo_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Lower_Fuse_Kappainfo_Plm(Status => Status);

      end loop Main_Block;
   end Lower_Pmu_Plms;


   procedure Lower_Ppwr_Exe_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Exe_Plm : LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Exe_Plm(Reg    => Ppwr_Exe_Plm,
                                    Addr   => Lw_Ppwr_Falcon_Exe_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Exe_Plm.F_Read_Protection :=
           Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Exe_Plm.F_Write_Protection :=
           Lw_Ppwr_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Exe_Plm(Reg    => Ppwr_Exe_Plm,
                                    Addr   => Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask,
                                    Status => Status);

      end loop Main_Block;

   end Lower_Ppwr_Exe_Plm;

   procedure Lower_Ppwr_Irqtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Irqtmr_Plm : LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Irqtmr_Plm(Reg    => Ppwr_Irqtmr_Plm,
                                       Addr   => Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Irqtmr_Plm.F_Read_Protection :=
           Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Irqtmr_Plm.F_Write_Protection :=
           Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Irqtmr_Plm(Reg    => Ppwr_Irqtmr_Plm,
                                       Addr   => Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);

      end loop Main_Block;

   end Lower_Ppwr_Irqtmr_Plm;

   procedure Lower_Ppwr_Dmem_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Dmem_Plm : LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Dmem_Plm(Reg    => Ppwr_Dmem_Plm,
                                     Addr   => Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Dmem_Plm.F_Read_Protection :=
           Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Dmem_Plm.F_Write_Protection :=
           Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Dmem_Plm(Reg    => Ppwr_Dmem_Plm,
                                     Addr   => Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask,
                                     Status => Status);

      end loop Main_Block;

   end Lower_Ppwr_Dmem_Plm;

   procedure Lower_Ppwr_Sctl_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Sctl_Plm : LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Sctl_Plm(Reg    => Ppwr_Sctl_Plm,
                                     Addr   => Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Sctl_Plm.F_Read_Protection :=
           Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;

         Ppwr_Sctl_Plm.F_Write_Protection_Level0 :=
           Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level0_Enable;
         Ppwr_Sctl_Plm.F_Write_Protection_Level1 :=
           Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level1_Enable;
         Ppwr_Sctl_Plm.F_Write_Protection_Level2 :=
           Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask_Write_Protection_Level2_Enable;

         Bar0_Reg_Wr32_Ppwr_Sctl_Plm(Reg    => Ppwr_Sctl_Plm,
                                     Addr   => Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask,
                                     Status => Status);

      end loop Main_Block;
   end Lower_Ppwr_Sctl_Plm;

   procedure Lower_Ppwr_Wdtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Wdtmr_Plm : LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Wdtmr_Plm(Reg    => Ppwr_Wdtmr_Plm,
                                      Addr   => Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask,
                                      Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Wdtmr_Plm.F_Read_Protection :=
           Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Wdtmr_Plm.F_Write_Protection :=
           Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Wdtmr_Plm(Reg    => Ppwr_Wdtmr_Plm,
                                      Addr   => Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask,
                                      Status => Status);

      end loop Main_Block;
   end Lower_Ppwr_Wdtmr_Plm;

   procedure Lower_Ppwr_Fbif_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Fbif_Regioncfg_Plm : LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Fbif_Regioncfg_Plm(Reg    => Ppwr_Fbif_Regioncfg_Plm,
                                               Addr   => Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask,
                                               Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Fbif_Regioncfg_Plm.F_Read_Protection :=
           Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Fbif_Regioncfg_Plm.F_Write_Protection :=
           Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Fbif_Regioncfg_Plm(Reg    => Ppwr_Fbif_Regioncfg_Plm,
                                               Addr   => Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask,
                                               Status => Status);

      end loop Main_Block;
   end Lower_Ppwr_Fbif_Regioncfg_Plm;

   procedure Lower_Ppwr_Pmu_Msgq_Head_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Pmu_Msgq_Head_Plm : LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Pmu_Msgq_Head_Plm(Reg    => Ppwr_Pmu_Msgq_Head_Plm,
                                              Addr   => Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Pmu_Msgq_Head_Plm.F_Read_Protection :=
           Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Pmu_Msgq_Head_Plm.F_Write_Protection :=
           Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Pmu_Msgq_Head_Plm(Reg    => Ppwr_Pmu_Msgq_Head_Plm,
                                              Addr   => Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask,
                                              Status => Status);

      end loop Main_Block;
   end Lower_Ppwr_Pmu_Msgq_Head_Plm;

   procedure Lower_Ppwr_Pmu_Msgq_Tail_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Pmu_Msgq_Tail_Plm : LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Pmu_Msgq_Tail_Plm(Reg    => Ppwr_Pmu_Msgq_Tail_Plm,
                                              Addr   => Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Pmu_Msgq_Tail_Plm.F_Read_Protection :=
           Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Ppwr_Pmu_Msgq_Tail_Plm.F_Write_Protection :=
           Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppwr_Pmu_Msgq_Tail_Plm(Reg    => Ppwr_Pmu_Msgq_Tail_Plm,
                                              Addr   => Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask,
                                              Status => Status);

      end loop Main_Block;
   end Lower_Ppwr_Pmu_Msgq_Tail_Plm;

   procedure Lower_Therm_Smartfan_One_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Therm_Smartfan_One_Plm : LW_THERM_SMARTFAN_PRIV_LEVEL_MASK_1_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Therm_Smartfan_Plm(Reg    => Therm_Smartfan_One_Plm,
                                          Addr   => Lw_Therm_Smartfan_Priv_Level_Mask_1,
                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Therm_Smartfan_One_Plm.F_Read_Protection :=
           Lw_Therm_Smartfan_Priv_Level_Mask_1_Read_Protection_All_Levels_Enabled;
         Therm_Smartfan_One_Plm.F_Write_Protection :=
           Lw_Therm_Smartfan_Priv_Level_Mask_1_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Therm_Smartfan_Plm(Reg    => Therm_Smartfan_One_Plm,
                                          Addr   => Lw_Therm_Smartfan_Priv_Level_Mask_1,
                                          Status => Status);

      end loop Main_Block;
   end Lower_Therm_Smartfan_One_Plm;

   procedure Lower_Therm_Vidctrl_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Therm_Vidctrl_Plm : LW_THERM_VIDCTRL_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Therm_Vidctrl_Plm(Reg    => Therm_Vidctrl_Plm,
                                         Addr   => Lw_Therm_Vidctrl_Priv_Level_Mask,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Therm_Vidctrl_Plm.F_Read_Protection :=
           Lw_Therm_Vidctrl_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Therm_Vidctrl_Plm.F_Write_Protection :=
           Lw_Therm_Vidctrl_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Therm_Vidctrl_Plm(Reg    => Therm_Vidctrl_Plm,
                                         Addr   => Lw_Therm_Vidctrl_Priv_Level_Mask,
                                         Status => Status);

      end loop Main_Block;
   end Lower_Therm_Vidctrl_Plm;

   procedure Lower_Fuse_Iddqinfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fuse_Iddqinfo_Plm : LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Fuse_Iddqinfo_Plm(Reg    => Fuse_Iddqinfo_Plm,
                                         Addr   => Lw_Fuse_Iddqinfo_Priv_Level_Mask,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fuse_Iddqinfo_Plm.F_Read_Protection :=
           Lw_Fuse_Iddqinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fuse_Iddqinfo_Plm.F_Write_Protection :=
           Lw_Fuse_Iddqinfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fuse_Iddqinfo_Plm(Reg    => Fuse_Iddqinfo_Plm,
                                         Addr   => Lw_Fuse_Iddqinfo_Priv_Level_Mask,
                                         Status => Status);

      end loop Main_Block;
   end Lower_Fuse_Iddqinfo_Plm;

   procedure Lower_Fuse_Sram_Vmin_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fuse_Sram_Vmin_Plm : LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Fuse_Sram_Vmin_Plm(Reg    => Fuse_Sram_Vmin_Plm,
                                         Addr   => Lw_Fuse_Sram_Vmin_Priv_Level_Mask,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fuse_Sram_Vmin_Plm.F_Read_Protection :=
           Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fuse_Sram_Vmin_Plm.F_Write_Protection :=
           Lw_Fuse_Sram_Vmin_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fuse_Sram_Vmin_Plm(Reg    => Fuse_Sram_Vmin_Plm,
                                         Addr   => Lw_Fuse_Sram_Vmin_Priv_Level_Mask,
                                         Status => Status);

      end loop Main_Block;
   end Lower_Fuse_Sram_Vmin_Plm;

   procedure Lower_Fuse_Speedoinfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fuse_Speedoinfo_Plm : LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Fuse_Speedoinfo_Plm(Reg    => Fuse_Speedoinfo_Plm,
                                         Addr   => Lw_Fuse_Speedoinfo_Priv_Level_Mask,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fuse_Speedoinfo_Plm.F_Read_Protection :=
           Lw_Fuse_Speedoinfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fuse_Speedoinfo_Plm.F_Write_Protection :=
           Lw_Fuse_Speedoinfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fuse_Speedoinfo_Plm(Reg    => Fuse_Speedoinfo_Plm,
                                         Addr   => Lw_Fuse_Speedoinfo_Priv_Level_Mask,
                                         Status => Status);

      end loop Main_Block;
   end Lower_Fuse_Speedoinfo_Plm;

   procedure Lower_Fuse_Kappainfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fuse_Kappainfo_Plm : LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_REGISTER;
   begin

      Main_Block:
      for Unused_Var_Loop in 1 .. 1 loop
         Bar0_Reg_Rd32_Fuse_Kappainfo_Plm(Reg    => Fuse_Kappainfo_Plm,
                                         Addr   => Lw_Fuse_Kappainfo_Priv_Level_Mask,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fuse_Kappainfo_Plm.F_Read_Protection :=
           Lw_Fuse_Kappainfo_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fuse_Kappainfo_Plm.F_Write_Protection :=
           Lw_Fuse_Kappainfo_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fuse_Kappainfo_Plm(Reg    => Fuse_Kappainfo_Plm,
                                         Addr   => Lw_Fuse_Kappainfo_Priv_Level_Mask,
                                         Status => Status);

      end loop Main_Block;
   end Lower_Fuse_Kappainfo_Plm;



end Update_Pmu_Plms_Tu10x;
