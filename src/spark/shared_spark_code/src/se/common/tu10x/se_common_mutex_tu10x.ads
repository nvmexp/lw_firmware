-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Dev_Se_Seb_Ada; use Dev_Se_Seb_Ada;
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;

--  @summary
--  SE Common Mutex module.
--
--  @description
--  This package contains procedures required to acquire and release SE common mutex.
--
--  Traceability -
--        \ImplementsUnitDesign{Se_Common_Mutex_Tu10x}
package Se_Common_Mutex_Tu10x
with SPARK_Mode => On
is

   -- SEC2 Mutex id reserved
   -- @value SEC2_MUTEX_EMEM          mutex to be used to write to EMEM through EMEMC/D ports
   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data between SEC2 and LWDEC
   -- @value SEC2_MUTEX_VPR_SCRATCH   mutex to be used to write to VPR scratch registers
   -- @value SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME
                                   -- mutex that all ACR binaries use to execute one at a time to avoid any race
   -- @value SEC2_MUTEX_BSI_WRITE     mutex to be used to share Playready data
   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data
   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data

   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data
   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data
   -- @value SEC2_MUTEX_PLAYREADY     mutex to be used to share Playready data

   type SEC2_MUTEX_RESERVATION is (
    SEC2_MUTEX_EMEM,
    SEC2_MUTEX_PLAYREADY,

    SEC2_MUTEX_VPR_SCRATCH,
    SEC2_MUTEX_ACR_ALLOW_ONLY_ONE_ACR_BINARY_AT_ANY_TIME,

    SEC2_MUTEX_BSI_WRITE,
    SEC2_MUTEX_WPR_SCRATCH,
    SEC2_MUTEX_COLD_BOOT_GC6_UDE_COMPLETION,

    SEC2_MUTEX_MMU_VPR_WPR_WRITE,
    SEC2_MUTEX_PDISP_SELWRE_SCRATCH_0,
    SEC2_MUTEX_PR_PDUB_PGC6_BSI_SELWRE_SCRATCH_8,

    SEC2_MUTEX_DECODE_TRAPS_WAR_TU10X_BUG_2369597
    -- can be reclaimed from Ampere onwards
    -- SEC2_MUTEX_UNUSED            = 11:15
                                   )
     with size => 8, Object_Size => 8;


   function Selwre_Mutex_Derive_GroupId(MutexEnum : LwU8)
   return LwU8
   is (Safe_Shift_Right(MutexEnum, 4))
     with Inline_Always,
     Global => null;


   function Selwre_Mutex_Derive_MutexId(MutexEnum : LwU8)
   return LwU8
   is
      (MutexEnum and 16#0000_00FF#)
     with Inline_Always,
     Global => null;

   --  Requirement: This function shall do the following:
   --               1) Verify that the mutex group index and mutex index itself both are valid.
   --  @param Reg  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 if mutex index is valid else error.
   function Selwre_Mutex_Group_Index_Is_Valid(Group_Index: LwU8;
                                              Mutex_Index: LwU8 )
   return Boolean
   is
     ( (Group_Index < LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1) and then
          (Mutex_Index < LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2) )
       with Inline_Always,
       Global => null;

   --  Requirement: This procedure shall do the following:
   --               1) Acquire the SE Common mutex.
   --  @param Reg  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 if mutex is acquired successfully else error.
   procedure Se_Acquire_Common_Mutex( MutexId: SEC2_MUTEX_RESERVATION;
                                      Status : out LW_UCODE_UNIFIED_ERROR_TYPE;
                                      Mutex_Token : out LwU8)
     with
       Global => null,
       Depends => ( Status => null,
                    Mutex_Token => MutexId);
   pragma Linker_Section(Se_Acquire_Common_Mutex, LINKER_SECTION_NAME);

   --  Requirement: This procedure shall do the following:
   --               1) Release the acquired SE Common mutex.
   --  @param Reg  Output parameter : LW_UCODE_UNIFIED_ERROR_TYPE status. UCODE_ERR_CODE_NOERROR
   --                                 if mutex is released successfully else error.
   procedure Se_Release_Common_Mutex( MutexId: SEC2_MUTEX_RESERVATION;
                                      HwTokenFromCaller: LwU8;
                                      Status : out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Global => null,
       Depends => ( Status => MutexId,
                    null => HwTokenFromCaller);
   pragma Linker_Section(Se_Release_Common_Mutex, LINKER_SECTION_NAME);


   --------------------- CONSTANTS -------------------
   LW_MUTEX_OWNER_ID_ILWALID: constant LwU8 := 0;
private

   --------------------- CONSTANTS -------------------
   SE_COMMON_MUTEX_TIMEOUT_VAL_USEC       : constant := 16#10_0000#;

end Se_Common_Mutex_Tu10x;
