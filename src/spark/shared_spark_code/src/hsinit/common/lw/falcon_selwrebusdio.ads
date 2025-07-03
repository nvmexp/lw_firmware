-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Se_Seb_Ada; use Dev_Se_Seb_Ada;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  Falcon Secure Bus transactions
--
--  @description
--  This package contains procedures for accessing registers via secure bus

package Falcon_Selwrebusdio
with SPARK_Mode => on
is


   -- This procedure reads register via the secure bus
   -- @param Reg_Addr Register address to be read. This is input.
   -- @param Reg_Val Register Value present at Reg_Addr. This is output.
   -- @param Status Status of the procedure.This is output.
   procedure Selwre_Bus_Read_Register( Reg_Addr : LW_SSE_BAR0_ADDR;
                                       Reg_Val  : out LwU32;
                                       Status   : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => ( if Status = UCODE_ERR_CODE_NOERROR
                   then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                       Ghost_Se_Status = NO_SE_ERROR
                         else
                           Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE or else
                             Ghost_Se_Status = SE_ERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State,
                  Output => Ghost_Se_Status),
     Depends => (Reg_Val => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Status => null,
                 Ghost_Se_Status => null,
                 null => (Status,
                          Reg_Addr)),
     Linker_Section => LINKER_SECTION_NAME;

   -- This procedure writes register via the secure bus
   -- @param Reg_Addr Register address to be written into. This is input.
   -- @param Reg_Val Register Value to be written at Reg_Addr. This is input.
   -- @param Status Status of the procedure.This is output.
   procedure Selwre_Bus_Write_Register( Reg_Addr : LW_SSE_BAR0_ADDR;
                                        Reg_Val : LwU32;
                                        Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => ( if Status = UCODE_ERR_CODE_NOERROR
                   then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                       Ghost_Se_Status = NO_SE_ERROR
                         else
                           Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE or else
                             Ghost_Se_Status = SE_ERROR),
                                   Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                                              In_Out => Ghost_Csb_Pri_Error_State,
                                              Output => Ghost_Se_Status),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Ghost_Se_Status => null,
                 null => (Reg_Val,
                          Status,
                          Reg_Addr)),
     Linker_Section => LINKER_SECTION_NAME;


private
   -- This procedure gets data for which a read request has been sent to secure bus.
   -- @param Reg_Val Register Value present at address for which read request has been sent via secure bus.
   -- This is output.
   -- @param Status Status of the procedure.This is output.
   procedure Selwre_Bus_Get_Data( Reg_Val : out LwU32;
                                  Status  : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => ( if Status = UCODE_ERR_CODE_NOERROR
                   then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                       Ghost_Se_Status = NO_SE_ERROR
                         else
                           Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE or else
                             Ghost_Se_Status = SE_ERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State,
                  Output => Ghost_Se_Status),
     Depends => ((Reg_Val, Status) => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Ghost_Se_Status => null,
                 null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   -- This procedure sends request on the secure bus for either read/write.
   -- @param Read_Request Boolean to indicate if it is a Read request or not.This is input.
   -- @param Reg_Addr Register address for which request is sent. This is input.
   -- @param Write_Val Value to be written at Reg_Addr in case Read_Request = False. This is input.
   -- @param Status Status of the procedure.This is output.
   procedure Selwre_Bus_Send_Request( Read_Request : Boolean;
                                      Reg_Addr     : LW_SSE_BAR0_ADDR;
                                      Write_Val    : LwU32;
                                      Status       : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => ( if Status = UCODE_ERR_CODE_NOERROR
                   then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                       Ghost_Se_Status = NO_SE_ERROR
                         else
                           Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE or else
                             Ghost_Se_Status = SE_ERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State,
                  Output => Ghost_Se_Status),
     Depends => (Status => Read_Request,
                 Ghost_Csb_Pri_Error_State => (Read_Request,
                                               Ghost_Csb_Pri_Error_State),
                 Ghost_Se_Status => Read_Request,
                 null => (Write_Val,
                          Status,
                          Reg_Addr)),
     Linker_Section => LINKER_SECTION_NAME;
end Falcon_Selwrebusdio;
