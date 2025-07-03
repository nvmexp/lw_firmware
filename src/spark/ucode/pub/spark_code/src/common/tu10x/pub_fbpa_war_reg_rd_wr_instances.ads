-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_


with Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x; use Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Pri_Ringstation_Sys_Ada; use Dev_Pri_Ringstation_Sys_Ada;

--  @summary
--  Provide access to FBPA war for TU10X.
--
--  @description
--  This package contains procedures to all the BAR0 reg read/write required for FBPA war.
--  WAR to allow CPU(level0) to access FBPA_PM registers
--
--
--  Traceability -
--        \ImplementsUnitDesign{Pub_Fbpa_War_Reg_Rd_Wr_Instances}
package Pub_Fbpa_War_Reg_Rd_Wr_Instances is

   procedure Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap12_Action is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER);

   procedure Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap13_Action is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER);

   procedure Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap14_Action is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER);

   --------

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data1 is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Mask is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data2 is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Action is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data1 is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Mask is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data2 is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Action is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data1 is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Mask is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data2 is new Bar0_Reg_Wr32_Readback_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_REGISTER);

   procedure Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Action is new Bar0_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER);



      ----- Linker Section Declarations------------

   pragma Linker_Section(Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap12_Action, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap13_Action, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap14_Action, LINKER_SECTION_NAME);

   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data1, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Mask, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data2, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Action, LINKER_SECTION_NAME);

   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data1, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Mask, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data2, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Action, LINKER_SECTION_NAME);

   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data1, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Mask, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data2, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Action, LINKER_SECTION_NAME);




end Pub_Fbpa_War_Reg_Rd_Wr_Instances;
