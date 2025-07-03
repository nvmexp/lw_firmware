-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x; use Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
with Dev_Graphics_Nobundle_Ada; use Dev_Graphics_Nobundle_Ada;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

package Pub_Gpcs_Plm_Bar0_Access_Instances is

   -- GPCS
   procedure Bar0_Reg_Rd32_Gpcs_Pvs_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Pvs_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_PVS_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Gpccs_Falcon_Addr is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_ADDR_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Arb_Wpr_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Arb_Wpr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Exe_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Exe_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Mthdctx_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Mthdctx_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_MTHDCTX_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Irqtmr_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Irqtmr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Arb_Regioncfg_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Gpcs_Arb_Regioncfg_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_GPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Tpcs_Sm_Private_Control_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Tpcs_Sm_Private_Control_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_SM_PRIVATE_CONTROL_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Tpccs_Arb_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Tpccs_Arb_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_WPR_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Tpccs_Arb_Regioncfg_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);
   procedure Bar0_Reg_Wr32_Tpccs_Arb_Regioncfg_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_ARB_FALCON_REGIONCFG_PRIV_LEVEL_MASK_REGISTER);

   procedure Bar0_Reg_Rd32_Gpcs_Mmu_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_REGISTER)
     with
       Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Gpcs_Mmu_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_MMU_PRIV_LEVEL_MASK_REGISTER)
     with
       Linker_Section => LINKER_SECTION_NAME;

   procedure Bar0_Reg_Rd32_Tpccs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER)
     with
       Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Tpccs_Rc_Lane_Cmd_Plm is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_CMD_PRIV_LEVEL_MASK_REGISTER)
     with
       Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Tpccs_Rc_Lane_Select is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PGRAPH_PRI_GPCS_TPCS_TPCCS_RC_LANE_SELECT_REGISTER)
     with
       Linker_Section => LINKER_SECTION_NAME;

   --- Linker Section ----

   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Pvs_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Pvs_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Rc_Lane_Cmd_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Rc_Lane_Cmd_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Gpccs_Falcon_Addr, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Arb_Wpr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Arb_Wpr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Exe_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Exe_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Mthdctx_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Mthdctx_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Irqtmr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Irqtmr_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Gpcs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Gpcs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Tpcs_Sm_Private_Control_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Tpcs_Sm_Private_Control_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Tpccs_Arb_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Tpccs_Arb_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Tpccs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Tpccs_Arb_Regioncfg_Plm, LINKER_SECTION_NAME);

end Pub_Gpcs_Plm_Bar0_Access_Instances;
