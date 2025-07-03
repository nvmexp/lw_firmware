-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Provide access to DEV FBPA war module.
--
--  @description
--  This package contains procedures to provide access to fbpa using decode traps.
--  WAR to allow CPU(level0) to access FBPA_PM registers
--
--
--  Traceability -
--        \ImplementsUnitDesign{Dev_Fbpa_War}
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

package Dev_Fbpa_War is
   --  Requirement: This procedure shall do the following:
   --               1) Ilwoke procedure to apply FBPA war for tu10x.
   --  @param Reg  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 If war was applied successfuly or else error.
   procedure Pub_Provide_Access_Fbpa(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Global => null,
       Linker_Section => LINKER_SECTION_NAME;
end Dev_Fbpa_War;
