-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining types
--
-- @description
--

package Rv_Brom_Types.Manifest is

    -- Define some common length array
    ARRAY128_LENGTH_IN_BIT      : constant := 128;
    ARRAY256_LENGTH_IN_BIT      : constant := 256;
    ARRAY3072_LENGTH_IN_BIT     : constant := 3072;
    ARRAY4096_LENGTH_IN_BIT     : constant := 4096;

    -- Define a 128 bit length array whose element size is 32 bit length
    DW_ARRAY128_GENERIC_LENGTH_IN_DWORD : constant := ARRAY128_LENGTH_IN_BIT / 32;  -- 4
    DW_ARRAY128_GENERIC_LAST_INDEX      : constant := DW_ARRAY128_GENERIC_LENGTH_IN_DWORD - 1; -- 3

    -- Define a 256 bit length array whose element size is 32 bit length
    DW_ARRAY256_GENERIC_LENGTH_IN_DWORD : constant := ARRAY256_LENGTH_IN_BIT / 32;  -- 8
    DW_ARRAY256_GENERIC_LAST_INDEX      : constant := DW_ARRAY256_GENERIC_LENGTH_IN_DWORD - 1; -- 7

    -- Define a 128 bit length array whose element size is 8 bit length
    B_ARRAY128_GENERIC_LENGTH_IN_BYTE  : constant := ARRAY128_LENGTH_IN_BIT / 8;  -- 16
    B_ARRAY128_GENERIC_LAST_INDEX      : constant := B_ARRAY128_GENERIC_LENGTH_IN_BYTE - 1; -- 15

    -- Define a 256 bit length array whose element size is 8 bit length
    B_ARRAY256_GENERIC_LENGTH_IN_DWORD : constant := ARRAY256_LENGTH_IN_BIT / 8;  -- 32
    B_ARRAY256_GENERIC_LAST_INDEX      : constant := B_ARRAY256_GENERIC_LENGTH_IN_DWORD - 1; -- 31

    -- Define a 4096 bit length array whose element size is 32 bit length
    DW_ARRAY4096_GENERIC_LENGTH_IN_DWORD : constant := ARRAY4096_LENGTH_IN_BIT / 32; -- 128
    DW_ARRAY4096_GENERIC_LAST_INDEX      : constant := DW_ARRAY4096_GENERIC_LENGTH_IN_DWORD - 1; -- 127

    -- Key128
    type Dw_Key128 is new Lw_DW_Block128 with
        Size => ARRAY128_LENGTH_IN_BIT, Object_Size => ARRAY128_LENGTH_IN_BIT;
    type B_Key128 is new Lw_B_Block128 with
        Size => ARRAY128_LENGTH_IN_BIT, Object_Size => ARRAY128_LENGTH_IN_BIT;

    -- Key256
    type Dw_Key256 is new Lw_DW_Block256 with
        Size => ARRAY256_LENGTH_IN_BIT, Object_Size => ARRAY256_LENGTH_IN_BIT;
    type B_Key256 is new Lw_B_Block256 with
        Size => ARRAY256_LENGTH_IN_BIT, Object_Size => ARRAY256_LENGTH_IN_BIT;

    function Lw_DW_Block256_To_Array is new Ada.Unchecked_Colwersion (Source => Lw_DW_Block256,
                                                                    Target => Lw_DW_Block256);

    function Dw_Key256_To_Array is new Ada.Unchecked_Colwersion (Source => Dw_Key256,
                                                                 Target => Lw_DW_Block256);

    function Dw_Key128_To_Array is new Ada.Unchecked_Colwersion (Source => Dw_Key128,
                                                                 Target => Lw_DW_Block128);

    type Root_Keys_Index is (SECRET0, SECRET1, SECRET2, PDUK, PMUK, KEK);
    for Root_Keys_Index use (
                             SECRET0 => 0,
                             SECRET1 => 1,
                             SECRET2 => 2,
                             PDUK    => 3,
                             PMUK    => 4,
                             KEK     => 5
                            );
    type Root_Keys is array (Root_Keys_Index) of aliased Dw_Key128;



    type Dw_Aes128_Cbc_Iv is new Lw_DW_Block128 with Size => 128, Object_Size => 128;


    -- Max number of device map entries
    MANIFEST_DEVICE_MAP_MAX_NUM       : constant := 32;
    MANIFEST_DEVICE_MAP_RANGE_LAST    : constant := MANIFEST_DEVICE_MAP_MAX_NUM / 8 - 1;
    -- Max number of Core-pmp entries
    MANIFEST_CORE_PMP_MAX_NUM         : constant := 32;
    MANIFEST_CORE_PMP_CFG_RANGE_LAST  : constant := MANIFEST_DEVICE_MAP_MAX_NUM / 8 - 1;
    MANIFEST_CORE_PMP_ADDR_RANGE_LAST : constant := MANIFEST_DEVICE_MAP_MAX_NUM - 1;

    -- Max number of Io-Pmp entries
    MANIFEST_IO_PMP_MAX_NUM           : constant := 32;
    MANIFEST_IO_PMP_RANGE_LAST        : constant := MANIFEST_IO_PMP_MAX_NUM - 1;

    -- Max number of Register Data-Pair entries
    MANIFEST_REGISTER_PAIR_MAX_NUM    : constant := 32;
    MANIFEST_REGISTER_PAIR_RANGE_LAST : constant := MANIFEST_REGISTER_PAIR_MAX_NUM - 1;

    -- Max number of stage2 OEM public key hash
    PKC_OEM_PKEY_HASH_MAX_NUM         : constant := 5;
    PKC_OEM_PKEY_HASH_RANGE_LAST      : constant := PKC_OEM_PKEY_HASH_MAX_NUM - 1;


    subtype Pkc_Oem_Pkey_Hash_Array_Range is LwU32 range 0 .. PKC_OEM_PKEY_HASH_RANGE_LAST;
    type Pkc_Oem_PKey_Hash_Array is array (Pkc_Oem_Pkey_Hash_Array_Range) of Lw_DW_Block384;


    subtype Mf_Device_Map_Range    is LwU32 range 0 .. MANIFEST_DEVICE_MAP_RANGE_LAST;
    subtype Mf_Core_Pmp_Cfg_Range  is LwU32 range 0 .. MANIFEST_CORE_PMP_CFG_RANGE_LAST;
    subtype Mf_Core_Pmp_Addr_Range is LwU32 range 0 .. MANIFEST_CORE_PMP_ADDR_RANGE_LAST;


    -- Configuratons for Io-Pmp registers RISCV_IOPMP_INDEX, RISCV_IOPMP_CFG, RISCV_IOPMP_ADDR_LO, RISCV_IOPMP_ADDR_HI
    -- @field Cfg Configuration for Io-Pmp
    -- @field Addr_Lo Low Address for Io-Pmp
    -- @field Addr_Hi High Address for Io-Pmp
    type Mf_Io_Pmp_Entry is limited record
        Cfg      : LwU32;
        User_Cfg : LwU32;
        Addr_Lo  : LwU32;
        Addr_Hi  : LwU32;
    end record with Size => 128, Object_Size => 128;


    -- First Nth register pair entries get configured by BR
    subtype Mf_Reg_Pair_Num is LwU8 range 0 .. MANIFEST_REGISTER_PAIR_MAX_NUM;

    subtype Mf_Io_Pmp_Array_Range is LwU8 range 0 .. MANIFEST_IO_PMP_RANGE_LAST;
    -- Array of Io-Pmp entries
    type Mf_Io_Pmp_Array is array (Mf_Io_Pmp_Array_Range) of Mf_Io_Pmp_Entry;

    -- Configurations for Register Address-Data pair. BR will perform read-modify-write on these registers (wdata = (rdata & andMask) | orMask).
    -- @field Address Address of Address-Data pair.
    -- @field And_Mask And mask
    -- @field Or_Mask Or mask
    type Mf_Register_Pair_Entry is limited record
        Address  : LwU64;
        And_Mask : LwU32;
        Or_Mask  : LwU32;
    end record with Size => 128, Object_Size => 128;

    subtype Mf_Register_Pair_Array_Range is LwU8 range 0 .. MANIFEST_REGISTER_PAIR_RANGE_LAST;
    type Mf_Register_Pair_Array is array (Mf_Register_Pair_Array_Range) of Mf_Register_Pair_Entry;


    -- Manifest Version, whose value should be updated when manifest layout changed, but we don't care about it actually.
    subtype Mf_Version is LwU8;

    -- Ucode ID 0 is not valid in RISCV BROM. Valid IDs range is [1 - 16], and the corresponding version registers are FALCON_UCODE_VERSION(0...15).
    subtype Mf_Ucode_Id is LwU8 range 1 .. 16;

    -- Ucode Version, full range of LwU8
    subtype Mf_Ucode_Version is LwU8;

    -- Engine Id Mask, full range of LwU32
    subtype Mf_Engine_Id_Mask is LwU32;

    -- FMC code size, it's a multiple of 256 bytes
    subtype Mf_Imem_Size_Block is LwU16 range 1 .. LwU16(Imem_Size_Block'Last);
    -- FMC data size, it's a multiple of 256 bytes
    subtype Mf_Dmem_Size_Block is LwU16 range 0 .. LwU16(Fmc_Data_Max_Size_Block'Last);

    -- Only the bit 0 is valid in GH100, if it's 1, the PDI info should be pad at the beginning of FMC code when callwlating the hash.
    subtype Mf_Pad_Info_Mask is LwU32 range 0 .. 1;

    -- MSPM(machine system priviledge mask) definition
    -- M mode PL mask. Only the lowest 4 bits can be non-zero
    -- bit 0 - indicates if PL0 is enabled (1) or disabled (0) in M mode. It is a read-only bit in HW so it has to be set here in the manifest
    -- bit 1 - indicates if PL1 is enabled (1) or disabled (0) in M mode.
    -- bit 2 - indicates if PL2 is enabled (1) or disabled (0) in M mode.
    -- bit 3 - indicates if PL3 is enabled (1) or disabled (0) in M mode.
    subtype Mf_Mspm_Mplm is LwU8 range 0 .. 2#1111#;
    -- M mode SCP transaction indicator. Only the lowest bit can be non-zero
    -- bit 0 - indicates if secure transaction is enabled (1) or disabled (0) in M mode.
    subtype Mf_Mspm_Msecm is LwU8 range 0 .. 1;



end Rv_Brom_Types.Manifest;
