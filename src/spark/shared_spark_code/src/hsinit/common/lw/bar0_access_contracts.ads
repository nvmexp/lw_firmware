-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

package Bar0_Access_Contracts
with SPARK_Mode => On,
  Ghost
is

   function Bar0_Access_Post_Contract(Status : LW_UCODE_UNIFIED_ERROR_TYPE) return Boolean
   is
     (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
         Status = FLCN_PRI_ERROR
      elsif Ghost_Bar0_Status = BAR0_WAITING then
        (Status = FLCN_BAR0_WAIT)
      else
         Status = UCODE_ERR_CODE_NOERROR)
        with
          Global => (Input => (Ghost_Bar0_Status,
                               Ghost_Csb_Pri_Error_State)),
     Depends => ( Bar0_Access_Post_Contract'Result => (Status,
                                                       Ghost_Bar0_Status,
                                                       Ghost_Csb_Pri_Error_State));          

   function Bar0_Access_Pre_Contract(Status : LW_UCODE_UNIFIED_ERROR_TYPE) return Boolean
   is
     (Status = UCODE_ERR_CODE_NOERROR and then
      Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
      Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
      Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET and then
      Ghost_Bar0_Status = BAR0_SUCCESSFUL)
     with
       Global => (Input => (Ghost_Bar0_Status,
                            Ghost_Csb_Pri_Error_State,
                            Ghost_Csb_Err_Reporting_State,
                            Ghost_Bar0_Tmout_State)),
     Depends => ( Bar0_Access_Pre_Contract'Result => (Status,
                                                      Ghost_Bar0_Status,
                                                      Ghost_Csb_Pri_Error_State,
                                                      Ghost_Csb_Err_Reporting_State,
                                                      Ghost_Bar0_Tmout_State));           
end Bar0_Access_Contracts;
