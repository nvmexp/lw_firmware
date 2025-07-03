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
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  HW and Arch Specific Helper Procedures required for Mutex
--
--  @description
--  This package contains procedure which will help to acquire/release mutex

package Falcon_Selwrebusdio_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   type Type_Of_Errors is
     (
      Error1,
      Error2,
      Error3);

   FALCON_DIO_TIMEOUT                : constant := 16#1000_0000#;
   FALC_DOC_CONTROL_MAX_FREE_ENTRIES : constant := 16#0000_0004#;

   procedure Read_Count(Count : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  (Status = FLCN_PRI_ERROR and then Count = 0)
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
           Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                      In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( (Count,
                  Status) => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Read_LwU32_Dic_D0(Val : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
          Status = FLCN_PRI_ERROR else
                  Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( (Val, Status) => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status),
       Linker_Section => LINKER_SECTION_NAME;

   procedure Write_Dic_Ctrl(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Write_LwU32_Doc_D0(Val : LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => (Status,
                          Val)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Write_LwU32_Doc_D1(Val : LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => (Status,
                          Val)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Read_Doc_Ctrl_Error(Error : out Boolean; Error_Type : Type_Of_Errors;
                                 Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  (Status = FLCN_PRI_ERROR and then Error = False) else
                      Status = UCODE_ERR_CODE_NOERROR),
         Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                    In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( Error => Error_Type,
                  Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Read_Ptimer0(Val : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
          (Status = FLCN_PRI_ERROR and then Val = 0) else
            Status = UCODE_ERR_CODE_NOERROR),
        Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                   In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( (Status, Val) => null,
                     Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status),
        Linker_Section => LINKER_SECTION_NAME;

end Falcon_Selwrebusdio_Falc_Specific_Tu10x;
