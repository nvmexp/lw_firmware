with Ada.Unchecked_Colwersion;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;
with Lw_Ref_Dev_Prgnlcl; use Lw_Ref_Dev_Prgnlcl;
with Lw_Ref_Dev_Riscv_Csr_64; use Lw_Ref_Dev_Riscv_Csr_64;
with System.Storage_Elements; use System.Storage_Elements;

package body Rv_Boot_Plugin_Sw_Interface with
Refined_State => (Pkc_Parameters_State => Parameters)
is

    -- Configuration for Encryption
    -- @field Enc_Derivation_Str
    -- @field Iv
    -- @field Auth_Tag
    type Mf_Encryption_Ovl is limited record
        Enc_Derivation_Str : Lw_B_Block64;
        Iv                 : Lw_DW_Block128;
        Auth_Tag           : Lw_B_Block128;
    end record with Size => 320, Object_Size => 320;

    -- Configuration for RISCV_SCP_SECRET_MASK(0..1) and RISCV_SCP_SECRET_MASK_LOCK(0..1)
    -- @field Mask Controls whether SCP secret is usable or not, 1 means enable, 0 for disable.
    -- @field Lock Controls the writability of RISCV_SCP_SECRET_MASK, 1 means lock, 0 for unlock.
    type Mf_Secret_Ovl is limited record
        Mask0 : LwU32;
        Mask1 : LwU32;
        Lock0 : LwU32;
        Lock1 : LwU32;
    end record with Size => 128, Object_Size => 128;

    -- Configuration for RISCV_DBGCTL and RISCV_DBGCTL_LOCK.
    -- @field Ctrl To allow ICD command or not, 1 will enable the corresponding ICD command, 0 for disable
    -- @field Ctrl_Lock Controls the writability of RISCV_DBGCTL, 1 means lock, 0 for unlock
    type Mf_Debug_Control_Ovl is limited record
        Ctrl  : LwU32;
        Lock  : LwU32;
    end record with Size => 64, Object_Size => 64;

    -- Configuration for RISCV_CSR_MSPM.MSECM/MPLM
    -- @field Mplm Controls the M mode PL Mask, only the lowest 4 bits can be non-zero.
    -- bit 0 - indicates if PL0 is enabled (1) or disabled (0) in M mode. It is a read-only bit in HW so it has to be set here in the manifest
    -- bit 1 - indicates if PL1 is enabled (1) or disabled (0) in M mode.
    -- bit 2 - indicates if PL2 is enabled (1) or disabled (0) in M mode.
    -- bit 3 - indicates if PL3 is enabled (1) or disabled (0) in M mode.
    -- @field Msecm Controls the M mode SECure status Mask, only the lowest bit can be non-zero.
    -- bit 0 - indicates if secure transaction is enabled (1) or disabled (0) in M mode.
    type Mf_Mspm_Ovl is limited record
        Mplm  : LwU8;
        Msecm : LwU8;
    end record with Size => 16, Object_Size => 16;


    -- Configuration for RISCV_DEVICEMAP_RISCVMMODE(0 .. 3)
    -- @field Device_Map Controls the M mode IO accessability from/to Peregrine.
    type Mf_Device_Map_Ovl is limited record
        Cfgs : Lw_DWords_Generic (Mf_Device_Map_Range);
    end record with Size => 128, Object_Size => 128;


    -- Configurations for Core-Pmp CSRs PMPCFG0/2, MEXTPMPCFG0/2, PMPADDR(0...15) and MEXTPMPADDR(0...15)
    -- @field Cfg Configuration of PMP entries
    -- @field Addr Address of PMP entries
    type Mf_Core_Pmp_Ovl is limited record
        Cfgs  : Lw_QWords_Generic (Mf_Core_Pmp_Cfg_Range);
        Addrs : Lw_QWords_Generic (Mf_Core_Pmp_Addr_Range);
    end record with Size => 2304, Object_Size => 2304;

    -- Record of Io-Pmp array
    type Mf_Io_Pmp_Ovl is limited record
        Pmps : Mf_Io_Pmp_Array;
    end record with Size => 4096, Object_Size => 4096;


    -- Record of Register Data-Pair array
    type Mf_Register_Pair_Ovl is limited record
        Pairs : Mf_Register_Pair_Array;
    end record with Size => 4096, Object_Size => 4096;

    -- Record for Manifest Overlay
    -- Define this type as limited type can bring us the following benefites
    -- * No assignment by default
    -- * Passed By-Reference
    -- @field Version Version of the manifest; it is placeholder and needs to be 0x2 in GH100/LS10/TH500
    -- @field Ucode_Id Ucode ID
    -- @field Ucode_Version Ucode version
    -- @field Is_Relaxed_Version_Check If set, allows a greater-or-equal-to check on ucodeVersion
    -- @field Engine_Id_Mask Engine ID mask.
    -- @field Code_Size_In_256_Bytes The sizes need to be a multiple of 256 bytes, the blobs need to be 0-padded.
    -- @field Data_Size_In_256_Bytes The sizes need to be a multiple of 256 bytes, the blobs need to be 0-padded.
    -- @field Fmc_Hash_Pad_Info_Mask In GA10x the LSb indicates whether the 64-bit PDI is prepended to FMC code/data when callwlating the digest; All other bits must be 0s
    -- @field Sw_Enc_Derivation_Str SW decryption key KDK's derivation string
    -- @field Fmc_Enc_Params Crypto inputs required to decrypt FMC
    -- @field Digest Hash of the FMC's code and data
    -- @field Reserved0 All 0s;
    -- @field Secret_Mask SCP HW secret mask
    -- @field Debug_Control ICD control
    -- @field Io_Pmp_Mode0 IO-PMP mode0
    -- @field Io_Pmp_Mode1 IO-PMP mode1
    -- @field Is_Dice Generate CDI or not
    -- @field Is_Kdf Execute Key derivation or not
    -- @field Mspm Max PRIV level and SCP secure indicator
    -- @field Reg_Pair_Num First N register-data pair entries get configured by BR
    -- @field Is_Attester Execute BR DECFUSEKEY and AK subroutines or not
    -- @field Reserved1 All 0s;
    -- @field Kdf_Constant 256-bit constant used in KDF
    -- @field Device_Map Device map
    -- @field Core_Pmp Core - PMP
    -- @field Io_Pmp IO-PMP
    -- @field Reg_Pair Register address/data pairs
    -- @field Reserved2 All 0s
    type Pkc_Manifest_Ovl is limited record
        Version                  : LwU8;
        Ucode_Id                 : LwU8;
        Ucode_Version            : LwU8;
        Is_Relaxed_Version_Check : LwU8;
        Engine_Id_Mask           : LwU32;
        Code_Size_In_256_Bytes   : LwU16;
        Data_Size_In_256_Bytes   : LwU16;
        Fmc_Hash_Pad_Info_Mask   : LwU32;
        Sw_Enc_Derivation_Str    : Lw_B_Block64;
        Fmc_Enc_Params           : Mf_Encryption_Ovl;
        Digest                   : Lw_DW_Block384;
        Reserved0                : Lw_Bytes_Generic (0 .. 15);
        Secret_Mask              : Mf_Secret_Ovl;
        Debug_Control            : Mf_Debug_Control_Ovl;
        Io_Pmp_Mode0             : LwU32;
        Io_Pmp_Mode1             : LwU32;
        Is_Dice                  : LwU8;
        Is_Kdf                   : LwU8;
        Mspm                     : Mf_Mspm_Ovl;
        Reg_Pair_Num             : LwU8;
        Is_Attester              : LwU8;
        Reserved1                : Lw_Bytes_Generic (0 .. 9);
        Kdf_Constant             : Lw_DW_Block256;
        Device_Map               : Mf_Device_Map_Ovl;
        Core_Pmp                 : Mf_Core_Pmp_Ovl;
        Io_Pmp                   : Mf_Io_Pmp_Ovl;
        Reg_Pair                 : Mf_Register_Pair_Ovl;
        Reserved2                : Lw_Bytes_Generic (0 .. 127);
    end record with Size => 13312, Object_Size => 13312;

    -- Record of Production and Debug mode signatures
    -- @field Sig_Prod Production-signed signature
    -- @field Sig_Debug Debug-signed signature
    type Stage1_Pkc_Signature_Ovl is limited record
        Sig_Prod    : Lw_DW_Block3072;
        Sig_Debug   : Lw_DW_Block3072;
    end record with Size => 6144, Object_Size => 6144;

    -- Header of Stage1 Pkc
    -- @field Maigc_Number
    -- @field Is_Dbg_Key Allow debug-encrypted binary running on pre-productoin silicon
    -- @field Reserved0 All 0s
    -- @field Manifest_Enc_Params Crypto inputs required to decrypt Manifest
    -- @field Reserved1 All 0s
    type Stage1_Pkc_Header_Ovl is limited record
        Maigc_Number        : LwU32;
        Is_Dbg_Key          : LwU8;
        Reserved0           : Lw_Bytes_Generic (0 .. 18);
        Manifest_Enc_Params : Mf_Encryption_Ovl;
        Reserved1           : Lw_Bytes_Generic (0 .. 63);
    end record with Size => 1024, Object_Size => 1024;

    -- Stage1(Lwpu) PKC boot parameters
    -- @field Signature RSA3k signature of Header and Manifest
    -- @field Header Signed but unencrypted part
    -- @field Manifest Signed and encrypted part
    type Stage1_Pkc_Parameters is limited record
        Signature   : Stage1_Pkc_Signature_Ovl;
        Header      : Stage1_Pkc_Header_Ovl;
        Manifest    : Pkc_Manifest_Ovl;
    end record with Size => 20480, Object_Size => 20480;


    -- SW provided public key
    -- @field N modules of RSA3k public key
    -- @field Reserved0 All 0s
    type Stage1_Pkc_Sw_PKey_Ovl is limited record
        N           : Lw_DW_Block3072;
        Reserved0   : Lw_Bytes_Generic (0 .. 127);
    end record with Size => 4096, Object_Size => 4096;


    -- Stage2 PKC OEM public key
    -- @field Hash_Index The golden hash index
    -- @field Hashs Hash array that stores the golden hash of OEM public keys
    -- @field N OEM provided RSA3k public Key modules
    type Stage2_Pkc_Oem_PKey_Ovl is limited record
        Hash_Index  : LwU32;
        Hashs       : Pkc_Oem_PKey_Hash_Array;
        N           : Lw_DW_Block3072;
    end record with Size => 5024, Object_Size => 5024;

    -- Stage2 PKC signature
    -- @field Magic_Number Magic number
    -- @field Reserved0 All 0s
    -- @field PKey_Oem OEM provided public key
    -- @field Sig_Oem OEM RSA3k signing signature
    type Stage2_Pkc_Signature_Ovl is limited record
        Magic_Number : LwU32;
        Reserved0    : Lw_Bytes_Generic (0 .. 7);
        PKey_Oem     : Stage2_Pkc_Oem_PKey_Ovl;
        Sig_Oem      : Lw_DW_Block3072;
    end record with Size => 8192, Object_Size => 8192;


    Parameters : aliased Stage1_Pkc_Parameters with
        Import, Address => System'To_Address(PKCPARAM_START_PA_ADDR);


    function Is_Debug_Mode_Enabled return HS_BOOL with
        Inline_Always;

    function Get_Signature_Address return System.Address is
    begin
        if Is_Debug_Mode_Enabled = HS_TRUE then
            return Dmem_Offset_To_Address (Rv_Brom_Memory_Layout.PKCPARAM_START + Dw_Rsa3k_Signature'Length * 4);
        else
            return Dmem_Offset_To_Address (Rv_Brom_Memory_Layout.PKCPARAM_START);
        end if;
    end Get_Signature_Address;

    function Get_End_Of_Imem_Overwritable_Region return Imem_Offset_Byte
    is
        Overwritable_Region_End_SE : aliased Storage_Element
        with Import, Link_Name => "__imem_overwritable_region_end";

        Overwritable_Region_End : constant System.Address := Overwritable_Region_End_SE'Address;
    begin
        return Address_To_Imem_Offset(Overwritable_Region_End);
    end Get_End_Of_Imem_Overwritable_Region;

    function Get_Ucode_Id return Mf_Ucode_Id is
    begin
        return Parameters.Manifest.Ucode_Id;
    end Get_Ucode_Id;

    function Get_Ucode_Version return Mf_Ucode_Version is
    begin
        return Parameters.Manifest.Ucode_Version;
    end Get_Ucode_Version;

    function Get_Is_Relaxed_Version_Check return HS_BOOL is
    begin
        return LwU8_To_HS_Bool (Parameters.Manifest.Is_Relaxed_Version_Check);
    end Get_Is_Relaxed_Version_Check;

    function Get_Engine_Id_Mask return Mf_Engine_Id_Mask is
    begin
        return Parameters.Manifest.Engine_Id_Mask;
    end Get_Engine_Id_Mask;

    function Get_Fmc_Code_Block_Size return Imem_Size_Block is
    begin
        return Imem_Size_Block(Parameters.Manifest.Code_Size_In_256_Bytes);
    end Get_Fmc_Code_Block_Size;

    function Get_Fmc_Data_Block_Size return Fmc_Data_Max_Size_Block is
    begin
        return Fmc_Data_Max_Size_Block(Parameters.Manifest.Data_Size_In_256_Bytes);
    end Get_Fmc_Data_Block_Size;


    function Get_Fmc_Hash_Pad_Info_Mask return Mf_Pad_Info_Mask is
    begin
        return Parameters.Manifest.Fmc_Hash_Pad_Info_Mask;
    end Get_Fmc_Hash_Pad_Info_Mask;

    function Get_Is_Attester return HS_BOOL
    is begin
        return LwU8_To_HS_Bool (Parameters.Manifest.Is_Attester);
    end Get_Is_Attester;

    function Get_Is_Dice return HS_BOOL
    is begin
        return LwU8_To_HS_Bool (Parameters.Manifest.Is_Dice);
    end Get_Is_Dice;

    function Get_Is_Kdf return HS_BOOL
    is begin
        return LwU8_To_HS_Bool (Parameters.Manifest.Is_Kdf);
    end Get_Is_Kdf;

    function Get_Kdf_Constant (I : Lw_DW_Block256_Range) return LwU32 is
    begin
        return Parameters.Manifest.Kdf_Constant (I);
    end Get_Kdf_Constant;

    function Get_Reg_Pair_Num return Mf_Reg_Pair_Num is
    begin
        return Parameters.Manifest.Reg_Pair_Num;
    end Get_Reg_Pair_Num;
    function Get_Manifest_Enc_Derivation_Str (I : LwU32) return LwU8 is
    begin
        return Parameters.Header.Manifest_Enc_Params.Enc_Derivation_Str(I);
    end Get_Manifest_Enc_Derivation_Str;
    function Get_Fmc_Enc_Derivation_Str (I : LwU32) return LwU8 is
    begin
        return Parameters.Manifest.Fmc_Enc_Params.Enc_Derivation_Str(I);
    end Get_Fmc_Enc_Derivation_Str;
    function Get_Sw_Enc_Derivation_Str (I : LwU32) return LwU8 is
    begin
        return Parameters.Manifest.Sw_Enc_Derivation_Str(I);
    end Get_Sw_Enc_Derivation_Str;
    function Get_Manifest_Cbc_Iv (I : LwU32) return LwU32 is
    begin
        return Parameters.Header.Manifest_Enc_Params.Iv(I);
    end Get_Manifest_Cbc_Iv;
    function Get_Fmc_Cbc_Iv (I : LwU32) return LwU32 is
    begin
        return Parameters.Manifest.Fmc_Enc_Params.Iv(I);
    end Get_Fmc_Cbc_Iv;

    UCODE_VERSION_READ_MAX_TIMEOUT : constant LwU32 := 16#FFFF_FFF_E#;

    type MMODE_GROUP_Field is record
        Readable : LwU1;
        Writable : LwU1;
        Reserved : LwU1;
        Lock     : LwU1;
    end record with Size => 4;
    for MMODE_GROUP_Field use record
        Readable at 0 range 0 .. 0;
        Writable at 0 range 1 .. 1;
        Reserved at 0 range 2 .. 2;
        Lock     at 0 range 3 .. 3;
    end record;

    type RISCV_MMODE_Register is record
        G0 : MMODE_GROUP_Field;
        G1 : MMODE_GROUP_Field;
        G2 : MMODE_GROUP_Field;
        G3 : MMODE_GROUP_Field;
        G4 : MMODE_GROUP_Field;
        G5 : MMODE_GROUP_Field;
        G6 : MMODE_GROUP_Field;
        G7 : MMODE_GROUP_Field;
    end record with Size => 32, Object_Size => 32;

    for RISCV_MMODE_Register use record
        G0 at 0 range 0 .. 3;
        G1 at 0 range 4 .. 7;
        G2 at 0 range 8 .. 11;
        G3 at 0 range 12 .. 15;
        G4 at 0 range 16 .. 19;
        G5 at 0 range 20 .. 23;
        G6 at 0 range 24 .. 27;
        G7 at 0 range 28 .. 31;
    end record;

    function Val_To_Reg is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                         Target => RISCV_MMODE_Register);

    function Val_To_Csr is new Ada.Unchecked_Colwersion (Source => LwU64,
                                                         Target => LW_RISCV_CSR_MEXTPMPCFG2_Register);

    procedure Reg_Wr32_Addr32 (Addr : LwU32; Data : LwU32) renames Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Addr32_Mmio;
    procedure Reg_Wr32_Addr64 (Addr : LwU64; Data : LwU32) renames Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Addr64_Mmio_Safe;
    procedure Reg_Rd32_Addr64 (Addr : LwU64; Data : out LwU32) renames Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Addr64_Mmio;
    procedure Csr_Wr64 (Addr : LwU32; Data : LwU64) renames Rv_Brom_Riscv_Core.Inst.Csrw;

    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_HWCFG2_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_ENGID_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_UCODE_VERSION_Register);
    procedure Csb_Reg_Rd32 is new Rv_Brom_Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_Register);

    -- Define Write procedure for Manifeset programming related registers
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_PMPCFG0_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_PMPCFG2_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG0_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG2_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_PMPADDR_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPADDR_Register);
    procedure Csr_Reg_Wr64 is new Rv_Brom_Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MLWRRUID_Register);

    procedure Csr_Reg_Rd64 is new Rv_Brom_Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG2_Register);

    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_INDEX_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_MODE_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_CFG_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DBGCTL_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DBGCTL_LOCK_Register);
    procedure Csb_Reg_Wr32 is new Rv_Brom_Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);

    procedure Sanitize_Manifest_Misc (Manifest : Pkc_Manifest_Ovl;
                                      Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;


    procedure Sanitize_Manifest_Ucode_Id_And_Version (Manifest : Pkc_Manifest_Ovl;
                                                      Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Engine_Id (Manifest : Pkc_Manifest_Ovl;
                                           Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Code_And_Data_Size (Manifest : Pkc_Manifest_Ovl;
                                                    Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Pad_Info_Mask (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Mspm (Manifest : Pkc_Manifest_Ovl;
                                                      Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Debug_Control (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Device_Map (Manifest : Pkc_Manifest_Ovl;
                                            Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;

    procedure Sanitize_Manifest_Register_Pair (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;


    function Is_Addr_Pair_Enabled return HS_BOOL with
        Inline_Always;



    procedure Sanitize_Manifest(Err_Code : in out Error_Codes) is
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            Sanitize_Manifest_Misc (Manifest => Parameters.Manifest,
                                    Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            -- yitianc-todo: lwdec1 can't access hwver through kfuse
            -- Sanitize_Manifest_Ucode_Id_And_Version (Manifest => Parameters.Manifest,
            --                                         Err_Code => Err_Code);
            -- exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Engine_Id (Manifest => Parameters.Manifest,
                                         Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Code_And_Data_Size (Manifest => Parameters.Manifest,
                                                  Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Pad_Info_Mask (Manifest => Parameters.Manifest,
                                             Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Mspm (Manifest => Parameters.Manifest,
                                    Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Debug_Control (Manifest => Parameters.Manifest,
                                             Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Device_Map (Manifest => Parameters.Manifest,
                                          Err_Code => Err_Code);
            exit Operation_Done when Err_Code /= OK;

            Sanitize_Manifest_Register_Pair (Manifest => Parameters.Manifest,
                                             Err_Code => Err_Code);

        end loop Operation_Done;

    end Sanitize_Manifest;


    procedure Sanitize_Manifest_Misc (Manifest : Pkc_Manifest_Ovl;
                                      Err_Code : in out Error_Codes) is

        Is_Relaxed_Check : constant HS_BOOL := LwU8_To_HS_Bool (Manifest.Is_Relaxed_Version_Check);
        Is_Dice          : constant HS_BOOL := LwU8_To_HS_Bool (Manifest.Is_Dice);
        Is_Kdf           : constant HS_BOOL := LwU8_To_HS_Bool (Manifest.Is_Kdf);
        Is_Attester      : constant HS_BOOL := LwU8_To_HS_Bool (Manifest.Is_Attester);
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            if not (Is_Relaxed_Check = HS_TRUE or else Is_Relaxed_Check = HS_FALSE) then
                Err_Code := CRITICAL_ERROR;
                exit Operation_Done;
            end if;

            if not (Is_Dice = HS_TRUE or else Is_Dice = HS_FALSE) then
                Err_Code := CRITICAL_ERROR;
                exit Operation_Done;
            end if;

            if not (Is_Kdf = HS_TRUE or else Is_Kdf = HS_FALSE) then
                Err_Code := CRITICAL_ERROR;
                exit Operation_Done;
            end if;

            if not (Is_Attester = HS_TRUE or else Is_Attester = HS_FALSE) then
                Err_Code := CRITICAL_ERROR;
                exit Operation_Done;
            end if;
        end loop Operation_Done;
    end Sanitize_Manifest_Misc;

    procedure Sanitize_Manifest_Ucode_Id_And_Version (Manifest : Pkc_Manifest_Ovl;
                                                      Err_Code : in out Error_Codes) is
        Ucode_Ver : LwU8;
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            if Manifest.Ucode_Id not in Mf_Ucode_Id'Range then
                Err_Code := MANIFEST_UCODE_ID_ERROR;
                exit Operation_Done;
            end if;

            -- Check ucode version
            Get_HW_Ucode_Version(Ucode_Ver);

            Err_Code := (if Get_Is_Relaxed_Version_Check = HS_TRUE then
                             (if Manifest.Ucode_Version >= Ucode_Ver then OK else MANIFEST_UCODE_VERSION_ERROR)
                         elsif Get_Is_Relaxed_Version_Check = HS_FALSE then
                             (if  Manifest.Ucode_Version = Ucode_Ver then OK else MANIFEST_UCODE_VERSION_ERROR)
                         else CRITICAL_ERROR);
        end loop Operation_Done;
    end Sanitize_Manifest_Ucode_Id_And_Version;

    procedure Sanitize_Manifest_Engine_Id (Manifest : Pkc_Manifest_Ovl;
                                           Err_Code : in out Error_Codes) is
        Family_Id : LwU8;
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            exit Operation_Done when Err_Code /= OK;
            -- Engine ID should equal to the given Engine_Id_Mask
            Family_Id := Get_HW_EngineId_Family_Id;
            if Family_Id = 0 or else
               ((Lw_Shift_Left(Value  => LwU32(1), Amount => Integer(Family_Id - 1)) and Manifest.Engine_Id_Mask) = 0)
            then
                Err_Code := MANIFEST_FAMILY_ID_ERROR;
                exit Operation_Done;
            end if;
        end loop Operation_Done;
    end Sanitize_Manifest_Engine_Id;

    -- todo: need to check ucode size not larger than tcm size - reserved space for boot plugin
    procedure Sanitize_Manifest_Code_And_Data_Size (Manifest : Pkc_Manifest_Ovl;
                                                    Err_Code : in out Error_Codes) is
        Fmc_Dmem_Limit : Dmem_Size_Block;
    begin
        Operation_Done:
        for Unused_Loop_Var in  1 .. 1 loop
            -- Code size should fit before the start of the non-overwritable region.
            if Manifest.Code_Size_In_256_Bytes not in Mf_Imem_Size_Block'Range or else
                LwU32(Manifest.Code_Size_In_256_Bytes) * 256 > LwU32(Get_End_Of_Imem_Overwritable_Region - Get_Fmc_Code_Imem_Offset)
            then
                Err_Code := MANIFEST_CODE_SIZE_ERROR;
                exit Operation_Done;
            end if;

            if Manifest.Data_Size_In_256_Bytes in Mf_Dmem_Size_Block'Range then
                Fmc_Dmem_Limit := Fmc_Data_Max_Size_Block'Last;

                if (Dmem_Size_Block(Dmem_Offset_Byte_To_Block(Get_Fmc_Data_Dmem_Offset)) + Dmem_Size_Block(Manifest.Data_Size_In_256_Bytes)) > Fmc_Dmem_Limit then
                    Err_Code := MANIFEST_DATA_SIZE_ERROR;
                    exit Operation_Done;
                end if;

            else
                Err_Code := MANIFEST_DATA_SIZE_ERROR;
                exit Operation_Done;
            end if;
        end loop Operation_Done;
    end Sanitize_Manifest_Code_And_Data_Size;

    procedure Sanitize_Manifest_Pad_Info_Mask (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) is
    begin
        if Manifest.Fmc_Hash_Pad_Info_Mask not in Mf_Pad_Info_Mask'Range then
            Err_Code := MANIFEST_PAD_INFO_MASK_ERROR;
        end if;
    end Sanitize_Manifest_Pad_Info_Mask;

    procedure Sanitize_Manifest_Mspm (Manifest : Pkc_Manifest_Ovl;
                                                      Err_Code : in out Error_Codes) is
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            if not (Manifest.Mspm.Mplm in Mf_Mspm_Mplm'Range and then Manifest.Mspm.Msecm in Mf_Mspm_Mplm'Range) then
                Err_Code := MANIFEST_MSPM_VALUE_ERROR;
                exit Operation_Done;
            end if;
        end loop Operation_Done;

    end Sanitize_Manifest_Mspm;

    procedure Sanitize_Manifest_Debug_Control (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) is
    begin
        pragma Unreferenced(Manifest);
        pragma Unreferenced(Err_Code);
        null;
    end Sanitize_Manifest_Debug_Control;

    procedure Sanitize_Manifest_Device_Map (Manifest : Pkc_Manifest_Ovl;
                                            Err_Code : in out Error_Codes) is
        Riscv_Mmode : RISCV_MMODE_Register;
    begin
        -- Lwrrently device groups are defined as followes, each row lists all the groups in one registers
        -- __|___G0__|____G1_____|_____G2______|___G3_____|___G4_____|____G5_____|___G6_____|___G7_____|
        -- 0 | MMODE | RISCV_CTL |  PIC        | TIMER    | HOSTIF   |   DMA     | PMB      | DIO      |
        -- 1 | KEY   | DEBUG     |  SHA        | KMEM     | BROM     | ROM_PATCH | IOPMP    | NOACCESS |
        -- 2 | SCP   | FBIF      | FALCON_ONLY | PRGN_CTL | SCR_GRP0 | SCR_GRP1  | SCR_GRP2 | SCR_GRP3 |
        -- 3 | PLM   | HUB_DIO   |             |          |          |           |          |          |

        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            -- The brom device must be set to read-enable, write-disable, lock. R=1, W=0, L=1
            -- Each group comprises 4 bits, they represent for L|0|W|R|

            Riscv_Mmode := Val_To_Reg (Manifest.Device_Map.Cfgs(1));
            if Riscv_Mmode.G4 = MMODE_GROUP_Field'(Readable => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_READ_ENABLE,
                                                   Writable => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_WRITE_DISABLE,
                                                   Reserved => 0,
                                                   Lock     => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_LOCK_LOCKED)
            then
                null; -- pass
            else
                Err_Code := MANIFEST_DEVICEMAP_BR_UNLOCK_ERROR;
                exit Operation_Done;
            end if;
        end loop Operation_Done;

    end Sanitize_Manifest_Device_Map;

    function Is_In_Local_Range (Addr : LwU64) return Boolean is
        ((Addr < RISCV_PA_INTIO_END) and then (Addr >= RISCV_PA_INTIO_START)) with Inline_Always;

    -- There are 4 global IO apetures totally, but different engine may not use all of them, for example the SEC and PWR will only use apeture 1.
    -- If one apeture is not used then its _END = _START = 0, thus the compiler won't generate any code for this function
    pragma Warnings (Off, Reason => "Turn off the warning, since some variant don't have the Global IO apeture thus causing this condition can only be true or false");
    function Is_In_Global_Io_1_Range (Addr : LwU64) return Boolean is
        ((Addr < RISCV_PA_EXTIO1_END) and then (Addr >= RISCV_PA_EXTIO1_START)) with Inline_Always;

    function Is_In_Global_Io_2_Range (Addr : LwU64) return Boolean is
        ((Addr < RISCV_PA_EXTIO2_END) and then (Addr >= RISCV_PA_EXTIO2_START)) with Inline_Always;

    function Is_In_Global_Io_3_Range (Addr : LwU64) return Boolean is
        ((Addr < RISCV_PA_EXTIO3_END) and then (Addr >= RISCV_PA_EXTIO3_START)) with Inline_Always;

    function Is_In_Global_Io_4_Range (Addr : LwU64) return Boolean is
        ((Addr < RISCV_PA_EXTIO4_END) and then (Addr >= RISCV_PA_EXTIO4_START)) with Inline_Always;
    pragma Warnings (On);

    function Is_In_Io_Range (Addr : LwU64) return Boolean is
        ( Is_In_Local_Range (Addr) or else
          Is_In_Global_Io_1_Range (Addr) or else
          Is_In_Global_Io_2_Range (Addr) or else
          Is_In_Global_Io_3_Range (Addr) or else
          Is_In_Global_Io_4_Range (Addr)
         ) with Inline_Always;


    procedure Sanitize_Manifest_Register_Pair (Manifest : Pkc_Manifest_Ovl;
                                               Err_Code : in out Error_Codes) is
    begin
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            exit Operation_Done when Err_Code /= OK;

            if Manifest.Reg_Pair_Num not in Mf_Reg_Pair_Num'Range then
                Err_Code := MANIFEST_REG_PAIR_ENTRY_NUM_ERROR;
                exit Operation_Done;
            end if;

            if Is_Addr_Pair_Enabled = HS_FALSE and then Manifest.Reg_Pair_Num > 0 then
                Err_Code := MANIFEST_REG_PAIR_ENTRY_NUM_ERROR;
                exit Operation_Done;
            end if;

            exit Operation_Done when Manifest.Reg_Pair_Num = 0;

            for I in Mf_Register_Pair_Array_Range'Range loop
                exit Operation_Done when I >= Get_Reg_Pair_Num;
                if not Is_In_Io_Range (Addr => Manifest.Reg_Pair.Pairs(I).Address) then
                    Err_Code := MANIFEST_REG_PAIR_ADDRESS_ERROR;
                    exit Operation_Done;
                end if;
            end loop;

        end loop Operation_Done;
    end Sanitize_Manifest_Register_Pair;

    procedure Program_Manifest_Io_Pmp is
    begin
        -- Config *ALL* IO-PMP entries
        -- Set all entries mode to NAPOT
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_RISCV_IOPMP_MODE_0_Address,
                       Data => LW_PRGNLCL_RISCV_IOPMP_MODE_Register'(Val => Parameters.Manifest.Io_Pmp_Mode0));
        Csb_Reg_Wr32 ( Addr => LW_PRGNLCL_RISCV_IOPMP_MODE_1_Address,
                       Data => LW_PRGNLCL_RISCV_IOPMP_MODE_Register'(Val => Parameters.Manifest.Io_Pmp_Mode1));

        for I in Parameters.Manifest.Io_Pmp.Pmps'Range loop
            Reg_Wr32_Addr32 (Addr => LW_PRGNLCL_RISCV_IOPMP_INDEX_Address,
                             Data => LwU32(I));

            Reg_Wr32_Addr32 (Addr => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Address,
                             Data => Parameters.Manifest.Io_Pmp.Pmps (I).Addr_Lo);

            Reg_Wr32_Addr32 (Addr => LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Address,
                             Data => Parameters.Manifest.Io_Pmp.Pmps (I).Addr_Hi);

            -- Cfg configuration must be the last step, otherwise the lock bit will Lock the entry causing the
            -- address/mode can't be updated.
            Reg_Wr32_Addr32 (Addr => LW_PRGNLCL_RISCV_IOPMP_CFG_Address,
                             Data => Parameters.Manifest.Io_Pmp.Pmps (I).Cfg);

        end loop;

    end Program_Manifest_Io_Pmp;

    procedure Program_Manifest_Debug_Control is
    begin
        -- The RISCV_DBGCTL.SINGLE_STEP_MODE must be updated in a clean state,
        -- So we need to use Fence.All before it to make sure all memory and IO ack is received
        Rv_Brom_Riscv_Core.Inst.Lwfence_All;
        Reg_Wr32_Addr32 ( Addr => LW_PRGNLCL_RISCV_DBGCTL_Address,
                          Data => Parameters.Manifest.Debug_Control.Ctrl);
        Rv_Brom_Riscv_Core.Inst.Lwfence_All;

        Reg_Wr32_Addr32 ( Addr => LW_PRGNLCL_RISCV_DBGCTL_LOCK_Address,
                          Data => Parameters.Manifest.Debug_Control.Lock);
        Rv_Brom_Riscv_Core.Inst.Lwfence_All;

    end Program_Manifest_Debug_Control;

    procedure Program_Manifest_Device_Map is
    begin
        for I in Parameters.Manifest.Device_Map.Cfgs'Range loop
            Reg_Wr32_Addr32 (Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_0_Address +
                                 LwU32 (I) * (LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_1_Address - LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_0_Address),
                             Data => Parameters.Manifest.Device_Map.Cfgs (I));
        end loop;
    end Program_Manifest_Device_Map;

    procedure Program_Manifest_Register_Pair (Err_Code : out Error_Codes) is
        Reg_Value : LwU32;
        Reg_Addr  : LwU64;
    begin
        -- Make sure programming with level 3
        Err_Code := OK;
        Operation_Done :
        for Unused_Loop_Var in  1 .. 1 loop
            exit Operation_Done when Is_Addr_Pair_Enabled = HS_FALSE;
            exit Operation_Done when Get_Reg_Pair_Num = 0;

            for I in Mf_Register_Pair_Array_Range'Range loop
                exit Operation_Done when I >= Get_Reg_Pair_Num;
                exit Operation_Done when Is_Addr_Pair_Enabled /= HS_TRUE;
                Reg_Addr := Parameters.Manifest.Reg_Pair.Pairs(I).Address;
                --vcast_dont_instrument_start
                pragma Assume (Reg_Addr mod 4 = 0);
                --vcast_dont_instrument_end

                Reg_Rd32_Addr64 (Addr => Reg_Addr,
                                 Data => Reg_Value);
                --vcast_dont_do_mcdc
                Reg_Value := Reg_Value and Parameters.Manifest.Reg_Pair.Pairs(I).And_Mask;
                --vcast_dont_do_mcdc
                Reg_Value := Reg_Value or Parameters.Manifest.Reg_Pair.Pairs(I).Or_Mask;
                Reg_Wr32_Addr64 (Addr => Reg_Addr,
                                 Data => Reg_Value);

                Rv_Boot_Plugin_Error_Handling.Check_Mmio_Or_Io_Pmp_Error (Err_Code => Err_Code);
                exit Operation_Done when Err_Code /= OK;
            end loop;
        end loop Operation_Done;
    end Program_Manifest_Register_Pair;

    function Is_Addr_Pair_Enabled return HS_BOOL is
        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_HWCFG2_Address,
                      Data => Hwcfg2);
        return (if Hwcfg2.Riscv_Br_Adpair = ADPAIR_ENABLE then HS_TRUE else HS_FALSE);
    end Is_Addr_Pair_Enabled;

    function Is_Debug_Mode_Enabled return HS_BOOL is
        Hwcfg2 : LW_PRGNLCL_FALCON_HWCFG2_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_HWCFG2_Address,
                      Data => Hwcfg2);
        return (if Hwcfg2.Dbgmode = DBGMODE_ENABLE then HS_TRUE else HS_FALSE);
    end Is_Debug_Mode_Enabled;

    procedure Program_Manifest_Core_Pmp is
        Lwrrent_Ext_Cfg2  : LW_RISCV_CSR_MEXTPMPCFG2_Register;
        Manifest_Ext_Cfg2 : LW_RISCV_CSR_MEXTPMPCFG2_Register;
    begin
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_0_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (0));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_1_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (1));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_2_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (2));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_3_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (3));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_4_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (4));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_5_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (5));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_6_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (6));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_7_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (7));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_8_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (8));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_9_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (9));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_10_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (10));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_11_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (11));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_12_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (12));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_13_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (13));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_14_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (14));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPADDR_15_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (15));


        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (16));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_1_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (17));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_2_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (18));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_3_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (19));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_4_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (20));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_5_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (21));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_6_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (22));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_7_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (23));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_8_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (24));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_9_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (25));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_10_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (26));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_11_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (27));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_12_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (28));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPADDR_13_Address,
                   Data => Parameters.Manifest.Core_Pmp.Addrs (29));

        -- The last 2 entries are used by BROM and the entry 31 is locked by default
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPCFG0_Address,
                   Data => Parameters.Manifest.Core_Pmp.Cfgs (0));
        Csr_Wr64 ( Addr => LW_RISCV_CSR_PMPCFG2_Address,
                   Data => Parameters.Manifest.Core_Pmp.Cfgs (1));

        Csr_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG0_Address,
                   Data => Parameters.Manifest.Core_Pmp.Cfgs(2));

        -- Override the Core-PMP cfg for entries 30
        Csr_Reg_Rd64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG2_Address,
                       Data => Lwrrent_Ext_Cfg2);

        Manifest_Ext_Cfg2 := Val_To_Csr (Parameters.Manifest.Core_Pmp.Cfgs (3));

        Lwrrent_Ext_Cfg2.Pmp29l := Manifest_Ext_Cfg2.Pmp29l;
        Lwrrent_Ext_Cfg2.Pmp29a := Manifest_Ext_Cfg2.Pmp29a;
        Lwrrent_Ext_Cfg2.Pmp29x := Manifest_Ext_Cfg2.Pmp29x;
        Lwrrent_Ext_Cfg2.Pmp29w := Manifest_Ext_Cfg2.Pmp29w;
        Lwrrent_Ext_Cfg2.Pmp29r := Manifest_Ext_Cfg2.Pmp29r;
        Lwrrent_Ext_Cfg2.Pmp28l := Manifest_Ext_Cfg2.Pmp28l;
        Lwrrent_Ext_Cfg2.Pmp28a := Manifest_Ext_Cfg2.Pmp28a;
        Lwrrent_Ext_Cfg2.Pmp28x := Manifest_Ext_Cfg2.Pmp28x;
        Lwrrent_Ext_Cfg2.Pmp28w := Manifest_Ext_Cfg2.Pmp28w;
        Lwrrent_Ext_Cfg2.Pmp28r := Manifest_Ext_Cfg2.Pmp28r;
        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG2_Address,
                       Data => Lwrrent_Ext_Cfg2);
        Csr_Reg_Rd64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG2_Address,
                       Data => Lwrrent_Ext_Cfg2);
        Lwrrent_Ext_Cfg2.Pmp27l := Manifest_Ext_Cfg2.Pmp27l;
        Lwrrent_Ext_Cfg2.Pmp27a := Manifest_Ext_Cfg2.Pmp27a;
        Lwrrent_Ext_Cfg2.Pmp27x := Manifest_Ext_Cfg2.Pmp27x;
        Lwrrent_Ext_Cfg2.Pmp27w := Manifest_Ext_Cfg2.Pmp27w;
        Lwrrent_Ext_Cfg2.Pmp27r := Manifest_Ext_Cfg2.Pmp27r;
        Lwrrent_Ext_Cfg2.Pmp26l := Manifest_Ext_Cfg2.Pmp26l;
        Lwrrent_Ext_Cfg2.Pmp26a := Manifest_Ext_Cfg2.Pmp26a;
        Lwrrent_Ext_Cfg2.Pmp26x := Manifest_Ext_Cfg2.Pmp26x;
        Lwrrent_Ext_Cfg2.Pmp26w := Manifest_Ext_Cfg2.Pmp26w;
        Lwrrent_Ext_Cfg2.Pmp26r := Manifest_Ext_Cfg2.Pmp26r;
        Lwrrent_Ext_Cfg2.Pmp25l := Manifest_Ext_Cfg2.Pmp25l;
        Lwrrent_Ext_Cfg2.Pmp25a := Manifest_Ext_Cfg2.Pmp25a;
        Lwrrent_Ext_Cfg2.Pmp25x := Manifest_Ext_Cfg2.Pmp25x;
        Lwrrent_Ext_Cfg2.Pmp25w := Manifest_Ext_Cfg2.Pmp25w;
        Lwrrent_Ext_Cfg2.Pmp25r := Manifest_Ext_Cfg2.Pmp25r;
        Lwrrent_Ext_Cfg2.Pmp24l := Manifest_Ext_Cfg2.Pmp24l;
        Lwrrent_Ext_Cfg2.Pmp24a := Manifest_Ext_Cfg2.Pmp24a;
        Lwrrent_Ext_Cfg2.Pmp24x := Manifest_Ext_Cfg2.Pmp24x;
        Lwrrent_Ext_Cfg2.Pmp24w := Manifest_Ext_Cfg2.Pmp24w;
        Lwrrent_Ext_Cfg2.Pmp24r := Manifest_Ext_Cfg2.Pmp24r;

        Csr_Reg_Wr64 ( Addr => LW_RISCV_CSR_MEXTPMPCFG2_Address,
                       Data => Lwrrent_Ext_Cfg2);
    end Program_Manifest_Core_Pmp;

    procedure Program_Manifest_Ucode_Id is
    begin
        Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MLWRRUID_Address,
                      Data => LW_RISCV_CSR_MLWRRUID_Register'(Rs0      => LW_RISCV_CSR_MLWRRUID_RS0_RST,
                                                              Mlwrruid => Get_Ucode_Id,
                                                              Wpri0    => LW_RISCV_CSR_MLWRRUID_WPRI0_RST,
                                                              Slwrruid => LW_RISCV_CSR_MLWRRUID_SLWRRUID_RST,
                                                              Ulwrruid => LW_RISCV_CSR_MLWRRUID_ULWRRUID_RST));
    end Program_Manifest_Ucode_Id;

    procedure Program_Manifest_Mspm is
    begin
        Rv_Brom_Riscv_Core.Lock_Priv_Level_And_Selwre_Mask (Selwre_Mode => (if Parameters.Manifest.Mspm.Msecm = 0 then HS_FALSE else HS_TRUE),
                                                            Priv_Level  => LwU4 (Parameters.Manifest.Mspm.Mplm));
    end Program_Manifest_Mspm;

    function Get_HW_EngineId_Family_Id return LwU8 is
        Engine_Id : LW_PRGNLCL_FALCON_ENGID_Register;
    begin
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_ENGID_Address,
                      Data => Engine_Id);
        return Engine_Id.Familyid;
    end Get_HW_EngineId_Family_Id;

    procedure Get_HW_Ucode_Version (Ucode_Ver : out LwU8) is
        Fuse_Load     : LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_Register;
        Ucode_Version : LW_PRGNLCL_FALCON_UCODE_VERSION_Register;
        Timer         : LwU32 := 0;
    begin
        loop
            Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_KFUSE_LOAD_CTL_Address,
                          Data => Fuse_Load);
            exit when (Fuse_Load.Hwver_Busy = BUSY_FALSE) and then
                      (Fuse_Load.Hwver_Valid = VALID_TRUE);

            if Timer > UCODE_VERSION_READ_MAX_TIMEOUT then
                Rv_Boot_Plugin_Error_Handling.Log_Info (Info_Code => UCODE_VERSION_READ_HANG);
            else
                Timer := Timer + 1;
            end if;
        end loop;

        -- Ucode Id 1 corresponds to the LW_PRGNLCL_FALCON_UCODE_VERSION_0_Address
        -- Ucode Id 2 corresponds to the LW_PRGNLCL_FALCON_UCODE_VERSION_1_Address
        Csb_Reg_Rd32 (Addr => LW_PRGNLCL_FALCON_UCODE_VERSION_0_Address +
                      (LW_PRGNLCL_FALCON_UCODE_VERSION_1_Address - LW_PRGNLCL_FALCON_UCODE_VERSION_0_Address) *
                      LwU32 (Get_Ucode_Id - 1),
                      Data => Ucode_Version);
        Ucode_Ver := Ucode_Version.Ver;
    end Get_HW_Ucode_Version;


end Rv_Boot_Plugin_Sw_Interface;
