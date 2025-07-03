-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package Dev_Falc_Spr is

   ------------------------------------------------
   ----------- 1. Register: SEC_SPR ---------------
   ------------------------------------------------

   --------------- Fields Declaration ---------------------
   -- 0 .. 15 : Secure block base
   type LW_FALC_SEC_SPR_SELWRE_BLK_BASE_Field is new LwU16;

   -- 16: Secure
   type LW_FALC_SEC_SPR_SELWRE_Field is (Clear, Set) with Size => 1;
   for LW_FALC_SEC_SPR_SELWRE_Field use (Clear => 0, Set => 1);

   -- 17: Encrypted
   type LW_FALC_SEC_SPR_ENCRYPTED_Field is (Clear, Set) with Size => 1;
   for LW_FALC_SEC_SPR_ENCRYPTED_Field use (Clear => 0, Set => 1);

   -- 18: Signature Valid
   type LW_FALC_SEC_SPR_SIGNATURE_VALID_Field is (Clear, Set) with Size => 1;
   for LW_FALC_SEC_SPR_SIGNATURE_VALID_Field use (Clear => 0, Set => 1);

   -- 19: Disable exceptions
   type LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_Field is (Clear, Set) with Size => 1;
   for LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_Field use (Clear => 0, Set => 1);

   -- 24 .. 31: Secure block count
   type LW_FALC_SEC_SPR_SELWRE_BLK_CNT_Field is new LwU8;

      --------------- Register Record Declaration -------------
   type LW_FALC_SEC_SPR_Register is record
      Selwre_Blk_Base       : LW_FALC_SEC_SPR_SELWRE_BLK_BASE_Field;
      Secure                : LW_FALC_SEC_SPR_SELWRE_Field;
      Encrypted             : LW_FALC_SEC_SPR_ENCRYPTED_Field;
      Sig_Valid             : LW_FALC_SEC_SPR_SIGNATURE_VALID_Field;
      Disable_Exceptions    : LW_FALC_SEC_SPR_DISABLE_EXCEPTIONS_Field;
      Selwre_Blk_Cnt        : LW_FALC_SEC_SPR_SELWRE_BLK_CNT_Field;
   end record
     with Size => 32, Object_Size => 32, Alignment => 4;

   for LW_FALC_SEC_SPR_Register use record
      Selwre_Blk_Base at 0 range 0 .. 15;
      Secure at 0 range 16 .. 16;
      Encrypted at 0 range 17 .. 17;
      Sig_Valid at 0 range 18 .. 18;
      Disable_Exceptions    at 0 range 19 .. 19;
      Selwre_Blk_Cnt at 0 range 24 .. 31;
   end record;

   ------------------------------------------------
   ----------- 2. Register: CSW_SPR ---------------
   ------------------------------------------------

   --------------- Fields Declaration ---------------------
   type LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields is (Disable, Enable) with Size => 1;
   for  LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields use (Disable => 0, Enable => 1);

   type LW_FALC_CSW_SPR_CLVL_Field is new LwU3 with Size => 3;

   --------------- Register Record Declaration -------------
   type LW_FALC_CSW_SPR_REGISTER is record
      F_Ie0       : LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields;
      F_Ie1       : LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields;
      F_Ie2       : LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields;
      F_Exception : LW_FALC_CSW_SPR_ENABLE_DISABLE_Fields;
      F_Clvl      : LW_FALC_CSW_SPR_CLVL_Field;
   end record
     with Size => 32, Object_Size => 32;

   for LW_FALC_CSW_SPR_REGISTER use record
      F_Ie0       at 0 range 16 .. 16;
      F_Ie1       at 0 range 17 .. 17;
      F_Ie2       at 0 range 18 .. 18;
      F_Exception at 0 range 24 .. 24;
      F_Clvl      at 0 range 26 .. 28;
   end record;

end Dev_Falc_Spr;
