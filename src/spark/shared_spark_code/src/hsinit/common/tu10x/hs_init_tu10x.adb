-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Bar0_Reg_Rd_Wr_Instances; use Bar0_Reg_Rd_Wr_Instances;
with Dev_Fpf; use Dev_Fpf;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;
with Ucode_Specific_Initializations; use Ucode_Specific_Initializations;
with Dev_Master; use Dev_Master;
with Ada.Unchecked_Colwersion;

package body Hs_Init_Tu10x
with SPARK_Mode => On
is


   procedure Read_Fpf_Fuse_Pub_Rev(Data : out LwU8;  Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Reg_Val : LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER;
   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         --
         -- Added to please prover for uninitialized "Data" in case of Pri Err in below Bar0 access
         --
         Data := 0;

         Bar0_Reg_Rd32_Fpf_Opt_Fuse(Reg    => Reg_Val,
                                    Addr   => Get_Ucode_Specific_Fpf_Reg_Addr,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Data := Reg_Val.Data;

      end loop Main_Block;

   end Read_Fpf_Fuse_Pub_Rev;

   procedure Read_Chip_Id(Chip_Id : out LwU9;  Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Reg_Val : LW_PMC_BOOT_42_Register;

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         --
         -- Added to please prover for uninitialized "Data" in case of Pri Err in below Bar0 access
         --
         Chip_Id := 0;
         Bar0_Reg_Rd32_Boot42(Reg    => Reg_Val,
                              Addr   => LW_PMC_BOOT_42_ADDR,
                              Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Chip_Id := Reg_Val.Chip_Id;

      end loop Main_Block;

   end Read_Chip_Id;


   procedure Read_Fuse_Opt(Data : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Reg_Val : LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Added to please prover for uninitialized "Data" in case of Pri Err in below Bar0 access
         Data := 0;

         Bar0_Reg_Rd32_Fuse_Opt_Fuse(Reg    => Reg_Val,
                                     Addr   => Get_Ucode_Specific_Fuse_Reg_Addr,
                                     Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Data := LwU8(Reg_Val.F_Data);

      end loop Main_Block;
   end Read_Fuse_Opt;

   procedure Get_Ucode_Rev_Val(Rev    : out LwU8;
                               Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Chip_ID : LwU9;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Rev := LwU8'Last;

         Read_Chip_Id(Chip_ID, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         case Chip_ID is
            when LW_PMC_BOOT_42_CHIP_ID_TU102 =>
               Rev := UCODE_TU102_UCODE_BUILD_VERSION;
            when LW_PMC_BOOT_42_CHIP_ID_TU104 =>
               Rev := UCODE_TU104_UCODE_BUILD_VERSION;
            when LW_PMC_BOOT_42_CHIP_ID_TU106 =>
               Rev := UCODE_TU106_UCODE_BUILD_VERSION;
            when others =>
               Status := CHIP_ID_ILWALID;
               Ghost_Sw_Revocation_Status := SW_REVOCATION_ILWALID;
               exit Main_Block;
         end case;

         Ghost_Sw_Revocation_Status := SW_REVOCATION_VALID;
      end loop Main_Block;

   end Get_Ucode_Rev_Val;

   pragma Assert (LwU32'Size = LW_FLCN_IMTAG_BLK_REGISTER'Size);
   function UC_Imtag_Blk_Reg is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                              Target => LW_FLCN_IMTAG_BLK_REGISTER);

   procedure Check_Imem_Blk_Validity (Val : LwU32;
                                      Ilwalidate : out LwBool)
   is
      Imem_Blk_Status : LW_FLCN_IMTAG_BLK_REGISTER;
   begin
      Ilwalidate := LW_FALSE;

      Imem_Blk_Status := UC_Imtag_Blk_Reg( Val );

      if Imblk_Is_Valid( Imem_Blk_Status ) = True and then
        Imem_Blk_Status.F_Blk_Selwre = 0
      then
         Ilwalidate := LW_TRUE;
      end if;

   end Check_Imem_Blk_Validity;

end Hs_Init_Tu10x;
