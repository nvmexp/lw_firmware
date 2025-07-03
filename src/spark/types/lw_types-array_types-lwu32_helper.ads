-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Ada.Unchecked_Colwersion;

--  @summary
--  Defining types
--
--  @description
--  Lw_Types.Array_Types is a child package of Lw_Types which declares different array types.

package Lw_Types.Array_Types.LwU32_Helper
with SPARK_Mode => On
is

   --  Array of LwU32 that will be used to represent contents of a single dword.
   subtype ARR_LwU32 is ARR_LwU32_IDX32( 0 .. LwU32( LwU32'Size/LwU32'Size - 1 ) )
     with Object_Size => 32;

   --  This is an instantiation of Unchecked colwersion that is needed to prove post-condition contract for
   --  Gfw_Image_Read. This will be used with the call to Gfw_Image_Read instantiated to read a dword from EEPROM.
   pragma Assert ( LwU32'Size = ARR_LwU32'Size );
   function UC_LwU32 is new Ada.Unchecked_Colwersion( Source => LwU32,
                                                      Target => ARR_LwU32 );

end Lw_Types.Array_Types.LwU32_Helper;
