-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;
with Dev_Fpf; use Dev_Fpf;

--  @summary
--  Defines functions that are specific to a Ucode
--
--  @description
--  Ucode_Specific_Initializations  package defines functions that return Addr of
--  fuse registers read for SW revocation.

package Ucode_Specific_Initializations
with SPARK_Mode => On
is

   function Get_Ucode_Specific_Fuse_Reg_Addr
     return BAR0_ADDR is
     (LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_ADDR)
     with
       Global => null,
       Depends => (Get_Ucode_Specific_Fuse_Reg_Addr'Result => null),
       Linker_Section => LINKER_SECTION_NAME;

   function Get_Ucode_Specific_Fpf_Reg_Addr
     return BAR0_ADDR is
     (LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_ADDR)
     with
       Global => null,
       Depends => (Get_Ucode_Specific_Fpf_Reg_Addr'Result => null),
       Linker_Section => LINKER_SECTION_NAME;

end Ucode_Specific_Initializations;
