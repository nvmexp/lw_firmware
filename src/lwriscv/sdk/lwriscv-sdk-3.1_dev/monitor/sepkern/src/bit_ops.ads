--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package Bit_Ops
is
    function Flip_Bits(Input : LwU32) return LwU32;

    procedure Modify_Field(In_out       : in out LwU32;
                           Field_Offset : LwU32;
                           Field_Width  : LwU32;
                           Field_Value  : LwU32)
    with
        Pre => Field_Width <= LwU32'Size and then
               Field_Offset < LwU32'Size and then
               (Field_Offset + Field_Width) <= LwU32'Size;
end Bit_Ops;
