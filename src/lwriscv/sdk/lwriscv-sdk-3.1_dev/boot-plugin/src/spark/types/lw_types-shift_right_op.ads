-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining types.
--
-- @description
-- Lw_Types.Shift_Right_Op is a child package of Lw_Types which declares right shift operations on LwU8, LwU16, LwU32
-- and LwU64.
package Lw_Types.Shift_Right_Op
with SPARK_Mode => On
is
   function Lw_Shift_Right (Value : LwU8; Amount : Natural) return LwU8
     is (Shift_Right (Value, Amount))
     with
       Pre => Amount <= LwU8'Size,
       Global => null,
       Depends => ( Lw_Shift_Right'Result => ( Value, Amount ) );

   function Lw_Shift_Right (Value : LwU16; Amount : Natural) return LwU16
     is (Shift_Right (Value, Amount))
     with
       Pre => Amount <= LwU16'Size,
       Global => null,
       Depends => ( Lw_Shift_Right'Result => ( Value, Amount ) );

   function Lw_Shift_Right (Value : LwU32; Amount : Natural) return LwU32
     is (Shift_Right (Value, Amount))
     with
       Pre => Amount <= LwU32'Size,
       Global => null,
       Depends => ( Lw_Shift_Right'Result => ( Value, Amount ) );

   function Lw_Shift_Right (Value : LwU64; Amount : Natural) return LwU64
     is (Shift_Right (Value, Amount))
     with
       Pre => Amount <= LwU64'Size,
       Global => null,
       Depends => ( Lw_Shift_Right'Result => ( Value, Amount ) );

end Lw_Types.Shift_Right_Op;
