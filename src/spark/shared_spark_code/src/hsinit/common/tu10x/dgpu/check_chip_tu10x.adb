package body Check_Chip_Tu10x is

   procedure Check_Correct_Chip(Chip_Id : LwU9; Status : out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin

      if Chip_Id /= LW_PMC_BOOT_42_CHIP_ID_TU102 and then
           Chip_Id /= LW_PMC_BOOT_42_CHIP_ID_TU104 and then
           Chip_Id /= LW_PMC_BOOT_42_CHIP_ID_TU106
      then
         Status := CHIP_ID_ILWALID;
      else
         Status := UCODE_ERR_CODE_NOERROR;
      end if;
   end Check_Correct_Chip;


end Check_Chip_Tu10x;
