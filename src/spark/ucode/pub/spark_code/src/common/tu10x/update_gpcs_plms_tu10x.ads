-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Bar0_Access_Contracts; use Bar0_Access_Contracts;

--  @summary
--  Update Gpcs and related Plm values
--
--  @description
--  This package provides routines to raise or lower all GPCCS/TPCCS and related Plms
--

package Update_Gpcs_Plms_Tu10x
with SPARK_Mode => On
is

   -- Main caller routine which calls sub routines to lower specific GPCS and related Plms.
   -- @param Status The Status of the routine.This is an OUT param.
   procedure Lower_Gpcs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

private
   procedure Lower_Gpccs_Pvs_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Rc_Lane_Cmd_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

      procedure Lower_Tpccs_Sm_Private_Control_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Arb_Wpr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Tpccs_Arb_Wpr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Exe_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Mthdctx_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Irqtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpccs_Arb_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Tpccs_Arb_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Tpccs_Rc_Lane_Cmd_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Gpcs_Mmu_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => Bar0_Access_Post_Contract(Status),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Bar0_Tmout_State),
                  In_Out => (Ghost_Csb_Pri_Error_State,
                             Ghost_Bar0_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

end Update_Gpcs_Plms_Tu10x;
