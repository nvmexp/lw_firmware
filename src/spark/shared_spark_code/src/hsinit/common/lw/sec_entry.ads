-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Array_Types; use Lw_Types.Array_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with System;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;

--  @summary
--  HS Init/Entry Sequence
--
--  @description
--  This package contains the calls to entire HS Init Sequence. This code is HS.

package Sec_Entry
with SPARK_Mode => On
is

   subtype Rand_Num16_Arr_Type is ARR_LwU32_IDX32( 0 .. 3 )
     with
       Object_Size => 16;
   type Rand_Num16_Byte_Struct is record
      Data : Rand_Num16_Arr_Type;
   end record
     with
       Size => 16, Object_Size => 16, Alignment => 16;
   Rand_Num16_Byte : Rand_Num16_Byte_Struct;

   Stack_Check_Guard : System.Address
     with Export => True,
     Convention => C,
     External_Name => "__stack_chk_guard";

   procedure Hs_Entry_Inline
     with
       Pre => (Ghost_Hs_Init_States_Tracker = SP_SETUP and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_DISBALED and then
                     Ghost_Csw_Spr_Status = CSW_EXCEPTION_BIT_ENABLE),
         Post => (Ghost_Hs_Init_States_Tracker = SSP_ENABLED and then
                    Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                      Ghost_Csw_Spr_Status = CSW_EXCEPTION_BIT_DISABLE),
       Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                             Ghost_Csb_Pri_Error_State,
                             Ghost_Csb_Err_Reporting_State,
                             Ghost_Csw_Spr_Status),
                  Output => (Rand_Num16_Byte,
                             Stack_Check_Guard)),
     Depends => ((Ghost_Hs_Init_States_Tracker,
                 Ghost_Csb_Err_Reporting_State) => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Ghost_Csw_Spr_Status => Ghost_Csw_Spr_Status,
                 Rand_Num16_Byte => null,
                 Stack_Check_Guard => null,
                 null => (Ghost_Hs_Init_States_Tracker,
                          Ghost_Csb_Err_Reporting_State)),
     Inline_Always;

   procedure Hs_Entry_NonInline(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Hs_Init_States_Tracker = SSP_ENABLED and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                     Ghost_Bar0_Tmout_State = BAR0_TIMOUT_NOTSET and then
                       Ghost_Halt_Action_State = HALT_ACTION_DISALLOWED and then
                         Ghost_Sw_Revocation_Rev_Fpf_Val = 0 and then
                           Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                             Ghost_Se_Status = NO_SE_ERROR and then
                               Ghost_Pka_Mutex_Status = MUTEX_RELEASED and then
                                 Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR and then
                                   Ghost_Vhr_Status = VHR_NOT_CLEARED and then
                                     Ghost_Vhr_Se_Status = VHR_SE_NO_ERROR and then
                                       Ghost_Chip_Validity_Status = CHIP_VALIDITY_NOT_VERIFIED and then
                                         Ghost_Sw_Revocation_Status = SW_REVOCATION_NOT_VERIFIED and then
                                           Ghost_Bar0_Status = BAR0_SUCCESSFUL),
                               Post => ( if Status = UCODE_ERR_CODE_NOERROR
                                           then
                                             Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                                               Ghost_Se_Status = NO_SE_ERROR and then
                                                 Ghost_Pka_Se_Mutex = PKA_SE_NO_ERROR and then
                                                   Ghost_Pka_Mutex_Status = MUTEX_RELEASED and then
                                                     Ghost_Vhr_Se_Status = VHR_SE_NO_ERROR and then
                                                       Ghost_Vhr_Status = VHR_CLEARED and then
                                                         Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                                                           Ghost_Bar0_Status = BAR0_SUCCESSFUL and then
                                                             Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET and then
                                                               Ghost_Chip_Validity_Status = CHIP_VALID and then
                                                                 Ghost_Sw_Revocation_Status = SW_REVOCATION_VALID
                                        ),
                               Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                                                     Ghost_Bar0_Tmout_State,
                                                     Ghost_Csb_Pri_Error_State,
                                                     Ghost_Se_Status,
                                                     Ghost_Pka_Mutex_Status,
                                                     Ghost_Vhr_Status,
                                                     Ghost_Chip_Validity_Status,
                                                     Ghost_Sw_Revocation_Status,
                                                     Ghost_Halt_Action_State,
                                                     Ghost_Bar0_Status,
                                                     Ghost_Pka_Se_Mutex,
                                                     Ghost_Vhr_Se_Status
                                                    ),
                                          Proof_In =>
                                            (Ghost_Csb_Err_Reporting_State, Ghost_Sw_Revocation_Rev_Fpf_Val)),
                               Depends => (Ghost_Bar0_Tmout_State => Ghost_Bar0_Tmout_State,
                                           (Ghost_Hs_Init_States_Tracker, Status) => null,
                                           Ghost_Halt_Action_State => Ghost_Halt_Action_State,
                                           Ghost_Csb_Pri_Error_State => null,
                                           Ghost_Se_Status => Ghost_Se_Status,
                                           Ghost_Pka_Mutex_Status => Ghost_Pka_Mutex_Status,
                                           Ghost_Vhr_Status => Ghost_Vhr_Status,
                                           Ghost_Chip_Validity_Status => Ghost_Chip_Validity_Status,
                                           Ghost_Sw_Revocation_Status => Ghost_Sw_Revocation_Status,
                                           Ghost_Bar0_Status => Ghost_Bar0_Status,
                                           Ghost_Pka_Se_Mutex => Ghost_Pka_Se_Mutex,
                                           Ghost_Vhr_Se_Status => Ghost_Vhr_Se_Status,
                                           null => (Ghost_Csb_Pri_Error_State,
                                                    Ghost_Hs_Init_States_Tracker,
                                                    Status)
                                          ),
                               Linker_Section => LINKER_SECTION_NAME;

   procedure Hs_Exit(Status : LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Ghost_Hs_Init_States_Tracker >= VHR_EMPTY_CHECKED and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                     Ghost_Hs_Exit_State_Tracker = HS_EXIT_NOT_STARTED
              ),
       Post => (Ghost_Hs_Exit_State_Tracker = CLEANUP_COMPLETED),
     Global => (Proof_In => (Ghost_Halt_Action_State,
                             Ghost_Csb_Err_Reporting_State),
                In_Out => (Ghost_Hs_Init_States_Tracker,
                           Ghost_Hs_Exit_State_Tracker)),
     Depends => (Ghost_Hs_Init_States_Tracker => null,
                 Ghost_Hs_Exit_State_Tracker => null,
                 null => (Status,
                          Ghost_Hs_Init_States_Tracker,
                          Ghost_Hs_Exit_State_Tracker)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Hs_Entry_Set_Sp(End_Of_Dmem : LwU32)
     with
       Pre => Ghost_Hs_Init_States_Tracker = HS_INIT_NOT_STARTED,
       Global => (Proof_In => Ghost_Hs_Init_States_Tracker),
     Import  => True,
     Convention => C,
     External_Name => "Hs_Entry_Set_Sp",
     Inline_Always;


private

   procedure Get_Rand_Num16_Byte_Addr( Rand_Num16_Byte_Addr : out UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
     with
       Post => LwU32( Rand_Num16_Byte_Addr ) mod 16 = 0,
     Global => null,
     Depends => ( Rand_Num16_Byte_Addr => null ),
     Inline_Always;

   procedure Enable_Ssp with
     Pre => Ghost_Hs_Init_States_Tracker = STACK_UNDERFLOW_INSTALLER,
     Post => Ghost_Hs_Init_States_Tracker = SSP_ENABLED,
     Global => (In_Out => Ghost_Hs_Init_States_Tracker,
                Output => (Stack_Check_Guard,
                           Rand_Num16_Byte)),
     Depends => (Ghost_Hs_Init_States_Tracker => null,
                 Stack_Check_Guard => null,
                 Rand_Num16_Byte => null,
                 null => Ghost_Hs_Init_States_Tracker
                ),
     Inline_Always;

   procedure Falc_Scp_Forget_Sig
     with
       Pre           => Ghost_Hs_Init_States_Tracker = SP_SETUP,
       Post          => Ghost_Hs_Init_States_Tracker = STALE_SIGNATURE_CLEARED,
       Global        => (Proof_In => Ghost_Hs_Init_States_Tracker),
     Import        => True,
     Convention    => C,
     External_Name => "falc_scp_forget_sig",
     Inline_Always;
end Sec_Entry;
