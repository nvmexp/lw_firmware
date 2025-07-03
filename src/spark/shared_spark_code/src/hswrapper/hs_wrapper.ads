-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

--  @summary
--  HS Wrapper Entry
--
--  @description
--  This package contains procedure which is in HS and will be called by NS entity
--  after HW Sig verif. It will call the Inline, Non Inline HS Init procedures followed
--  by the HS Binary core.

package Hs_Wrapper is
     procedure Hs_Wrapper_Entry
     with
       Pre => Ghost_Hs_Init_States_Tracker = HS_INIT_NOT_STARTED,
       Export => True,
       Convention => C,
       External_Name => "hs_wrapper_entry";
   pragma Machine_Attribute (Hs_Wrapper_Entry, "no_stack_protect");
private
  procedure Hs_Entry
     with
       Pre => Ghost_Hs_Init_States_Tracker = SP_SETUP,
       Global =>(In_Out => Ghost_Hs_Init_States_Tracker),
     Linker_Section => LINKER_SECTION_NAME;

end Hs_Wrapper;
