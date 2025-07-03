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
with Bar0_Access_Contracts; use Bar0_Access_Contracts;

--  @summary
--  Update Pmu and related Plm values
--
--  @description
--  This package provides routines to raise or lower all PMU and related Plms
--
package Update_Pmu_Plms_Tu10x
with SPARK_Mode => On
is

   -- Main caller routine which calls sub routines to lower specific PMU and related Plms.
   -- @param Status The Status of the routine.This is an OUT param.
   procedure Lower_Pmu_Plms(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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
   procedure Lower_Ppwr_Exe_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Irqtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Dmem_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Sctl_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Wdtmr_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Fbif_Regioncfg_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Pmu_Msgq_Head_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Ppwr_Pmu_Msgq_Tail_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Therm_Smartfan_One_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Therm_Vidctrl_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   procedure Lower_Fuse_Iddqinfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET,
       Global => (Proof_In => Ghost_Bar0_Tmout_State),
       Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Fuse_Sram_Vmin_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET,
       Global => (Proof_In => Ghost_Bar0_Tmout_State),
       Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Fuse_Speedoinfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET,
       Global => (Proof_In => Ghost_Bar0_Tmout_State),
       Linker_Section => LINKER_SECTION_NAME;

   procedure Lower_Fuse_Kappainfo_Plm(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET,
       Global => (Proof_In => Ghost_Bar0_Tmout_State),
       Linker_Section => LINKER_SECTION_NAME;

end Update_Pmu_Plms_Tu10x;
