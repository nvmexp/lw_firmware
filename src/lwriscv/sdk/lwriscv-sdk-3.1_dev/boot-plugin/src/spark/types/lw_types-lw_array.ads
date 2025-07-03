-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
-- 
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining array generic types
--  @description
-- Lw_Types.Lw_Array is a child package of Lw_Types which declares different array types.
with Ada.Unchecked_Colwersion;

package Lw_Types.Lw_Array is  

    type Arr_LwU1_Idx8 is array (LwU8 range <>) of LwU1;

    -- Define a unconstraint array whose element type is LwU8
    type Arr_LwU8_Idx8   is array (LwU8 range <>)  of LwU8;
    type Arr_LwU8_Idx32  is array (LwU32 range <>) of LwU8;
    
    type Lw_Bits_Generic   is array (LwU32 range <>) of LwU1;
    type Lw_Bytes_Generic  is array (LwU32 range <>) of LwU8;
    type Lw_DWords_Generic is array (LwU32 range <>) of LwU32;
    type Lw_QWords_Generic is array (LwU32 range <>) of LwU64;
      
    subtype Lw_B_Block32_Range   is LwU32 range 0 .. 3;
    subtype Lw_B_Block48_Range   is LwU32 range 0 .. 5;
    subtype Lw_B_Block56_Range   is LwU32 range 0 .. 6;
    subtype Lw_B_Block64_Range   is LwU32 range 0 .. 7;
    subtype Lw_B_Block128_Range  is LwU32 range 0 .. 15;
    subtype Lw_B_Block160_Range  is LwU32 range 0 .. 19;
    subtype Lw_B_Block192_Range  is LwU32 range 0 .. 23;
    subtype Lw_B_Block256_Range  is LwU32 range 0 .. 31;
    subtype Lw_B_Block384_Range  is LwU32 range 0 .. 47;
    subtype Lw_B_Block448_Range  is LwU32 range 0 .. 55;
    subtype Lw_B_Block1024_Range is LwU32 range 0 .. 127;
    subtype Lw_B_Block3072_Range is LwU32 range 0 .. 383;
    subtype Lw_B_Block8192_Range is LwU32 range 0 .. 1023;

    
    type Lw_B_Block32   is new Lw_Bytes_Generic (Lw_B_Block32_Range)   with Object_Size => 32;
    type Lw_B_Block48   is new Lw_Bytes_Generic (Lw_B_Block48_Range)   with Object_Size => 48;
    type Lw_B_Block56   is new Lw_Bytes_Generic (Lw_B_Block56_Range)   with Object_Size => 56;
    type Lw_B_Block64   is new Lw_Bytes_Generic (Lw_B_Block64_Range)   with Object_Size => 64;
    type Lw_B_Block128  is new Lw_Bytes_Generic (Lw_B_Block128_Range)  with Object_Size => 128;
    type Lw_B_Block160  is new Lw_Bytes_Generic (Lw_B_Block160_Range)  with Object_Size => 160;
    type Lw_B_Block192  is new Lw_Bytes_Generic (Lw_B_Block192_Range)  with Object_Size => 192;
    type Lw_B_Block256  is new Lw_Bytes_Generic (Lw_B_Block256_Range)  with Object_Size => 256;
    type Lw_B_Block384  is new Lw_Bytes_Generic (Lw_B_Block384_Range)  with Object_Size => 384;
    type Lw_B_Block448  is new Lw_Bytes_Generic (Lw_B_Block448_Range)  with Object_Size => 448;
    type Lw_B_Block1024 is new Lw_Bytes_Generic (Lw_B_Block1024_Range) with Object_Size => 1024;
    type Lw_B_Block3072 is new Lw_Bytes_Generic (Lw_B_Block3072_Range) with Object_Size => 3072;
    type Lw_B_Block8192 is new Lw_Bytes_Generic (Lw_B_Block8192_Range) with Object_Size => 8192;
    
    subtype Lw_DW_Block64_Range   is LwU32 range 0 .. 1;
    subtype Lw_DW_Block128_Range  is LwU32 range 0 .. 3;
    subtype Lw_DW_Block160_Range  is LwU32 range 0 .. 4;
    subtype Lw_DW_Block192_Range  is LwU32 range 0 .. 5;
    subtype Lw_DW_Block256_Range  is LwU32 range 0 .. 7;
    subtype Lw_DW_Block384_Range  is LwU32 range 0 .. 11;
    subtype Lw_DW_Block448_Range  is LwU32 range 0 .. 13;
    subtype Lw_DW_Block512_Range  is LwU32 range 0 .. 15;
    subtype Lw_DW_Block768_Range  is LwU32 range 0 .. 23;
    subtype Lw_DW_Block3072_Range is LwU32 range 0 .. 95;
    subtype Lw_DW_Block4096_Range is LwU32 range 0 .. 127;

    type Lw_DW_Block64   is new Lw_DWords_Generic (Lw_DW_Block64_Range)   with Object_Size => 64;
    type Lw_DW_Block128  is new Lw_DWords_Generic (Lw_DW_Block128_Range)  with Object_Size => 128;
    type Lw_DW_Block160  is new Lw_DWords_Generic (Lw_DW_Block160_Range)  with Object_Size => 160;
    type Lw_DW_Block192  is new Lw_DWords_Generic (Lw_DW_Block192_Range)  with Object_Size => 192;
    type Lw_DW_Block256  is new Lw_DWords_Generic (Lw_DW_Block256_Range)  with Object_Size => 256;
    type Lw_DW_Block384  is new Lw_DWords_Generic (Lw_DW_Block384_Range)  with Object_Size => 384;
    type Lw_DW_Block448  is new Lw_DWords_Generic (Lw_DW_Block448_Range)  with Object_Size => 448;
    type Lw_DW_Block512  is new Lw_DWords_Generic (Lw_DW_Block512_Range)  with Object_Size => 512;
    type Lw_DW_Block768  is new Lw_DWords_Generic (Lw_DW_Block768_Range)  with Object_Size => 768;
    type Lw_DW_Block3072 is new Lw_DWords_Generic (Lw_DW_Block3072_Range) with Object_Size => 3072;
    type Lw_DW_Block4096 is new Lw_DWords_Generic (Lw_DW_Block4096_Range) with Object_Size => 4096;
    
    subtype Lw_Bit_Pack8_Range  is LwU32 range 0 .. 7;
    subtype Lw_Bit_Pack32_Range is LwU32 range 0 .. 31;
    subtype Lw_Bit_Pack64_Range is LwU32 range 0 .. 63;
    
    -- The bits of Byte/Dword or QWord
    type Lw_Bit_Pack8  is new Lw_Bits_Generic (Lw_Bit_Pack8_Range)  with Pack, Object_Size => 8, Alignment => 1;
    type Lw_Bit_Pack32 is new Lw_Bits_Generic (Lw_Bit_Pack32_Range) with Pack, Object_Size => 32, Alignment => 4;
    type Lw_Bit_Pack64 is new Lw_Bits_Generic (Lw_Bit_Pack64_Range) with Pack, Object_Size => 64, Alignment => 8;

    subtype Lw_Byte_Pack16_Range is LwU32 range 0 .. 1;
    subtype Lw_Byte_Pack32_Range is LwU32 range 0 .. 3;
    subtype Lw_Byte_Pack64_Range is LwU32 range 0 .. 7;
    
    -- The bytes of Word/DWord/QWord
    -- Dword 0x12345678
    -- Index   3 2 1 0    
    type Lw_Byte_Pack16 is new Lw_Bytes_Generic (Lw_Byte_Pack16_Range) with Pack, Object_Size => 16, Alignment => 2;
    type Lw_Byte_Pack32 is new Lw_Bytes_Generic (Lw_Byte_Pack32_Range) with Pack, Object_Size => 32, Alignment => 4;
    type Lw_Byte_Pack64 is new Lw_Bytes_Generic (Lw_Byte_Pack64_Range) with Pack, Object_Size => 64, Alignment => 8;
    
    subtype Lw_DWord_Pack64_Range is LwU32 range 0 .. 1;
    -- The DWords of QWord
    type Lw_DWord_Pack64 is new Lw_DWords_Generic (Lw_DWord_Pack64_Range) with Pack, Object_Size => 64, Alignment => 8;
    
    
    -- Define colwertors
    pragma Assert (Lw_Bit_Pack8'Size = LwU8'Size);
    pragma Assert (Lw_Bit_Pack8'Alignment = LwU8'Alignment);
    function To_LwU8      is new Ada.Unchecked_Colwersion (Source => Lw_Bit_Pack8,
                                                           Target => LwU8);
    function To_Bit_Pack8 is new Ada.Unchecked_Colwersion (Source => LwU8,
                                                           Target => Lw_Bit_Pack8);
    
    pragma Assert (Lw_Bit_Pack32'Size = LwU32'Size);
    pragma Assert (Lw_Bit_Pack32'Alignment = LwU32'Alignment);
    function To_LwU32      is new Ada.Unchecked_Colwersion (Source => Lw_Bit_Pack32,
                                                            Target => LwU32);
    function To_Bit_Pack32 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                            Target => Lw_Bit_Pack32);

    
    pragma Assert (Lw_Bit_Pack64'Size = LwU64'Size);
    pragma Assert (Lw_Bit_Pack64'Alignment = LwU64'Alignment);
    function To_LwU64      is new Ada.Unchecked_Colwersion (Source => Lw_Bit_Pack64,
                                                            Target => LwU64);
    function To_Bit_Pack64 is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                            Target => Lw_Bit_Pack64);
    
    pragma Assert (Lw_Byte_Pack16'Size = LwU16'Size);
    pragma Assert (Lw_Byte_Pack16'Alignment = LwU16'Alignment);
    function To_LwU16      is new Ada.Unchecked_Colwersion (Source => Lw_Byte_Pack16,
                                                            Target => LwU16);
    function To_Byte_Pack16 is new Ada.Unchecked_Colwersion (Source => LwU16,
                                                             Target => Lw_Byte_Pack16);
    
    
    pragma Assert (Lw_Byte_Pack32'Size = LwU32'Size);
    pragma Assert (Lw_Byte_Pack32'Alignment = LwU32'Alignment);
    function To_LwU32       is new Ada.Unchecked_Colwersion (Source => Lw_Byte_Pack32,
                                                             Target => LwU32);
    function To_Byte_Pack32 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                             Target => Lw_Byte_Pack32);
    
    pragma Assert (Lw_Byte_Pack64'Size = LwU64'Size);
    pragma Assert (Lw_Byte_Pack64'Alignment = LwU64'Alignment);
    function To_LwU64       is new Ada.Unchecked_Colwersion (Source => Lw_Byte_Pack64,
                                                             Target => LwU64);
    function To_Byte_Pack64 is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                             Target => Lw_Byte_Pack64);
    
    pragma Assert (Lw_DWord_Pack64'Size = LwU64'Size);
    function To_LwU64        is new Ada.Unchecked_Colwersion (Source => Lw_DWord_Pack64,
                                                              Target => LwU64);
    function To_DWord_Pack64 is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                              Target => Lw_DWord_Pack64);
    
    
end Lw_Types.Lw_Array;
