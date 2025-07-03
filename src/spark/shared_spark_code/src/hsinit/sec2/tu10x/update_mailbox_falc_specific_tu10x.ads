-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  HW and Arch Specific Mailbox Update Procedures
--
--  @description
--  This package contains procedure which will UPDATE SEC2 MAILBOX with value of
--  Status, that will be read by RM.

package Update_Mailbox_Falc_Specific_Tu10x is

   procedure Update_Status_In_Mailbox( Status : LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Ghost_Hs_Exit_State_Tracker = HS_EXIT_NOT_STARTED and then
               Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => Ghost_Hs_Exit_State_Tracker = MAILBOX_UPDATED,
     Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                In_Out => Ghost_Hs_Exit_State_Tracker),
     Depends => (Ghost_Hs_Exit_State_Tracker => Ghost_Hs_Exit_State_Tracker,
                 null => Status),
     Linker_Section => LINKER_SECTION_NAME;

end Update_Mailbox_Falc_Specific_Tu10x;
