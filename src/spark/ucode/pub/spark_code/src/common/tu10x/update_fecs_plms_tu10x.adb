-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Graphics_Nobundle_Ada; use Dev_Graphics_Nobundle_Ada;
with Dev_Gc6_Island_Ada; use Dev_Gc6_Island_Ada;
with Dev_Pri_Ringstation_Sys_Ada; use Dev_Pri_Ringstation_Sys_Ada;
with Dev_Fb_Ada; use Dev_Fb_Ada;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;
with Pub_Fecs_Bar0_Reg_Rd_Wr_Instances; use Pub_Fecs_Bar0_Reg_Rd_Wr_Instances;

package body Update_Fecs_Plms_Tu10x
with SPARK_Mode => On
is

   procedure Lower_Fecs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- 1) RMW LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK
         Lower_Fecs_Pvs_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 2) RMW LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER
         Lower_Fecs_Rc_Lane_Cmd_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 3) RMW LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER
         Lower_Fecs_Arb_Wpr_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 4) RMW LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER
         Lower_Fecs_Mthdctx_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 5) RMW LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER
         Lower_Fecs_Irqtmr_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 6) RMW LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK
         Lower_Fecs_Arb_Falcon_Regioncfg_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 7) RMW LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER
         Lower_Fecs_Exe_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 8) RMW LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK
         Lower_Ppriv_Sys_Priv_Holdoff_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 9) RMW LW_PFB_PRI_MMU_PRIV_LEVEL_MASK
         Lower_Pfb_Pri_Mmu_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 10) RMW LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK
         Lower_Fuse_Floorsweep_Plms(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- 11) RMW LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK
         -- TODO adjoshi: This PLM is source isolated to FECS by VBIOS.
         -- Uncomment when source isolation is removed.
         -- Lower_Pgc6_Aon_Selwre_Scratch_Group_15_Plms(Status => Status);

      end loop Main_Block;

   end Lower_Fecs_Plms;


   procedure Lower_Fecs_Rc_Lane_Cmd_Plms( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Rc_Lane_Cmd_Plm : LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER;
      Fecs_Falcon_Addr     : LW_PGRAPH_PRI_FECS_FALCON_ADDR_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         --
         -- SPECIAL CASE:
         -- The LW_PGRAPH_PRI_FECS_FALCON_ADDR register must be written to 0x3F before the LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK is written.
         --
         Fecs_Falcon_Addr.F_Lsb := 16#0000_003F#;
         Fecs_Falcon_Addr.F_Msb := 0;
         Bar0_Reg_Wr32_Fecs_Falcon_Addr (Reg    => Fecs_Falcon_Addr,
                                         Addr   => Lw_Pgraph_Pri_Fecs_Falcon_Addr,
                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Fecs_Rc_Lane_Cmd_Plm(Reg  => Fecs_Rc_Lane_Cmd_Plm,
                                            Addr => Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask,
                                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Rc_Lane_Cmd_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Rc_Lane_Cmd_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Fecs_Rc_Lane_Cmd_Plm(Reg  => Fecs_Rc_Lane_Cmd_Plm,
                                            Addr => Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask,
                                            Status => Status);
      end loop Main_Block;

   end Lower_Fecs_Rc_Lane_Cmd_Plms;

   procedure Lower_Fecs_Arb_Wpr_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Arb_Wpr_Plm : LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Arb_Wpr_Plm(Reg  => Fecs_Arb_Wpr_Plm,
                                        Addr => Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask,
                                        Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Arb_Wpr_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Arb_Wpr_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fecs_Arb_Wpr_Plm(Reg  => Fecs_Arb_Wpr_Plm,
                                        Addr => Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask,
                                        Status => Status);
      end loop Main_Block;

   end Lower_Fecs_Arb_Wpr_Plms;

   procedure Lower_Fecs_Mthdctx_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Mthdctx_Plm : LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Mthdctx_Plm(Reg  => Fecs_Mthdctx_Plm,
                                        Addr => Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask,
                                        Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Mthdctx_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Mthdctx_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fecs_Mthdctx_Plm(Reg  => Fecs_Mthdctx_Plm,
                                        Addr => Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask,
                                        Status => Status);
      end loop Main_Block;
   end Lower_Fecs_Mthdctx_Plms;

   procedure Lower_Fecs_Irqtmr_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Irqtmr_Plm : LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Irqtmr_Plm(Reg  => Fecs_Irqtmr_Plm,
                                       Addr => Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Irqtmr_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Irqtmr_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fecs_Irqtmr_Plm(Reg  => Fecs_Irqtmr_Plm,
                                       Addr => Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask,
                                       Status => Status);
      end loop Main_Block;
   end Lower_Fecs_Irqtmr_Plms;

   procedure Lower_Fecs_Arb_Falcon_Regioncfg_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Arb_Regioncfg_Plm : LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Arb_Regioncfg_Plm(Reg  => Fecs_Arb_Regioncfg_Plm,
                                              Addr => Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Arb_Regioncfg_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Arb_Regioncfg_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fecs_Arb_Regioncfg_Plm(Reg  => Fecs_Arb_Regioncfg_Plm,
                                              Addr => Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask,
                                              Status => Status);
      end loop Main_Block;

   end Lower_Fecs_Arb_Falcon_Regioncfg_Plms;

   procedure Lower_Fecs_Exe_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Exe_Plm : LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Exe_Plm(Reg  => Fecs_Exe_Plm,
                                    Addr => Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Exe_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Exe_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fecs_Exe_Plm(Reg  => Fecs_Exe_Plm,
                                    Addr => Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;

   end Lower_Fecs_Exe_Plms;

   procedure Lower_Fecs_Pvs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fecs_Pvs_Plm : LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_REGISTER;

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fecs_Pvs_Plm(Reg  => Fecs_Pvs_Plm,
                                    Addr => Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fecs_Pvs_Plm.F_Read_Protection  :=
           Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fecs_Pvs_Plm.F_Write_Protection :=
           Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask_Write_Protection_All_Levels_Enabled_Fuse0;

         Bar0_Reg_Wr32_Fecs_Pvs_Plm(Reg  => Fecs_Pvs_Plm,
                                    Addr => Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;

   end Lower_Fecs_Pvs_Plms;


   procedure Lower_Ppriv_Sys_Priv_Holdoff_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Sys_Priv_Holdoff : LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Ppriv_Sys_Priv_Holdoff_Plm(Reg  => Sys_Priv_Holdoff,
                                    Addr => Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Sys_Priv_Holdoff.F_Read_Protection  :=
           Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Sys_Priv_Holdoff.F_Write_Protection :=
           Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Ppriv_Sys_Priv_Holdoff_Plm(Reg  => Sys_Priv_Holdoff,
                                    Addr => Lw_Ppriv_Sys_Priv_Holdoff_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;

   end Lower_Ppriv_Sys_Priv_Holdoff_Plms;

   procedure Lower_Pfb_Pri_Mmu_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Pfb_Pri_Mmu : LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Pfb_Pri_Mmu_Plm(Reg  => Pfb_Pri_Mmu,
                                    Addr => Lw_Pfb_Pri_Mmu_Priv_Level_Mask,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Pfb_Pri_Mmu.F_Read_Protection  :=
           Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Pfb_Pri_Mmu.F_Write_Protection :=
           Lw_Pfb_Pri_Mmu_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Pfb_Pri_Mmu_Plm(Reg  => Pfb_Pri_Mmu,
                                    Addr => Lw_Pfb_Pri_Mmu_Priv_Level_Mask,
                                    Status => Status);
      end loop Main_Block;

   end Lower_Pfb_Pri_Mmu_Plms;

   procedure Lower_Fuse_Floorsweep_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Fuse_Floorsweep_Plm : LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Fuse_Floorsweep_Plm(Reg  => Fuse_Floorsweep_Plm,
                                              Addr => Lw_Fuse_Floorsweep_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Fuse_Floorsweep_Plm.F_Read_Protection  :=
           Lw_Fuse_Floorsweep_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Fuse_Floorsweep_Plm.F_Write_Protection :=
           Lw_Fuse_Floorsweep_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Fuse_Floorsweep_Plm(Reg  => Fuse_Floorsweep_Plm,
                                              Addr => Lw_Fuse_Floorsweep_Priv_Level_Mask,
                                              Status => Status);
      end loop Main_Block;

   end Lower_Fuse_Floorsweep_Plms;

   procedure Lower_Pgc6_Aon_Selwre_Scratch_Group_15_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Pgc6_Aon_Selwre_Scratch_Group_15_Plm : LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Bar0_Reg_Rd32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm(Reg  => Pgc6_Aon_Selwre_Scratch_Group_15_Plm,
                                              Addr => Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask,
                                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Pgc6_Aon_Selwre_Scratch_Group_15_Plm.F_Read_Protection  :=
           Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Read_Protection_All_Levels_Enabled;
         Pgc6_Aon_Selwre_Scratch_Group_15_Plm.F_Write_Protection :=
           Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask_Write_Protection_All_Levels_Enabled;

         Bar0_Reg_Wr32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm(Reg  => Pgc6_Aon_Selwre_Scratch_Group_15_Plm,
                                              Addr => Lw_Pgc6_Aon_Selwre_Scratch_Group_15_Priv_Level_Mask,
                                              Status => Status);
      end loop Main_Block;

   end Lower_Pgc6_Aon_Selwre_Scratch_Group_15_Plms;

end Update_Fecs_Plms_Tu10x;

