-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
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
--  PMU reset module.
--
--  @description
--  This package contains procedures and required wrappers to reset pmu falcon
--
--  Traceability -
--        \ImplementsUnitDesign{Pmu_Reset}
package Pmu_Reset
with SPARK_Mode => On
is
   --  Requirement: This procedure shall do the following:
   --               1) Reset PMU falcon.
   --               2) Read back CPUCTL and SCTL register to verify that PMU is in Halted in NS state.
   --  @param Status  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 if pmu reset is successfull else error.
   procedure Reset_Pmu(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Ghost_Pmu_Reset_Status = PMU_NOT_RESET and then
       Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      elsif
                        Ghost_Pmu_Reset_Status = PMU_NOT_RESET then
                          Status = UCODE_PUB_ERR_PMU_RESET else
                            Status = UCODE_ERR_CODE_NOERROR),
                 Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                                         Ghost_Bar0_Tmout_State),
                            In_Out => (Ghost_Csb_Pri_Error_State,
                                       Ghost_Bar0_Status,
                                       Ghost_Pmu_Reset_Status)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  Ghost_Pmu_Reset_Status => Ghost_Pmu_Reset_Status,
                  null => (Ghost_Bar0_Status,
                           Ghost_Csb_Pri_Error_State,
                           Status)),
     Linker_Section => LINKER_SECTION_NAME;

private
   --  Requirement: This procedure shall do the following:
   --               1) Internal API private to package.
   --               2) Bring the PMU out of reset state.
   --  @param Status  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 when no error is recieved in bar0 reg read/write else error.
   procedure Bring_Pmu_Out_Of_Reset (Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

   --  Requirement: This procedure shall do the following:
   --               1) Internal API private to package.
   --               2) Puts PMU in reset state.
   --  @param Status  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 when no error is recieved in bar0 reg read/write else error.

   procedure Put_Pmu_In_Reset (Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
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

end Pmu_Reset;
