-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x; use Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Graphics_Nobundle_Ada; use Dev_Graphics_Nobundle_Ada;
with Dev_Gc6_Island_Ada; use Dev_Gc6_Island_Ada;
with Dev_Pri_Ringstation_Sys_Ada; use Dev_Pri_Ringstation_Sys_Ada;
with Dev_Fb_Ada; use Dev_Fb_Ada;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;

package Pub_Fecs_Bar0_Reg_Rd_Wr_Instances is

   -- FECS
   procedure Bar0_Reg_Rd32_Fecs_Pvs_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Pvs_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_PVS_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Arb_Wpr_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Arb_Wpr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Mthdctx_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Mthdctx_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Irqtmr_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Irqtmr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Arb_Regioncfg_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Arb_Regioncfg_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Exe_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Exe_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fecs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fecs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER);
      procedure Bar0_Reg_Wr32_Fecs_Falcon_Addr is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_FECS_FALCON_ADDR_REGISTER);

   procedure Bar0_Reg_Rd32_Ppriv_Sys_Priv_Holdoff_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Ppriv_Sys_Priv_Holdoff_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRIV_HOLDOFF_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Pfb_Pri_Mmu_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Pfb_Pri_Mmu_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PFB_PRI_MMU_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Fuse_Floorsweep_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Fuse_Floorsweep_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_FUSE_FLOORSWEEP_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGC6_AON_SELWRE_SCRATCH_GROUP_15_PRIV_LEVEL_MASK_REGISTER);

   ----- Linker Section Declarations------------

   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Pvs_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Pvs_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Arb_Wpr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Arb_Wpr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Mthdctx_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Mthdctx_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Irqtmr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Irqtmr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Exe_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Exe_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fecs_Rc_Lane_Cmd_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Rc_Lane_Cmd_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fecs_Falcon_Addr, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Ppriv_Sys_Priv_Holdoff_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Priv_Holdoff_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Pfb_Pri_Mmu_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Pfb_Pri_Mmu_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fuse_Floorsweep_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Fuse_Floorsweep_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Pgc6_Aon_Selwre_Scratch_Group_15_Plm, LINKER_SECTION_NAME);

end Pub_Fecs_Bar0_Reg_Rd_Wr_Instances;
