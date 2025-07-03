with System; use System;
with Lw_Types; use Lw_Types;
with Lw_Types.Lw_Array; use Lw_Types.Lw_Array;

with Rv_Brom_Types; use Rv_Brom_Types;
with Rv_Brom_Types.Manifest; use Rv_Brom_Types.Manifest;
with Rv_Boot_Plugin_Error_Handling; use Rv_Boot_Plugin_Error_Handling;
with Rv_Brom_Memory_Layout; use Rv_Brom_Memory_Layout;
with Rv_Boot_Plugin_Memory_Layout; use Rv_Boot_Plugin_Memory_Layout;

with Rv_Brom_Riscv_Core;

package Rv_Boot_Plugin_Sw_Interface with
Abstract_State => Pkc_Parameters_State
is

   -- type Sw_Encryption_Derivation_String is new Arr_LwU8_Idx32

    PKC_VERIFICATION_MANIFEST_SIZE_IN_BYTE : constant := 1664;      -- The size is 1664 Bytes
    PKC_VERIFICATION_MANIFEST_SIZE_IN_BIT  : constant := PKC_VERIFICATION_MANIFEST_SIZE_IN_BYTE * 8;
    PKC_VERIFICATION_MANIFEST_HEADER_SIZE_IN_BYTE : constant := 128;      -- The size is 128 Bytes
    PKC_VERIFICATION_MANIFEST_HEADER_SIZE_IN_BIT  : constant := PKC_VERIFICATION_MANIFEST_HEADER_SIZE_IN_BYTE * 8;


    -- Rsa3k Signature definition
    -- The length of RSA3K signature is 3072 bits which equals to 96 dwords or 384 bytes.
    type Dw_Rsa3k_Signature is new Lw_DW_Block3072;
    RSA3K_SIGNATURE_LENGTH_IN_DWORD : constant := Dw_Rsa3k_Signature'Length;  -- 96 dwords
    RSA3K_SIGNATURE_LENGTH_IN_BYTE  : constant := RSA3K_SIGNATURE_LENGTH_IN_DWORD * 4;   -- 384 bytes
    RSA3K_SIGNATURE_LENGTH_IN_BIT   : constant := RSA3K_SIGNATURE_LENGTH_IN_BYTE * 8;    -- 3072 bits


    -- Rsa3K public key component definition
    type Dw_Rsa3k_Pkey_Comp is new Lw_DW_Block3072;
    RSA3K_PKEY_COMP_LENGTH_IN_DWORD : constant := Dw_Rsa3k_Pkey_Comp'Length;           -- 96 dwords
    RSA3K_PKEY_COMP_LENGTH_IN_BYTE  : constant := RSA3K_PKEY_COMP_LENGTH_IN_DWORD * 4; -- 384 bytes
    RSA3K_PKEY_COMP_LENGTH_IN_BIT   : constant := RSA3K_PKEY_COMP_LENGTH_IN_BYTE * 8;  -- 3072 bits
    RSA3K_PKEY_COMP_LENGTH_IN_BYTE_BLOCK_ALIGNED : constant := ((RSA3K_PKEY_COMP_LENGTH_IN_BYTE - 1)/256 + 1)*256; -- 512 bytes


    type Rsa3k_Pkey_Comp_Name is (N, E, NP, NS);
    for Rsa3k_Pkey_Comp_Name use (
                                  N  => 0,
                                  E  => 1,
                                  NP => 2,
                                  NS => 3
                                 );

    function Get_Ucode_Id return Mf_Ucode_Id;
    function Get_Ucode_Version return Mf_Ucode_Version;
    function Get_Is_Relaxed_Version_Check return HS_BOOL;
    function Get_Engine_Id_Mask return Mf_Engine_Id_Mask;
    function Get_Fmc_Code_Block_Size return Imem_Size_Block;
    function Get_Fmc_Data_Block_Size return Fmc_Data_Max_Size_Block;
    function Get_Fmc_Hash_Pad_Info_Mask return Mf_Pad_Info_Mask;
    function Get_Is_Attester return HS_BOOL;
    function Get_Is_Dice return HS_BOOL;
    function Get_Is_Kdf return HS_BOOL;
    function Get_Kdf_Constant (I : Lw_DW_Block256_Range) return LwU32;
    function Get_Reg_Pair_Num return Mf_Reg_Pair_Num;

    function Get_Manifest_Enc_Derivation_Str (I : LwU32) return LwU8 with
        Pre => I in Lw_B_Block64'Range,
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;

    function Get_Fmc_Enc_Derivation_Str (I : LwU32) return LwU8 with
        Pre => I in Lw_B_Block64'Range,
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;

    function Get_Sw_Enc_Derivation_Str (I : LwU32) return LwU8 with
        Pre => I in Lw_B_Block64'Range,
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;

    function Get_Manifest_Cbc_Iv (I : LwU32) return LwU32 with
        Pre => I in Dw_Aes128_Cbc_Iv'Range,
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;

    function Get_Fmc_Cbc_Iv (I : LwU32) return LwU32 with
        Pre => I in Dw_Aes128_Cbc_Iv'Range,
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;


    procedure Sanitize_Manifest (Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Global => (Input => Pkc_Parameters_State,
                   In_Out => Rv_Brom_Riscv_Core.Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Program_Manifest_Io_Pmp with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Program_Manifest_Debug_Control with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Program_Manifest_Register_Pair (Err_Code : out Error_Codes) with
        Global => (Input => Pkc_Parameters_State),
        Inline_Always;

    procedure Program_Manifest_Mspm with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    procedure Program_Manifest_Device_Map with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;

    procedure Program_Manifest_Ucode_Id with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    procedure Program_Manifest_Core_Pmp with
        Global => (Input => Pkc_Parameters_State,
                   Output => Rv_Brom_Riscv_Core.Rv_Csr.Csr_Reg_Wr64_Hw_Model.Csr_State),
        Inline_Always;

    function Get_Boot_Plugin_Code_Imem_Offset return Imem_Size_Byte is
        (Imem_Size_Byte (Rv_Boot_Plugin_Memory_Layout.BOOT_PLUGIN_CODE_START)) with
         Post => Get_Boot_Plugin_Code_Imem_Offset'Result mod 256 = 0,
         Inline_Always;

    function Get_Brom_Stack_Size return Dmem_Size_Byte is
        (Dmem_Size_Byte (Rv_Brom_Memory_Layout.BROMDATA_SIZE)) with
         Post => Get_Brom_Stack_Size'Result mod 256 = 0,
         Inline_Always;

    function Get_Brom_Stack_Start return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Rv_Brom_Memory_Layout.BROMDATA_START))
        with Post => Get_Brom_Stack_Start'Result mod 256 = 0,
        Inline_Always;

    function Get_Pkc_Boot_Parameters_Dmem_Offset return Dmem_Offset_Byte is
        (Dmem_Offset_Byte(Rv_Brom_Memory_Layout.PKCPARAM_START))
        with Post => (Get_Pkc_Boot_Parameters_Dmem_Offset'Result mod 256) = 0,
        Inline_Always;

    function Get_Pkc_Boot_Parameters_Block_Size return Dmem_Size_Block is
        (Dmem_Size_Byte_To_Block(Rv_Brom_Memory_Layout.PKCPARAM_SIZE))
        with Inline_Always;

    function Get_Pkc_Boot_Parameters_Byte_Size return LwU32 is
        (LwU32(Rv_Brom_Memory_Layout.PKCPARAM_SIZE))
        with Inline_Always;

    function Get_Stage2_Pkc_Boot_Parameters_Byte_Size return LwU32 is
        (LwU32(Rv_Brom_Memory_Layout.STAGE2_PKCPARAM_SIZE))
        with Inline_Always;

    function Get_Sw_Pubkey_Byte_Size return LwU32 is
        (LwU32(Rv_Brom_Memory_Layout.SWPUBKEY_SIZE))
        with Inline_Always;

    function Get_Signature_Address return System.Address
        with Inline_Always;

    function Get_Manifest_Dmem_Offset return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Rv_Brom_Memory_Layout.PKCPARAM_START + Dw_Rsa3k_Signature'Length * 4 * 2 + PKC_VERIFICATION_MANIFEST_HEADER_SIZE_IN_BYTE))
        with Post => (Get_Manifest_Dmem_Offset'Result > Get_Pkc_Boot_Parameters_Dmem_Offset) and then
                     (Get_Manifest_Dmem_Offset'Result mod 128 = 0),
        Inline_Always;

    function Get_Manifest_Byte_Size return Dmem_Size_Byte is
        (Dmem_Size_Byte (PKC_VERIFICATION_MANIFEST_SIZE_IN_BYTE))
        with Post => Get_Manifest_Byte_Size'Result mod 128 = 0,
        Inline_Always;

    function Get_Manifest_Header_Dmem_Offset return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Rv_Brom_Memory_Layout.PKCPARAM_START + Dw_Rsa3k_Signature'Length * 4 * 2))
        with Post => (Get_Manifest_Header_Dmem_Offset'Result > Get_Pkc_Boot_Parameters_Dmem_Offset) and then
                     (Get_Manifest_Header_Dmem_Offset'Result mod 256 = 0),
        Inline_Always;

    function Get_Manifest_Header_Byte_Size return Dmem_Size_Byte is
        (Dmem_Size_Byte (PKC_VERIFICATION_MANIFEST_HEADER_SIZE_IN_BYTE))
        with Post => Get_Manifest_Header_Byte_Size'Result mod 128 = 0,
        Inline_Always;

    function Get_Rsa3k_PKey_Modulus_Dmem_Offset return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Rv_Brom_Memory_Layout.SWPUBKEY_START))
        with Post => (Get_Rsa3k_PKey_Modulus_Dmem_Offset'Result mod 256) = 0,
        Inline_Always;

    function Get_Rsa3k_PKey_Modulus_Block_Size return Dmem_Size_Block is
        (Dmem_Size_Byte_To_Block (RSA3K_PKEY_COMP_LENGTH_IN_BYTE_BLOCK_ALIGNED)) with
        Inline_Always;  -- 512 bytes

    function Get_Fmc_Code_Imem_Offset return Imem_Offset_Byte is
        (Imem_Offset_Byte (Rv_Brom_Memory_Layout.FMCCODE_START)) with
        Post => (Get_Fmc_Code_Imem_Offset'Result mod 256) = 0,
        Inline_Always;

    -- Get the end of the IMEM region which we can overwrite with FMC code
    function Get_End_Of_Imem_Overwritable_Region return Imem_Offset_Byte with
        Inline_Always;

    function Get_Fmc_Data_Dmem_Offset return Dmem_Offset_Byte is
        (Dmem_Offset_Byte (Rv_Brom_Memory_Layout.FMCDATA_START)) with
        Post => (Get_Fmc_Data_Dmem_Offset'Result mod 256) = 0,
        Inline_Always;

    function Get_HW_EngineId_Family_Id return LwU8 with
        Inline_Always;


    procedure Get_HW_Ucode_Version (Ucode_Ver : out LwU8) with
        Global => (Input => Pkc_Parameters_State,
                   In_Out => Rv_Brom_Riscv_Core.Csb_Reg.Csb_Reg_Wr32_Hw_Model.Mmio_State),
        Inline_Always;


end Rv_Boot_Plugin_Sw_Interface;

