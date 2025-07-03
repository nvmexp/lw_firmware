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
--  Pub HS Entry
--
--  @description
--  This package provides HS Entry function for PUB
--

package Pub_Entry
with SPARK_Mode => On
is

   -- HS Entry function for PUB which calls HS Init sequence (Common Spark code) and
   -- Pub Specific code (Pmu Reset, Lower Plms, Fbpa War etc)
   procedure Pub_Core_Entry(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Hs_Init_States_Tracker = SW_REVOCATION_CHECKED and then
                       Ghost_Pmu_Reset_Status = PMU_NOT_RESET),
           Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                      Status = FLCN_PRI_ERROR
                        elsif Ghost_Bar0_Status = BAR0_WAITING then
                          Status = FLCN_BAR0_WAIT
                            elsif Ghost_Pmu_Reset_Status = PMU_NOT_RESET then
                              Status = UCODE_PUB_ERR_PMU_RESET
                                else
                                  Status = UCODE_ERR_CODE_NOERROR
                   ),
           Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                                   Ghost_Bar0_Tmout_State,
                                   Ghost_Hs_Init_States_Tracker),
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

   procedure Binary_Core_Entry(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE) renames Pub_Core_Entry;

end Pub_Entry;
