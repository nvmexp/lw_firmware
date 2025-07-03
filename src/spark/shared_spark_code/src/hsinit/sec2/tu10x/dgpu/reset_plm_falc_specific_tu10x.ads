-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types.Boolean; use Lw_Types.Boolean;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  Falcon reset PLM operations
--
--  @description
--  This package contains procedures to raise/lower falcon reset PLM.
package Reset_Plm_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Update_Falcon_Reset_PLM(isRaise : LwBool; Status : out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ((Ghost_Hs_Init_States_Tracker = VHR_EMPTY_CHECKED or else
               Ghost_Hs_Init_States_Tracker = SCP_REGISTERS_CLEARED) and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => Ghost_Hs_Init_States_Tracker = RESET_PLM_PROTECTED,
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State, In_Out => Ghost_Hs_Init_States_Tracker),
     Depends => ( (Ghost_Hs_Init_States_Tracker, Status) => null, null => (Ghost_Hs_Init_States_Tracker, isRaise) ),
     Linker_Section => LINKER_SECTION_NAME;

end Reset_Plm_Falc_Specific_Tu10x;
