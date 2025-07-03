-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;

package Typecast is

   -- Define the System.Address to/from LwU64 colwertor
   function Address_To_LwU64 is new Ada.Unchecked_Colwersion (Source => System.Address,
                                                              Target => LwU64) with Inline_Always;

   function LwU64_To_Address is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                              Target => System.Address) with Inline_Always;

end Typecast;
