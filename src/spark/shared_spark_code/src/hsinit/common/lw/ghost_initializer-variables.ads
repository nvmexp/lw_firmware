-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Ghost Package.
--
--  @description
--  This package defines list of Ghost variables for Proving only. They are not
--  part of the binary.

with Lw_Types; use Lw_Types;

package Ghost_Initializer.Variables
with SPARK_Mode => On,
  Ghost
is

   --  State which defines CSW.E bit status.
   Ghost_Csw_Spr_Status : CSW_EXCEPTION_BIT_STATUS := CSW_EXCEPTION_BIT_ENABLE;
   --  State which defines CSB Err Reporting status.
   Ghost_Csb_Err_Reporting_State : CSB_ERR_REPORTING := CSB_ERR_REPORTING_DISBALED;
   --  State which defines States of HS Init.
   Ghost_Hs_Init_States_Tracker : HS_INIT_STATES := HS_INIT_NOT_STARTED;
   --  State which defines Bar0 Timeout register status.
   Ghost_Bar0_Tmout_State : BAR0_TMOUT_SET_STATE := BAR0_TIMOUT_NOTSET;
   --  State which defines status of permission of Halt Action.
   Ghost_Halt_Action_State : HALT_ACTION_STATE := HALT_ACTION_DISALLOWED;
   --  State which defines Revision derived from FPF Fuse.
   Ghost_Sw_Revocation_Rev_Fpf_Val : LwU8 := 0;
   -- State which is set if there is a CSB PRI ERROR
   Ghost_Csb_Pri_Error_State : CSB_PRI_ERROR_STATE := CSB_PRI_ERROR_FALSE;
   -- State Which keeps track of validity of Engine Id
   Ghost_Valid_Engine_State : VALID_ENGINE_STATE := VALID_ENGINE;
   -- Status for SE
   Ghost_Se_Status : SEBUS_ERROR_STATUS := NO_SE_ERROR;
   -- Status for PKA Mutex
   Ghost_Pka_Mutex_Status : PKA_MUTEX_STATUS := MUTEX_RELEASED;
   -- Status for VHR Check
   Ghost_Vhr_Status : VHR_STATUS := VHR_NOT_CLEARED;
   -- Status for BAR0 Transactions
   Ghost_Bar0_Status : BAR0_ACCESS_STATUS := BAR0_SUCCESSFUL;
   -- Status for Chip validity
   Ghost_Chip_Validity_Status : CHIP_VALIDITY_STATUS := CHIP_VALIDITY_NOT_VERIFIED;
   -- Status for SW Ucode Revocation
   Ghost_Sw_Revocation_Status : SW_REVOCATION_STATUS := SW_REVOCATION_NOT_VERIFIED;
   -- Status for PMU Reset
   Ghost_Pmu_Reset_Status : RESET_PMU_STATUS := PMU_NOT_RESET;
   -- Status for PKA SE Errors
   Ghost_Pka_Se_Mutex : PKA_SE_STATUS := PKA_SE_NO_ERROR;
   -- Status for VHR SE Errors
   Ghost_Vhr_Se_Status : VHR_SE_STATUS := VHR_SE_NO_ERROR;
   -- HS Exit State Tracker
   Ghost_Hs_Exit_State_Tracker : HS_EXIT_STATE_TRACKER := HS_EXIT_NOT_STARTED;


end Ghost_Initializer.Variables;
