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
--  Auto VHR Check
--
--  @description
--  This package contains procedure to clear the VHR part of BROM


package Se_Vhr
with SPARK_Mode => On
is


   procedure Se_Vhr_Empty_Check( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Hs_Init_States_Tracker = NS_BLOCKS_ILWALIDATED and then
                     Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                       Ghost_Se_Status = NO_SE_ERROR and then
                         Ghost_Pka_Mutex_Status = MUTEX_RELEASED and then
                           Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR and then
                             Ghost_Vhr_Status = VHR_NOT_CLEARED and then
                               Ghost_Vhr_Se_Status = VHR_SE_NO_ERROR and then
                                 Ghost_Halt_Action_State = HALT_ACTION_DISALLOWED),

                     Post => ( if Status = UCODE_ERR_CODE_NOERROR
                                 then
                                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                                     Ghost_Vhr_Se_Status = VHR_SE_NO_ERROR and then
                                       Ghost_Se_Status = NO_SE_ERROR and then
                                         Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR and then
                                           Ghost_Pka_Mutex_Status = MUTEX_RELEASED and then
                                             Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                                               Ghost_Hs_Init_States_Tracker = VHR_EMPTY_CHECKED and then
                                                 Ghost_Vhr_Status = VHR_CLEARED
                              ),
                     Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                                In_Out => (Ghost_Hs_Init_States_Tracker,
                                           Ghost_Csb_Pri_Error_State,
                                           Ghost_Se_Status,
                                           Ghost_Pka_Mutex_Status,
                                           Ghost_Vhr_Status,
                                           Ghost_Halt_Action_State,
                                           Ghost_Pka_Se_Mutex,
                                           Ghost_Vhr_Se_Status)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Vhr_Status => Ghost_Vhr_Status,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Se_Status => Ghost_Se_Status,
                 Ghost_Pka_Mutex_Status => Ghost_Pka_Mutex_Status,
                 Ghost_Halt_Action_State => Ghost_Halt_Action_State,
                 Ghost_Pka_Se_Mutex => Ghost_Pka_Se_Mutex,
                 Ghost_Vhr_Se_Status => Ghost_Vhr_Se_Status,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State)),
     Linker_Section => LINKER_SECTION_NAME;

end Se_Vhr;
