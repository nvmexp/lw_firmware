-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_


with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Se_Seb_Ada; use Dev_Se_Seb_Ada;

--  @summary
--  HW and Arch Specific VHR Helper functions - AUTO
--
--  @description
--  This package contains helper procedures to empty VHR part of BROM

with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

package Se_Vhr_Helper_Falc_Specific_Tu10x is

   procedure Callwlate_Sse_Addr(Sse_Addr : out LW_SSE_BAR0_ADDR; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    (Status = UCODE_ERR_CODE_NOERROR and then
                           Sse_Addr /= LW_SSE_BAR0_ADDR'Last)),
         Global => (In_Out => (Ghost_Csb_Pri_Error_State),
                    Proof_In => Ghost_Csb_Err_Reporting_State),
     Depends => ((Sse_Addr, Status, Ghost_Csb_Pri_Error_State) => null,
                 null => (Ghost_Csb_Pri_Error_State,
                          Status)),
       Linker_Section => LINKER_SECTION_NAME;

   procedure Callwlate_Ent_Num_Mask(Ent_Num_Mask : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
       Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                  Status = FLCN_PRI_ERROR else
                    Status = UCODE_ERR_CODE_NOERROR),
         Global => (In_Out => (Ghost_Csb_Pri_Error_State),
                    Proof_In => Ghost_Csb_Err_Reporting_State),
     Depends => ((Status, Ghost_Csb_Pri_Error_State, Ent_Num_Mask) => null,
                 null => (Ghost_Csb_Pri_Error_State,
                          Status)),
       Linker_Section => LINKER_SECTION_NAME;

private
  -- function Base_Addr_Helper( Base_Addr : LW_CSEC_FALCON_VHRCFG_BASE_REGISTER)
  --                           return LW_SSE_BAR0_ADDR
  --   with
  --     Inline_Always;

   function Shift_Left_Helper( Value : LwU32;
                               Amount : LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD )
                              return LwU32
     with
       Pre => Value >= 1,
       Post => Shift_Left_Helper'Result >= 1,
       Global => null,
       Inline_Always;

end Se_Vhr_Helper_Falc_Specific_Tu10x;

