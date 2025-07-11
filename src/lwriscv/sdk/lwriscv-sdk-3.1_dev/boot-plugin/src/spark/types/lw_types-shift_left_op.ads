-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining types
--
-- @description
-- Lw_Types.Shift_Left_Op is a child package of Lw_Types which declares left shift operations on LwU8, LwU16, LwU32
-- and LwU64. This specification of Shift_Left helps meet equivalent of Cert C rule INT24-C in ADA.
package Lw_Types.Shift_Left_Op
with SPARK_Mode => On
is
   function Lw_Shift_Left (Value : LwU8; Amount : Natural) return LwU8
     is (Shift_Left (Value, Amount))
     with
       Pre => Amount <= LwU8'Size and then
       (Amount = 0 or else (Value < 2 ** (LwU8'Size - Amount)) ),
     Global => null,
     Depends => ( Lw_Shift_Left'Result => ( Value, Amount ) );

   function Lw_Shift_Left (Value : LwU16; Amount : Natural) return LwU16
     is (Shift_Left (Value, Amount))
     with
       Pre => Amount <= LwU16'Size and then
       (Amount = 0 or else (Value < 2 ** (LwU16'Size - Amount)) ),
     Global => null,
     Depends => ( Lw_Shift_Left'Result => ( Value, Amount ) );

   function Lw_Shift_Left (Value : LwU32; Amount : Natural) return LwU32
     is (Shift_Left (Value, Amount))
     with
       Pre => Amount <= LwU32'Size and then
       (Amount = 0 or else (Value < 2 ** (LwU32'Size - Amount)) ),
     Global => null,
     Depends => ( Lw_Shift_Left'Result => ( Value, Amount ) );

   function Lw_Shift_Left (Value : LwU64; Amount : Natural) return LwU64
     is (Shift_Left (Value, Amount))
     with
       Pre => Amount <= LwU64'Size and then
       (Amount = 0 or else (Value < 2 ** (LwU64'Size - Amount)) ),
     Global => null,
     Depends => ( Lw_Shift_Left'Result => ( Value, Amount ) );

end Lw_Types.Shift_Left_Op;
