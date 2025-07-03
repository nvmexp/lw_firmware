-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Provide access to FBPA war for TU10X.
--
--  @description
--  This package contains procedures to provide access to fbpa using decode traps.
--  WAR to allow CPU(level0) to access FBPA_PM registers
--
--
--  Traceability -
--        \ImplementsUnitDesign{Fbpa_War_tu10x}
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

package Fbpa_War_tu10x is
   --  Requirement: This procedure shall do the following:
   --               1) Acquire mutex to access decode traps in atomic manner.
   --               2) Sets the decode traps 12,13,14 for fbpa war.
   --               3) Release the acquired mutex.
   --  @param Status Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 if  successfull else error.
   procedure Provide_Access_Fbpa_To_Cpu_Bug_2369597(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Global => null,
       Linker_Section => LINKER_SECTION_NAME;


private
     --  Requirement: This procedure shall do the following:
   --               1) Sets the decode traps 12.
     --  @param Status Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status.
   --                                     UCODE_ERR_CODE_NOERROR if successfull else error.
   procedure Setting_Decode_Trap12(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Global => null,
       Linker_Section => LINKER_SECTION_NAME;


     --  Requirement: This procedure shall do the following:
   --               1) Sets the decode traps 13.
     --  @param Status Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status.
   --                                     UCODE_ERR_CODE_NOERROR if successfull else error.
   procedure Setting_Decode_Trap13(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Global => null,
       Linker_Section => LINKER_SECTION_NAME;


     --  Requirement: This procedure shall do the following:
   --               1) Sets the decode traps 14.
     --  @param Status Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status.
   --                                     UCODE_ERR_CODE_NOERROR if successfull else error.
   procedure Setting_Decode_Trap14(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Global => null,
       Linker_Section => LINKER_SECTION_NAME;
end Fbpa_War_tu10x;
