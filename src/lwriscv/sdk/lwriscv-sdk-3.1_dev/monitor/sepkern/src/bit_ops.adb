--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;
with Lw_Types.Shift_Right_Op; use Lw_Types.Shift_Right_Op;

package body Bit_Ops
is
    function Flip_Bits (Input : LwU32) return LwU32
    is
    begin
        return Input xor 16#FFFF_FFFF#;
    end Flip_Bits;

    procedure Modify_Field(In_out       : in out LwU32;
                           Field_Offset : LwU32;
                           Field_Width  : LwU32;
                           Field_Value  : LwU32)
    with
        SPARK_Mode => Off
    is
        Set_Mask  : LwU32 := Lw_Shift_Left (LwU32(1), Natural(Field_Width)) - 1;
        Clr_Mask  : LwU32 := Flip_Bits (Lw_Shift_Left (Set_Mask, Natural(Field_Offset)));
        Set_Value : LwU32 := Lw_Shift_Left ((Field_Value and Set_Mask), Natural(Field_Offset));
    begin
        In_out := (In_out and Clr_Mask) or Set_Value;

    end Modify_Field;
end Bit_Ops;
