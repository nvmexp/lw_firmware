-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x; use Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
with Dev_Master; use Dev_Master;
with Dev_Fpf; use Dev_Fpf;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

--  @summary
--  HW and Arch specific BAR0 register Access instances
--
--  @description
--  This package contains instances of procedures required to access BAR0 registers
--  Each register has seperate instances which become seperate functions at runtime

package Bar0_Reg_Rd_Wr_Instances is

   procedure Bar0_Reg_Rd32_Boot42 is new Bar0_Reg_Rd32_Generic (GENERIC_REGISTER => LW_PMC_BOOT_42_Register);
   procedure Bar0_Reg_Rd32_Fuse_Opt_Fuse is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER);
   procedure Bar0_Reg_Rd32_Fpf_Opt_Fuse is new Bar0_Reg_Rd32_Generic
     (GENERIC_REGISTER => LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER);

   ----- Linker Section Declarations------------
   pragma Linker_Section(Bar0_Reg_Rd32_Boot42, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fuse_Opt_Fuse, LINKER_SECTION_NAME);
   pragma Linker_Section(Bar0_Reg_Rd32_Fpf_Opt_Fuse, LINKER_SECTION_NAME);

end Bar0_Reg_Rd_Wr_Instances;
