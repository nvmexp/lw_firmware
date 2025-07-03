-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Reg_Rd_Wr_Csb_Falc_Specific_Tu10x; use Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;
with Lw_Types; use Lw_Types;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

--  @summary
--  HW and Arch specific CSB register Access instances
--
--  @description
--  This package contains instances of procedures required to access CSB registers
--  Each register has seperate instances which become seperate functions at runtime

package Csb_Reg_Rd_Wr_Instances
with SPARK_Mode => On
is

   procedure Csb_Reg_Rd32_LwU32 is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LwU32);
   pragma Linker_Section(Csb_Reg_Rd32_LwU32, LINKER_SECTION_NAME);

   procedure Csb_Reg_Rd32_Hwcfg1 is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LW_CSEC_FALCON_HWCFG1_REGISTER);
   procedure Csb_Reg_Rd32_EngineId is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LW_CSEC_FALCON_ENGID_REGISTER);
   procedure Csb_Reg_Rd32_Timestamp is new Csb_Reg_Rd32_Generic(GENERIC_REGISTER => LW_CSEC_FALCON_PTIMER0_REGISTER);
   procedure Csb_Reg_Rd32_Ptimer0 is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LW_CSEC_FALCON_PTIMER0_REGISTER);
   procedure Csb_Reg_Rd32_Doc_Ctrl is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LW_CSEC_FALCON_DOC_CTRL_REGISTER);
   procedure Csb_Reg_Wr32_LwU32 is new Csb_Reg_Wr32_Generic (GENERIC_REGISTER => LwU32);
   procedure Csb_Reg_Rd32_Dic_Ctrl is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER => LW_CSEC_FALCON_DIC_CTRL_REGISTER);
   procedure Csb_Reg_Wr32_Dic_Ctrl is new Csb_Reg_Wr32_Generic (GENERIC_REGISTER =>
                                                                   LW_CSEC_FALCON_DIC_CTRL_REGISTER);
   procedure Csb_Reg_Rd32_Flcn_Reset_PLM is new Csb_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_REGISTER);
   procedure Csb_Reg_Wr32_Flcn_Reset_PLM is new Csb_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_CSEC_FALCON_RESET_PRIV_LEVEL_MASK_REGISTER);

   procedure Csb_Reg_Rd32_Bar0Csr is new Csb_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_CSEC_BAR0_CSR_REGISTER);

   procedure Csb_Reg_Wr32_Stall_Bar0Csr is new Csb_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_CSEC_BAR0_CSR_REGISTER);

   procedure Csb_Reg_Wr32_Mailbox is new Csb_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_CSEC_FALCON_MAILBOX0_REGISTER);

   procedure Csb_Reg_Rd32_Vhrcfg_Entnum is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER =>
                                                                        LW_CSEC_FALCON_VHRCFG_ENTNUM_REGISTER);

   procedure Csb_Reg_Rd32_Vhrcfg_Base is new Csb_Reg_Rd32_Generic (GENERIC_REGISTER =>
                                                                      LW_CSEC_FALCON_VHRCFG_BASE_REGISTER);

   procedure Csb_Reg_Wr32_Bar0_Tmout is new Csb_Reg_Wr32_Generic(GENERIC_REGISTER => LW_CSEC_BAR0_TMOUT_REGISTER);


   procedure Csb_Reg_Rd32_Hwcfg is new Csb_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_CSEC_FALCON_HWCFG_REGISTER);
   procedure Csb_Reg_Wr32_Hwcfg is new Csb_Reg_Wr32_Generic
     (GENERIC_REGISTER => LW_CSEC_FALCON_HWCFG_REGISTER);


   --   package Csb_Reg_Access_Cmembase is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
   --     (GENERIC_REGISTER => LW_CSEC_FALCON_CMEMBASE_REGISTER);

   --   package Csb_Reg_Access_Stackcfg is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
   --     (GENERIC_REGISTER => LW_CSEC_FALCON_STACKCFG_REGISTER);

   --   package Csb_Reg_Access_Csberrstat is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
   --     (GENERIC_REGISTER => LW_CSEC_FALCON_CSBERRSTAT_REGISTER);




   ----- Linker Section Declarations------------

   pragma Linker_Section(Csb_Reg_Rd32_Ptimer0, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Doc_Ctrl, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_LwU32, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Dic_Ctrl, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Dic_Ctrl, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Flcn_Reset_PLM, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Flcn_Reset_PLM, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_EngineId, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Hwcfg1, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Timestamp, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Bar0Csr, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Stall_Bar0Csr, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Mailbox, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Vhrcfg_Entnum, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Bar0_Tmout, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Vhrcfg_Base, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Rd32_Hwcfg, LINKER_SECTION_NAME);
   pragma Linker_Section(Csb_Reg_Wr32_Hwcfg, LINKER_SECTION_NAME);
end Csb_Reg_Rd_Wr_Instances;
