-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Defining types
--
--  @description
--  Lw_Types.Shift_Right_Op is a child package of Lw_Types which declares right shift operations on LwU8, LwU16, LwU32
--  and LwU64.
package Lw_Types.Shift_Right_Op
with SPARK_Mode => On
is

   --  Requirement: This procedure shall do the following:
   --               1) Takes LwU8 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right ( Value : LwU8; Amount : Natural ) return LwU8
   is ( Shift_Right ( Value, Amount ) )
     with
       Pre => Amount > 0 and then
       Amount <= LwU8'Size,
       Global => null,
       Depends => ( Safe_Shift_Right'Result => ( Value, Amount ) ),
       Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes LwU16 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right ( Value : LwU16; Amount : Natural ) return LwU16
   is ( Shift_Right ( Value, Amount ) )
     with
       Pre => Amount > 0 and then
       Amount <= LwU16'Size,
       Global => null,
       Depends => ( Safe_Shift_Right'Result => ( Value, Amount ) ),
       Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes LwU32 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right ( Value : LwU32; Amount : Natural ) return LwU32
   is ( Shift_Right ( Value, Amount ) )
     with
       Pre => Amount > 0 and then
       Amount <= LwU32'Size,
       Global => null,
       Depends => ( Safe_Shift_Right'Result => ( Value, Amount ) ),
       Inline_Always;

   --  Requirement: This procedure shall do the following:
   --               1) Takes LwU64 as input and right shift by the amount specified.
   --  @param  Value Input parameter : Value to be right shifted
   --  @param  Amount Input parameter : Amount of right shift
   --  @return Right shifted value by the amount specified
   function Safe_Shift_Right ( Value : LwU64; Amount : Natural ) return LwU64
   is ( Shift_Right ( Value, Amount ) )
     with
       Pre => Amount > 0 and then
       Amount <= LwU64'Size,
       Global => null,
       Depends => ( Safe_Shift_Right'Result => ( Value, Amount ) ),
       Inline_Always;

end Lw_Types.Shift_Right_Op;
