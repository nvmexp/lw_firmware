-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  Tu10x specific for Auto, HW Independent Check correct chip
--
--  @description
--  This package contains procedures for checking if binary is running on correct
--  chip.

package Check_Chip_Tu10x is

   procedure Check_Correct_Chip ( Chip_Id : LwU9; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Chip_Validity_Status = CHIP_VALIDITY_NOT_VERIFIED),
     Post => ( if Ghost_Chip_Validity_Status = CHIP_ILWALID then
                 Status = CHIP_ID_ILWALID
                   else
                 (Status = UCODE_ERR_CODE_NOERROR and then
                      Ghost_Chip_Validity_Status = CHIP_VALID)),
       Global => (In_Out => Ghost_Chip_Validity_Status),
       Depends => ( Status => Chip_Id,
                    Ghost_Chip_Validity_Status => Chip_Id,
                    null => (Ghost_Chip_Validity_Status,
                             Status)),
     Linker_Section => LINKER_SECTION_NAME;


end Check_Chip_Tu10x;
