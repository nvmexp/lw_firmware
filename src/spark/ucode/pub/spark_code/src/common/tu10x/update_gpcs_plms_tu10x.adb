-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Pub_Gpcs_Plm_Bar0_Access_Instances; use Pub_Gpcs_Plm_Bar0_Access_Instances;
with Dev_Graphics_Nobundle_Ada; use Dev_Graphics_Nobundle_Ada;

package body Update_Gpcs_Plms_Tu10x
with SPARK_Mode => On
is

   procedure Lower_Gpcs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- 1. LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK
         Lower_Gpccs_Pvs_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 2. LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR and LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK
         Lower_Gpccs_Rc_Lane_Cmd_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 3. LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK
         Lower_Tpccs_Sm_Private_Control_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 4. LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK
         Lower_Gpccs_Arb_Wpr_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 5. LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK
         Lower_Tpccs_Arb_Wpr_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 6. LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK
         Lower_Gpccs_Exe_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 7. LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK
         Lower_Gpccs_Mthdctx_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 8. LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK
         Lower_Gpccs_Irqtmr_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 9. LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK
         Lower_Gpccs_Arb_Regioncfg_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 10. LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK
         Lower_Tpccs_Arb_Regioncfg_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 11. LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT and LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK
         Lower_Tpccs_Rc_Lane_Cmd_Plm(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 12. LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK
         Lower_Gpcs_Mmu_Plm(Status => Status);

      end loop Main_Block;
   end Lower_Gpcs_Plms;


   procedure Lower_Gpccs_Pvs_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpccs_Pvs_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Pvs_Plm(Reg  => Gpccs_Pvs_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpccs_Pvs_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpccs_Pvs_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Gpcs_Pvs_Plm(Reg  => Gpccs_Pvs_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;
   end Lower_Gpccs_Pvs_Plm;

   procedure Lower_Gpccs_Rc_Lane_Cmd_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpccs_Rc_Lane_Cmd_Plm  : LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER;
      Gpcs_Gpccs_Falcon_Addr : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         --
         -- Special Case
         -- The LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR register must be written to 0x3F before the LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK is written.
         --
         Gpcs_Gpccs_Falcon_Addr.F_Lsb := 16#0000_003F#;
         Gpcs_Gpccs_Falcon_Addr.F_Msb := 0;
         Bar0_Reg_Wr32_Gpcs_Gpccs_Falcon_Addr(Reg => Gpcs_Gpccs_Falcon_Addr,
                                              Addr   => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Addr,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Gpcs_Rc_Lane_Cmd_Plm(Reg  => Gpccs_Rc_Lane_Cmd_Plm,
                                            Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask,
                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpccs_Rc_Lane_Cmd_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpccs_Rc_Lane_Cmd_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Gpcs_Rc_Lane_Cmd_Plm(Reg  => Gpccs_Rc_Lane_Cmd_Plm,
                                            Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask,
                                            Status => Status);
      end loop Main_Block;

   end Lower_Gpccs_Rc_Lane_Cmd_Plm;

   procedure Lower_Gpccs_Arb_Wpr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpccs_Arb_Wpr_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 ..1 loop

         Bar0_Reg_Rd32_Gpcs_Arb_Wpr_Plm(Reg  => Gpccs_Arb_Wpr_Plm,
                                        Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask,
                                        Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpccs_Arb_Wpr_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpccs_Arb_Wpr_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Arb_Wpr_Plm(Reg  => Gpccs_Arb_Wpr_Plm,
                                        Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask,
                                        Status => Status);
      end loop Main_Block;

   end Lower_Gpccs_Arb_Wpr_Plm;

   procedure Lower_Gpccs_Exe_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpccs_Exe_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Exe_Plm(Reg => Gpccs_Exe_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpccs_Exe_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpccs_Exe_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Exe_Plm(Reg => Gpccs_Exe_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;
   end Lower_Gpccs_Exe_Plm;


   procedure Lower_Gpccs_Mthdctx_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpccs_Mthdctx_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Mthdctx_Plm(Reg => Gpccs_Mthdctx_Plm,
                                        Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask,
                                        Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpccs_Mthdctx_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpccs_Mthdctx_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Mthdctx_Plm(Reg => Gpccs_Mthdctx_Plm,
                                        Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask,
                                        Status => Status);
      end loop Main_Block;

   end Lower_Gpccs_Mthdctx_Plm;


   procedure Lower_Gpccs_Irqtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpcs_Irqtmr_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Irqtmr_Plm(Reg  => Gpcs_Irqtmr_Plm,
                                       Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpcs_Irqtmr_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpcs_Irqtmr_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Irqtmr_Plm(Reg  => Gpcs_Irqtmr_Plm,
                                       Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);
      end loop Main_Block;

   end Lower_Gpccs_Irqtmr_Plm;

   procedure Lower_Gpccs_Arb_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpcs_Arb_Regioncfg_Plm : LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Arb_Regioncfg_Plm(Reg  => Gpcs_Arb_Regioncfg_Plm,
                                              Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpcs_Arb_Regioncfg_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpcs_Arb_Regioncfg_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Arb_Regioncfg_Plm(Reg  => Gpcs_Arb_Regioncfg_Plm,
                                              Addr => Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                              Status => Status);
      end loop Main_Block;

   end Lower_Gpccs_Arb_Regioncfg_Plm;

   procedure Lower_Tpccs_Sm_Private_Control_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Tpcs_Sm_Private_Control_Plm : LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Tpcs_Sm_Private_Control_Plm(Reg => Tpcs_Sm_Private_Control_Plm,
                                                   Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask,
                                                   Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Tpcs_Sm_Private_Control_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Tpcs_Sm_Private_Control_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Tpcs_Sm_Private_Control_Plm(Reg  => Tpcs_Sm_Private_Control_Plm,
                                                   Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask,
                                                   Status => Status);
      end loop Main_Block;

   end Lower_Tpccs_Sm_Private_Control_Plm;

   procedure Lower_Tpccs_Arb_Wpr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Tpccs_Arb_Wpr_Plm : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Tpccs_Arb_Plm(Reg => Tpccs_Arb_Wpr_Plm,
                                     Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Tpccs_Arb_Wpr_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Tpccs_Arb_Wpr_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Tpccs_Arb_Plm(Reg => Tpccs_Arb_Wpr_Plm,
                                     Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask,
                                     Status => Status);
      end loop Main_Block;

   end Lower_Tpccs_Arb_Wpr_Plm;

   procedure Lower_Tpccs_Arb_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Tpccs_Arb_Regioncfg_Plm : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Tpccs_Arb_Regioncfg_Plm(Reg  => Tpccs_Arb_Regioncfg_Plm,
                                               Addr =>
                                                 Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                               Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Tpccs_Arb_Regioncfg_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Tpccs_Arb_Regioncfg_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Tpccs_Arb_Regioncfg_Plm(Reg  => Tpccs_Arb_Regioncfg_Plm,
                                               Addr =>
                                                 Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                               Status => Status);
      end loop Main_Block;

   end Lower_Tpccs_Arb_Regioncfg_Plm;

   procedure Lower_Tpccs_Rc_Lane_Cmd_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Tpccs_Rc_Lane_Cmd_Plm : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER;
      Tpccs_Rc_Lane_Select  : LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         --
         -- Special Case
         -- The LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT register must be written to LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_INDEX_ALL
         -- before the LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK register is written.
         --
         Tpccs_Rc_Lane_Select.F_Index := Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Index_All;
         Tpccs_Rc_Lane_Select.F_Auto_Increment := Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select_Auto_Increment_Disabled;
         Bar0_Reg_Wr32_Tpccs_Rc_Lane_Select (Reg     => Tpccs_Rc_Lane_Select,
                                             Addr    => Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Select,
                                             Status  => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Tpccs_Rc_Lane_Cmd_Plm(Reg  => Tpccs_Rc_Lane_Cmd_Plm,
                                             Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask,
                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Tpccs_Rc_Lane_Cmd_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Tpccs_Rc_Lane_Cmd_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Tpccs_Rc_Lane_Cmd_Plm(Reg  => Tpccs_Rc_Lane_Cmd_Plm,
                                             Addr => Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask,
                                             Status => Status);
      end loop Main_Block;

   end Lower_Tpccs_Rc_Lane_Cmd_Plm;

   procedure Lower_Gpcs_Mmu_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Gpcs_Mmu_Plm : LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Gpcs_Mmu_Plm(Reg  => Gpcs_Mmu_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Gpcs_Mmu_Plm.F_Read_Protection :=
           Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Gpcs_Mmu_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Gpcs_Mmu_Plm(Reg  => Gpcs_Mmu_Plm,
                                    Addr => Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;

   end Lower_Gpcs_Mmu_Plm;


end Update_Gpcs_Plms_Tu10x;
