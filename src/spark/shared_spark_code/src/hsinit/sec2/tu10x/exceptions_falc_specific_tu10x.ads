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
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Reg_Rd_Wr_Csb_Falc_Specific_Tu10x; use Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;

--  @summary
--  HW and Arch Specific Exception Procedures
--
--  @description
--  This package contains procedure which will set Stack Underflow Installer and
--  set some register Interrupt brkpoint registers.

package Exceptions_Falc_Specific_Tu10x
with SPARK_Mode => On
is
   package Csb_Reg_Access_Ibrkpt1 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_IBRKPT1_REGISTER);

   package Csb_Reg_Access_Ibrkpt2 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_IBRKPT2_REGISTER);

   package Csb_Reg_Access_Ibrkpt3 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_IBRKPT3_REGISTER);

   package Csb_Reg_Access_Ibrkpt4 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_IBRKPT4_REGISTER);

   package Csb_Reg_Access_Ibrkpt5 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_IBRKPT5_REGISTER);

   package Csb_Reg_Access_Stackcfg is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_STACKCFG_REGISTER);

   -- Todo : add success condition in post
   procedure Stack_Underflow_Installer(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Hs_Init_States_Tracker = EXCEPTION_INSTALLER_HANG and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    Status = FLCN_PRI_ERROR else
                      (Status = UCODE_ERR_CODE_NOERROR and then
                               Ghost_Hs_Init_States_Tracker = STACK_UNDERFLOW_INSTALLER)),
           Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State),
                      Proof_In => Ghost_Csb_Err_Reporting_State),
     Depends => (Status => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Inline_Always;

   -- Todo : add no error status in post
   procedure Set_Ibrkpt_Registers(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State, In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Inline_Always;

end Exceptions_Falc_Specific_Tu10x;
