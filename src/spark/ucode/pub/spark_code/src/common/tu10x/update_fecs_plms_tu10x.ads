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
--  Update FECS Plm values
--
--  @description
--  This package provides routines to raise or lower all FECS and related Plms
--

package Update_Fecs_Plms_Tu10x
with SPARK_Mode => On
is

   -- Main caller routine which calls sub routines to lower specific FECS and related Plms.
   -- @param Status The Status of the routine..This is an OUT param.
   procedure Lower_Fecs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Pvs_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Rc_Lane_Cmd_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Arb_Wpr_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Mthdctx_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Irqtmr_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Arb_Falcon_Regioncfg_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fecs_Exe_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppriv_Sys_Priv_Holdoff_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Pfb_Pri_Mmu_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fuse_Floorsweep_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Pgc6_Aon_Selwre_Scratch_Group_15_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

end Update_Fecs_Plms_Tu10x;
