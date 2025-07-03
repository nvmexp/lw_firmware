-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Hs_Init_Tu10x; use Hs_Init_Tu10x;
with Check_Chip_Tu10x; use Check_Chip_Tu10x;
with Utility; use Utility;
with Lw_Types.Boolean; use Lw_Types.Boolean;
with Hs_Init_Falc_Specific_Tu10x; use Hs_Init_Falc_Specific_Tu10x;

package body Hs_Init
with SPARK_Mode => on
is

   procedure Is_Valid_Ucode_Rev ( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      revLockValChipOptionFuse : LwU8;
      revLockValFpf            : LwU8;
      revLockValFirmware       : LwU8;

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Get_Ucode_Rev_Chip_Option_Fuse_Val(Revlock_Fuse_Val => revLockValChipOptionFuse, Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Get_Ucode_Rev_Val(Rev    => revLockValFirmware,
                           Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Get_Ucode_Rev_Fpf_Val(Rev    => revLockValFpf, Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- TODO : Check with harish code
         if revLockValFirmware < revLockValChipOptionFuse or else
           revLockValFirmware < revLockValFpf or else
           revLockValFpf = ILWALID_FPF_REV_VAL
         then
            Status := ILWALID_UCODE_REVISION;
            -- Update Ghost variable for SW Revocation Status
            Ghost_Sw_Revocation_Status := SW_REVOCATION_ILWALID;

            exit Main_Block;
         end if;

         -- Update HS Init State in ghost variable for SPARK Prover
         Ghost_Hs_Init_States_Tracker := SW_REVOCATION_CHECKED;

         -- Update SW Revocation State in ghost variable for SPARK Prover
         Ghost_Sw_Revocation_Status := SW_REVOCATION_VALID;

      end loop Main_Block;

   end Is_Valid_Ucode_Rev;

   procedure Get_Ucode_Rev_Fpf_Val(Rev    : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      Data    : LwU8;
   begin

      Main_Block:
      for Unused_Loop_Var_Main in 1 .. 1 loop

         Rev := 0;
         -- Ghost_Sw_Revocation_Rev_Fpf_Val has to have value in
         -- case below CSB transaction fails
         pragma Assume (Ghost_Sw_Revocation_Rev_Fpf_Val = 0);

         Read_Fpf_Fuse_Pub_Rev(Data, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Ghost_Sw_Revocation_Rev_Fpf_Val assume value to be Data to prove logic
         pragma Assume (Ghost_Sw_Revocation_Rev_Fpf_Val = Data);

         if Data not in 0 | 1 | 3 | 7 | 15 | 31 | 63 | 127 | 255
         then
            Rev := ILWALID_FPF_REV_VAL;
         else
            Loop_Until:
            for Unused_Loop_Var in 1 .. 8 loop
               exit Loop_Until when Data = 0;

               Rev := Rev + 1;
               Data := Shift_Right(Value  => Data,
                                   Amount => 1);
            end loop Loop_Until;
         end if;

      end loop Main_Block;

   end Get_Ucode_Rev_Fpf_Val;



   procedure Get_Ucode_Rev_Chip_Option_Fuse_Val(Revlock_Fuse_Val : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Data : LwU8;
   begin
      Revlock_Fuse_Val:= 0;

      Main_Block:

      for Unused_Loop_Var in 1 .. 1 loop

         Read_Fuse_Opt(Data, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Revlock_Fuse_Val := Data;
      end loop Main_Block;

   end Get_Ucode_Rev_Chip_Option_Fuse_Val;

   procedure Is_Valid_Chip( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Chip_Id : LwU9;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Read_Chip_Id(Chip_Id => Chip_Id,
                      Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Check_Correct_Chip(Chip_Id, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Update HS Init State in ghost variable for SPARK Prover
         Ghost_Hs_Init_States_Tracker := BOOT42_CHECKED;

      end loop Main_Block;

   end Is_Valid_Chip;

   procedure Ilwalidate_Ns_Blocks( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is

      pragma Suppress(All_Checks);

      Imem_Blks       : LwU32;
      Temp            : LwU32;
      Ilwalidate      : LwBool;

   begin

      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop


         Callwlate_Imem_Size_In_256B(Imem_Size => Imem_Blks,
                                     Status    => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         for Blk_No in 0 .. ( Imem_Blks - 1 ) loop

            Falc_Imblk_C( Temp, Blk_No );
            Check_Imem_Blk_Validity(Val        => Temp,
                                    Ilwalidate => Ilwalidate);

            if Ilwalidate = LW_TRUE
            then
               Falc_Imilw_C(Blk_No);
            end if;

         end loop;

         -- Update HS Init State in ghost variable for SPARK Prover
         Ghost_Hs_Init_States_Tracker := NS_BLOCKS_ILWALIDATED;

      end loop Main_Block;

   end Ilwalidate_Ns_Blocks;
end Hs_Init;
