-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining Bootrom types
--
-- @description
-- Defines all Bootrom types dealing with the Imem/Dmem size and offset.

with Ada.Unchecked_Colwersion;
with System;

with Lw_Types; use Lw_Types;
with Lw_Types.Lw_Array; use Lw_Types.Lw_Array;
with Rv_Brom_Memory_Layout; use Rv_Brom_Memory_Layout;
package Rv_Brom_Types is

    -- Change the HS_BOOL to 8 bits length and renaming to HS_TRUE and HS_FALSE
    type HS_BOOL is (HS_FALSE, HS_TRUE) with Size => 8, Object_Size => 8;
    for HS_BOOL use
      (HS_FALSE => 16#55#, HS_TRUE => 16#AA#);


    -- Define the type of Imem/Dmem_Size_Bit
    type Imem_Size_Bit is new LwU32 range 0 .. RISCV_IMEM_BITSIZE;
    type Dmem_Size_Bit is new LwU32 range 0 .. RISCV_DMEM_BITSIZE;

    -- Define the type of Imem/Dmem_Offset_Bit
    type Imem_Offset_Bit is new LwU32 range 0 .. RISCV_IMEM_BITSIZE_LIMIT;
    type Dmem_Offset_Bit is new LwU32 range 0 .. RISCV_DMEM_BITSIZE_LIMIT;

    -- Define the type of Imem/Dmem_Size_Byte
    type Imem_Size_Byte is new LwU32 range 0 .. RISCV_IMEM_BYTESIZE;
    type Dmem_Size_Byte is new LwU32 range 0 .. RISCV_DMEM_BYTESIZE;

    -- Define the type of Imem/Dmem_Offset_Byte
    type Imem_Offset_Byte is new LwU32 range 0 .. RISCV_IMEM_BYTESIZE_LIMIT;
    type Dmem_Offset_Byte is new LwU32 range 0 .. RISCV_DMEM_BYTESIZE_LIMIT;


    -- Define the type of Imem/Dmem_Size_Dword Dword is 32 bits
    type Imem_Size_Dword is new LwU32 range 0 .. RISCV_IMEM_DWORDSIZE;
    type Dmem_Size_Dword is new LwU32 range 0 .. RISCV_DMEM_DWORDSIZE;

    -- Define the type of Imem/Dmem_Offset_Dword Dword is 32 bits
    type Imem_Offset_Dword is new LwU32 range 0 .. RISCV_IMEM_DWORDSIZE_LIMIT;
    type Dmem_Offset_Dword is new LwU32 range 0 .. RISCV_DMEM_DWORDSIZE_LIMIT;


    -- Define the type of Imem/Dmem_Size_Block, each block is 256-byte length
    type Imem_Size_Block is new LwU32 range 0 .. RISCV_IMEM_BLOCKSIZE;
    type Dmem_Size_Block is new LwU32 range 0 .. RISCV_DMEM_BLOCKSIZE;

    -- Define the type of Imem/Dmem_Offset_Block, each block is 256-byte length
    type Imem_Offset_Block is new LwU32 range 0 .. RISCV_IMEM_BLOCKSIZE_LIMIT;
    type Dmem_Offset_Block is new LwU32 range 0 .. RISCV_DMEM_BLOCKSIZE_LIMIT;

    -- Define the Brom stack region
    type Stack_Range_Byte is new Dmem_Size_Byte range BROMDATA_START .. BROMDATA_END;

    -- If RTL key is used, then the FMC data can use the space up to stack start
    subtype Fmc_Data_Max_Size_Byte is Dmem_Size_Byte range FMCDATA_START .. FMCDATA_MAX_END;
    subtype Fmc_Data_Max_Offset_Byte is Dmem_Offset_Byte range FMCDATA_START .. FMCDATA_MAX_LIMIT;
    subtype Fmc_Data_Max_Size_Block is Dmem_Size_Block range FMCDATA_START .. FMCDATA_MAX_BLOCKEND;

    -- If S/W key is used, the FMC data shall not use the space oclwpied by S/W public key
    subtype Fmc_Data_Min_Size_Block is Dmem_Size_Block range FMCDATA_START .. FMCDATA_BLOCKEND;


    type Imem_Pa_Offset_Byte is new LwU64 range RISCV_PA_IMEM_START .. RISCV_PA_IMEM_BYTESIZE_LIMIT;
    type Dmem_Pa_Offset_Byte is new LwU64 range RISCV_PA_DMEM_START .. RISCV_PA_DMEM_BYTESIZE_LIMIT;

    type Tcm_Size_Byte is new LwU32 range 0 .. RISCV_TCM_MAX_BYTESIZE;
    type Tcm_Offset_Byte is new LwU32 range 0 .. RISCV_TCM_MAX_BYTESIZE_LIMIT;

    function LwU8_To_HS_Bool is new Ada.Unchecked_Colwersion (Source => LwU8,
                                                              Target => HS_BOOL) with Inline_Always;

    -- Define the RISCV PA address space
    type Rv_Sys_Offset_Byte is new LwU64;

    subtype Imem_Sys_Offset is Rv_Sys_Offset_Byte range RISCV_PA_IMEM_START .. RISCV_PA_IMEM_BYTESIZE_LIMIT;
    subtype Dmem_Sys_Offset is Rv_Sys_Offset_Byte range RISCV_PA_DMEM_START .. RISCV_PA_DMEM_BYTESIZE_LIMIT;





    -- Define Imem/Dmem_Offset to Imem/Dmem_PA type colwertor
    function Imem_Offset_To_Sys (Offset : Imem_Offset_Byte) return Imem_Sys_Offset is
        (Imem_Sys_Offset (LwU32 (Offset) + LwU32 (Imem_Sys_Offset'First))) with
        Pre => LwU64(Offset) <= LwU64 (Imem_Sys_Offset'Last) - LwU64 (Imem_Sys_Offset'First),
        Inline_Always;

    function Dmem_Offset_To_Sys (Offset : Dmem_Offset_Byte) return Dmem_Sys_Offset is
        (Dmem_Sys_Offset (LwU32 (Offset) + LwU32 (Dmem_Sys_Offset'First))) with
        Pre => LwU64 (Offset) <= LwU64 (Dmem_Sys_Offset'Last) - LwU64 (Dmem_Sys_Offset'First),
        Inline_Always;

    function Imem_Offset_To_LwU32 (Offset : Imem_Offset_Byte) return LwU32 is
       (LwU32 (Offset)) with Inline_Always;
    function Dmem_Offset_To_LwU32 (Offset : Dmem_Offset_Byte) return LwU32 is
       (LwU32 (Offset)) with Inline_Always;

    function Dmem_Size_Byte_To_Block (Bytes : Dmem_Size_Byte) return Dmem_Size_Block is
        (Dmem_Size_Block (Bytes / 256))
        with
           Pre => LwU32 (Bytes / 256) <= LwU32 (Dmem_Size_Block'Last) and then
                  (Bytes mod 256) = 0,
        Inline_Always;


    function Dmem_Offset_Byte_To_Block (Bytes : Dmem_Offset_Byte) return Dmem_Offset_Block is
        (Dmem_Offset_Block (Bytes / 256)) with
            Pre => LwU32 (Bytes / 256) <= LwU32 (Dmem_Offset_Block'Last) and then
            (Bytes mod 256) = 0,
            Inline_Always;

    function Dmem_Size_Block_To_Byte (Block : Dmem_Size_Block) return Dmem_Size_Byte is
        (Dmem_Size_Byte (Block * 256)) with
            Post => Dmem_Size_Block_To_Byte'Result mod 256 = 0,
            Inline_Always;

    function Dmem_Size_Bit_To_Block (Bits : Dmem_Size_Bit) return Dmem_Size_Block is
       (Dmem_Size_Block (Bits / 256 / 8)) with
            Pre => LwU32 (Bits / 256 / 8) <= LwU32 (Dmem_Size_Block'Last) and then
            (Bits mod (256 * 8)) = 0,
            Inline_Always;

    function Dmem_Size_Bit_To_Byte (Bits : Dmem_Size_Bit) return Dmem_Size_Byte is
        (Dmem_Size_Byte (Bits / 8)) with
            Pre => LwU32 (Bits / 8) <= LwU32 (Dmem_Size_Byte'Last) and then
            (Bits mod (8)) = 0,
            Inline_Always;

      -- Colwert the LwU64 to System.Address
    function LwU64_To_Address is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                               Target => System.Address) with Inline_Always;

    -- The Address_To_* colwertors are not recommeded to use
    -- Define the System.Address to LwU64 colwertor
    function Address_To_LwU64 is new Ada.Unchecked_Colwersion (Source => System.Address,
                                                               Target => LwU64) with Inline_Always;

    -- Define the System.Address to Imem/Dmem_Offset type colwertor
    function Address_To_Imem_Offset (Source : System.Address) return Imem_Offset_Byte is
        (Imem_Offset_Byte (Address_To_LwU64 (Source) - LwU64 (RISCV_PA_IMEM_START))) with
        Pre => Address_To_LwU64 (Source) >= LwU64(RISCV_PA_IMEM_START) and then
        (Address_To_LwU64 (Source) - LwU64(RISCV_PA_IMEM_START)) <= LwU64 (Imem_Offset_Byte'Last),
        Inline_Always;
    function Address_To_Dmem_Offset (Source : System.Address) return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Address_To_LwU64 (Source) - LwU64 (RISCV_PA_DMEM_START))) with
        Pre => Address_To_LwU64 (Source) >= LwU64(RISCV_PA_DMEM_START) and then
        (Address_To_LwU64 (Source) - LwU64(RISCV_PA_DMEM_START)) <= LwU64 (Dmem_Offset_Byte'Last),
        Inline_Always;

    -- We'd better stick the use of *_Offset_To_Address colwersion functions.
    function Imem_Offset_To_Address (Offset : Imem_Offset_Byte) return System.Address is
        (LwU64_To_Address (LwU64 (Offset) + LwU64 (RISCV_PA_IMEM_START))) with
        Pre => LwU64 (Offset) <= LwU64'Last - RISCV_PA_IMEM_START,
        Inline_Always;
    function Dmem_Offset_To_Address (Offset : Dmem_Offset_Byte) return System.Address is
        (LwU64_To_Address (LwU64 (Offset) + LwU64 (RISCV_PA_DMEM_START))) with
        Pre => LwU64 (Offset) <= LwU64'Last - RISCV_PA_DMEM_START,
        Inline_Always;


    -- Define primitive types
    subtype Bit_Byte_Index is LwU8 range 0 .. 7;
    subtype Byte_Dword_Index is LwU8 range 0 .. 3;
    subtype Bit_Dword_Index is LwU8 range 0 .. 31;
    -- Bits respresentation for one Byte
    type Bit_Byte is new Arr_LwU1_Idx8 (Bit_Byte_Index)  with Pack, Size => 8, Object_Size => 8;
    -- Byte respresentation for one dword
    type Byte_Dword is new Arr_LwU8_Idx8 (Byte_Dword_Index) with Pack, Size => 32, Object_Size => 32;
    -- Bits respresentation for one dword
    type Bit_Dword is new Arr_LwU1_Idx8 (Bit_Dword_Index) with Pack, Size => 32, Object_Size => 32;

    -- Colwert the Dword to Byte Array
    function To_Bytes is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                       Target => Byte_Dword) with Inline_Always;
    -- Colwert the Bytes array to Dword
    function To_Dword is new Ada.Unchecked_Colwersion (Source => Byte_Dword,
                                                       Target => LwU32) with Inline_Always;
    -- Colwert the Byte to Bit Array
    function To_Bits is new Ada.Unchecked_Colwersion (Source => LwU8,
                                                      Target => Bit_Byte) with Inline_Always;
    -- Colwert the Dword to Bit Array
    function To_Bits is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                      Target => Bit_Dword) with Inline_Always;

    -- Return the ByteX of Dword
    -- Dword 0x12345678
    -- Index   3 2 1 0
    function Byte0 (Dword : LwU32) return LwU8 is
        (To_Bytes (Dword) (0)) with
            Inline_Always;
    function Byte1 (Dword : LwU32) return LwU8 is
        (To_Bytes (Dword) (1)) with
            Inline_Always;
    function Byte2 (Dword : LwU32) return LwU8 is
        (To_Bytes (Dword) (2)) with
            Inline_Always;
    function Byte3 (Dword : LwU32) return LwU8 is
        (To_Bytes (Dword) (3)) with
            Inline_Always;


    -- Return the BitX of Byte
    -- Byte  0b00110101
    -- Index   76543210
    function Bits0 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (0)) with
            Inline_Always;
    function Bits1 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (1)) with
            Inline_Always;
    function Bits2 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (2)) with
            Inline_Always;
    function Bits3 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (3)) with
            Inline_Always;
    function Bits4 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (4)) with
            Inline_Always;
    function Bits5 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (5)) with
            Inline_Always;
    function Bits6 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (6)) with
            Inline_Always;
    function Bits7 (Byte : LwU8) return LwU1 is
        (To_Bits (Byte) (7)) with
            Inline_Always;

    type Csr_Bits64 is array (LwU8 range 0..63) of LwU1 with
        Size => 64, Object_Size => 64, Pack;

    function To_LwU64 is new Ada.Unchecked_Colwersion (Source => Csr_Bits64,
                                                       Target => LwU64) with Inline_Always;





end Rv_Brom_Types;
