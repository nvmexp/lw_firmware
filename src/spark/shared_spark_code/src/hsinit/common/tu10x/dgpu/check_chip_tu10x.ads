with Lw_Types; use Lw_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Dev_Master; use Dev_Master;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;

package Check_Chip_Tu10x is

   procedure Check_Correct_Chip ( Chip_Id : LwU9; Status : out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Post => ( if Chip_Id = LW_PMC_BOOT_42_CHIP_ID_TU102 or else
                   Chip_Id = LW_PMC_BOOT_42_CHIP_ID_TU104 or else
                     Chip_Id = LW_PMC_BOOT_42_CHIP_ID_TU106 then
                   Status = UCODE_ERR_CODE_NOERROR
                ),
       Global => null,
       Depends => ( Status => Chip_Id),
       Linker_Section => LINKER_SECTION_NAME;


end Check_Chip_Tu10x;
