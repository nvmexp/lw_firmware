-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;

package Dev_Master
with SPARK_Mode => On
is
   -----------------------------LW_PMC_BOOT_42--------------------------------
   type LW_PMC_BOOT_42_Register is record
      Minor_Extended_Revision  : LwU4;
      Minor_Revision           : LwU4;
      Major_Revision           : LwU4;
      Chip_Id                  : LwU9;
      Rsvd                     : LwU3;
   end record
      with Size => 32, Object_Size => 32;

   for LW_PMC_BOOT_42_Register use record
      Minor_Extended_Revision  at 0 range  8 .. 11;
      Minor_Revision           at 0 range 12 .. 15;
      Major_Revision           at 0 range 16 .. 19;
      Chip_Id                  at 0 range 20 .. 28;
      Rsvd                     at 0 range 29 .. 31;
   end record;

   LW_PMC_BOOT_42_CHIP_ID_G94    : constant := 16#00000094#;
   LW_PMC_BOOT_42_CHIP_ID_G98    : constant := 16#00000098#;
   LW_PMC_BOOT_42_CHIP_ID_GT200  : constant := 16#000000A0#;
   LW_PMC_BOOT_42_CHIP_ID_GT206  : constant := 16#000000A6#;
   LW_PMC_BOOT_42_CHIP_ID_IGT206 : constant := 16#000000A7#;
   LW_PMC_BOOT_42_CHIP_ID_GT212  : constant := 16#000000A2#;
   LW_PMC_BOOT_42_CHIP_ID_IGT209 : constant := 16#000000A9#;
   LW_PMC_BOOT_42_CHIP_ID_GT214  : constant := 16#000000A4#;
   LW_PMC_BOOT_42_CHIP_ID_GT215  : constant := 16#000000A3#;
   LW_PMC_BOOT_42_CHIP_ID_GT216  : constant := 16#000000A5#;
   LW_PMC_BOOT_42_CHIP_ID_GT218  : constant := 16#000000A8#;
   LW_PMC_BOOT_42_CHIP_ID_GF100  : constant := 16#000000C0#;
   LW_PMC_BOOT_42_CHIP_ID_GF104  : constant := 16#000000C4#;
   LW_PMC_BOOT_42_CHIP_ID_GF105  : constant := 16#000000C5#;
   LW_PMC_BOOT_42_CHIP_ID_GF106  : constant := 16#000000C3#;
   LW_PMC_BOOT_42_CHIP_ID_GF108  : constant := 16#000000C1#;
   LW_PMC_BOOT_42_CHIP_ID_GF110  : constant := 16#000000D0#;
   LW_PMC_BOOT_42_CHIP_ID_GF110D : constant := 16#000000C6#;
   LW_PMC_BOOT_42_CHIP_ID_GF110F : constant := 16#000000C7#;
   LW_PMC_BOOT_42_CHIP_ID_GF112  : constant := 16#000000D2#;
   LW_PMC_BOOT_42_CHIP_ID_GF114  : constant := 16#000000D4#;
   LW_PMC_BOOT_42_CHIP_ID_GF116  : constant := 16#000000D6#;
   LW_PMC_BOOT_42_CHIP_ID_GF117  : constant := 16#000000D7#;
   LW_PMC_BOOT_42_CHIP_ID_GF118  : constant := 16#000000D8#;
   LW_PMC_BOOT_42_CHIP_ID_GF119  : constant := 16#000000D9#;
   LW_PMC_BOOT_42_CHIP_ID_GF11A  : constant := 16#000000CA#;
   LW_PMC_BOOT_42_CHIP_ID_GK100  : constant := 16#000000E0#;
   LW_PMC_BOOT_42_CHIP_ID_GK104  : constant := 16#000000E4#;
   LW_PMC_BOOT_42_CHIP_ID_GK106  : constant := 16#000000E6#;
   LW_PMC_BOOT_42_CHIP_ID_GK107  : constant := 16#000000E7#;
   LW_PMC_BOOT_42_CHIP_ID_GK110  : constant := 16#000000F0#;
   LW_PMC_BOOT_42_CHIP_ID_GK20A  : constant := 16#000000EA#;
   LW_PMC_BOOT_42_CHIP_ID_GK207  : constant := 16#00000107#;
   LW_PMC_BOOT_42_CHIP_ID_GK208  : constant := 16#00000108#;
   LW_PMC_BOOT_42_CHIP_ID_GK209  : constant := 16#00000109#;
   LW_PMC_BOOT_42_CHIP_ID_GM000  : constant := 16#00000110#;
   LW_PMC_BOOT_42_CHIP_ID_GM107  : constant := 16#00000117#;
   LW_PMC_BOOT_42_CHIP_ID_GM108  : constant := 16#00000118#;
   LW_PMC_BOOT_42_CHIP_ID_GM109  : constant := 16#00000119#;
   LW_PMC_BOOT_42_CHIP_ID_GM20C  : constant := 16#0000011C#;
   LW_PMC_BOOT_42_CHIP_ID_GM20B  : constant := 16#0000012B#;
   LW_PMC_BOOT_42_CHIP_ID_GM20D  : constant := 16#0000012D#;
   LW_PMC_BOOT_42_CHIP_ID_GM104  : constant := 16#00000114#;
   LW_PMC_BOOT_42_CHIP_ID_GM204  : constant := 16#00000124#;
   LW_PMC_BOOT_42_CHIP_ID_GM206  : constant := 16#00000126#;
   LW_PMC_BOOT_42_CHIP_ID_GM200  : constant := 16#00000120#;
   LW_PMC_BOOT_42_CHIP_ID_GP000  : constant := 16#00000131#;
   LW_PMC_BOOT_42_CHIP_ID_GP100  : constant := 16#00000130#;
   LW_PMC_BOOT_42_CHIP_ID_GP102  : constant := 16#00000132#;
   LW_PMC_BOOT_42_CHIP_ID_GP104  : constant := 16#00000134#;
   LW_PMC_BOOT_42_CHIP_ID_GP104V : constant := 16#00000139#;
   LW_PMC_BOOT_42_CHIP_ID_GP106  : constant := 16#00000136#;
   LW_PMC_BOOT_42_CHIP_ID_GP107F : constant := 16#00000133#;
   LW_PMC_BOOT_42_CHIP_ID_GP107  : constant := 16#00000137#;
   LW_PMC_BOOT_42_CHIP_ID_GP108  : constant := 16#00000138#;
   LW_PMC_BOOT_42_CHIP_ID_GP108V : constant := 16#0000013A#;
   LW_PMC_BOOT_42_CHIP_ID_GP10B  : constant := 16#0000013B#;
   LW_PMC_BOOT_42_CHIP_ID_GP10D  : constant := 16#0000013D#;
   LW_PMC_BOOT_42_CHIP_ID_GV100  : constant := 16#00000140#;
   LW_PMC_BOOT_42_CHIP_ID_GV000  : constant := 16#00000141#;
   LW_PMC_BOOT_42_CHIP_ID_GV100F : constant := 16#0000014F#;
   LW_PMC_BOOT_42_CHIP_ID_GV10B  : constant := 16#0000014B#;
   LW_PMC_BOOT_42_CHIP_ID_GV11B  : constant := 16#0000015B#;
   LW_PMC_BOOT_42_CHIP_ID_TU100  : constant := 16#00000160#;
   LW_PMC_BOOT_42_CHIP_ID_TU101  : constant := 16#00000160#;
   LW_PMC_BOOT_42_CHIP_ID_LR10   : constant := 16#00000160#;
   LW_PMC_BOOT_42_CHIP_ID_TU000  : constant := 16#00000161#;
   LW_PMC_BOOT_42_CHIP_ID_TU102  : constant := 16#00000162#;
   LW_PMC_BOOT_42_CHIP_ID_TU104  : constant := 16#00000164#;
   LW_PMC_BOOT_42_CHIP_ID_TU105  : constant := 16#00000165#;
   LW_PMC_BOOT_42_CHIP_ID_TU106  : constant := 16#00000166#;
   LW_PMC_BOOT_42_CHIP_ID_TU107  : constant := 16#00000167#;
   LW_PMC_BOOT_42_CHIP_ID_TU117  : constant := 16#00000167#;
   LW_PMC_BOOT_42_CHIP_ID_TU116  : constant := 16#00000168#;
   LW_PMC_BOOT_42_CHIP_ID_GA100  : constant := 16#00000170#;
   LW_PMC_BOOT_42_CHIP_ID_GA000  : constant := 16#00000171#;
   LW_PMC_BOOT_42_CHIP_ID_GA102  : constant := 16#00000172#;
   LW_PMC_BOOT_42_CHIP_ID_G000   : constant := 16#000001E0#;
   ------------------------------------ Offsets of various registers --------------------------------
   LW_PMC_BOOT_42_ADDR                           : constant BAR0_ADDR := 16#0A00#;


end Dev_Master;
