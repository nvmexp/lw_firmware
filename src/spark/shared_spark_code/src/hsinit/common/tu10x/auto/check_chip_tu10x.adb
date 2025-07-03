-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Master; use Dev_Master;

package body Check_Chip_Tu10x is

   procedure Check_Correct_Chip(Chip_Id : LwU9; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         if Chip_Id /= LW_PMC_BOOT_42_CHIP_ID_TU104 then
            Status := CHIP_ID_ILWALID;
            Ghost_Chip_Validity_Status := CHIP_ILWALID;
            exit Main_Block;
         end if;

         -- Global ghost variable for Chip Validity Status is updated here
         Ghost_Chip_Validity_Status := CHIP_VALID;

      end loop Main_Block;

   end Check_Correct_Chip;

end Check_Chip_Tu10x;
