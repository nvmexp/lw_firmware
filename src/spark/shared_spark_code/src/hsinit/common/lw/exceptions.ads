-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Ghost_Initializer; use Ghost_Initializer;
with Lw_Types; use Lw_Types;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with  Ucode_Post_Codes; use Ucode_Post_Codes;

--  @summary
--  Falcon exceptions Installing/Handling
--
--  @description
--  This package contains procedures for installing and handling various exceptions raised by falcon

package Exceptions
with SPARK_Mode => On
is

   -- Installs minimum Exception Handler and sets up Exception vector and other registers required
   -- for exceptions.
   procedure Exception_Installer_Hang(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ( Status = UCODE_ERR_CODE_NOERROR and then
                  Ghost_Hs_Init_States_Tracker = HS_RESTART_PREVENTED and then
                    Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                      Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                        Ghost_Csw_Spr_Status = CSW_EXCEPTION_BIT_ENABLE),
           Post => ( if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                       Status = FLCN_PRI_ERROR
                         else
                           Status = UCODE_ERR_CODE_NOERROR and then
                             Ghost_Hs_Init_States_Tracker = EXCEPTION_INSTALLER_HANG and then
                               Ghost_Csw_Spr_Status = CSW_EXCEPTION_BIT_DISABLE),
             Global => ( In_Out => (Ghost_Hs_Init_States_Tracker,
                                    Ghost_Csb_Pri_Error_State,
                                    Ghost_Csw_Spr_Status),
                         Proof_In => Ghost_Csb_Err_Reporting_State),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                  Ghost_Csw_Spr_Status => Ghost_Csw_Spr_Status,
                  null => Status),
     Inline_Always;

   -- This function returns Stack Bottom Value, below which the stack should not grow.
   -- This value is either a linker script variable or is a constant.
   -- @return LwU32 Stack bottom value
   function Get_Stack_Bottom return LwU32
     with
       Post        => Get_Stack_Bottom'Result <= LwU32( LwU16'Last ),
     Global       => null,
     Inline_Always;

   -- Exception Handler. It is just an infinite loop and this procedure never returns.
   procedure Exception_Handler_Selwre_Hang
     with
       Export => True,
       Convention => C,
       Global => null,
       No_Return;
   pragma Linker_Section(Exception_Handler_Selwre_Hang, LINKER_SECTION_NAME);
   PUB_STACK_BOTTOM_LIMIT : constant LwU32 := 16#0000_C000#;
private

   -- Installs Exception Handler which is just an infinite loop
   procedure Install_Exception_Handler_Hang_To_Ev
     with
       Import     => True,
       Convention => C,
       Global     => null,
       Inline_Always;
end Exceptions;
