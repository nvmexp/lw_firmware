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
--  HW and Arch specific procedures to acquire/release Mutex
--
--  @description
--  This package contains procedure to acquire/release mutex

package Se_Pka_Mutex_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Read_Timestamp(Data : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  (Status = FLCN_PRI_ERROR and then Data = 0)else
                    Status = UCODE_ERR_CODE_NOERROR),
         Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                    In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ((Data, Status) => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
         Linker_Section => LINKER_SECTION_NAME;

end Se_Pka_Mutex_Falc_Specific_Tu10x;
