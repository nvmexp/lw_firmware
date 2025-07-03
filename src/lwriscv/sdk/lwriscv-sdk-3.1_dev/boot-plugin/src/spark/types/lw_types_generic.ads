-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining Operators of LwUx types.
--
-- @description
-- Override the +, -, *, / operators of LwUx Types.

generic
   type Base_Type is mod <>;
package Lw_Types_Generic with SPARK_Mode => On is

   type NType is new Base_Type;

   overriding
   function "+" (Left, Right : NType) return NType
   is (NType (Base_Type (Left) + Base_Type (Right)))
     with Pre => (Left <= NType'Last - Right), Inline_Always;

   overriding
   function "-" (Left, Right : NType) return NType
   is (NType (Base_Type (Left) - Base_Type (Right)))
     with Pre => (Left >= Right), Inline_Always;

   overriding
   function "/" (Left, Right : NType) return NType
   is (NType (Base_Type (Left) / Base_Type (Right)))
     with Pre => (Right /= 0), Inline_Always;

   overriding
   function "*" (Left, Right : NType) return NType
   is (NType (Base_Type (Left) * Base_Type (Right)))
     with Pre => (Right = 0 or else Left <= NType'Last / Right), Inline_Always;

end Lw_Types_Generic;
