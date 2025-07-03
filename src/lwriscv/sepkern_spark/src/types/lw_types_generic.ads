-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

generic 
    type Base_Type is mod <>;

package Lw_Types_Generic
    with SPARK_Mode => On
is

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
