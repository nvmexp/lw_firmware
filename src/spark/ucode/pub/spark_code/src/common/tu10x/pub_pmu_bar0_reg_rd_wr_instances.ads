-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x; use Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
with Dev_Pwr_Pri_Ada; use Dev_Pwr_Pri_Ada;
with Dev_Therm_Ada; use Dev_Therm_Ada;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;

package Pub_Pmu_Bar0_Reg_Rd_Wr_Instances is

   procedure Bar0_Reg_Rd32_Ppwr_Exe_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Irqtmr_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Dmem_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Sctl_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Wdtmr_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Fbif_Regioncfg_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Pmu_Msgq_Head_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Ppwr_Pmu_Msgq_Tail_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Therm_Smartfan_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_THERM_SMARTFAN_PRIV_LEVEL_MASK_1_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Therm_Vidctrl_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_THERM_VIDCTRL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Fuse_Iddqinfo_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Fuse_Sram_Vmin_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Fuse_Speedoinfo_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Rd32_Fuse_Kappainfo_Plm is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;

   -- PMU Reset registers
   procedure Bar0_Reg_Rd32_Ppwr_Falcon_Engine is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_ENGINE_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
    procedure Bar0_Reg_Rd32_Ppwr_Falcon_Cpuctl is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_CPUCTL_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
    procedure Bar0_Reg_Rd32_Ppwr_Falcon_Sctl is new Bar0_Reg_Rd32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_SCTL_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;

 ------------------------------------------------------------------------------------------------------
   procedure Bar0_Reg_Wr32_Ppwr_Exe_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_EXE_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Irqtmr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_IRQTMR_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Dmem_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_DMEM_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Sctl_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_SCTL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Wdtmr_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_WDTMR_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Fbif_Regioncfg_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_FBIF_REGIONCFG_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Pmu_Msgq_Head_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_PMU_MSGQ_HEAD_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Ppwr_Pmu_Msgq_Tail_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_PPWR_PMU_MSGQ_TAIL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Therm_Smartfan_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_THERM_SMARTFAN_PRIV_LEVEL_MASK_1_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Therm_Vidctrl_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_THERM_VIDCTRL_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Fuse_Iddqinfo_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_FUSE_IDDQINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Fuse_Sram_Vmin_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_FUSE_SRAM_VMIN_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Fuse_Speedoinfo_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_FUSE_SPEEDOINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;
   procedure Bar0_Reg_Wr32_Fuse_Kappainfo_Plm is new Bar0_Reg_Wr32_Readback_Generic
     ( GENERIC_REGISTER => LW_FUSE_KAPPAINFO_PRIV_LEVEL_MASK_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;

   -- PMU Reset registers
   procedure Bar0_Reg_Wr32_Ppwr_Falcon_Engine is new Bar0_Reg_Wr32_Generic
     ( GENERIC_REGISTER => LW_PPWR_FALCON_ENGINE_REGISTER )
     with Linker_Section => LINKER_SECTION_NAME;

end Pub_Pmu_Bar0_Reg_Rd_Wr_Instances;
