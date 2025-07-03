-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Implement sizeof operations for different types.
--
--  @description
--  Defines functions which return size of a particular type.

package Lw_Types.Get_Size
with SPARK_Mode => On
is

   --  This function does the following:
   --  1) Accepts Size as input.
   --  2) Outputs the equivalent Size in Bytes.
   pragma Suppress(All_Checks);
   function Get_Size_Bytes( Size : LwU32 )
                           return LwU32 is
     ( Size/LwU8'Size )
     with
       Pre => ( Size >= LwU8'Size and then
                  Size mod LwU8'Size = 0 ),
     Global => null,
     Depends => ( Get_Size_Bytes'Result => Size ),
     Inline_Always;

   --  This function does the following:
   --  1) Accepts Size as input.
   --  2) Outputs the equivalent Size in Dwords.
   function Get_Size_Dwords( Size : LwU32 )
                            return LwU32 is
     ( Size/LwU32'Size )
     with
       Pre => ( Size >= LwU32'Size and then
                  Size mod LwU32'Size = 0 ),
     Global => null,
     Depends => ( Get_Size_Dwords'Result => Size ),
     Inline_Always;

end Lw_Types.Get_Size;
