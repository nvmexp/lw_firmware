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
--  HW Indpendent but Arch specific procedures to acquire/release Mutex
--
--  @description
--  This package contains procedure to acquire/release mutex


package Se_Pka_Mutex_Tu10x
with SPARK_Mode => On
is

   procedure Se_Acquire_Pka_Mutex( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Se_Status = NO_SE_ERROR and then
                     Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                       Ghost_Pka_Mutex_Status = MUTEX_RELEASED and then
                         Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR),
             Post => (if Status = UCODE_ERR_CODE_NOERROR
                        then
                          Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                            Ghost_Se_Status = NO_SE_ERROR and then
                              Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR and then
                                Ghost_Pka_Mutex_Status = MUTEX_ACQUIRED
                     ),
             Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                        In_Out => (Ghost_Se_Status,
                                   Ghost_Csb_Pri_Error_State,
                                   Ghost_Pka_Mutex_Status,
                                   Ghost_Pka_Se_Mutex)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  Ghost_Se_Status => null,
                  Ghost_Pka_Mutex_Status => Ghost_Pka_Mutex_Status,
                  Ghost_Pka_Se_Mutex => Ghost_Pka_Se_Mutex,
                  null => (Status,
                           Ghost_Se_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Se_Release_Pka_Mutex( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Ghost_Pka_Mutex_Status = MUTEX_ACQUIRED and then
                 Status = UCODE_ERR_CODE_NOERROR and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Status = UCODE_ERR_CODE_NOERROR
                  then
                    Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                      Ghost_Se_Status = NO_SE_ERROR and then
                        Ghost_Pka_Mutex_Status = MUTEX_RELEASED
               ),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Pka_Mutex_Status),
                  Output => Ghost_Se_Status),
     Depends => ( (Status,
                  Ghost_Csb_Pri_Error_State) => null,
                  Ghost_Se_Status => null,
                  Ghost_Pka_Mutex_Status => Ghost_Pka_Mutex_Status,
                  null => (Status,
                           Ghost_Csb_Pri_Error_State)),
     Linker_Section => LINKER_SECTION_NAME;

private

   SE_PKA_MUTEX_TIMEOUT_VAL_USEC       : constant := 16#10_0000#;
   LW_SSE_SE_CTRL_PKA_MUTEX_RC_SUCCESS : constant := 16#1#;

end Se_Pka_Mutex_Tu10x;
