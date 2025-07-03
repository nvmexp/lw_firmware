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
--  This package defines the initialization function for Ghost variables.
package Ghost_Initializer
with SPARK_Mode => On,
  Ghost
is

   --  Values for CSW[24:24]
   --  @value CSW_EXCEPTION_BIT_DISABLE Clear CSW.E bit.
   --  @value CSW_EXCEPTION_BIT_ENABLE  Set CSW.E bit.
   type CSW_EXCEPTION_BIT_STATUS is
     (
      CSW_EXCEPTION_BIT_DISABLE,
      CSW_EXCEPTION_BIT_ENABLE
     ) with size => 1;

   --------- CSB Error Reporting --------------

   --  Enum for status of CSBERRSTAT register.
   -- If CSBERRSTAT_ENABLE = TRUE , then first value else second value.
   type CSB_ERR_REPORTING is
     (
      CSB_ERR_REPORTING_ENABLED,
      CSB_ERR_REPORTING_DISBALED
     ) with size => 1;

   --  Enum for status of BAR0_TMOUT register.
   -- If TMOUT is set then , then first value else second value.
   type BAR0_TMOUT_SET_STATE is
     (
      BAR0_TIMOUT_SET,
      BAR0_TIMOUT_NOTSET
     ) with size => 1;

   --  Enum for Permission of HALT Action.
   -- If Halt is allowed , then first value else second value.
   type HALT_ACTION_STATE is
     (
      HALT_ACTION_ALLOWED,
      HALT_ACTION_DISALLOWED
     ) with size =>1;

   -------------- HS Init Steps ---------------------------
   --  Enum for various states of HS Init. Still few steps need to be added.
   -- Used for tracking of correct HS Init sequence.

   type HS_INIT_STATES is
     (
      HS_INIT_NOT_STARTED,
      SP_SETUP,
      STALE_SIGNATURE_CLEARED,
      SCP_REGISTERS_CLEARED,
      GPRS_SCRUBBED,
      DMEM_SCRUBBED,
      CSB_ERROR_REPORTING_ENABLED,
      CMEMBASE_APERTURE_DISABLED,
      TRACE_PC_BUFFER_DISABLED,
      HS_RESTART_PREVENTED,
      EXCEPTION_INSTALLER_HANG,
      STACK_UNDERFLOW_INSTALLER,
      SSP_ENABLED,
      NS_BLOCKS_ILWALIDATED,
      VHR_EMPTY_CHECKED,
      RESET_PLM_PROTECTED,
      BAR0_TMOUT_SET,
      BOOT42_CHECKED,
      SW_REVOCATION_CHECKED
     )
       with Object_Size => 8, Size => 8;

   for HS_INIT_STATES use
     (
      HS_INIT_NOT_STARTED          =>  0,
      SP_SETUP                     =>  1,
      STALE_SIGNATURE_CLEARED      =>  2,
      SCP_REGISTERS_CLEARED        =>  3,
      GPRS_SCRUBBED                =>  4,
      DMEM_SCRUBBED                =>  5,
      CSB_ERROR_REPORTING_ENABLED  =>  6,
      CMEMBASE_APERTURE_DISABLED   =>  7,
      TRACE_PC_BUFFER_DISABLED     =>  8,
      HS_RESTART_PREVENTED         =>  9,
      EXCEPTION_INSTALLER_HANG     => 10,
      STACK_UNDERFLOW_INSTALLER    => 11,
      SSP_ENABLED                  => 12,
      NS_BLOCKS_ILWALIDATED        => 13,
      VHR_EMPTY_CHECKED            => 14,
      RESET_PLM_PROTECTED          => 15,
      BAR0_TMOUT_SET               => 16,
      BOOT42_CHECKED               => 17,
      SW_REVOCATION_CHECKED        => 18
     );

   --------- PRI Error Reported --------------

   --  Enum for Valid State of CSBERRSTAT register.
   -- If CSBERRSTAT_VALID = TRUE , then we have hit PRI ERROR else we are good.
   type CSB_PRI_ERROR_STATE is
     (
      CSB_PRI_ERROR_FALSE,
      CSB_PRI_ERROR_TRUE
     ) with size => 1;

   ---------Valid Falcon Engine --------------

   --  Enum for Valid State of CSBERRSTAT register.
   -- If CSBERRSTAT_VALID = TRUE , then we have hit PRI ERROR else we are good.
   type VALID_ENGINE_STATE is
     (
      ILWALID_ENGINE,
      VALID_ENGINE
     ) with size => 1;

   ---------SE Library --------------

   --  Enum for all Status of Secure Engine module.
   type SEBUS_ERROR_STATUS is
     (
      NO_SE_ERROR,
      SE_ERROR
     ) with size => 3;

   ---------PKA Mutex --------------

   --  Enum for all Status of PKA Mutex.
   type PKA_MUTEX_STATUS is
     (
      MUTEX_ACQUIRED,
      MUTEX_RELEASED
     ) with size => 1;

   --  Enum for all Status of VHR Check
   type VHR_STATUS is
     (
      VHR_NOT_CLEARED,
      VHR_CLEARED
     ) with size => 3;

   --  Enum for all modelling BAR0 Wait Idle
   type BAR0_ACCESS_STATUS is
     (
      BAR0_SUCCESSFUL,
      BAR0_WAITING
     ) with size => 1;

   type CHIP_VALIDITY_STATUS is
     (
      CHIP_VALIDITY_NOT_VERIFIED,
      CHIP_ILWALID,
      CHIP_VALID
     ) with size => 2;

   type SW_REVOCATION_STATUS is
     (
      SW_REVOCATION_NOT_VERIFIED,
      SW_REVOCATION_ILWALID,
      SW_REVOCATION_VALID
     ) with size => 2;

   type RESET_PMU_STATUS is
     (
      PMU_HAS_RESET,
      PMU_NOT_RESET
     ) with size => 1;

   type PKA_SE_STATUS is
     (
      PKA_SE_ERROR,
      PKA_SE_NO_ERROR
     ) with Size => 1;

   type VHR_SE_STATUS is
     (
      VHR_SE_ERROR,
      VHR_SE_NO_ERROR
     ) with Size => 1;

   type HS_EXIT_STATE_TRACKER is
     (
      HS_EXIT_NOT_STARTED,
      MAILBOX_UPDATED,
      CLEANUP_COMPLETED
     );

end Ghost_Initializer;
