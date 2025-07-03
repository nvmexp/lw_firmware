typedef struct Core_Pmp_Cfg_Array {
	unsigned char byte[32];
} __attribute__((packed)) Core_Pmp_Cfg_Array;

typedef struct Core_Pmp_Addr_Array {
	unsigned long long dword[32];
} __attribute__((packed)) Core_Pmp_Addr_Array;

typedef struct Io_Pmp_Cfg_Array {
	unsigned int word[32];
} __attribute__((packed)) Io_Pmp_Cfg_Array;
 
typedef struct Io_Pmp_Addr_Array {
	unsigned int word[32];
} __attribute__((packed)) Io_Pmp_Addr_Array;

typedef struct Sha256_Digest_Dword_Array {
	unsigned int word[8];
} __attribute__((packed)) Sha256_Digest_Dword_Array;   //size = 8*4 = 32 bytes

typedef struct Aes128_Cbc_Iv_Dword_Array {
	unsigned int word[4];
} __attribute__((packed)) Aes128_Cbc_Iv_Dword_Array;   //size = 4*4   = 16 bytes

typedef struct Manifest_Core_Pmp_Mem_Overlay {
	Core_Pmp_Cfg_Array  Cfg;
	Core_Pmp_Addr_Array Addr;
} __attribute__((packed)) Manifest_Core_Pmp_Mem_Overlay;   //size = 32*9 bytes

typedef struct Manifest_Io_Pmp_Mem_Overlay {
	Io_Pmp_Cfg_Array  Cfg;
	Io_Pmp_Addr_Array AddrLo;
	Io_Pmp_Addr_Array AddrHi;
} __attribute__((packed)) Manifest_Io_Pmp_Mem_Overlay;   //size = 32*4 + 32*4 + 32*4= 384 bytes

typedef struct Manifest_Debug_Mem_Overlay {
	unsigned int Debug_Ctrl;
	unsigned int Debug_Ctrl_Lock;
} __attribute__((packed)) Manifest_Debug_Mem_Overlay;

typedef struct Manifest_Secret_Mem_Overlay {
	unsigned int Scp_Secret_Mask0;
	unsigned int Scp_Secret_Mask1;
	unsigned int Scp_Secret_Mask_lock0;
	unsigned int Scp_Secret_Mask_lock1;
} __attribute__((packed)) Manifest_Secret_Mem_Overlay;

typedef struct Manifest_MSPM_Mem_Overlay {
	unsigned char M_Priv_Level_Mask;
	unsigned char M_Selwre_Status_Mask;
} __attribute__((packed)) Manifest_MSPM_Mem_Overlay;

typedef struct Device_Map_Cfg_Array {
    unsigned int word[4];
} __attribute__((packed)) Device_Map_Cfg_Array;

typedef struct Manifest_Device_Map_Mem_Overlay {
	Device_Map_Cfg_Array Cfg;
} __attribute__((packed)) Manifest_Device_Map_Mem_Overlay;

typedef struct Manifest_RegisterPair
{
    unsigned long addr[32];
    unsigned int andMask[32];
    unsigned int orMask[32];
} __attribute__((packed)) Manifest_RegisterPair;

typedef struct Pkc_Verification_Manifest_Mem_Overlay {
	unsigned char Version;
	unsigned char Ucode_Id;
	unsigned char Ucode_Version;
	unsigned char Is_Relaxed_Version_check;
	unsigned int Engine_Id_Mask;
	unsigned short Code_Size_In_256_Bytes;
	unsigned short Data_Size_In_256_Bytes;
    unsigned int fmc_hash_pad_info_bit_mask;
	Aes128_Cbc_Iv_Dword_Array Cbc_Iv;

	Sha256_Digest_Dword_Array Digest;

	Manifest_Secret_Mem_Overlay Secret_Mask;
	Manifest_Debug_Mem_Overlay Debug_Control;
	unsigned long long Io_Pmp_Mode;

	unsigned char rsvd0;
	unsigned char Is_Dec_Fuse_Keys;
	Manifest_MSPM_Mem_Overlay Mspm;
    unsigned char number_of_valid_pairs;
    unsigned char rsvd1[43];
	Manifest_Device_Map_Mem_Overlay Device_Map;

	Manifest_Core_Pmp_Mem_Overlay Core_Pmp;

	Manifest_Io_Pmp_Mem_Overlay Io_Pmp;

    Manifest_RegisterPair Register_Pair;

	//-- exelwtion environment settings
    unsigned char rsvd3[320]; // rsvd fields 
} __attribute__((packed)) Pkc_Verification_Manifest_Mem_Overlay;
