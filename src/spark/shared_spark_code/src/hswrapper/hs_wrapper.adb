-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ucode_Post_Codes; use Ucode_Post_Codes;
with Sec_Entry; use Sec_Entry;
with Pub_Entry; use Pub_Entry;
with Lw_Types; use Lw_Types;
with Cleanup; use Cleanup;

pragma Warnings(Off, "no entities of * are referenced"); -- WAR
with Hs_Init_Falc_Specific_Tu10x; use Hs_Init_Falc_Specific_Tu10x;
with Exceptions_Falc_Specific_Tu10x; use Exceptions_Falc_Specific_Tu10x;
pragma Warnings(On, "no entities of * are referenced");

package body Hs_Wrapper is

   procedure Hs_Entry
   is
      Status : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
   begin
      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         Hs_Entry_Inline;

         Hs_Entry_NonInline(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Binary_Core_Entry(Status);
         --Update_Status_In_Mailbox(Status);
         --Ucode_Halt;

      end loop Main_Block;

      Hs_Exit(Status);
   end Hs_Entry;

   procedure Hs_Wrapper_Entry
   is
   begin
      -- Step 1: Set SP
      -- Setting SP to end of dmem which is 0xFFFF.
      -- This eventually sets SP to point to 0xFFFC.
      Hs_Entry_Set_Sp(Shift_Left(Value  => FLCN_DMEM_SIZE_IN_BLK,
                                 Amount => FLCN_DMEM_BLK_ALIGN_BITS) -1);

      -- Since this procedure is imported from C, we assume that this step is done
      --      Ghost_Hs_Init_States_Tracker := SP_SETUP;
      pragma Assume(Ghost_Hs_Init_States_Tracker = SP_SETUP);
      -- Ilwokes main HS_Entry
      Hs_Entry;
      Ucode_Halt;
   end Hs_Wrapper_Entry;
end Hs_Wrapper;
