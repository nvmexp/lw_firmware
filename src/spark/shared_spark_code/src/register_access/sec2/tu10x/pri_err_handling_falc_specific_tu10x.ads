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
with Csb_Access_Contracts; use Csb_Access_Contracts;

--  @summary
--  HW and Arch specific Pri Err Handling
--
--  @description
--  This package contains instances of procedure required to handle PRI errs

package Pri_Err_Handling_Falc_Specific_Tu10x
with SPARK_Mode => On
is


   procedure Check_Csb_Err_Value(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Csb_Access_Pre_Contract(Status),
             Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                        Status = FLCN_PRI_ERROR else
                          Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Inline_Always;

end Pri_Err_Handling_Falc_Specific_Tu10x;
