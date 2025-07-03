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
--  Lw_Types.Array_Types is a child package of Lw_Types which declares different array types.
package Lw_Types.Array_Types
with SPARK_Mode => On
is

   --  ARR_LwU32_IDX8 is an unconstrained array of LwU32s whose index can be in range 0 to 255.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_LwU32_IDX8 is array (LwU8 range <>) of LwU32;

   --  ARR_LwU8_IDX8 is an unconstrained array of LwU8s whose index can be in range 0 to 255.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_LwU8_IDX8 is array (LwU8 range <>) of LwU8;

   --  ARR_LwU8_IDX8 is an unconstrained array of LwU8s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_LwU8_IDX32 is array (LwU32 range <>) of LwU8;

   --  ARR_LwU32_IDX32 is an unconstrained array of LwU32s whose index can be in range 0 to 2^32-1.
   --  Refer to  https://learn.adacore.com/courses/intro-to-ada/chapters/arrays.html#unconstrained-arrays.
   type ARR_LwU32_IDX32 is array (LwU32 range <>) of LwU32;

end Lw_Types.Array_Types;
