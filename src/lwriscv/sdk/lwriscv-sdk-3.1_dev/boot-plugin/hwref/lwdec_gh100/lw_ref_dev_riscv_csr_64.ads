with Lw_Types; use Lw_Types;

package Lw_Ref_Dev_Riscv_Csr_64 with
    SPARK_Mode => On
is

    type LW_RISCV_CSR_MISA_MXL_Field is (MXL_32, MXL_64, MXL_128) with
        Size => 2;
    for LW_RISCV_CSR_MISA_MXL_Field use (MXL_32 => 16#1#, MXL_64 => 16#2#, MXL_128 => 16#3#);
    LW_RISCV_CSR_MISA_WPRI_RST  : constant LwU36 := 16#0#;
    LW_RISCV_CSR_MISA_EXT2_RST  : constant LwU23 := 16#12_8224#;
    LW_RISCV_CSR_MISA_EXT_C_RST : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MISA_EXT1_RST  : constant LwU2  := 16#0#;

    type LW_RISCV_CSR_MISA_Register is record
        Mxl   : LW_RISCV_CSR_MISA_MXL_Field;
        Wpri  : LwU36;
        Ext2  : LwU23;
        Ext_C : LwU1;
        Ext1  : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MISA_Register use record
        Mxl   at 0 range 62 .. 63;
        Wpri  at 0 range 26 .. 61;
        Ext2  at 0 range  3 .. 25;
        Ext_C at 0 range  2 ..  2;
        Ext1  at 0 range  0 ..  1;
    end record;

    LW_RISCV_CSR_MVENDORID_VENDORH_RST : constant LwU32 := 16#0#;
    LW_RISCV_CSR_MVENDORID_VENDORL_RST : constant LwU32 := 16#1eb#;

    type LW_RISCV_CSR_MVENDORID_Register is record
        Vendorh : LwU32;
        Vendorl : LwU32;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MVENDORID_Register use record
        Vendorh at 0 range 32 .. 63;
        Vendorl at 0 range  0 .. 31;
    end record;

    LW_RISCV_CSR_MARCHID_MSB_RST           : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MARCHID_RS1_RST           : constant LwU47 := 16#0#;
    LW_RISCV_CSR_MARCHID_CORE_MAJOR_RST    : constant LwU4  := 16#2#;
    LW_RISCV_CSR_MARCHID_CORE_MINOR_RST    : constant LwU4  := 16#0#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_128KB : constant       := 16#8#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_64KB  : constant       := 16#7#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_32KB  : constant       := 16#6#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_16KB  : constant       := 16#5#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_8KB   : constant       := 16#4#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_4KB   : constant       := 16#3#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_2KB   : constant       := 16#2#;
    LW_RISCV_CSR_MARCHID_DCACHE_SIZE_0KB   : constant       := 16#0#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_128KB : constant       := 16#8#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_64KB  : constant       := 16#7#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_32KB  : constant       := 16#6#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_16KB  : constant       := 16#5#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_8KB   : constant       := 16#4#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_4KB   : constant       := 16#3#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_2KB   : constant       := 16#2#;
    LW_RISCV_CSR_MARCHID_ICACHE_SIZE_0KB   : constant       := 16#0#;

    type LW_RISCV_CSR_MARCHID_Register is record
        Msb         : LwU1;
        Rs1         : LwU47;
        Core_Major  : LwU4;
        Core_Minor  : LwU4;
        Dcache_Size : LwU4;
        Icache_Size : LwU4;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MARCHID_Register use record
        Msb         at 0 range 63 .. 63;
        Rs1         at 0 range 16 .. 62;
        Core_Major  at 0 range 12 .. 15;
        Core_Minor  at 0 range  8 .. 11;
        Dcache_Size at 0 range  4 ..  7;
        Icache_Size at 0 range  0 ..  3;
    end record;

    LW_RISCV_CSR_MIMPID_IMPID_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MIMPID_Register is record
        Impid : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MIMPID_Register use record
        Impid at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MHARTID_HART_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MHARTID_Register is record
        Hart : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MHARTID_Register use record
        Hart at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MSTATUS_WPRI4_RST : constant LwU27 := 16#0#;
    LW_RISCV_CSR_MSTATUS_SXL_RST   : constant LwU2  := 16#2#;
    LW_RISCV_CSR_MSTATUS_UXL_RST   : constant LwU2  := 16#2#;
    LW_RISCV_CSR_MSTATUS_WPRI3_RST : constant LwU8  := 16#0#;
    type LW_RISCV_CSR_MSTATUS_MMI_Field is (MMI_DISABLE, MMI_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_MMI_Field use (MMI_DISABLE => 16#0#, MMI_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSTATUS_TSR_Field is (TSR_DISABLE, TSR_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_TSR_Field use (TSR_DISABLE => 16#0#, TSR_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSTATUS_TW_Field is (TW_DISABLE, TW_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_TW_Field use (TW_DISABLE => 16#0#, TW_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSTATUS_TVM_Field is (TVM_DISABLE, TVM_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_TVM_Field use (TVM_DISABLE => 16#0#, TVM_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSTATUS_MXR_Field is (MXR_DISABLE, MXR_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_MXR_Field use (MXR_DISABLE => 16#0#, MXR_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_SUM_DISABLE : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_MPRV_Field is (MPRV_DISABLE, MPRV_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_MPRV_Field use (MPRV_DISABLE => 16#0#, MPRV_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_XS_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_FS_Field is (FS_OFF, FS_INITIAL, FS_CLEAN, FS_DIRTY) with
        Size => 2;
    for LW_RISCV_CSR_MSTATUS_FS_Field use (FS_OFF => 16#0#, FS_INITIAL => 16#1#, FS_CLEAN => 16#2#, FS_DIRTY => 16#3#);
    type LW_RISCV_CSR_MSTATUS_MPP_Field is (MPP_USER, MPP_SUPERVISOR, MPP_MACHINE) with
        Size => 2;
    for LW_RISCV_CSR_MSTATUS_MPP_Field use (MPP_USER => 16#0#, MPP_SUPERVISOR => 16#1#, MPP_MACHINE => 16#3#);
    LW_RISCV_CSR_MSTATUS_WPRI2_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_SPP_Field is (SPP_USER, SPP_SUPERVISOR) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_SPP_Field use (SPP_USER => 16#0#, SPP_SUPERVISOR => 16#1#);
    type LW_RISCV_CSR_MSTATUS_MPIE_Field is (MPIE_DISABLE, MPIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_MPIE_Field use (MPIE_DISABLE => 16#0#, MPIE_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_SPIE_Field is (SPIE_DISABLE, SPIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_SPIE_Field use (SPIE_DISABLE => 16#0#, SPIE_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_UPIE_DISABLE : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_MIE_Field is (MIE_DISABLE, MIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_MIE_Field use (MIE_DISABLE => 16#0#, MIE_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_WPRI0_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MSTATUS_SIE_Field is (SIE_DISABLE, SIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSTATUS_SIE_Field use (SIE_DISABLE => 16#0#, SIE_ENABLE => 16#1#);
    LW_RISCV_CSR_MSTATUS_UIE_DISABLE : constant LwU1 := 16#0#;

    type LW_RISCV_CSR_MSTATUS_Register is record
        Sd    : LwU1;
        Wpri4 : LwU27;
        Sxl   : LwU2;
        Uxl   : LwU2;
        Wpri3 : LwU8;
        Mmi   : LW_RISCV_CSR_MSTATUS_MMI_Field;
        Tsr   : LW_RISCV_CSR_MSTATUS_TSR_Field;
        Tw    : LW_RISCV_CSR_MSTATUS_TW_Field;
        Tvm   : LW_RISCV_CSR_MSTATUS_TVM_Field;
        Mxr   : LW_RISCV_CSR_MSTATUS_MXR_Field;
        Sum   : LwU1;
        Mprv  : LW_RISCV_CSR_MSTATUS_MPRV_Field;
        Xs    : LwU2;
        Fs    : LW_RISCV_CSR_MSTATUS_FS_Field;
        Mpp   : LW_RISCV_CSR_MSTATUS_MPP_Field;
        Wpri2 : LwU2;
        Spp   : LW_RISCV_CSR_MSTATUS_SPP_Field;
        Mpie  : LW_RISCV_CSR_MSTATUS_MPIE_Field;
        Wpri1 : LwU1;
        Spie  : LW_RISCV_CSR_MSTATUS_SPIE_Field;
        Upie  : LwU1;
        Mie   : LW_RISCV_CSR_MSTATUS_MIE_Field;
        Wpri0 : LwU1;
        Sie   : LW_RISCV_CSR_MSTATUS_SIE_Field;
        Uie   : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MSTATUS_Register use record
        Sd    at 0 range 63 .. 63;
        Wpri4 at 0 range 36 .. 62;
        Sxl   at 0 range 34 .. 35;
        Uxl   at 0 range 32 .. 33;
        Wpri3 at 0 range 24 .. 31;
        Mmi   at 0 range 23 .. 23;
        Tsr   at 0 range 22 .. 22;
        Tw    at 0 range 21 .. 21;
        Tvm   at 0 range 20 .. 20;
        Mxr   at 0 range 19 .. 19;
        Sum   at 0 range 18 .. 18;
        Mprv  at 0 range 17 .. 17;
        Xs    at 0 range 15 .. 16;
        Fs    at 0 range 13 .. 14;
        Mpp   at 0 range 11 .. 12;
        Wpri2 at 0 range  9 .. 10;
        Spp   at 0 range  8 ..  8;
        Mpie  at 0 range  7 ..  7;
        Wpri1 at 0 range  6 ..  6;
        Spie  at 0 range  5 ..  5;
        Upie  at 0 range  4 ..  4;
        Mie   at 0 range  3 ..  3;
        Wpri0 at 0 range  2 ..  2;
        Sie   at 0 range  1 ..  1;
        Uie   at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MTVEC_BASE_RST : constant LwU62 := 16#2_0001#;
    type LW_RISCV_CSR_MTVEC_MODE_Field is (MODE_DIRECT, MODE_VECTORED) with
        Size => 2;
    for LW_RISCV_CSR_MTVEC_MODE_Field use (MODE_DIRECT => 16#0#, MODE_VECTORED => 16#1#);

    type LW_RISCV_CSR_MTVEC_Register is record
        Base : LwU62;
        Mode : LW_RISCV_CSR_MTVEC_MODE_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MTVEC_Register use record
        Base at 0 range 2 .. 63;
        Mode at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_MEDELEG_WPRI3_RST : constant LwU35 := 16#0#;
    type LW_RISCV_CSR_MEDELEG_USS_Field is (USS_NODELEG, USS_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_USS_Field use (USS_NODELEG => 16#0#, USS_DELEG => 16#1#);
    LW_RISCV_CSR_MEDELEG_WPRI2_RST : constant LwU12 := 16#0#;
    type LW_RISCV_CSR_MEDELEG_STORE_PAGE_FAULT_Field is (FAULT_NODELEG, FAULT_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_STORE_PAGE_FAULT_Field use (FAULT_NODELEG => 16#0#, FAULT_DELEG => 16#1#);
    LW_RISCV_CSR_MEDELEG_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MEDELEG_LOAD_PAGE_FAULT_Field is (FAULT_NODELEG, FAULT_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_LOAD_PAGE_FAULT_Field use (FAULT_NODELEG => 16#0#, FAULT_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_FETCH_PAGE_FAULT_Field is (FAULT_NODELEG, FAULT_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_FETCH_PAGE_FAULT_Field use (FAULT_NODELEG => 16#0#, FAULT_DELEG => 16#1#);
    LW_RISCV_CSR_MEDELEG_WPRI0_RST : constant LwU3 := 16#0#;
    type LW_RISCV_CSR_MEDELEG_USER_ECALL_Field is (ECALL_NODELEG, ECALL_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_USER_ECALL_Field use (ECALL_NODELEG => 16#0#, ECALL_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_FAULT_STORE_Field is (STORE_NODELEG, STORE_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_FAULT_STORE_Field use (STORE_NODELEG => 16#0#, STORE_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_MISALIGNED_STORE_Field is (STORE_NODELEG, STORE_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_MISALIGNED_STORE_Field use (STORE_NODELEG => 16#0#, STORE_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_FAULT_LOAD_Field is (LOAD_NODELEG, LOAD_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_FAULT_LOAD_Field use (LOAD_NODELEG => 16#0#, LOAD_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_MISALIGNED_LOAD_Field is (LOAD_NODELEG, LOAD_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_MISALIGNED_LOAD_Field use (LOAD_NODELEG => 16#0#, LOAD_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_BREAKPOINT_Field is (BREAKPOINT_NODELEG, BREAKPOINT_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_BREAKPOINT_Field use (BREAKPOINT_NODELEG => 16#0#, BREAKPOINT_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_ILLEGAL_INSTRUCTION_Field is (INSTRUCTION_NODELEG, INSTRUCTION_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_ILLEGAL_INSTRUCTION_Field use (INSTRUCTION_NODELEG => 16#0#, INSTRUCTION_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_FAULT_FETCH_Field is (FETCH_NODELEG, FETCH_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_FAULT_FETCH_Field use (FETCH_NODELEG => 16#0#, FETCH_DELEG => 16#1#);
    type LW_RISCV_CSR_MEDELEG_MISALIGNED_FETCH_Field is (FETCH_NODELEG, FETCH_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MEDELEG_MISALIGNED_FETCH_Field use (FETCH_NODELEG => 16#0#, FETCH_DELEG => 16#1#);

    type LW_RISCV_CSR_MEDELEG_Register is record
        Wpri3               : LwU35;
        Uss                 : LW_RISCV_CSR_MEDELEG_USS_Field;
        Wpri2               : LwU12;
        Store_Page_Fault    : LW_RISCV_CSR_MEDELEG_STORE_PAGE_FAULT_Field;
        Wpri1               : LwU1;
        Load_Page_Fault     : LW_RISCV_CSR_MEDELEG_LOAD_PAGE_FAULT_Field;
        Fetch_Page_Fault    : LW_RISCV_CSR_MEDELEG_FETCH_PAGE_FAULT_Field;
        Wpri0               : LwU3;
        User_Ecall          : LW_RISCV_CSR_MEDELEG_USER_ECALL_Field;
        Fault_Store         : LW_RISCV_CSR_MEDELEG_FAULT_STORE_Field;
        Misaligned_Store    : LW_RISCV_CSR_MEDELEG_MISALIGNED_STORE_Field;
        Fault_Load          : LW_RISCV_CSR_MEDELEG_FAULT_LOAD_Field;
        Misaligned_Load     : LW_RISCV_CSR_MEDELEG_MISALIGNED_LOAD_Field;
        Breakpoint          : LW_RISCV_CSR_MEDELEG_BREAKPOINT_Field;
        Illegal_Instruction : LW_RISCV_CSR_MEDELEG_ILLEGAL_INSTRUCTION_Field;
        Fault_Fetch         : LW_RISCV_CSR_MEDELEG_FAULT_FETCH_Field;
        Misaligned_Fetch    : LW_RISCV_CSR_MEDELEG_MISALIGNED_FETCH_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MEDELEG_Register use record
        Wpri3               at 0 range 29 .. 63;
        Uss                 at 0 range 28 .. 28;
        Wpri2               at 0 range 16 .. 27;
        Store_Page_Fault    at 0 range 15 .. 15;
        Wpri1               at 0 range 14 .. 14;
        Load_Page_Fault     at 0 range 13 .. 13;
        Fetch_Page_Fault    at 0 range 12 .. 12;
        Wpri0               at 0 range  9 .. 11;
        User_Ecall          at 0 range  8 ..  8;
        Fault_Store         at 0 range  7 ..  7;
        Misaligned_Store    at 0 range  6 ..  6;
        Fault_Load          at 0 range  5 ..  5;
        Misaligned_Load     at 0 range  4 ..  4;
        Breakpoint          at 0 range  3 ..  3;
        Illegal_Instruction at 0 range  2 ..  2;
        Fault_Fetch         at 0 range  1 ..  1;
        Misaligned_Fetch    at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MIDELEG_WPRI2_RST : constant LwU54 := 16#0#;
    type LW_RISCV_CSR_MIDELEG_SEID_Field is (SEID_NODELEG, SEID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_SEID_Field use (SEID_NODELEG => 16#0#, SEID_DELEG => 16#1#);
    type LW_RISCV_CSR_MIDELEG_UEID_Field is (UEID_NODELEG, UEID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_UEID_Field use (UEID_NODELEG => 16#0#, UEID_DELEG => 16#1#);
    LW_RISCV_CSR_MIDELEG_WPRI1_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MIDELEG_STID_Field is (STID_NODELEG, STID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_STID_Field use (STID_NODELEG => 16#0#, STID_DELEG => 16#1#);
    type LW_RISCV_CSR_MIDELEG_UTID_Field is (UTID_NODELEG, UTID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_UTID_Field use (UTID_NODELEG => 16#0#, UTID_DELEG => 16#1#);
    LW_RISCV_CSR_MIDELEG_WPRI0_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MIDELEG_SSID_Field is (SSID_NODELEG, SSID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_SSID_Field use (SSID_NODELEG => 16#0#, SSID_DELEG => 16#1#);
    type LW_RISCV_CSR_MIDELEG_USID_Field is (USID_NODELEG, USID_DELEG) with
        Size => 1;
    for LW_RISCV_CSR_MIDELEG_USID_Field use (USID_NODELEG => 16#0#, USID_DELEG => 16#1#);

    type LW_RISCV_CSR_MIDELEG_Register is record
        Wpri2 : LwU54;
        Seid  : LW_RISCV_CSR_MIDELEG_SEID_Field;
        Ueid  : LW_RISCV_CSR_MIDELEG_UEID_Field;
        Wpri1 : LwU2;
        Stid  : LW_RISCV_CSR_MIDELEG_STID_Field;
        Utid  : LW_RISCV_CSR_MIDELEG_UTID_Field;
        Wpri0 : LwU2;
        Ssid  : LW_RISCV_CSR_MIDELEG_SSID_Field;
        Usid  : LW_RISCV_CSR_MIDELEG_USID_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MIDELEG_Register use record
        Wpri2 at 0 range 10 .. 63;
        Seid  at 0 range  9 ..  9;
        Ueid  at 0 range  8 ..  8;
        Wpri1 at 0 range  6 ..  7;
        Stid  at 0 range  5 ..  5;
        Utid  at 0 range  4 ..  4;
        Wpri0 at 0 range  2 ..  3;
        Ssid  at 0 range  1 ..  1;
        Usid  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MIP_WIRI3_RST : constant LwU52 := 16#0#;
    LW_RISCV_CSR_MIP_MEIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_WIRI2_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_SEIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_UEIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_MTIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_WIRI1_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_STIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_UTIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_MSIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_WIRI0_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_SSIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIP_USIP_RST  : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_MIP_Register is record
        Wiri3 : LwU52;
        Meip  : LwU1;
        Wiri2 : LwU1;
        Seip  : LwU1;
        Ueip  : LwU1;
        Mtip  : LwU1;
        Wiri1 : LwU1;
        Stip  : LwU1;
        Utip  : LwU1;
        Msip  : LwU1;
        Wiri0 : LwU1;
        Ssip  : LwU1;
        Usip  : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MIP_Register use record
        Wiri3 at 0 range 12 .. 63;
        Meip  at 0 range 11 .. 11;
        Wiri2 at 0 range 10 .. 10;
        Seip  at 0 range  9 ..  9;
        Ueip  at 0 range  8 ..  8;
        Mtip  at 0 range  7 ..  7;
        Wiri1 at 0 range  6 ..  6;
        Stip  at 0 range  5 ..  5;
        Utip  at 0 range  4 ..  4;
        Msip  at 0 range  3 ..  3;
        Wiri0 at 0 range  2 ..  2;
        Ssip  at 0 range  1 ..  1;
        Usip  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MIE_WPRI3_RST : constant LwU52 := 16#0#;
    LW_RISCV_CSR_MIE_MEIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_WPRI2_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_SEIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_UEIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_MTIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_WPRI1_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_STIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_UTIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_MSIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_WPRI0_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_SSIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MIE_USIE_RST  : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_MIE_Register is record
        Wpri3 : LwU52;
        Meie  : LwU1;
        Wpri2 : LwU1;
        Seie  : LwU1;
        Ueie  : LwU1;
        Mtie  : LwU1;
        Wpri1 : LwU1;
        Stie  : LwU1;
        Utie  : LwU1;
        Msie  : LwU1;
        Wpri0 : LwU1;
        Ssie  : LwU1;
        Usie  : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MIE_Register use record
        Wpri3 at 0 range 12 .. 63;
        Meie  at 0 range 11 .. 11;
        Wpri2 at 0 range 10 .. 10;
        Seie  at 0 range  9 ..  9;
        Ueie  at 0 range  8 ..  8;
        Mtie  at 0 range  7 ..  7;
        Wpri1 at 0 range  6 ..  6;
        Stie  at 0 range  5 ..  5;
        Utie  at 0 range  4 ..  4;
        Msie  at 0 range  3 ..  3;
        Wpri0 at 0 range  2 ..  2;
        Ssie  at 0 range  1 ..  1;
        Usie  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST : constant LwU64 := 16#0#;
    LW_RISCV_CSR_MHPMCOUNTER_MCYCLE    : constant       := 16#b00#;
    LW_RISCV_CSR_MHPMCOUNTER_MINSTRET  : constant       := 16#b02#;

    type LW_RISCV_CSR_MHPMCOUNTER_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MHPMCOUNTER_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MCOUNTEREN_RS0_RST     : constant LwU32 := 16#0#;
    LW_RISCV_CSR_MCOUNTEREN_HPM_DISABLE : constant LwU29 := 16#0#;
    type LW_RISCV_CSR_MCOUNTEREN_IR_Field is (IR_DISABLE, IR_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MCOUNTEREN_IR_Field use (IR_DISABLE => 16#0#, IR_ENABLE => 16#1#);
    type LW_RISCV_CSR_MCOUNTEREN_TM_Field is (TM_DISABLE, TM_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MCOUNTEREN_TM_Field use (TM_DISABLE => 16#0#, TM_ENABLE => 16#1#);
    type LW_RISCV_CSR_MCOUNTEREN_CY_Field is (CY_DISABLE, CY_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MCOUNTEREN_CY_Field use (CY_DISABLE => 16#0#, CY_ENABLE => 16#1#);

    type LW_RISCV_CSR_MCOUNTEREN_Register is record
        Rs0 : LwU32;
        Hpm : LwU29;
        Ir  : LW_RISCV_CSR_MCOUNTEREN_IR_Field;
        Tm  : LW_RISCV_CSR_MCOUNTEREN_TM_Field;
        Cy  : LW_RISCV_CSR_MCOUNTEREN_CY_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MCOUNTEREN_Register use record
        Rs0 at 0 range 32 .. 63;
        Hpm at 0 range  3 .. 31;
        Ir  at 0 range  2 ..  2;
        Tm  at 0 range  1 ..  1;
        Cy  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MHPMEVENT_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MHPMEVENT_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MHPMEVENT_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MSCRATCH_SCRATCH_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MSCRATCH_Register is record
        Scratch : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MSCRATCH_Register use record
        Scratch at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MEPC_EPC_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MEPC_Register is record
        Epc : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MEPC_Register use record
        Epc at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MCAUSE_INT_RST            : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MCAUSE_WLRL_RST           : constant LwU58 := 16#0#;
    LW_RISCV_CSR_MCAUSE_EXCODE_RST         : constant LwU5  := 16#0#;
    LW_RISCV_CSR_MCAUSE_EXCODE_IAMA        : constant LwU5  := 16#0#;
    LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT  : constant LwU5  := 16#1#;
    LW_RISCV_CSR_MCAUSE_EXCODE_ILL         : constant LwU5  := 16#2#;
    LW_RISCV_CSR_MCAUSE_EXCODE_BKPT        : constant LwU5  := 16#3#;
    LW_RISCV_CSR_MCAUSE_EXCODE_LAMA        : constant LwU5  := 16#4#;
    LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT  : constant LwU5  := 16#5#;
    LW_RISCV_CSR_MCAUSE_EXCODE_SAMA        : constant LwU5  := 16#6#;
    LW_RISCV_CSR_MCAUSE_EXCODE_SACC_FAULT  : constant LwU5  := 16#7#;
    LW_RISCV_CSR_MCAUSE_EXCODE_UCALL       : constant LwU5  := 16#8#;
    LW_RISCV_CSR_MCAUSE_EXCODE_SCALL       : constant LwU5  := 16#9#;
    LW_RISCV_CSR_MCAUSE_EXCODE_MCALL       : constant LwU5  := 16#b#;
    LW_RISCV_CSR_MCAUSE_EXCODE_IPAGE_FAULT : constant LwU5  := 16#c#;
    LW_RISCV_CSR_MCAUSE_EXCODE_LPAGE_FAULT : constant LwU5  := 16#d#;
    LW_RISCV_CSR_MCAUSE_EXCODE_SPAGE_FAULT : constant LwU5  := 16#f#;
    LW_RISCV_CSR_MCAUSE_EXCODE_USS         : constant LwU5  := 16#1c#;
    LW_RISCV_CSR_MCAUSE_EXCODE_SSS         : constant LwU5  := 16#1d#;
    LW_RISCV_CSR_MCAUSE_EXCODE_U_SWINT     : constant LwU5  := 16#0#;
    LW_RISCV_CSR_MCAUSE_EXCODE_S_SWINT     : constant LwU5  := 16#1#;
    LW_RISCV_CSR_MCAUSE_EXCODE_M_SWINT     : constant LwU5  := 16#3#;
    LW_RISCV_CSR_MCAUSE_EXCODE_U_TINT      : constant LwU5  := 16#4#;
    LW_RISCV_CSR_MCAUSE_EXCODE_S_TINT      : constant LwU5  := 16#5#;
    LW_RISCV_CSR_MCAUSE_EXCODE_M_TINT      : constant LwU5  := 16#7#;
    LW_RISCV_CSR_MCAUSE_EXCODE_U_EINT      : constant LwU5  := 16#8#;
    LW_RISCV_CSR_MCAUSE_EXCODE_S_EINT      : constant LwU5  := 16#9#;
    LW_RISCV_CSR_MCAUSE_EXCODE_M_EINT      : constant LwU5  := 16#b#;

    type LW_RISCV_CSR_MCAUSE_Register is record
        Int    : LwU1;
        Wlrl   : LwU58;
        Excode : LwU5;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MCAUSE_Register use record
        Int    at 0 range 63 .. 63;
        Wlrl   at 0 range  5 .. 62;
        Excode at 0 range  0 ..  4;
    end record;

    LW_RISCV_CSR_MCAUSE2_WPRI_RST            : constant LwU40 := 16#0#;
    LW_RISCV_CSR_MCAUSE2_CAUSE_NO            : constant LwU8  := 16#0#;
    LW_RISCV_CSR_MCAUSE2_CAUSE_EBREAK        : constant LwU8  := 16#17#;
    LW_RISCV_CSR_MCAUSE2_CAUSE_HW_TRIGGER    : constant LwU8  := 16#18#;
    LW_RISCV_CSR_MCAUSE2_CAUSE_U_SINGLE_STEP : constant LwU8  := 16#19#;
    LW_RISCV_CSR_MCAUSE2_ERROR_NO            : constant LwU16 := 16#0#;

    type LW_RISCV_CSR_MCAUSE2_Register is record
        Wpri  : LwU40;
        Cause : LwU8;
        Error : LwU16;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MCAUSE2_Register use record
        Wpri  at 0 range 24 .. 63;
        Cause at 0 range 16 .. 23;
        Error at 0 range  0 .. 15;
    end record;

    LW_RISCV_CSR_MTVAL_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MTVAL_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MTVAL_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MMTE_WPRI3_RST : constant LwU57 := 16#0#;
    type LW_RISCV_CSR_MMTE_MPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MMTE_MPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_MMTE_WPRI2_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MMTE_SPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MMTE_SPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_MMTE_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MMTE_UPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MMTE_UPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_MMTE_WPRI0_RST : constant LwU2 := 16#0#;

    type LW_RISCV_CSR_MMTE_Register is record
        Wpri3  : LwU57;
        Mpm_En : LW_RISCV_CSR_MMTE_MPM_EN_Field;
        Wpri2  : LwU1;
        Spm_En : LW_RISCV_CSR_MMTE_SPM_EN_Field;
        Wpri1  : LwU1;
        Upm_En : LW_RISCV_CSR_MMTE_UPM_EN_Field;
        Wpri0  : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MMTE_Register use record
        Wpri3  at 0 range 7 .. 63;
        Mpm_En at 0 range 6 ..  6;
        Wpri2  at 0 range 5 ..  5;
        Spm_En at 0 range 4 ..  4;
        Wpri1  at 0 range 3 ..  3;
        Upm_En at 0 range 2 ..  2;
        Wpri0  at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_MPMMASK_VAL_RST  : constant LwU16 := 16#0#;
    LW_RISCV_CSR_MPMMASK_WPRI_RST : constant LwU48 := 16#0#;

    type LW_RISCV_CSR_MPMMASK_Register is record
        Val  : LwU16;
        Wpri : LwU48;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MPMMASK_Register use record
        Val  at 0 range 48 .. 63;
        Wpri at 0 range  0 .. 47;
    end record;

    type LW_RISCV_CSR_PMPCFG0_PMP7L_Field is (PMP7L_UNLOCK, PMP7L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP7L_Field use (PMP7L_UNLOCK => 16#0#, PMP7L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP7S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP7A_Field is (PMP7A_OFF, PMP7A_TOR, PMP7A_NA4, PMP7A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP7A_Field use (PMP7A_OFF => 16#0#, PMP7A_TOR => 16#1#, PMP7A_NA4 => 16#2#, PMP7A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP7X_Field is (PMP7X_DENIED, PMP7X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP7X_Field use (PMP7X_DENIED => 16#0#, PMP7X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP7W_Field is (PMP7W_DENIED, PMP7W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP7W_Field use (PMP7W_DENIED => 16#0#, PMP7W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP7R_Field is (PMP7R_DENIED, PMP7R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP7R_Field use (PMP7R_DENIED => 16#0#, PMP7R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP6L_Field is (PMP6L_UNLOCK, PMP6L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP6L_Field use (PMP6L_UNLOCK => 16#0#, PMP6L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP6S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP6A_Field is (PMP6A_OFF, PMP6A_TOR, PMP6A_NA4, PMP6A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP6A_Field use (PMP6A_OFF => 16#0#, PMP6A_TOR => 16#1#, PMP6A_NA4 => 16#2#, PMP6A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP6X_Field is (PMP6X_DENIED, PMP6X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP6X_Field use (PMP6X_DENIED => 16#0#, PMP6X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP6W_Field is (PMP6W_DENIED, PMP6W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP6W_Field use (PMP6W_DENIED => 16#0#, PMP6W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP6R_Field is (PMP6R_DENIED, PMP6R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP6R_Field use (PMP6R_DENIED => 16#0#, PMP6R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP5L_Field is (PMP5L_UNLOCK, PMP5L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP5L_Field use (PMP5L_UNLOCK => 16#0#, PMP5L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP5S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP5A_Field is (PMP5A_OFF, PMP5A_TOR, PMP5A_NA4, PMP5A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP5A_Field use (PMP5A_OFF => 16#0#, PMP5A_TOR => 16#1#, PMP5A_NA4 => 16#2#, PMP5A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP5X_Field is (PMP5X_DENIED, PMP5X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP5X_Field use (PMP5X_DENIED => 16#0#, PMP5X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP5W_Field is (PMP5W_DENIED, PMP5W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP5W_Field use (PMP5W_DENIED => 16#0#, PMP5W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP5R_Field is (PMP5R_DENIED, PMP5R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP5R_Field use (PMP5R_DENIED => 16#0#, PMP5R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP4L_Field is (PMP4L_UNLOCK, PMP4L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP4L_Field use (PMP4L_UNLOCK => 16#0#, PMP4L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP4S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP4A_Field is (PMP4A_OFF, PMP4A_TOR, PMP4A_NA4, PMP4A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP4A_Field use (PMP4A_OFF => 16#0#, PMP4A_TOR => 16#1#, PMP4A_NA4 => 16#2#, PMP4A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP4X_Field is (PMP4X_DENIED, PMP4X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP4X_Field use (PMP4X_DENIED => 16#0#, PMP4X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP4W_Field is (PMP4W_DENIED, PMP4W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP4W_Field use (PMP4W_DENIED => 16#0#, PMP4W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP4R_Field is (PMP4R_DENIED, PMP4R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP4R_Field use (PMP4R_DENIED => 16#0#, PMP4R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP3L_Field is (PMP3L_UNLOCK, PMP3L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP3L_Field use (PMP3L_UNLOCK => 16#0#, PMP3L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP3S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP3A_Field is (PMP3A_OFF, PMP3A_TOR, PMP3A_NA4, PMP3A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP3A_Field use (PMP3A_OFF => 16#0#, PMP3A_TOR => 16#1#, PMP3A_NA4 => 16#2#, PMP3A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP3X_Field is (PMP3X_DENIED, PMP3X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP3X_Field use (PMP3X_DENIED => 16#0#, PMP3X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP3W_Field is (PMP3W_DENIED, PMP3W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP3W_Field use (PMP3W_DENIED => 16#0#, PMP3W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP3R_Field is (PMP3R_DENIED, PMP3R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP3R_Field use (PMP3R_DENIED => 16#0#, PMP3R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP2L_Field is (PMP2L_UNLOCK, PMP2L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP2L_Field use (PMP2L_UNLOCK => 16#0#, PMP2L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP2S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP2A_Field is (PMP2A_OFF, PMP2A_TOR, PMP2A_NA4, PMP2A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP2A_Field use (PMP2A_OFF => 16#0#, PMP2A_TOR => 16#1#, PMP2A_NA4 => 16#2#, PMP2A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP2X_Field is (PMP2X_DENIED, PMP2X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP2X_Field use (PMP2X_DENIED => 16#0#, PMP2X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP2W_Field is (PMP2W_DENIED, PMP2W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP2W_Field use (PMP2W_DENIED => 16#0#, PMP2W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP2R_Field is (PMP2R_DENIED, PMP2R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP2R_Field use (PMP2R_DENIED => 16#0#, PMP2R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP1L_Field is (PMP1L_UNLOCK, PMP1L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP1L_Field use (PMP1L_UNLOCK => 16#0#, PMP1L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP1S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP1A_Field is (PMP1A_OFF, PMP1A_TOR, PMP1A_NA4, PMP1A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP1A_Field use (PMP1A_OFF => 16#0#, PMP1A_TOR => 16#1#, PMP1A_NA4 => 16#2#, PMP1A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP1X_Field is (PMP1X_DENIED, PMP1X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP1X_Field use (PMP1X_DENIED => 16#0#, PMP1X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP1W_Field is (PMP1W_DENIED, PMP1W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP1W_Field use (PMP1W_DENIED => 16#0#, PMP1W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP1R_Field is (PMP1R_DENIED, PMP1R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP1R_Field use (PMP1R_DENIED => 16#0#, PMP1R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP0L_Field is (PMP0L_UNLOCK, PMP0L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP0L_Field use (PMP0L_UNLOCK => 16#0#, PMP0L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG0_PMP0S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG0_PMP0A_Field is (PMP0A_OFF, PMP0A_TOR, PMP0A_NA4, PMP0A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG0_PMP0A_Field use (PMP0A_OFF => 16#0#, PMP0A_TOR => 16#1#, PMP0A_NA4 => 16#2#, PMP0A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG0_PMP0X_Field is (PMP0X_DENIED, PMP0X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP0X_Field use (PMP0X_DENIED => 16#0#, PMP0X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP0W_Field is (PMP0W_DENIED, PMP0W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP0W_Field use (PMP0W_DENIED => 16#0#, PMP0W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG0_PMP0R_Field is (PMP0R_DENIED, PMP0R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG0_PMP0R_Field use (PMP0R_DENIED => 16#0#, PMP0R_PERMITTED => 16#1#);

    type LW_RISCV_CSR_PMPCFG0_Register is record
        Pmp7l : LW_RISCV_CSR_PMPCFG0_PMP7L_Field;
        Pmp7s : LwU2;
        Pmp7a : LW_RISCV_CSR_PMPCFG0_PMP7A_Field;
        Pmp7x : LW_RISCV_CSR_PMPCFG0_PMP7X_Field;
        Pmp7w : LW_RISCV_CSR_PMPCFG0_PMP7W_Field;
        Pmp7r : LW_RISCV_CSR_PMPCFG0_PMP7R_Field;
        Pmp6l : LW_RISCV_CSR_PMPCFG0_PMP6L_Field;
        Pmp6s : LwU2;
        Pmp6a : LW_RISCV_CSR_PMPCFG0_PMP6A_Field;
        Pmp6x : LW_RISCV_CSR_PMPCFG0_PMP6X_Field;
        Pmp6w : LW_RISCV_CSR_PMPCFG0_PMP6W_Field;
        Pmp6r : LW_RISCV_CSR_PMPCFG0_PMP6R_Field;
        Pmp5l : LW_RISCV_CSR_PMPCFG0_PMP5L_Field;
        Pmp5s : LwU2;
        Pmp5a : LW_RISCV_CSR_PMPCFG0_PMP5A_Field;
        Pmp5x : LW_RISCV_CSR_PMPCFG0_PMP5X_Field;
        Pmp5w : LW_RISCV_CSR_PMPCFG0_PMP5W_Field;
        Pmp5r : LW_RISCV_CSR_PMPCFG0_PMP5R_Field;
        Pmp4l : LW_RISCV_CSR_PMPCFG0_PMP4L_Field;
        Pmp4s : LwU2;
        Pmp4a : LW_RISCV_CSR_PMPCFG0_PMP4A_Field;
        Pmp4x : LW_RISCV_CSR_PMPCFG0_PMP4X_Field;
        Pmp4w : LW_RISCV_CSR_PMPCFG0_PMP4W_Field;
        Pmp4r : LW_RISCV_CSR_PMPCFG0_PMP4R_Field;
        Pmp3l : LW_RISCV_CSR_PMPCFG0_PMP3L_Field;
        Pmp3s : LwU2;
        Pmp3a : LW_RISCV_CSR_PMPCFG0_PMP3A_Field;
        Pmp3x : LW_RISCV_CSR_PMPCFG0_PMP3X_Field;
        Pmp3w : LW_RISCV_CSR_PMPCFG0_PMP3W_Field;
        Pmp3r : LW_RISCV_CSR_PMPCFG0_PMP3R_Field;
        Pmp2l : LW_RISCV_CSR_PMPCFG0_PMP2L_Field;
        Pmp2s : LwU2;
        Pmp2a : LW_RISCV_CSR_PMPCFG0_PMP2A_Field;
        Pmp2x : LW_RISCV_CSR_PMPCFG0_PMP2X_Field;
        Pmp2w : LW_RISCV_CSR_PMPCFG0_PMP2W_Field;
        Pmp2r : LW_RISCV_CSR_PMPCFG0_PMP2R_Field;
        Pmp1l : LW_RISCV_CSR_PMPCFG0_PMP1L_Field;
        Pmp1s : LwU2;
        Pmp1a : LW_RISCV_CSR_PMPCFG0_PMP1A_Field;
        Pmp1x : LW_RISCV_CSR_PMPCFG0_PMP1X_Field;
        Pmp1w : LW_RISCV_CSR_PMPCFG0_PMP1W_Field;
        Pmp1r : LW_RISCV_CSR_PMPCFG0_PMP1R_Field;
        Pmp0l : LW_RISCV_CSR_PMPCFG0_PMP0L_Field;
        Pmp0s : LwU2;
        Pmp0a : LW_RISCV_CSR_PMPCFG0_PMP0A_Field;
        Pmp0x : LW_RISCV_CSR_PMPCFG0_PMP0X_Field;
        Pmp0w : LW_RISCV_CSR_PMPCFG0_PMP0W_Field;
        Pmp0r : LW_RISCV_CSR_PMPCFG0_PMP0R_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_PMPCFG0_Register use record
        Pmp7l at 0 range 63 .. 63;
        Pmp7s at 0 range 61 .. 62;
        Pmp7a at 0 range 59 .. 60;
        Pmp7x at 0 range 58 .. 58;
        Pmp7w at 0 range 57 .. 57;
        Pmp7r at 0 range 56 .. 56;
        Pmp6l at 0 range 55 .. 55;
        Pmp6s at 0 range 53 .. 54;
        Pmp6a at 0 range 51 .. 52;
        Pmp6x at 0 range 50 .. 50;
        Pmp6w at 0 range 49 .. 49;
        Pmp6r at 0 range 48 .. 48;
        Pmp5l at 0 range 47 .. 47;
        Pmp5s at 0 range 45 .. 46;
        Pmp5a at 0 range 43 .. 44;
        Pmp5x at 0 range 42 .. 42;
        Pmp5w at 0 range 41 .. 41;
        Pmp5r at 0 range 40 .. 40;
        Pmp4l at 0 range 39 .. 39;
        Pmp4s at 0 range 37 .. 38;
        Pmp4a at 0 range 35 .. 36;
        Pmp4x at 0 range 34 .. 34;
        Pmp4w at 0 range 33 .. 33;
        Pmp4r at 0 range 32 .. 32;
        Pmp3l at 0 range 31 .. 31;
        Pmp3s at 0 range 29 .. 30;
        Pmp3a at 0 range 27 .. 28;
        Pmp3x at 0 range 26 .. 26;
        Pmp3w at 0 range 25 .. 25;
        Pmp3r at 0 range 24 .. 24;
        Pmp2l at 0 range 23 .. 23;
        Pmp2s at 0 range 21 .. 22;
        Pmp2a at 0 range 19 .. 20;
        Pmp2x at 0 range 18 .. 18;
        Pmp2w at 0 range 17 .. 17;
        Pmp2r at 0 range 16 .. 16;
        Pmp1l at 0 range 15 .. 15;
        Pmp1s at 0 range 13 .. 14;
        Pmp1a at 0 range 11 .. 12;
        Pmp1x at 0 range 10 .. 10;
        Pmp1w at 0 range  9 ..  9;
        Pmp1r at 0 range  8 ..  8;
        Pmp0l at 0 range  7 ..  7;
        Pmp0s at 0 range  5 ..  6;
        Pmp0a at 0 range  3 ..  4;
        Pmp0x at 0 range  2 ..  2;
        Pmp0w at 0 range  1 ..  1;
        Pmp0r at 0 range  0 ..  0;
    end record;

    type LW_RISCV_CSR_PMPCFG2_PMP15L_Field is (PMP15L_UNLOCK, PMP15L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP15L_Field use (PMP15L_UNLOCK => 16#0#, PMP15L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP15S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP15A_Field is (PMP15A_OFF, PMP15A_TOR, PMP15A_NA4, PMP15A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP15A_Field use (PMP15A_OFF => 16#0#, PMP15A_TOR => 16#1#, PMP15A_NA4 => 16#2#, PMP15A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP15X_Field is (PMP15X_DENIED, PMP15X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP15X_Field use (PMP15X_DENIED => 16#0#, PMP15X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP15W_Field is (PMP15W_DENIED, PMP15W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP15W_Field use (PMP15W_DENIED => 16#0#, PMP15W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP15R_Field is (PMP15R_DENIED, PMP15R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP15R_Field use (PMP15R_DENIED => 16#0#, PMP15R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP14L_Field is (PMP14L_UNLOCK, PMP14L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP14L_Field use (PMP14L_UNLOCK => 16#0#, PMP14L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP14S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP14A_Field is (PMP14A_OFF, PMP14A_TOR, PMP14A_NA4, PMP14A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP14A_Field use (PMP14A_OFF => 16#0#, PMP14A_TOR => 16#1#, PMP14A_NA4 => 16#2#, PMP14A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP14X_Field is (PMP14X_DENIED, PMP14X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP14X_Field use (PMP14X_DENIED => 16#0#, PMP14X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP14W_Field is (PMP14W_DENIED, PMP14W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP14W_Field use (PMP14W_DENIED => 16#0#, PMP14W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP14R_Field is (PMP14R_DENIED, PMP14R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP14R_Field use (PMP14R_DENIED => 16#0#, PMP14R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP13L_Field is (PMP13L_UNLOCK, PMP13L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP13L_Field use (PMP13L_UNLOCK => 16#0#, PMP13L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP13S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP13A_Field is (PMP13A_OFF, PMP13A_TOR, PMP13A_NA4, PMP13A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP13A_Field use (PMP13A_OFF => 16#0#, PMP13A_TOR => 16#1#, PMP13A_NA4 => 16#2#, PMP13A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP13X_Field is (PMP13X_DENIED, PMP13X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP13X_Field use (PMP13X_DENIED => 16#0#, PMP13X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP13W_Field is (PMP13W_DENIED, PMP13W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP13W_Field use (PMP13W_DENIED => 16#0#, PMP13W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP13R_Field is (PMP13R_DENIED, PMP13R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP13R_Field use (PMP13R_DENIED => 16#0#, PMP13R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP12L_Field is (PMP12L_UNLOCK, PMP12L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP12L_Field use (PMP12L_UNLOCK => 16#0#, PMP12L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP12S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP12A_Field is (PMP12A_OFF, PMP12A_TOR, PMP12A_NA4, PMP12A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP12A_Field use (PMP12A_OFF => 16#0#, PMP12A_TOR => 16#1#, PMP12A_NA4 => 16#2#, PMP12A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP12X_Field is (PMP12X_DENIED, PMP12X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP12X_Field use (PMP12X_DENIED => 16#0#, PMP12X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP12W_Field is (PMP12W_DENIED, PMP12W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP12W_Field use (PMP12W_DENIED => 16#0#, PMP12W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP12R_Field is (PMP12R_DENIED, PMP12R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP12R_Field use (PMP12R_DENIED => 16#0#, PMP12R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP11L_Field is (PMP11L_UNLOCK, PMP11L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP11L_Field use (PMP11L_UNLOCK => 16#0#, PMP11L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP11S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP11A_Field is (PMP11A_OFF, PMP11A_TOR, PMP11A_NA4, PMP11A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP11A_Field use (PMP11A_OFF => 16#0#, PMP11A_TOR => 16#1#, PMP11A_NA4 => 16#2#, PMP11A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP11X_Field is (PMP11X_DENIED, PMP11X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP11X_Field use (PMP11X_DENIED => 16#0#, PMP11X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP11W_Field is (PMP11W_DENIED, PMP11W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP11W_Field use (PMP11W_DENIED => 16#0#, PMP11W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP11R_Field is (PMP11R_DENIED, PMP11R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP11R_Field use (PMP11R_DENIED => 16#0#, PMP11R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP10L_Field is (PMP10L_UNLOCK, PMP10L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP10L_Field use (PMP10L_UNLOCK => 16#0#, PMP10L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP10S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP10A_Field is (PMP10A_OFF, PMP10A_TOR, PMP10A_NA4, PMP10A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP10A_Field use (PMP10A_OFF => 16#0#, PMP10A_TOR => 16#1#, PMP10A_NA4 => 16#2#, PMP10A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP10X_Field is (PMP10X_DENIED, PMP10X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP10X_Field use (PMP10X_DENIED => 16#0#, PMP10X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP10W_Field is (PMP10W_DENIED, PMP10W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP10W_Field use (PMP10W_DENIED => 16#0#, PMP10W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP10R_Field is (PMP10R_DENIED, PMP10R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP10R_Field use (PMP10R_DENIED => 16#0#, PMP10R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP9L_Field is (PMP9L_UNLOCK, PMP9L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP9L_Field use (PMP9L_UNLOCK => 16#0#, PMP9L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP9S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP9A_Field is (PMP9A_OFF, PMP9A_TOR, PMP9A_NA4, PMP9A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP9A_Field use (PMP9A_OFF => 16#0#, PMP9A_TOR => 16#1#, PMP9A_NA4 => 16#2#, PMP9A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP9X_Field is (PMP9X_DENIED, PMP9X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP9X_Field use (PMP9X_DENIED => 16#0#, PMP9X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP9W_Field is (PMP9W_DENIED, PMP9W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP9W_Field use (PMP9W_DENIED => 16#0#, PMP9W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP9R_Field is (PMP9R_DENIED, PMP9R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP9R_Field use (PMP9R_DENIED => 16#0#, PMP9R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP8L_Field is (PMP8L_UNLOCK, PMP8L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP8L_Field use (PMP8L_UNLOCK => 16#0#, PMP8L_LOCK => 16#1#);
    LW_RISCV_CSR_PMPCFG2_PMP8S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_PMPCFG2_PMP8A_Field is (PMP8A_OFF, PMP8A_TOR, PMP8A_NA4, PMP8A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_PMPCFG2_PMP8A_Field use (PMP8A_OFF => 16#0#, PMP8A_TOR => 16#1#, PMP8A_NA4 => 16#2#, PMP8A_NAPOT => 16#3#);
    type LW_RISCV_CSR_PMPCFG2_PMP8X_Field is (PMP8X_DENIED, PMP8X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP8X_Field use (PMP8X_DENIED => 16#0#, PMP8X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP8W_Field is (PMP8W_DENIED, PMP8W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP8W_Field use (PMP8W_DENIED => 16#0#, PMP8W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_PMPCFG2_PMP8R_Field is (PMP8R_DENIED, PMP8R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_PMPCFG2_PMP8R_Field use (PMP8R_DENIED => 16#0#, PMP8R_PERMITTED => 16#1#);

    type LW_RISCV_CSR_PMPCFG2_Register is record
        Pmp15l : LW_RISCV_CSR_PMPCFG2_PMP15L_Field;
        Pmp15s : LwU2;
        Pmp15a : LW_RISCV_CSR_PMPCFG2_PMP15A_Field;
        Pmp15x : LW_RISCV_CSR_PMPCFG2_PMP15X_Field;
        Pmp15w : LW_RISCV_CSR_PMPCFG2_PMP15W_Field;
        Pmp15r : LW_RISCV_CSR_PMPCFG2_PMP15R_Field;
        Pmp14l : LW_RISCV_CSR_PMPCFG2_PMP14L_Field;
        Pmp14s : LwU2;
        Pmp14a : LW_RISCV_CSR_PMPCFG2_PMP14A_Field;
        Pmp14x : LW_RISCV_CSR_PMPCFG2_PMP14X_Field;
        Pmp14w : LW_RISCV_CSR_PMPCFG2_PMP14W_Field;
        Pmp14r : LW_RISCV_CSR_PMPCFG2_PMP14R_Field;
        Pmp13l : LW_RISCV_CSR_PMPCFG2_PMP13L_Field;
        Pmp13s : LwU2;
        Pmp13a : LW_RISCV_CSR_PMPCFG2_PMP13A_Field;
        Pmp13x : LW_RISCV_CSR_PMPCFG2_PMP13X_Field;
        Pmp13w : LW_RISCV_CSR_PMPCFG2_PMP13W_Field;
        Pmp13r : LW_RISCV_CSR_PMPCFG2_PMP13R_Field;
        Pmp12l : LW_RISCV_CSR_PMPCFG2_PMP12L_Field;
        Pmp12s : LwU2;
        Pmp12a : LW_RISCV_CSR_PMPCFG2_PMP12A_Field;
        Pmp12x : LW_RISCV_CSR_PMPCFG2_PMP12X_Field;
        Pmp12w : LW_RISCV_CSR_PMPCFG2_PMP12W_Field;
        Pmp12r : LW_RISCV_CSR_PMPCFG2_PMP12R_Field;
        Pmp11l : LW_RISCV_CSR_PMPCFG2_PMP11L_Field;
        Pmp11s : LwU2;
        Pmp11a : LW_RISCV_CSR_PMPCFG2_PMP11A_Field;
        Pmp11x : LW_RISCV_CSR_PMPCFG2_PMP11X_Field;
        Pmp11w : LW_RISCV_CSR_PMPCFG2_PMP11W_Field;
        Pmp11r : LW_RISCV_CSR_PMPCFG2_PMP11R_Field;
        Pmp10l : LW_RISCV_CSR_PMPCFG2_PMP10L_Field;
        Pmp10s : LwU2;
        Pmp10a : LW_RISCV_CSR_PMPCFG2_PMP10A_Field;
        Pmp10x : LW_RISCV_CSR_PMPCFG2_PMP10X_Field;
        Pmp10w : LW_RISCV_CSR_PMPCFG2_PMP10W_Field;
        Pmp10r : LW_RISCV_CSR_PMPCFG2_PMP10R_Field;
        Pmp9l  : LW_RISCV_CSR_PMPCFG2_PMP9L_Field;
        Pmp9s  : LwU2;
        Pmp9a  : LW_RISCV_CSR_PMPCFG2_PMP9A_Field;
        Pmp9x  : LW_RISCV_CSR_PMPCFG2_PMP9X_Field;
        Pmp9w  : LW_RISCV_CSR_PMPCFG2_PMP9W_Field;
        Pmp9r  : LW_RISCV_CSR_PMPCFG2_PMP9R_Field;
        Pmp8l  : LW_RISCV_CSR_PMPCFG2_PMP8L_Field;
        Pmp8s  : LwU2;
        Pmp8a  : LW_RISCV_CSR_PMPCFG2_PMP8A_Field;
        Pmp8x  : LW_RISCV_CSR_PMPCFG2_PMP8X_Field;
        Pmp8w  : LW_RISCV_CSR_PMPCFG2_PMP8W_Field;
        Pmp8r  : LW_RISCV_CSR_PMPCFG2_PMP8R_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_PMPCFG2_Register use record
        Pmp15l at 0 range 63 .. 63;
        Pmp15s at 0 range 61 .. 62;
        Pmp15a at 0 range 59 .. 60;
        Pmp15x at 0 range 58 .. 58;
        Pmp15w at 0 range 57 .. 57;
        Pmp15r at 0 range 56 .. 56;
        Pmp14l at 0 range 55 .. 55;
        Pmp14s at 0 range 53 .. 54;
        Pmp14a at 0 range 51 .. 52;
        Pmp14x at 0 range 50 .. 50;
        Pmp14w at 0 range 49 .. 49;
        Pmp14r at 0 range 48 .. 48;
        Pmp13l at 0 range 47 .. 47;
        Pmp13s at 0 range 45 .. 46;
        Pmp13a at 0 range 43 .. 44;
        Pmp13x at 0 range 42 .. 42;
        Pmp13w at 0 range 41 .. 41;
        Pmp13r at 0 range 40 .. 40;
        Pmp12l at 0 range 39 .. 39;
        Pmp12s at 0 range 37 .. 38;
        Pmp12a at 0 range 35 .. 36;
        Pmp12x at 0 range 34 .. 34;
        Pmp12w at 0 range 33 .. 33;
        Pmp12r at 0 range 32 .. 32;
        Pmp11l at 0 range 31 .. 31;
        Pmp11s at 0 range 29 .. 30;
        Pmp11a at 0 range 27 .. 28;
        Pmp11x at 0 range 26 .. 26;
        Pmp11w at 0 range 25 .. 25;
        Pmp11r at 0 range 24 .. 24;
        Pmp10l at 0 range 23 .. 23;
        Pmp10s at 0 range 21 .. 22;
        Pmp10a at 0 range 19 .. 20;
        Pmp10x at 0 range 18 .. 18;
        Pmp10w at 0 range 17 .. 17;
        Pmp10r at 0 range 16 .. 16;
        Pmp9l  at 0 range 15 .. 15;
        Pmp9s  at 0 range 13 .. 14;
        Pmp9a  at 0 range 11 .. 12;
        Pmp9x  at 0 range 10 .. 10;
        Pmp9w  at 0 range  9 ..  9;
        Pmp9r  at 0 range  8 ..  8;
        Pmp8l  at 0 range  7 ..  7;
        Pmp8s  at 0 range  5 ..  6;
        Pmp8a  at 0 range  3 ..  4;
        Pmp8x  at 0 range  2 ..  2;
        Pmp8w  at 0 range  1 ..  1;
        Pmp8r  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_PMPADDR_WIRI_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_PMPADDR_ADDR_RST : constant LwU62 := 16#0#;

    type LW_RISCV_CSR_PMPADDR_Register is record
        Wiri : LwU2;
        Addr : LwU62;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_PMPADDR_Register use record
        Wiri at 0 range 62 .. 63;
        Addr at 0 range  0 .. 61;
    end record;

    type LW_RISCV_CSR_MEXTPMPCFG0_PMP23L_Field is (PMP23L_UNLOCK, PMP23L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP23L_Field use (PMP23L_UNLOCK => 16#0#, PMP23L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP23S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP23A_Field is (PMP23A_OFF, PMP23A_TOR, PMP23A_NA4, PMP23A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP23A_Field use (PMP23A_OFF => 16#0#, PMP23A_TOR => 16#1#, PMP23A_NA4 => 16#2#, PMP23A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP23X_Field is (PMP23X_DENIED, PMP23X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP23X_Field use (PMP23X_DENIED => 16#0#, PMP23X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP23W_Field is (PMP23W_DENIED, PMP23W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP23W_Field use (PMP23W_DENIED => 16#0#, PMP23W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP23R_Field is (PMP23R_DENIED, PMP23R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP23R_Field use (PMP23R_DENIED => 16#0#, PMP23R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP22L_Field is (PMP22L_UNLOCK, PMP22L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP22L_Field use (PMP22L_UNLOCK => 16#0#, PMP22L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP22S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP22A_Field is (PMP22A_OFF, PMP22A_TOR, PMP22A_NA4, PMP22A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP22A_Field use (PMP22A_OFF => 16#0#, PMP22A_TOR => 16#1#, PMP22A_NA4 => 16#2#, PMP22A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP22X_Field is (PMP22X_DENIED, PMP22X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP22X_Field use (PMP22X_DENIED => 16#0#, PMP22X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP22W_Field is (PMP22W_DENIED, PMP22W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP22W_Field use (PMP22W_DENIED => 16#0#, PMP22W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP22R_Field is (PMP22R_DENIED, PMP22R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP22R_Field use (PMP22R_DENIED => 16#0#, PMP22R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP21L_Field is (PMP21L_UNLOCK, PMP21L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP21L_Field use (PMP21L_UNLOCK => 16#0#, PMP21L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP21S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP21A_Field is (PMP21A_OFF, PMP21A_TOR, PMP21A_NA4, PMP21A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP21A_Field use (PMP21A_OFF => 16#0#, PMP21A_TOR => 16#1#, PMP21A_NA4 => 16#2#, PMP21A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP21X_Field is (PMP21X_DENIED, PMP21X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP21X_Field use (PMP21X_DENIED => 16#0#, PMP21X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP21W_Field is (PMP21W_DENIED, PMP21W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP21W_Field use (PMP21W_DENIED => 16#0#, PMP21W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP21R_Field is (PMP21R_DENIED, PMP21R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP21R_Field use (PMP21R_DENIED => 16#0#, PMP21R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP20L_Field is (PMP20L_UNLOCK, PMP20L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP20L_Field use (PMP20L_UNLOCK => 16#0#, PMP20L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP20S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP20A_Field is (PMP20A_OFF, PMP20A_TOR, PMP20A_NA4, PMP20A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP20A_Field use (PMP20A_OFF => 16#0#, PMP20A_TOR => 16#1#, PMP20A_NA4 => 16#2#, PMP20A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP20X_Field is (PMP20X_DENIED, PMP20X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP20X_Field use (PMP20X_DENIED => 16#0#, PMP20X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP20W_Field is (PMP20W_DENIED, PMP20W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP20W_Field use (PMP20W_DENIED => 16#0#, PMP20W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP20R_Field is (PMP20R_DENIED, PMP20R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP20R_Field use (PMP20R_DENIED => 16#0#, PMP20R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP19L_Field is (PMP19L_UNLOCK, PMP19L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP19L_Field use (PMP19L_UNLOCK => 16#0#, PMP19L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP19S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP19A_Field is (PMP19A_OFF, PMP19A_TOR, PMP19A_NA4, PMP19A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP19A_Field use (PMP19A_OFF => 16#0#, PMP19A_TOR => 16#1#, PMP19A_NA4 => 16#2#, PMP19A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP19X_Field is (PMP19X_DENIED, PMP19X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP19X_Field use (PMP19X_DENIED => 16#0#, PMP19X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP19W_Field is (PMP19W_DENIED, PMP19W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP19W_Field use (PMP19W_DENIED => 16#0#, PMP19W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP19R_Field is (PMP19R_DENIED, PMP19R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP19R_Field use (PMP19R_DENIED => 16#0#, PMP19R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP18L_Field is (PMP18L_UNLOCK, PMP18L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP18L_Field use (PMP18L_UNLOCK => 16#0#, PMP18L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP18S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP18A_Field is (PMP18A_OFF, PMP18A_TOR, PMP18A_NA4, PMP18A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP18A_Field use (PMP18A_OFF => 16#0#, PMP18A_TOR => 16#1#, PMP18A_NA4 => 16#2#, PMP18A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP18X_Field is (PMP18X_DENIED, PMP18X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP18X_Field use (PMP18X_DENIED => 16#0#, PMP18X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP18W_Field is (PMP18W_DENIED, PMP18W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP18W_Field use (PMP18W_DENIED => 16#0#, PMP18W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP18R_Field is (PMP18R_DENIED, PMP18R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP18R_Field use (PMP18R_DENIED => 16#0#, PMP18R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP17L_Field is (PMP17L_UNLOCK, PMP17L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP17L_Field use (PMP17L_UNLOCK => 16#0#, PMP17L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP17S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP17A_Field is (PMP17A_OFF, PMP17A_TOR, PMP17A_NA4, PMP17A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP17A_Field use (PMP17A_OFF => 16#0#, PMP17A_TOR => 16#1#, PMP17A_NA4 => 16#2#, PMP17A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP17X_Field is (PMP17X_DENIED, PMP17X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP17X_Field use (PMP17X_DENIED => 16#0#, PMP17X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP17W_Field is (PMP17W_DENIED, PMP17W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP17W_Field use (PMP17W_DENIED => 16#0#, PMP17W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP17R_Field is (PMP17R_DENIED, PMP17R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP17R_Field use (PMP17R_DENIED => 16#0#, PMP17R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP16L_Field is (PMP16L_UNLOCK, PMP16L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP16L_Field use (PMP16L_UNLOCK => 16#0#, PMP16L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG0_PMP16S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP16A_Field is (PMP16A_OFF, PMP16A_TOR, PMP16A_NA4, PMP16A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP16A_Field use (PMP16A_OFF => 16#0#, PMP16A_TOR => 16#1#, PMP16A_NA4 => 16#2#, PMP16A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP16X_Field is (PMP16X_DENIED, PMP16X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP16X_Field use (PMP16X_DENIED => 16#0#, PMP16X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP16W_Field is (PMP16W_DENIED, PMP16W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP16W_Field use (PMP16W_DENIED => 16#0#, PMP16W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG0_PMP16R_Field is (PMP16R_DENIED, PMP16R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG0_PMP16R_Field use (PMP16R_DENIED => 16#0#, PMP16R_PERMITTED => 16#1#);

    type LW_RISCV_CSR_MEXTPMPCFG0_Register is record
        Pmp23l : LW_RISCV_CSR_MEXTPMPCFG0_PMP23L_Field;
        Pmp23s : LwU2;
        Pmp23a : LW_RISCV_CSR_MEXTPMPCFG0_PMP23A_Field;
        Pmp23x : LW_RISCV_CSR_MEXTPMPCFG0_PMP23X_Field;
        Pmp23w : LW_RISCV_CSR_MEXTPMPCFG0_PMP23W_Field;
        Pmp23r : LW_RISCV_CSR_MEXTPMPCFG0_PMP23R_Field;
        Pmp22l : LW_RISCV_CSR_MEXTPMPCFG0_PMP22L_Field;
        Pmp22s : LwU2;
        Pmp22a : LW_RISCV_CSR_MEXTPMPCFG0_PMP22A_Field;
        Pmp22x : LW_RISCV_CSR_MEXTPMPCFG0_PMP22X_Field;
        Pmp22w : LW_RISCV_CSR_MEXTPMPCFG0_PMP22W_Field;
        Pmp22r : LW_RISCV_CSR_MEXTPMPCFG0_PMP22R_Field;
        Pmp21l : LW_RISCV_CSR_MEXTPMPCFG0_PMP21L_Field;
        Pmp21s : LwU2;
        Pmp21a : LW_RISCV_CSR_MEXTPMPCFG0_PMP21A_Field;
        Pmp21x : LW_RISCV_CSR_MEXTPMPCFG0_PMP21X_Field;
        Pmp21w : LW_RISCV_CSR_MEXTPMPCFG0_PMP21W_Field;
        Pmp21r : LW_RISCV_CSR_MEXTPMPCFG0_PMP21R_Field;
        Pmp20l : LW_RISCV_CSR_MEXTPMPCFG0_PMP20L_Field;
        Pmp20s : LwU2;
        Pmp20a : LW_RISCV_CSR_MEXTPMPCFG0_PMP20A_Field;
        Pmp20x : LW_RISCV_CSR_MEXTPMPCFG0_PMP20X_Field;
        Pmp20w : LW_RISCV_CSR_MEXTPMPCFG0_PMP20W_Field;
        Pmp20r : LW_RISCV_CSR_MEXTPMPCFG0_PMP20R_Field;
        Pmp19l : LW_RISCV_CSR_MEXTPMPCFG0_PMP19L_Field;
        Pmp19s : LwU2;
        Pmp19a : LW_RISCV_CSR_MEXTPMPCFG0_PMP19A_Field;
        Pmp19x : LW_RISCV_CSR_MEXTPMPCFG0_PMP19X_Field;
        Pmp19w : LW_RISCV_CSR_MEXTPMPCFG0_PMP19W_Field;
        Pmp19r : LW_RISCV_CSR_MEXTPMPCFG0_PMP19R_Field;
        Pmp18l : LW_RISCV_CSR_MEXTPMPCFG0_PMP18L_Field;
        Pmp18s : LwU2;
        Pmp18a : LW_RISCV_CSR_MEXTPMPCFG0_PMP18A_Field;
        Pmp18x : LW_RISCV_CSR_MEXTPMPCFG0_PMP18X_Field;
        Pmp18w : LW_RISCV_CSR_MEXTPMPCFG0_PMP18W_Field;
        Pmp18r : LW_RISCV_CSR_MEXTPMPCFG0_PMP18R_Field;
        Pmp17l : LW_RISCV_CSR_MEXTPMPCFG0_PMP17L_Field;
        Pmp17s : LwU2;
        Pmp17a : LW_RISCV_CSR_MEXTPMPCFG0_PMP17A_Field;
        Pmp17x : LW_RISCV_CSR_MEXTPMPCFG0_PMP17X_Field;
        Pmp17w : LW_RISCV_CSR_MEXTPMPCFG0_PMP17W_Field;
        Pmp17r : LW_RISCV_CSR_MEXTPMPCFG0_PMP17R_Field;
        Pmp16l : LW_RISCV_CSR_MEXTPMPCFG0_PMP16L_Field;
        Pmp16s : LwU2;
        Pmp16a : LW_RISCV_CSR_MEXTPMPCFG0_PMP16A_Field;
        Pmp16x : LW_RISCV_CSR_MEXTPMPCFG0_PMP16X_Field;
        Pmp16w : LW_RISCV_CSR_MEXTPMPCFG0_PMP16W_Field;
        Pmp16r : LW_RISCV_CSR_MEXTPMPCFG0_PMP16R_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MEXTPMPCFG0_Register use record
        Pmp23l at 0 range 63 .. 63;
        Pmp23s at 0 range 61 .. 62;
        Pmp23a at 0 range 59 .. 60;
        Pmp23x at 0 range 58 .. 58;
        Pmp23w at 0 range 57 .. 57;
        Pmp23r at 0 range 56 .. 56;
        Pmp22l at 0 range 55 .. 55;
        Pmp22s at 0 range 53 .. 54;
        Pmp22a at 0 range 51 .. 52;
        Pmp22x at 0 range 50 .. 50;
        Pmp22w at 0 range 49 .. 49;
        Pmp22r at 0 range 48 .. 48;
        Pmp21l at 0 range 47 .. 47;
        Pmp21s at 0 range 45 .. 46;
        Pmp21a at 0 range 43 .. 44;
        Pmp21x at 0 range 42 .. 42;
        Pmp21w at 0 range 41 .. 41;
        Pmp21r at 0 range 40 .. 40;
        Pmp20l at 0 range 39 .. 39;
        Pmp20s at 0 range 37 .. 38;
        Pmp20a at 0 range 35 .. 36;
        Pmp20x at 0 range 34 .. 34;
        Pmp20w at 0 range 33 .. 33;
        Pmp20r at 0 range 32 .. 32;
        Pmp19l at 0 range 31 .. 31;
        Pmp19s at 0 range 29 .. 30;
        Pmp19a at 0 range 27 .. 28;
        Pmp19x at 0 range 26 .. 26;
        Pmp19w at 0 range 25 .. 25;
        Pmp19r at 0 range 24 .. 24;
        Pmp18l at 0 range 23 .. 23;
        Pmp18s at 0 range 21 .. 22;
        Pmp18a at 0 range 19 .. 20;
        Pmp18x at 0 range 18 .. 18;
        Pmp18w at 0 range 17 .. 17;
        Pmp18r at 0 range 16 .. 16;
        Pmp17l at 0 range 15 .. 15;
        Pmp17s at 0 range 13 .. 14;
        Pmp17a at 0 range 11 .. 12;
        Pmp17x at 0 range 10 .. 10;
        Pmp17w at 0 range  9 ..  9;
        Pmp17r at 0 range  8 ..  8;
        Pmp16l at 0 range  7 ..  7;
        Pmp16s at 0 range  5 ..  6;
        Pmp16a at 0 range  3 ..  4;
        Pmp16x at 0 range  2 ..  2;
        Pmp16w at 0 range  1 ..  1;
        Pmp16r at 0 range  0 ..  0;
    end record;

    type LW_RISCV_CSR_MEXTPMPCFG2_PMP31L_Field is (PMP31L_UNLOCK, PMP31L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP31L_Field use (PMP31L_UNLOCK => 16#0#, PMP31L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP31S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP31A_Field is (PMP31A_OFF, PMP31A_TOR, PMP31A_NA4, PMP31A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP31A_Field use (PMP31A_OFF => 16#0#, PMP31A_TOR => 16#1#, PMP31A_NA4 => 16#2#, PMP31A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP31X_Field is (PMP31X_DENIED, PMP31X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP31X_Field use (PMP31X_DENIED => 16#0#, PMP31X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP31W_Field is (PMP31W_DENIED, PMP31W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP31W_Field use (PMP31W_DENIED => 16#0#, PMP31W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP31R_Field is (PMP31R_DENIED, PMP31R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP31R_Field use (PMP31R_DENIED => 16#0#, PMP31R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP30L_Field is (PMP30L_UNLOCK, PMP30L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP30L_Field use (PMP30L_UNLOCK => 16#0#, PMP30L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP30S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP30A_Field is (PMP30A_OFF, PMP30A_TOR, PMP30A_NA4, PMP30A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP30A_Field use (PMP30A_OFF => 16#0#, PMP30A_TOR => 16#1#, PMP30A_NA4 => 16#2#, PMP30A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP30X_Field is (PMP30X_DENIED, PMP30X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP30X_Field use (PMP30X_DENIED => 16#0#, PMP30X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP30W_Field is (PMP30W_DENIED, PMP30W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP30W_Field use (PMP30W_DENIED => 16#0#, PMP30W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP30R_Field is (PMP30R_DENIED, PMP30R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP30R_Field use (PMP30R_DENIED => 16#0#, PMP30R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP29L_Field is (PMP29L_UNLOCK, PMP29L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP29L_Field use (PMP29L_UNLOCK => 16#0#, PMP29L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP29S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP29A_Field is (PMP29A_OFF, PMP29A_TOR, PMP29A_NA4, PMP29A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP29A_Field use (PMP29A_OFF => 16#0#, PMP29A_TOR => 16#1#, PMP29A_NA4 => 16#2#, PMP29A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP29X_Field is (PMP29X_DENIED, PMP29X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP29X_Field use (PMP29X_DENIED => 16#0#, PMP29X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP29W_Field is (PMP29W_DENIED, PMP29W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP29W_Field use (PMP29W_DENIED => 16#0#, PMP29W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP29R_Field is (PMP29R_DENIED, PMP29R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP29R_Field use (PMP29R_DENIED => 16#0#, PMP29R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP28L_Field is (PMP28L_UNLOCK, PMP28L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP28L_Field use (PMP28L_UNLOCK => 16#0#, PMP28L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP28S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP28A_Field is (PMP28A_OFF, PMP28A_TOR, PMP28A_NA4, PMP28A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP28A_Field use (PMP28A_OFF => 16#0#, PMP28A_TOR => 16#1#, PMP28A_NA4 => 16#2#, PMP28A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP28X_Field is (PMP28X_DENIED, PMP28X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP28X_Field use (PMP28X_DENIED => 16#0#, PMP28X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP28W_Field is (PMP28W_DENIED, PMP28W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP28W_Field use (PMP28W_DENIED => 16#0#, PMP28W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP28R_Field is (PMP28R_DENIED, PMP28R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP28R_Field use (PMP28R_DENIED => 16#0#, PMP28R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP27L_Field is (PMP27L_UNLOCK, PMP27L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP27L_Field use (PMP27L_UNLOCK => 16#0#, PMP27L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP27S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP27A_Field is (PMP27A_OFF, PMP27A_TOR, PMP27A_NA4, PMP27A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP27A_Field use (PMP27A_OFF => 16#0#, PMP27A_TOR => 16#1#, PMP27A_NA4 => 16#2#, PMP27A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP27X_Field is (PMP27X_DENIED, PMP27X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP27X_Field use (PMP27X_DENIED => 16#0#, PMP27X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP27W_Field is (PMP27W_DENIED, PMP27W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP27W_Field use (PMP27W_DENIED => 16#0#, PMP27W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP27R_Field is (PMP27R_DENIED, PMP27R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP27R_Field use (PMP27R_DENIED => 16#0#, PMP27R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP26L_Field is (PMP26L_UNLOCK, PMP26L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP26L_Field use (PMP26L_UNLOCK => 16#0#, PMP26L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP26S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP26A_Field is (PMP26A_OFF, PMP26A_TOR, PMP26A_NA4, PMP26A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP26A_Field use (PMP26A_OFF => 16#0#, PMP26A_TOR => 16#1#, PMP26A_NA4 => 16#2#, PMP26A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP26X_Field is (PMP26X_DENIED, PMP26X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP26X_Field use (PMP26X_DENIED => 16#0#, PMP26X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP26W_Field is (PMP26W_DENIED, PMP26W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP26W_Field use (PMP26W_DENIED => 16#0#, PMP26W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP26R_Field is (PMP26R_DENIED, PMP26R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP26R_Field use (PMP26R_DENIED => 16#0#, PMP26R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP25L_Field is (PMP25L_UNLOCK, PMP25L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP25L_Field use (PMP25L_UNLOCK => 16#0#, PMP25L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP25S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP25A_Field is (PMP25A_OFF, PMP25A_TOR, PMP25A_NA4, PMP25A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP25A_Field use (PMP25A_OFF => 16#0#, PMP25A_TOR => 16#1#, PMP25A_NA4 => 16#2#, PMP25A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP25X_Field is (PMP25X_DENIED, PMP25X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP25X_Field use (PMP25X_DENIED => 16#0#, PMP25X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP25W_Field is (PMP25W_DENIED, PMP25W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP25W_Field use (PMP25W_DENIED => 16#0#, PMP25W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP25R_Field is (PMP25R_DENIED, PMP25R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP25R_Field use (PMP25R_DENIED => 16#0#, PMP25R_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP24L_Field is (PMP24L_UNLOCK, PMP24L_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP24L_Field use (PMP24L_UNLOCK => 16#0#, PMP24L_LOCK => 16#1#);
    LW_RISCV_CSR_MEXTPMPCFG2_PMP24S_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP24A_Field is (PMP24A_OFF, PMP24A_TOR, PMP24A_NA4, PMP24A_NAPOT) with
        Size => 2;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP24A_Field use (PMP24A_OFF => 16#0#, PMP24A_TOR => 16#1#, PMP24A_NA4 => 16#2#, PMP24A_NAPOT => 16#3#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP24X_Field is (PMP24X_DENIED, PMP24X_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP24X_Field use (PMP24X_DENIED => 16#0#, PMP24X_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP24W_Field is (PMP24W_DENIED, PMP24W_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP24W_Field use (PMP24W_DENIED => 16#0#, PMP24W_PERMITTED => 16#1#);
    type LW_RISCV_CSR_MEXTPMPCFG2_PMP24R_Field is (PMP24R_DENIED, PMP24R_PERMITTED) with
        Size => 1;
    for LW_RISCV_CSR_MEXTPMPCFG2_PMP24R_Field use (PMP24R_DENIED => 16#0#, PMP24R_PERMITTED => 16#1#);

    type LW_RISCV_CSR_MEXTPMPCFG2_Register is record
        Pmp31l : LW_RISCV_CSR_MEXTPMPCFG2_PMP31L_Field;
        Pmp31s : LwU2;
        Pmp31a : LW_RISCV_CSR_MEXTPMPCFG2_PMP31A_Field;
        Pmp31x : LW_RISCV_CSR_MEXTPMPCFG2_PMP31X_Field;
        Pmp31w : LW_RISCV_CSR_MEXTPMPCFG2_PMP31W_Field;
        Pmp31r : LW_RISCV_CSR_MEXTPMPCFG2_PMP31R_Field;
        Pmp30l : LW_RISCV_CSR_MEXTPMPCFG2_PMP30L_Field;
        Pmp30s : LwU2;
        Pmp30a : LW_RISCV_CSR_MEXTPMPCFG2_PMP30A_Field;
        Pmp30x : LW_RISCV_CSR_MEXTPMPCFG2_PMP30X_Field;
        Pmp30w : LW_RISCV_CSR_MEXTPMPCFG2_PMP30W_Field;
        Pmp30r : LW_RISCV_CSR_MEXTPMPCFG2_PMP30R_Field;
        Pmp29l : LW_RISCV_CSR_MEXTPMPCFG2_PMP29L_Field;
        Pmp29s : LwU2;
        Pmp29a : LW_RISCV_CSR_MEXTPMPCFG2_PMP29A_Field;
        Pmp29x : LW_RISCV_CSR_MEXTPMPCFG2_PMP29X_Field;
        Pmp29w : LW_RISCV_CSR_MEXTPMPCFG2_PMP29W_Field;
        Pmp29r : LW_RISCV_CSR_MEXTPMPCFG2_PMP29R_Field;
        Pmp28l : LW_RISCV_CSR_MEXTPMPCFG2_PMP28L_Field;
        Pmp28s : LwU2;
        Pmp28a : LW_RISCV_CSR_MEXTPMPCFG2_PMP28A_Field;
        Pmp28x : LW_RISCV_CSR_MEXTPMPCFG2_PMP28X_Field;
        Pmp28w : LW_RISCV_CSR_MEXTPMPCFG2_PMP28W_Field;
        Pmp28r : LW_RISCV_CSR_MEXTPMPCFG2_PMP28R_Field;
        Pmp27l : LW_RISCV_CSR_MEXTPMPCFG2_PMP27L_Field;
        Pmp27s : LwU2;
        Pmp27a : LW_RISCV_CSR_MEXTPMPCFG2_PMP27A_Field;
        Pmp27x : LW_RISCV_CSR_MEXTPMPCFG2_PMP27X_Field;
        Pmp27w : LW_RISCV_CSR_MEXTPMPCFG2_PMP27W_Field;
        Pmp27r : LW_RISCV_CSR_MEXTPMPCFG2_PMP27R_Field;
        Pmp26l : LW_RISCV_CSR_MEXTPMPCFG2_PMP26L_Field;
        Pmp26s : LwU2;
        Pmp26a : LW_RISCV_CSR_MEXTPMPCFG2_PMP26A_Field;
        Pmp26x : LW_RISCV_CSR_MEXTPMPCFG2_PMP26X_Field;
        Pmp26w : LW_RISCV_CSR_MEXTPMPCFG2_PMP26W_Field;
        Pmp26r : LW_RISCV_CSR_MEXTPMPCFG2_PMP26R_Field;
        Pmp25l : LW_RISCV_CSR_MEXTPMPCFG2_PMP25L_Field;
        Pmp25s : LwU2;
        Pmp25a : LW_RISCV_CSR_MEXTPMPCFG2_PMP25A_Field;
        Pmp25x : LW_RISCV_CSR_MEXTPMPCFG2_PMP25X_Field;
        Pmp25w : LW_RISCV_CSR_MEXTPMPCFG2_PMP25W_Field;
        Pmp25r : LW_RISCV_CSR_MEXTPMPCFG2_PMP25R_Field;
        Pmp24l : LW_RISCV_CSR_MEXTPMPCFG2_PMP24L_Field;
        Pmp24s : LwU2;
        Pmp24a : LW_RISCV_CSR_MEXTPMPCFG2_PMP24A_Field;
        Pmp24x : LW_RISCV_CSR_MEXTPMPCFG2_PMP24X_Field;
        Pmp24w : LW_RISCV_CSR_MEXTPMPCFG2_PMP24W_Field;
        Pmp24r : LW_RISCV_CSR_MEXTPMPCFG2_PMP24R_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MEXTPMPCFG2_Register use record
        Pmp31l at 0 range 63 .. 63;
        Pmp31s at 0 range 61 .. 62;
        Pmp31a at 0 range 59 .. 60;
        Pmp31x at 0 range 58 .. 58;
        Pmp31w at 0 range 57 .. 57;
        Pmp31r at 0 range 56 .. 56;
        Pmp30l at 0 range 55 .. 55;
        Pmp30s at 0 range 53 .. 54;
        Pmp30a at 0 range 51 .. 52;
        Pmp30x at 0 range 50 .. 50;
        Pmp30w at 0 range 49 .. 49;
        Pmp30r at 0 range 48 .. 48;
        Pmp29l at 0 range 47 .. 47;
        Pmp29s at 0 range 45 .. 46;
        Pmp29a at 0 range 43 .. 44;
        Pmp29x at 0 range 42 .. 42;
        Pmp29w at 0 range 41 .. 41;
        Pmp29r at 0 range 40 .. 40;
        Pmp28l at 0 range 39 .. 39;
        Pmp28s at 0 range 37 .. 38;
        Pmp28a at 0 range 35 .. 36;
        Pmp28x at 0 range 34 .. 34;
        Pmp28w at 0 range 33 .. 33;
        Pmp28r at 0 range 32 .. 32;
        Pmp27l at 0 range 31 .. 31;
        Pmp27s at 0 range 29 .. 30;
        Pmp27a at 0 range 27 .. 28;
        Pmp27x at 0 range 26 .. 26;
        Pmp27w at 0 range 25 .. 25;
        Pmp27r at 0 range 24 .. 24;
        Pmp26l at 0 range 23 .. 23;
        Pmp26s at 0 range 21 .. 22;
        Pmp26a at 0 range 19 .. 20;
        Pmp26x at 0 range 18 .. 18;
        Pmp26w at 0 range 17 .. 17;
        Pmp26r at 0 range 16 .. 16;
        Pmp25l at 0 range 15 .. 15;
        Pmp25s at 0 range 13 .. 14;
        Pmp25a at 0 range 11 .. 12;
        Pmp25x at 0 range 10 .. 10;
        Pmp25w at 0 range  9 ..  9;
        Pmp25r at 0 range  8 ..  8;
        Pmp24l at 0 range  7 ..  7;
        Pmp24s at 0 range  5 ..  6;
        Pmp24a at 0 range  3 ..  4;
        Pmp24x at 0 range  2 ..  2;
        Pmp24w at 0 range  1 ..  1;
        Pmp24r at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MEXTPMPADDR_WIRI_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_MEXTPMPADDR_ADDR_RST : constant LwU62 := 16#0#;

    type LW_RISCV_CSR_MEXTPMPADDR_Register is record
        Wiri : LwU2;
        Addr : LwU62;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MEXTPMPADDR_Register use record
        Wiri at 0 range 62 .. 63;
        Addr at 0 range  0 .. 61;
    end record;

    LW_RISCV_CSR_MSCRATCH2_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MSCRATCH2_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MSCRATCH2_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MTIMECMP_TIME_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_MTIMECMP_Register is record
        Time : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MTIMECMP_Register use record
        Time at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MCFG_WPRI4_RST : constant LwU48 := 16#0#;
    type LW_RISCV_CSR_MCFG_MPOSTIO_Field is (MPOSTIO_FALSE, MPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_MPOSTIO_Field use (MPOSTIO_FALSE => 16#0#, MPOSTIO_TRUE => 16#1#);
    LW_RISCV_CSR_MCFG_WPRI5_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MCFG_SPOSTIO_Field is (SPOSTIO_FALSE, SPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_SPOSTIO_Field use (SPOSTIO_FALSE => 16#0#, SPOSTIO_TRUE => 16#1#);
    type LW_RISCV_CSR_MCFG_UPOSTIO_Field is (UPOSTIO_FALSE, UPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_UPOSTIO_Field use (UPOSTIO_FALSE => 16#0#, UPOSTIO_TRUE => 16#1#);
    type LW_RISCV_CSR_MCFG_MSBC_Field is (MSBC_FALSE, MSBC_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_MSBC_Field use (MSBC_FALSE => 16#0#, MSBC_TRUE => 16#1#);
    LW_RISCV_CSR_MCFG_WPRI3_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MCFG_SSBC_Field is (SSBC_FALSE, SSBC_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_SSBC_Field use (SSBC_FALSE => 16#0#, SSBC_TRUE => 16#1#);
    type LW_RISCV_CSR_MCFG_USBC_Field is (USBC_FALSE, USBC_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_USBC_Field use (USBC_FALSE => 16#0#, USBC_TRUE => 16#1#);
    LW_RISCV_CSR_MCFG_WPRI2_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MCFG_SSSE_Field is (SSSE_FALSE, SSSE_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_SSSE_Field use (SSSE_FALSE => 16#0#, SSSE_TRUE => 16#1#);
    type LW_RISCV_CSR_MCFG_USSE_Field is (USSE_FALSE, USSE_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_USSE_Field use (USSE_FALSE => 16#0#, USSE_TRUE => 16#1#);
    type LW_RISCV_CSR_MCFG_MWFP_DIS_Field is (DIS_FALSE, DIS_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_MWFP_DIS_Field use (DIS_FALSE => 16#0#, DIS_TRUE => 16#1#);
    LW_RISCV_CSR_MCFG_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MCFG_SWFP_DIS_Field is (DIS_FALSE, DIS_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MCFG_SWFP_DIS_Field use (DIS_FALSE => 16#0#, DIS_TRUE => 16#1#);
    LW_RISCV_CSR_MCFG_UWFP_DIS_FALSE : constant LwU1 := 16#0#;

    type LW_RISCV_CSR_MCFG_Register is record
        Wpri4    : LwU48;
        Mpostio  : LW_RISCV_CSR_MCFG_MPOSTIO_Field;
        Wpri5    : LwU1;
        Spostio  : LW_RISCV_CSR_MCFG_SPOSTIO_Field;
        Upostio  : LW_RISCV_CSR_MCFG_UPOSTIO_Field;
        Msbc     : LW_RISCV_CSR_MCFG_MSBC_Field;
        Wpri3    : LwU1;
        Ssbc     : LW_RISCV_CSR_MCFG_SSBC_Field;
        Usbc     : LW_RISCV_CSR_MCFG_USBC_Field;
        Wpri2    : LwU2;
        Ssse     : LW_RISCV_CSR_MCFG_SSSE_Field;
        Usse     : LW_RISCV_CSR_MCFG_USSE_Field;
        Mwfp_Dis : LW_RISCV_CSR_MCFG_MWFP_DIS_Field;
        Wpri1    : LwU1;
        Swfp_Dis : LW_RISCV_CSR_MCFG_SWFP_DIS_Field;
        Uwfp_Dis : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MCFG_Register use record
        Wpri4    at 0 range 16 .. 63;
        Mpostio  at 0 range 15 .. 15;
        Wpri5    at 0 range 14 .. 14;
        Spostio  at 0 range 13 .. 13;
        Upostio  at 0 range 12 .. 12;
        Msbc     at 0 range 11 .. 11;
        Wpri3    at 0 range 10 .. 10;
        Ssbc     at 0 range  9 ..  9;
        Usbc     at 0 range  8 ..  8;
        Wpri2    at 0 range  6 ..  7;
        Ssse     at 0 range  5 ..  5;
        Usse     at 0 range  4 ..  4;
        Mwfp_Dis at 0 range  3 ..  3;
        Wpri1    at 0 range  2 ..  2;
        Swfp_Dis at 0 range  1 ..  1;
        Uwfp_Dis at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_MBPCFG_WPRI2_RST       : constant LwU42 := 16#0#;
    LW_RISCV_CSR_MBPCFG_RAS_FLUSH_TRUE  : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MBPCFG_BHT_FLUSH_TRUE  : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MBPCFG_BTB_FLUSHM_TRUE : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MBPCFG_WPRI1_RST       : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MBPCFG_BTB_FLUSHS_TRUE : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MBPCFG_BTB_FLUSHU_TRUE : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MBPCFG_WPRI0_RST       : constant LwU13 := 16#0#;
    type LW_RISCV_CSR_MBPCFG_RAS_EN_Field is (EN_FALSE, EN_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MBPCFG_RAS_EN_Field use (EN_FALSE => 16#0#, EN_TRUE => 16#1#);
    type LW_RISCV_CSR_MBPCFG_BHT_EN_Field is (EN_FALSE, EN_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MBPCFG_BHT_EN_Field use (EN_FALSE => 16#0#, EN_TRUE => 16#1#);
    type LW_RISCV_CSR_MBPCFG_BTB_EN_Field is (EN_FALSE, EN_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MBPCFG_BTB_EN_Field use (EN_FALSE => 16#0#, EN_TRUE => 16#1#);

    type LW_RISCV_CSR_MBPCFG_Register is record
        Wpri2      : LwU42;
        Ras_Flush  : LwU1;
        Bht_Flush  : LwU1;
        Btb_Flushm : LwU1;
        Wpri1      : LwU1;
        Btb_Flushs : LwU1;
        Btb_Flushu : LwU1;
        Wpri0      : LwU13;
        Ras_En     : LW_RISCV_CSR_MBPCFG_RAS_EN_Field;
        Bht_En     : LW_RISCV_CSR_MBPCFG_BHT_EN_Field;
        Btb_En     : LW_RISCV_CSR_MBPCFG_BTB_EN_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MBPCFG_Register use record
        Wpri2      at 0 range 22 .. 63;
        Ras_Flush  at 0 range 21 .. 21;
        Bht_Flush  at 0 range 20 .. 20;
        Btb_Flushm at 0 range 19 .. 19;
        Wpri1      at 0 range 18 .. 18;
        Btb_Flushs at 0 range 17 .. 17;
        Btb_Flushu at 0 range 16 .. 16;
        Wpri0      at 0 range  3 .. 15;
        Ras_En     at 0 range  2 ..  2;
        Bht_En     at 0 range  1 ..  1;
        Btb_En     at 0 range  0 ..  0;
    end record;

    type LW_RISCV_CSR_MDCACHEOP_ADDR_MODE_Field is (MODE_VA, MODE_PA) with
        Size => 1;
    for LW_RISCV_CSR_MDCACHEOP_ADDR_MODE_Field use (MODE_VA => 16#0#, MODE_PA => 16#1#);
    type LW_RISCV_CSR_MDCACHEOP_MODE_Field is (MODE_ILW_ALL, MODE_ILW_LINE, MODE_CLR_ALL, MODE_CLR_LINE) with
        Size => 2;
    for LW_RISCV_CSR_MDCACHEOP_MODE_Field use (MODE_ILW_ALL => 16#0#, MODE_ILW_LINE => 16#1#, MODE_CLR_ALL => 16#2#, MODE_CLR_LINE => 16#3#);

    type LW_RISCV_CSR_MDCACHEOP_Register is record
        Addr      : LwU58;
        Rs0       : LwU3;
        Addr_Mode : LW_RISCV_CSR_MDCACHEOP_ADDR_MODE_Field;
        Mode      : LW_RISCV_CSR_MDCACHEOP_MODE_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MDCACHEOP_Register use record
        Addr      at 0 range 6 .. 63;
        Rs0       at 0 range 3 ..  5;
        Addr_Mode at 0 range 2 ..  2;
        Mode      at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_MMPUCTL_WPRI1_RST           : constant LwU39 := 16#0#;
    LW_RISCV_CSR_MMPUCTL_HASH_LWMPU_EN_RST   : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MMPUCTL_HASH_LWMPU_EN_FALSE : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MMPUCTL_HASH_LWMPU_EN_TRUE  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MMPUCTL_ENTRY_COUNT_RST     : constant LwU8  := 16#80#;
    LW_RISCV_CSR_MMPUCTL_WPRI0_RST           : constant LwU9  := 16#0#;
    LW_RISCV_CSR_MMPUCTL_START_INDEX_RST     : constant LwU7  := 16#0#;

    type LW_RISCV_CSR_MMPUCTL_Register is record
        Wpri1         : LwU39;
        Hash_Lwmpu_En : LwU1;
        Entry_Count   : LwU8;
        Wpri0         : LwU9;
        Start_Index   : LwU7;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MMPUCTL_Register use record
        Wpri1         at 0 range 25 .. 63;
        Hash_Lwmpu_En at 0 range 24 .. 24;
        Entry_Count   at 0 range 16 .. 23;
        Wpri0         at 0 range  7 .. 15;
        Start_Index   at 0 range  0 ..  6;
    end record;

    LW_RISCV_CSR_MLWRRUID_RS0_RST      : constant LwU32 := 16#0#;
    LW_RISCV_CSR_MLWRRUID_MLWRRUID_RST : constant LwU8  := 16#0#;
    LW_RISCV_CSR_MLWRRUID_WPRI0_RST    : constant LwU8  := 16#0#;
    LW_RISCV_CSR_MLWRRUID_SLWRRUID_RST : constant LwU8  := 16#0#;
    LW_RISCV_CSR_MLWRRUID_ULWRRUID_RST : constant LwU8  := 16#0#;

    type LW_RISCV_CSR_MLWRRUID_Register is record
        Rs0      : LwU32;
        Mlwrruid : LwU8;
        Wpri0    : LwU8;
        Slwrruid : LwU8;
        Ulwrruid : LwU8;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MLWRRUID_Register use record
        Rs0      at 0 range 32 .. 63;
        Mlwrruid at 0 range 24 .. 31;
        Wpri0    at 0 range 16 .. 23;
        Slwrruid at 0 range  8 .. 15;
        Ulwrruid at 0 range  0 ..  7;
    end record;

    LW_RISCV_CSR_MOPT_CMD_HALT : constant LwU5 := 16#0#;

    type LW_RISCV_CSR_MOPT_Register is record
        Wpri : LwU59;
        Cmd  : LwU5;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MOPT_Register use record
        Wpri at 0 range 5 .. 63;
        Cmd  at 0 range 0 ..  4;
    end record;

    LW_RISCV_CSR_MMISCOPEN_WPRI_RST : constant LwU63 := 16#0#;
    type LW_RISCV_CSR_MMISCOPEN_DCACHEOP_Field is (DCACHEOP_DISABLE, DCACHEOP_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MMISCOPEN_DCACHEOP_Field use (DCACHEOP_DISABLE => 16#0#, DCACHEOP_ENABLE => 16#1#);

    type LW_RISCV_CSR_MMISCOPEN_Register is record
        Wpri     : LwU63;
        Dcacheop : LW_RISCV_CSR_MMISCOPEN_DCACHEOP_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MMISCOPEN_Register use record
        Wpri     at 0 range 1 .. 63;
        Dcacheop at 0 range 0 ..  0;
    end record;

    LW_RISCV_CSR_MLDSTATTR_RS0_RST       : constant LwU34 := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_VPR_RST       : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_WPR_RST       : constant LwU5  := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_L2C_RD_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_L2C_WR_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_COHERENT_RST  : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MLDSTATTR_CACHEABLE_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MLDSTATTR_RS1_RST       : constant LwU18 := 16#0#;

    type LW_RISCV_CSR_MLDSTATTR_Register is record
        Rs0       : LwU34;
        Vpr       : LwU1;
        Wpr       : LwU5;
        L2c_Rd    : LwU2;
        L2c_Wr    : LwU2;
        Coherent  : LwU1;
        Cacheable : LwU1;
        Rs1       : LwU18;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MLDSTATTR_Register use record
        Rs0       at 0 range 30 .. 63;
        Vpr       at 0 range 29 .. 29;
        Wpr       at 0 range 24 .. 28;
        L2c_Rd    at 0 range 22 .. 23;
        L2c_Wr    at 0 range 20 .. 21;
        Coherent  at 0 range 19 .. 19;
        Cacheable at 0 range 18 .. 18;
        Rs1       at 0 range  0 .. 17;
    end record;

    LW_RISCV_CSR_MFETCHATTR_RS0_RST       : constant LwU34 := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_VPR_RST       : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_WPR_RST       : constant LwU5  := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_L2C_RD_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_L2C_WR_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_COHERENT_RST  : constant LwU1  := 16#1#;
    LW_RISCV_CSR_MFETCHATTR_CACHEABLE_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_MFETCHATTR_RS1_RST       : constant LwU18 := 16#0#;

    type LW_RISCV_CSR_MFETCHATTR_Register is record
        Rs0       : LwU34;
        Vpr       : LwU1;
        Wpr       : LwU5;
        L2c_Rd    : LwU2;
        L2c_Wr    : LwU2;
        Coherent  : LwU1;
        Cacheable : LwU1;
        Rs1       : LwU18;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MFETCHATTR_Register use record
        Rs0       at 0 range 30 .. 63;
        Vpr       at 0 range 29 .. 29;
        Wpr       at 0 range 24 .. 28;
        L2c_Rd    at 0 range 22 .. 23;
        L2c_Wr    at 0 range 20 .. 21;
        Coherent  at 0 range 19 .. 19;
        Cacheable at 0 range 18 .. 18;
        Rs1       at 0 range  0 .. 17;
    end record;

    LW_RISCV_CSR_FFLAGS_RS0_RST    : constant LwU59 := 16#0#;
    LW_RISCV_CSR_FFLAGS_FFLAGS_RST : constant LwU5  := 16#0#;

    type LW_RISCV_CSR_FFLAGS_Register is record
        Rs0    : LwU59;
        Fflags : LwU5;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_FFLAGS_Register use record
        Rs0    at 0 range 5 .. 63;
        Fflags at 0 range 0 ..  4;
    end record;

    LW_RISCV_CSR_FRM_RS0_RST : constant LwU61 := 16#0#;
    LW_RISCV_CSR_FRM_FRM_RST : constant LwU3  := 16#0#;

    type LW_RISCV_CSR_FRM_Register is record
        Rs0 : LwU61;
        Frm : LwU3;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_FRM_Register use record
        Rs0 at 0 range 3 .. 63;
        Frm at 0 range 0 ..  2;
    end record;

    LW_RISCV_CSR_FCSR_RS0_RST    : constant LwU56 := 16#0#;
    LW_RISCV_CSR_FCSR_FRM_RST    : constant LwU3  := 16#0#;
    LW_RISCV_CSR_FCSR_FFLAGS_RST : constant LwU5  := 16#0#;

    type LW_RISCV_CSR_FCSR_Register is record
        Rs0    : LwU56;
        Frm    : LwU3;
        Fflags : LwU5;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_FCSR_Register use record
        Rs0    at 0 range 8 .. 63;
        Frm    at 0 range 5 ..  7;
        Fflags at 0 range 0 ..  4;
    end record;

    LW_RISCV_CSR_HPMCOUNTER_VALUE_RST : constant LwU64 := 16#0#;
    LW_RISCV_CSR_HPMCOUNTER_CYCLE     : constant       := 16#c00#;
    LW_RISCV_CSR_HPMCOUNTER_TIME      : constant       := 16#c01#;
    LW_RISCV_CSR_HPMCOUNTER_INSTRET   : constant       := 16#c02#;

    type LW_RISCV_CSR_HPMCOUNTER_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_HPMCOUNTER_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MSPM_WPRI3_RST : constant LwU32 := 16#0#;
    type LW_RISCV_CSR_MSPM_MMLOCK_Field is (MMLOCK_UNLOCK, MMLOCK_LOCK) with
        Size => 1;
    for LW_RISCV_CSR_MSPM_MMLOCK_Field use (MMLOCK_UNLOCK => 16#0#, MMLOCK_LOCK => 16#1#);
    LW_RISCV_CSR_MSPM_WPRI2_RST : constant LwU11 := 16#0#;
    type LW_RISCV_CSR_MSPM_MSECM_Field is (MSECM_INSEC, MSECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MSPM_MSECM_Field use (MSECM_INSEC => 16#0#, MSECM_SEC => 16#1#);
    LW_RISCV_CSR_MSPM_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MSPM_SSECM_Field is (SSECM_INSEC, SSECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MSPM_SSECM_Field use (SSECM_INSEC => 16#0#, SSECM_SEC => 16#1#);
    type LW_RISCV_CSR_MSPM_USECM_Field is (USECM_INSEC, USECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MSPM_USECM_Field use (USECM_INSEC => 16#0#, USECM_SEC => 16#1#);
    LW_RISCV_CSR_MSPM_MPLM_LEVEL0 : constant LwU4 := 16#1#;
    LW_RISCV_CSR_MSPM_MPLM_LEVEL1 : constant LwU4 := 16#3#;
    LW_RISCV_CSR_MSPM_MPLM_LEVEL2 : constant LwU4 := 16#5#;
    LW_RISCV_CSR_MSPM_MPLM_LEVEL3 : constant LwU4 := 16#f#;
    LW_RISCV_CSR_MSPM_WPRI0_RST   : constant LwU4 := 16#0#;
    LW_RISCV_CSR_MSPM_SPLM_LEVEL0 : constant LwU4 := 16#1#;
    LW_RISCV_CSR_MSPM_SPLM_LEVEL1 : constant LwU4 := 16#3#;
    LW_RISCV_CSR_MSPM_SPLM_LEVEL2 : constant LwU4 := 16#5#;
    LW_RISCV_CSR_MSPM_SPLM_LEVEL3 : constant LwU4 := 16#f#;
    LW_RISCV_CSR_MSPM_UPLM_LEVEL0 : constant LwU4 := 16#1#;
    LW_RISCV_CSR_MSPM_UPLM_LEVEL1 : constant LwU4 := 16#3#;
    LW_RISCV_CSR_MSPM_UPLM_LEVEL2 : constant LwU4 := 16#5#;
    LW_RISCV_CSR_MSPM_UPLM_LEVEL3 : constant LwU4 := 16#f#;

    type LW_RISCV_CSR_MSPM_Register is record
        Wpri3  : LwU32;
        Mmlock : LW_RISCV_CSR_MSPM_MMLOCK_Field;
        Wpri2  : LwU11;
        Msecm  : LW_RISCV_CSR_MSPM_MSECM_Field;
        Wpri1  : LwU1;
        Ssecm  : LW_RISCV_CSR_MSPM_SSECM_Field;
        Usecm  : LW_RISCV_CSR_MSPM_USECM_Field;
        Mplm   : LwU4;
        Wpri0  : LwU4;
        Splm   : LwU4;
        Uplm   : LwU4;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MSPM_Register use record
        Wpri3  at 0 range 32 .. 63;
        Mmlock at 0 range 31 .. 31;
        Wpri2  at 0 range 20 .. 30;
        Msecm  at 0 range 19 .. 19;
        Wpri1  at 0 range 18 .. 18;
        Ssecm  at 0 range 17 .. 17;
        Usecm  at 0 range 16 .. 16;
        Mplm   at 0 range 12 .. 15;
        Wpri0  at 0 range  8 .. 11;
        Splm   at 0 range  4 ..  7;
        Uplm   at 0 range  0 ..  3;
    end record;

    LW_RISCV_CSR_MDBGCTL_WIRI_RST : constant LwU58 := 16#0#;
    type LW_RISCV_CSR_MDBGCTL_MICD_DIS_Field is (DIS_FALSE, DIS_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MDBGCTL_MICD_DIS_Field use (DIS_FALSE => 16#0#, DIS_TRUE => 16#1#);
    type LW_RISCV_CSR_MDBGCTL_ICDCSRPRV_S_OVERRIDE_Field is (OVERRIDE_FALSE, OVERRIDE_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MDBGCTL_ICDCSRPRV_S_OVERRIDE_Field use (OVERRIDE_FALSE => 16#0#, OVERRIDE_TRUE => 16#1#);
    type LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_Field is (ICDMEMPRV_U, ICDMEMPRV_S, ICDMEMPRV_M) with
        Size => 2;
    for LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_Field use (ICDMEMPRV_U => 16#0#, ICDMEMPRV_S => 16#1#, ICDMEMPRV_M => 16#3#);
    type LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_OVERRIDE_Field is (OVERRIDE_FALSE, OVERRIDE_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_OVERRIDE_Field use (OVERRIDE_FALSE => 16#0#, OVERRIDE_TRUE => 16#1#);
    type LW_RISCV_CSR_MDBGCTL_ICDPL_Field is (ICDPL_USE_PL0, ICDPL_USE_LWR) with
        Size => 1;
    for LW_RISCV_CSR_MDBGCTL_ICDPL_Field use (ICDPL_USE_PL0 => 16#0#, ICDPL_USE_LWR => 16#1#);

    type LW_RISCV_CSR_MDBGCTL_Register is record
        Wiri                 : LwU58;
        Micd_Dis             : LW_RISCV_CSR_MDBGCTL_MICD_DIS_Field;
        Icdcsrprv_S_Override : LW_RISCV_CSR_MDBGCTL_ICDCSRPRV_S_OVERRIDE_Field;
        Icdmemprv            : LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_Field;
        Icdmemprv_Override   : LW_RISCV_CSR_MDBGCTL_ICDMEMPRV_OVERRIDE_Field;
        Icdpl                : LW_RISCV_CSR_MDBGCTL_ICDPL_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MDBGCTL_Register use record
        Wiri                 at 0 range 6 .. 63;
        Micd_Dis             at 0 range 5 ..  5;
        Icdcsrprv_S_Override at 0 range 4 ..  4;
        Icdmemprv            at 0 range 2 ..  3;
        Icdmemprv_Override   at 0 range 1 ..  1;
        Icdpl                at 0 range 0 ..  0;
    end record;

    LW_RISCV_CSR_MRSP_WPRI2_RST : constant LwU52 := 16#0#;
    type LW_RISCV_CSR_MRSP_MRSEC_Field is (MRSEC_INSEC, MRSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MRSP_MRSEC_Field use (MRSEC_INSEC => 16#0#, MRSEC_SEC => 16#1#);
    LW_RISCV_CSR_MRSP_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_MRSP_SRSEC_Field is (SRSEC_INSEC, SRSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MRSP_SRSEC_Field use (SRSEC_INSEC => 16#0#, SRSEC_SEC => 16#1#);
    type LW_RISCV_CSR_MRSP_URSEC_Field is (URSEC_INSEC, URSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_MRSP_URSEC_Field use (URSEC_INSEC => 16#0#, URSEC_SEC => 16#1#);
    type LW_RISCV_CSR_MRSP_MRPL_Field is (MRPL_LEVEL0, MRPL_LEVEL1, MRPL_LEVEL2, MRPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_MRSP_MRPL_Field use (MRPL_LEVEL0 => 16#0#, MRPL_LEVEL1 => 16#1#, MRPL_LEVEL2 => 16#2#, MRPL_LEVEL3 => 16#3#);
    LW_RISCV_CSR_MRSP_WPRI0_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_MRSP_SRPL_Field is (SRPL_LEVEL0, SRPL_LEVEL1, SRPL_LEVEL2, SRPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_MRSP_SRPL_Field use (SRPL_LEVEL0 => 16#0#, SRPL_LEVEL1 => 16#1#, SRPL_LEVEL2 => 16#2#, SRPL_LEVEL3 => 16#3#);
    type LW_RISCV_CSR_MRSP_URPL_Field is (URPL_LEVEL0, URPL_LEVEL1, URPL_LEVEL2, URPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_MRSP_URPL_Field use (URPL_LEVEL0 => 16#0#, URPL_LEVEL1 => 16#1#, URPL_LEVEL2 => 16#2#, URPL_LEVEL3 => 16#3#);

    type LW_RISCV_CSR_MRSP_Register is record
        Wpri2 : LwU52;
        Mrsec : LW_RISCV_CSR_MRSP_MRSEC_Field;
        Wpri1 : LwU1;
        Srsec : LW_RISCV_CSR_MRSP_SRSEC_Field;
        Ursec : LW_RISCV_CSR_MRSP_URSEC_Field;
        Mrpl  : LW_RISCV_CSR_MRSP_MRPL_Field;
        Wpri0 : LwU2;
        Srpl  : LW_RISCV_CSR_MRSP_SRPL_Field;
        Urpl  : LW_RISCV_CSR_MRSP_URPL_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MRSP_Register use record
        Wpri2 at 0 range 12 .. 63;
        Mrsec at 0 range 11 .. 11;
        Wpri1 at 0 range 10 .. 10;
        Srsec at 0 range  9 ..  9;
        Ursec at 0 range  8 ..  8;
        Mrpl  at 0 range  6 ..  7;
        Wpri0 at 0 range  4 ..  5;
        Srpl  at 0 range  2 ..  3;
        Urpl  at 0 range  0 ..  1;
    end record;

    LW_RISCV_CSR_SPM_WIRI1_RST : constant LwU47 := 16#0#;
    type LW_RISCV_CSR_SPM_USECM_Field is (USECM_INSEC, USECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_SPM_USECM_Field use (USECM_INSEC => 16#0#, USECM_SEC => 16#1#);
    LW_RISCV_CSR_SPM_WIRI0_RST   : constant LwU12 := 16#0#;
    LW_RISCV_CSR_SPM_UPLM_LEVEL0 : constant LwU4  := 16#1#;
    LW_RISCV_CSR_SPM_UPLM_LEVEL1 : constant LwU4  := 16#3#;
    LW_RISCV_CSR_SPM_UPLM_LEVEL2 : constant LwU4  := 16#5#;
    LW_RISCV_CSR_SPM_UPLM_LEVEL3 : constant LwU4  := 16#f#;

    type LW_RISCV_CSR_SPM_Register is record
        Wiri1 : LwU47;
        Usecm : LW_RISCV_CSR_SPM_USECM_Field;
        Wiri0 : LwU12;
        Uplm  : LwU4;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SPM_Register use record
        Wiri1 at 0 range 17 .. 63;
        Usecm at 0 range 16 .. 16;
        Wiri0 at 0 range  4 .. 15;
        Uplm  at 0 range  0 ..  3;
    end record;

    LW_RISCV_CSR_RSP_WPRI1_RST : constant LwU55 := 16#0#;
    type LW_RISCV_CSR_RSP_URSEC_Field is (URSEC_INSEC, URSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_RSP_URSEC_Field use (URSEC_INSEC => 16#0#, URSEC_SEC => 16#1#);
    LW_RISCV_CSR_RSP_WPRI0_RST : constant LwU6 := 16#0#;
    type LW_RISCV_CSR_RSP_URPL_Field is (URPL_LEVEL0, URPL_LEVEL1, URPL_LEVEL2, URPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_RSP_URPL_Field use (URPL_LEVEL0 => 16#0#, URPL_LEVEL1 => 16#1#, URPL_LEVEL2 => 16#2#, URPL_LEVEL3 => 16#3#);

    type LW_RISCV_CSR_RSP_Register is record
        Wpri1 : LwU55;
        Ursec : LW_RISCV_CSR_RSP_URSEC_Field;
        Wpri0 : LwU6;
        Urpl  : LW_RISCV_CSR_RSP_URPL_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_RSP_Register use record
        Wpri1 at 0 range 9 .. 63;
        Ursec at 0 range 8 ..  8;
        Wpri0 at 0 range 2 ..  7;
        Urpl  at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_TSELECT_RS0_RST   : constant LwU58 := 16#0#;
    LW_RISCV_CSR_TSELECT_INDEX_RST : constant LwU6  := 16#0#;

    type LW_RISCV_CSR_TSELECT_Register is record
        Rs0   : LwU58;
        Index : LwU6;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_TSELECT_Register use record
        Rs0   at 0 range 6 .. 63;
        Index at 0 range 0 ..  5;
    end record;

    LW_RISCV_CSR_TDATA1_TDATA1_TYPE_ADDR_DATA : constant LwU4  := 16#2#;
    LW_RISCV_CSR_TDATA1_DMODE_RST             : constant LwU1  := 16#0#;
    LW_RISCV_CSR_TDATA1_MASKMAX_RST           : constant LwU6  := 16#0#;
    LW_RISCV_CSR_TDATA1_RS0_RST               : constant LwU33 := 16#0#;
    LW_RISCV_CSR_TDATA1_TDATA1_SELECT_RST     : constant LwU1  := 16#0#;
    LW_RISCV_CSR_TDATA1_TIMING_RST            : constant LwU1  := 16#0#;
    LW_RISCV_CSR_TDATA1_ACTION_DBG            : constant LwU6  := 16#0#;
    LW_RISCV_CSR_TDATA1_ACTION_ICD            : constant LwU6  := 16#1#;
    LW_RISCV_CSR_TDATA1_CHAIN_RST             : constant LwU1  := 16#0#;
    LW_RISCV_CSR_TDATA1_MATCH_RST             : constant LwU4  := 16#0#;
    type LW_RISCV_CSR_TDATA1_M_Field is (M_DISABLE, M_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_TDATA1_M_Field use (M_DISABLE => 16#0#, M_ENABLE => 16#1#);
    LW_RISCV_CSR_TDATA1_RS1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_TDATA1_S_Field is (S_DISABLE, S_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_TDATA1_S_Field use (S_DISABLE => 16#0#, S_ENABLE => 16#1#);
    type LW_RISCV_CSR_TDATA1_U_Field is (U_DISABLE, U_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_TDATA1_U_Field use (U_DISABLE => 16#0#, U_ENABLE => 16#1#);
    LW_RISCV_CSR_TDATA1_EXELWTE_RST : constant LwU1 := 16#0#;
    LW_RISCV_CSR_TDATA1_STORE_RST   : constant LwU1 := 16#0#;
    LW_RISCV_CSR_TDATA1_LOAD_RST    : constant LwU1 := 16#0#;

    type LW_RISCV_CSR_TDATA1_Register is record
        Tdata1_Type   : LwU4;
        Dmode         : LwU1;
        Maskmax       : LwU6;
        Rs0           : LwU33;
        Tdata1_Select : LwU1;
        Timing        : LwU1;
        Action        : LwU6;
        Chain         : LwU1;
        Match         : LwU4;
        M             : LW_RISCV_CSR_TDATA1_M_Field;
        Rs1           : LwU1;
        S             : LW_RISCV_CSR_TDATA1_S_Field;
        U             : LW_RISCV_CSR_TDATA1_U_Field;
        Execute       : LwU1;
        Store         : LwU1;
        Load          : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_TDATA1_Register use record
        Tdata1_Type   at 0 range 60 .. 63;
        Dmode         at 0 range 59 .. 59;
        Maskmax       at 0 range 53 .. 58;
        Rs0           at 0 range 20 .. 52;
        Tdata1_Select at 0 range 19 .. 19;
        Timing        at 0 range 18 .. 18;
        Action        at 0 range 12 .. 17;
        Chain         at 0 range 11 .. 11;
        Match         at 0 range  7 .. 10;
        M             at 0 range  6 ..  6;
        Rs1           at 0 range  5 ..  5;
        S             at 0 range  4 ..  4;
        U             at 0 range  3 ..  3;
        Execute       at 0 range  2 ..  2;
        Store         at 0 range  1 ..  1;
        Load          at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_TDATA2_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_TDATA2_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_TDATA2_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_DCSR_Register is record
        Rs0 : LwU62;
        Prv : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_DCSR_Register use record
        Rs0 at 0 range 2 .. 63;
        Prv at 0 range 0 ..  1;
    end record;

    type LW_RISCV_CSR_MBIND_Register is record
        Gfid                  : LwU6;
        Inst_Blk_Ptr_Aperture : LwU2;
        Inst_Blk_Ptr_Adr      : LwU40;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MBIND_Register use record
        Gfid                  at 0 range 54 .. 59;
        Inst_Blk_Ptr_Aperture at 0 range 52 .. 53;
        Inst_Blk_Ptr_Adr      at 0 range  0 .. 39;
    end record;

    type LW_RISCV_CSR_MTLBILWDATA1_Register is record
        Pdb_Adr_Hi                   : LwU5;
        Ack_Type                     : LwU2;
        Sys_Membar                   : LwU1;
        Cancel_Target_Gpc_Id         : LwU3;
        Cancel_Target_Client_Unit_Id : LwU7;
        Mmu_Replay                   : LwU3;
        Target_Ava                   : LwU1;
        Page_Table_Level             : LwU3;
        Pdb_Adr                      : LwU35;
        Target                       : LwU2;
        Pdb                          : LwU1;
        Gpc                          : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MTLBILWDATA1_Register use record
        Pdb_Adr_Hi                   at 0 range 59 .. 63;
        Ack_Type                     at 0 range 57 .. 58;
        Sys_Membar                   at 0 range 56 .. 56;
        Cancel_Target_Gpc_Id         at 0 range 53 .. 55;
        Cancel_Target_Client_Unit_Id at 0 range 46 .. 52;
        Mmu_Replay                   at 0 range 43 .. 45;
        Target_Ava                   at 0 range 42 .. 42;
        Page_Table_Level             at 0 range 39 .. 41;
        Pdb_Adr                      at 0 range  4 .. 38;
        Target                       at 0 range  2 ..  3;
        Pdb                          at 0 range  1 ..  1;
        Gpc                          at 0 range  0 ..  0;
    end record;

    type LW_RISCV_CSR_MTLBILWOP_Register is record
        Target_Adr : LwU45;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MTLBILWOP_Register use record
        Target_Adr at 0 range 0 .. 44;
    end record;

    type LW_RISCV_CSR_MFLUSH_Register is record
        Rs0                   : LwU63;
        Host2fb_Membar_Type_T : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MFLUSH_Register use record
        Rs0                   at 0 range 1 .. 63;
        Host2fb_Membar_Type_T at 0 range 0 ..  0;
    end record;

    type LW_RISCV_CSR_ML2SYSILW_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_ML2SYSILW_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_ML2PEERILW_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_ML2PEERILW_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_ML2CLNCOMP_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_ML2CLNCOMP_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_ML2FLHDTY_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_ML2FLHDTY_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_MSYSOPEN_WPRI_RST : constant LwU56 := 16#0#;
    type LW_RISCV_CSR_MSYSOPEN_L2FLHDTY_Field is (L2FLHDTY_DISABLE, L2FLHDTY_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_L2FLHDTY_Field use (L2FLHDTY_DISABLE => 16#0#, L2FLHDTY_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_L2CLNCOMP_Field is (L2CLNCOMP_DISABLE, L2CLNCOMP_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_L2CLNCOMP_Field use (L2CLNCOMP_DISABLE => 16#0#, L2CLNCOMP_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_L2PEERILW_Field is (L2PEERILW_DISABLE, L2PEERILW_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_L2PEERILW_Field use (L2PEERILW_DISABLE => 16#0#, L2PEERILW_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_L2SYSILW_Field is (L2SYSILW_DISABLE, L2SYSILW_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_L2SYSILW_Field use (L2SYSILW_DISABLE => 16#0#, L2SYSILW_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_FLUSH_Field is (FLUSH_DISABLE, FLUSH_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_FLUSH_Field use (FLUSH_DISABLE => 16#0#, FLUSH_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_TLBILWOP_Field is (TLBILWOP_DISABLE, TLBILWOP_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_TLBILWOP_Field use (TLBILWOP_DISABLE => 16#0#, TLBILWOP_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_TLBILWDATA1_Field is (TLBILWDATA1_DISABLE, TLBILWDATA1_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_TLBILWDATA1_Field use (TLBILWDATA1_DISABLE => 16#0#, TLBILWDATA1_ENABLE => 16#1#);
    type LW_RISCV_CSR_MSYSOPEN_BIND_Field is (BIND_DISABLE, BIND_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_MSYSOPEN_BIND_Field use (BIND_DISABLE => 16#0#, BIND_ENABLE => 16#1#);

    type LW_RISCV_CSR_MSYSOPEN_Register is record
        Wpri        : LwU56;
        L2flhdty    : LW_RISCV_CSR_MSYSOPEN_L2FLHDTY_Field;
        L2clncomp   : LW_RISCV_CSR_MSYSOPEN_L2CLNCOMP_Field;
        L2peerilw   : LW_RISCV_CSR_MSYSOPEN_L2PEERILW_Field;
        L2sysilw    : LW_RISCV_CSR_MSYSOPEN_L2SYSILW_Field;
        Flush       : LW_RISCV_CSR_MSYSOPEN_FLUSH_Field;
        Tlbilwop    : LW_RISCV_CSR_MSYSOPEN_TLBILWOP_Field;
        Tlbilwdata1 : LW_RISCV_CSR_MSYSOPEN_TLBILWDATA1_Field;
        Bind        : LW_RISCV_CSR_MSYSOPEN_BIND_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MSYSOPEN_Register use record
        Wpri        at 0 range 8 .. 63;
        L2flhdty    at 0 range 7 ..  7;
        L2clncomp   at 0 range 6 ..  6;
        L2peerilw   at 0 range 5 ..  5;
        L2sysilw    at 0 range 4 ..  4;
        Flush       at 0 range 3 ..  3;
        Tlbilwop    at 0 range 2 ..  2;
        Tlbilwdata1 at 0 range 1 ..  1;
        Bind        at 0 range 0 ..  0;
    end record;

    type LW_RISCV_CSR_MROMPROT0_Register is record
        Rom_Prot : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MROMPROT0_Register use record
        Rom_Prot at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_MROMPROT1_Register is record
        Rom_Prot : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MROMPROT1_Register use record
        Rom_Prot at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_MROMPROT2_Register is record
        Rom_Prot : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MROMPROT2_Register use record
        Rom_Prot at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_MROMPROT3_Register is record
        Rom_Prot : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_MROMPROT3_Register use record
        Rom_Prot at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_BIND_Register is record
        Gfid                  : LwU6;
        Inst_Blk_Ptr_Aperture : LwU2;
        Inst_Blk_Ptr_Adr      : LwU40;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_BIND_Register use record
        Gfid                  at 0 range 54 .. 59;
        Inst_Blk_Ptr_Aperture at 0 range 52 .. 53;
        Inst_Blk_Ptr_Adr      at 0 range  0 .. 39;
    end record;

    type LW_RISCV_CSR_TLBILWDATA1_Register is record
        Pdb_Adr_Hi                   : LwU5;
        Ack_Type                     : LwU2;
        Sys_Membar                   : LwU1;
        Cancel_Target_Gpc_Id         : LwU3;
        Cancel_Target_Client_Unit_Id : LwU7;
        Mmu_Replay                   : LwU3;
        Target_Ava                   : LwU1;
        Page_Table_Level             : LwU3;
        Pdb_Adr                      : LwU35;
        Target                       : LwU2;
        Pdb                          : LwU1;
        Gpc                          : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_TLBILWDATA1_Register use record
        Pdb_Adr_Hi                   at 0 range 59 .. 63;
        Ack_Type                     at 0 range 57 .. 58;
        Sys_Membar                   at 0 range 56 .. 56;
        Cancel_Target_Gpc_Id         at 0 range 53 .. 55;
        Cancel_Target_Client_Unit_Id at 0 range 46 .. 52;
        Mmu_Replay                   at 0 range 43 .. 45;
        Target_Ava                   at 0 range 42 .. 42;
        Page_Table_Level             at 0 range 39 .. 41;
        Pdb_Adr                      at 0 range  4 .. 38;
        Target                       at 0 range  2 ..  3;
        Pdb                          at 0 range  1 ..  1;
        Gpc                          at 0 range  0 ..  0;
    end record;

    type LW_RISCV_CSR_TLBILWOP_Register is record
        Target_Adr : LwU45;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_TLBILWOP_Register use record
        Target_Adr at 0 range 0 .. 44;
    end record;

    type LW_RISCV_CSR_FLUSH_Register is record
        Rs0                   : LwU63;
        Host2fb_Membar_Type_T : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_FLUSH_Register use record
        Rs0                   at 0 range 1 .. 63;
        Host2fb_Membar_Type_T at 0 range 0 ..  0;
    end record;

    type LW_RISCV_CSR_L2SYSILW_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_L2SYSILW_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_L2PEERILW_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_L2PEERILW_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_L2CLNCOMP_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_L2CLNCOMP_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_L2FLHDTY_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_L2FLHDTY_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_LWFENCEIO_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_LWFENCEIO_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_LWFENCEMEM_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_LWFENCEMEM_Register use record
        Value at 0 range 0 .. 63;
    end record;

    type LW_RISCV_CSR_LWFENCEALL_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_LWFENCEALL_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SSTATUS_WPRI5_RST : constant LwU29 := 16#0#;
    LW_RISCV_CSR_SSTATUS_UXL_RST   : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SSTATUS_WPRI4_RST : constant LwU12 := 16#0#;
    LW_RISCV_CSR_SSTATUS_MXR_RST   : constant LwU1  := 16#0#;
    type LW_RISCV_CSR_SSTATUS_SUM_Field is (SUM_DISABLE, SUM_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SSTATUS_SUM_Field use (SUM_DISABLE => 16#0#, SUM_ENABLE => 16#1#);
    LW_RISCV_CSR_SSTATUS_WPRI3_RST : constant LwU1 := 16#0#;
    LW_RISCV_CSR_SSTATUS_XS_OFF    : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_SSTATUS_FS_Field is (FS_OFF, FS_INITIAL, FS_CLEAN, FS_DIRTY) with
        Size => 2;
    for LW_RISCV_CSR_SSTATUS_FS_Field use (FS_OFF => 16#0#, FS_INITIAL => 16#1#, FS_CLEAN => 16#2#, FS_DIRTY => 16#3#);
    LW_RISCV_CSR_SSTATUS_WPRI2_RST : constant LwU4 := 16#0#;
    type LW_RISCV_CSR_SSTATUS_SPP_Field is (SPP_USER, SPP_SUPERVISOR) with
        Size => 1;
    for LW_RISCV_CSR_SSTATUS_SPP_Field use (SPP_USER => 16#0#, SPP_SUPERVISOR => 16#1#);
    LW_RISCV_CSR_SSTATUS_WPRI1_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_SSTATUS_SPIE_Field is (SPIE_DISABLE, SPIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SSTATUS_SPIE_Field use (SPIE_DISABLE => 16#0#, SPIE_ENABLE => 16#1#);
    LW_RISCV_CSR_SSTATUS_UPIE_DISABLE : constant LwU1 := 16#0#;
    LW_RISCV_CSR_SSTATUS_WPRI0_RST    : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_SSTATUS_SIE_Field is (SIE_DISABLE, SIE_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SSTATUS_SIE_Field use (SIE_DISABLE => 16#0#, SIE_ENABLE => 16#1#);
    LW_RISCV_CSR_SSTATUS_UIE_DISABLE : constant LwU1 := 16#0#;

    type LW_RISCV_CSR_SSTATUS_Register is record
        Sd    : LwU1;
        Wpri5 : LwU29;
        Uxl   : LwU2;
        Wpri4 : LwU12;
        Mxr   : LwU1;
        Sum   : LW_RISCV_CSR_SSTATUS_SUM_Field;
        Wpri3 : LwU1;
        Xs    : LwU2;
        Fs    : LW_RISCV_CSR_SSTATUS_FS_Field;
        Wpri2 : LwU4;
        Spp   : LW_RISCV_CSR_SSTATUS_SPP_Field;
        Wpri1 : LwU2;
        Spie  : LW_RISCV_CSR_SSTATUS_SPIE_Field;
        Upie  : LwU1;
        Wpri0 : LwU2;
        Sie   : LW_RISCV_CSR_SSTATUS_SIE_Field;
        Uie   : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SSTATUS_Register use record
        Sd    at 0 range 63 .. 63;
        Wpri5 at 0 range 34 .. 62;
        Uxl   at 0 range 32 .. 33;
        Wpri4 at 0 range 20 .. 31;
        Mxr   at 0 range 19 .. 19;
        Sum   at 0 range 18 .. 18;
        Wpri3 at 0 range 17 .. 17;
        Xs    at 0 range 15 .. 16;
        Fs    at 0 range 13 .. 14;
        Wpri2 at 0 range  9 .. 12;
        Spp   at 0 range  8 ..  8;
        Wpri1 at 0 range  6 ..  7;
        Spie  at 0 range  5 ..  5;
        Upie  at 0 range  4 ..  4;
        Wpri0 at 0 range  2 ..  3;
        Sie   at 0 range  1 ..  1;
        Uie   at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_STVEC_BASE_RST : constant LwU62 := 16#0#;
    type LW_RISCV_CSR_STVEC_MODE_Field is (MODE_DIRECT, MODE_VECTORED) with
        Size => 2;
    for LW_RISCV_CSR_STVEC_MODE_Field use (MODE_DIRECT => 16#0#, MODE_VECTORED => 16#1#);

    type LW_RISCV_CSR_STVEC_Register is record
        Base : LwU62;
        Mode : LW_RISCV_CSR_STVEC_MODE_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_STVEC_Register use record
        Base at 0 range 2 .. 63;
        Mode at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_SSCRATCH_SCRATCH_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SSCRATCH_Register is record
        Scratch : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SSCRATCH_Register use record
        Scratch at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SIP_WIRI2_RST : constant LwU54 := 16#0#;
    LW_RISCV_CSR_SIP_SEIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIP_UEIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIP_WIRI1_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SIP_STIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIP_UTIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIP_WIRI0_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SIP_SSIP_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIP_USIP_RST  : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_SIP_Register is record
        Wiri2 : LwU54;
        Seip  : LwU1;
        Ueip  : LwU1;
        Wiri1 : LwU2;
        Stip  : LwU1;
        Utip  : LwU1;
        Wiri0 : LwU2;
        Ssip  : LwU1;
        Usip  : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SIP_Register use record
        Wiri2 at 0 range 10 .. 63;
        Seip  at 0 range  9 ..  9;
        Ueip  at 0 range  8 ..  8;
        Wiri1 at 0 range  6 ..  7;
        Stip  at 0 range  5 ..  5;
        Utip  at 0 range  4 ..  4;
        Wiri0 at 0 range  2 ..  3;
        Ssip  at 0 range  1 ..  1;
        Usip  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_SIE_WPRI2_RST : constant LwU54 := 16#0#;
    LW_RISCV_CSR_SIE_SEIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIE_UEIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIE_WPRI1_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SIE_STIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIE_UTIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIE_WPRI0_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SIE_SSIE_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SIE_USIE_RST  : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_SIE_Register is record
        Wpri2 : LwU54;
        Seie  : LwU1;
        Ueie  : LwU1;
        Wpri1 : LwU2;
        Stie  : LwU1;
        Utie  : LwU1;
        Wpri0 : LwU2;
        Ssie  : LwU1;
        Usie  : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SIE_Register use record
        Wpri2 at 0 range 10 .. 63;
        Seie  at 0 range  9 ..  9;
        Ueie  at 0 range  8 ..  8;
        Wpri1 at 0 range  6 ..  7;
        Stie  at 0 range  5 ..  5;
        Utie  at 0 range  4 ..  4;
        Wpri0 at 0 range  2 ..  3;
        Ssie  at 0 range  1 ..  1;
        Usie  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_STVAL_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_STVAL_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_STVAL_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SEPC_EPC_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SEPC_Register is record
        Epc : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SEPC_Register use record
        Epc at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SCAUSE_INT_RST            : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SCAUSE_WLRL_RST           : constant LwU58 := 16#0#;
    LW_RISCV_CSR_SCAUSE_EXCODE_RST         : constant LwU5  := 16#0#;
    LW_RISCV_CSR_SCAUSE_EXCODE_IAMA        : constant LwU5  := 16#0#;
    LW_RISCV_CSR_SCAUSE_EXCODE_IACC_FAULT  : constant LwU5  := 16#1#;
    LW_RISCV_CSR_SCAUSE_EXCODE_ILL         : constant LwU5  := 16#2#;
    LW_RISCV_CSR_SCAUSE_EXCODE_BKPT        : constant LwU5  := 16#3#;
    LW_RISCV_CSR_SCAUSE_EXCODE_LAMA        : constant LwU5  := 16#4#;
    LW_RISCV_CSR_SCAUSE_EXCODE_LACC_FAULT  : constant LwU5  := 16#5#;
    LW_RISCV_CSR_SCAUSE_EXCODE_SAMA        : constant LwU5  := 16#6#;
    LW_RISCV_CSR_SCAUSE_EXCODE_SACC_FAULT  : constant LwU5  := 16#7#;
    LW_RISCV_CSR_SCAUSE_EXCODE_UCALL       : constant LwU5  := 16#8#;
    LW_RISCV_CSR_SCAUSE_EXCODE_SCALL       : constant LwU5  := 16#9#;
    LW_RISCV_CSR_SCAUSE_EXCODE_IPAGE_FAULT : constant LwU5  := 16#c#;
    LW_RISCV_CSR_SCAUSE_EXCODE_LPAGE_FAULT : constant LwU5  := 16#d#;
    LW_RISCV_CSR_SCAUSE_EXCODE_SPAGE_FAULT : constant LwU5  := 16#f#;
    LW_RISCV_CSR_SCAUSE_EXCODE_USS         : constant LwU5  := 16#1c#;
    LW_RISCV_CSR_SCAUSE_EXCODE_U_SWINT     : constant LwU5  := 16#0#;
    LW_RISCV_CSR_SCAUSE_EXCODE_S_SWINT     : constant LwU5  := 16#1#;
    LW_RISCV_CSR_SCAUSE_EXCODE_U_TINT      : constant LwU5  := 16#4#;
    LW_RISCV_CSR_SCAUSE_EXCODE_S_TINT      : constant LwU5  := 16#5#;
    LW_RISCV_CSR_SCAUSE_EXCODE_U_EINT      : constant LwU5  := 16#8#;
    LW_RISCV_CSR_SCAUSE_EXCODE_S_EINT      : constant LwU5  := 16#9#;

    type LW_RISCV_CSR_SCAUSE_Register is record
        Int    : LwU1;
        Wlrl   : LwU58;
        Excode : LwU5;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SCAUSE_Register use record
        Int    at 0 range 63 .. 63;
        Wlrl   at 0 range  5 .. 62;
        Excode at 0 range  0 ..  4;
    end record;

    LW_RISCV_CSR_SCAUSE2_WPRI_RST            : constant LwU40 := 16#0#;
    LW_RISCV_CSR_SCAUSE2_CAUSE_NO            : constant LwU8  := 16#0#;
    LW_RISCV_CSR_SCAUSE2_CAUSE_EBREAK        : constant LwU8  := 16#17#;
    LW_RISCV_CSR_SCAUSE2_CAUSE_HW_TRIGGER    : constant LwU8  := 16#18#;
    LW_RISCV_CSR_SCAUSE2_CAUSE_U_SINGLE_STEP : constant LwU8  := 16#19#;
    LW_RISCV_CSR_SCAUSE2_ERROR_NO            : constant LwU16 := 16#0#;

    type LW_RISCV_CSR_SCAUSE2_Register is record
        Wpri  : LwU40;
        Cause : LwU8;
        Error : LwU16;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SCAUSE2_Register use record
        Wpri  at 0 range 24 .. 63;
        Cause at 0 range 16 .. 23;
        Error at 0 range  0 .. 15;
    end record;

    LW_RISCV_CSR_SATP_MODE_BARE       : constant LwU4  := 16#0#;
    LW_RISCV_CSR_SATP_MODE_LWMPU      : constant LwU4  := 16#f#;
    LW_RISCV_CSR_SATP_MODE_HASH_LWMPU : constant LwU4  := 16#e#;
    LW_RISCV_CSR_SATP_ASID_BARE       : constant LwU16 := 16#0#;
    LW_RISCV_CSR_SATP_PPN_BARE        : constant LwU44 := 16#0#;

    type LW_RISCV_CSR_SATP_Register is record
        Mode : LwU4;
        Asid : LwU16;
        Ppn  : LwU44;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SATP_Register use record
        Mode at 0 range 60 .. 63;
        Asid at 0 range 44 .. 59;
        Ppn  at 0 range  0 .. 43;
    end record;

    LW_RISCV_CSR_SCOUNTEREN_RS0_RST     : constant LwU32 := 16#0#;
    LW_RISCV_CSR_SCOUNTEREN_HPM_DISABLE : constant LwU29 := 16#0#;
    type LW_RISCV_CSR_SCOUNTEREN_IR_Field is (IR_DISABLE, IR_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SCOUNTEREN_IR_Field use (IR_DISABLE => 16#0#, IR_ENABLE => 16#1#);
    type LW_RISCV_CSR_SCOUNTEREN_TM_Field is (TM_DISABLE, TM_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SCOUNTEREN_TM_Field use (TM_DISABLE => 16#0#, TM_ENABLE => 16#1#);
    type LW_RISCV_CSR_SCOUNTEREN_CY_Field is (CY_DISABLE, CY_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SCOUNTEREN_CY_Field use (CY_DISABLE => 16#0#, CY_ENABLE => 16#1#);

    type LW_RISCV_CSR_SCOUNTEREN_Register is record
        Rs0 : LwU32;
        Hpm : LwU29;
        Ir  : LW_RISCV_CSR_SCOUNTEREN_IR_Field;
        Tm  : LW_RISCV_CSR_SCOUNTEREN_TM_Field;
        Cy  : LW_RISCV_CSR_SCOUNTEREN_CY_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SCOUNTEREN_Register use record
        Rs0 at 0 range 32 .. 63;
        Hpm at 0 range  3 .. 31;
        Ir  at 0 range  2 ..  2;
        Tm  at 0 range  1 ..  1;
        Cy  at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_SSPM_WPRI1_RST : constant LwU46 := 16#0#;
    type LW_RISCV_CSR_SSPM_SSECM_Field is (SSECM_INSEC, SSECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_SSPM_SSECM_Field use (SSECM_INSEC => 16#0#, SSECM_SEC => 16#1#);
    type LW_RISCV_CSR_SSPM_USECM_Field is (USECM_INSEC, USECM_SEC) with
        Size => 1;
    for LW_RISCV_CSR_SSPM_USECM_Field use (USECM_INSEC => 16#0#, USECM_SEC => 16#1#);
    LW_RISCV_CSR_SSPM_WPRI0_RST   : constant LwU8 := 16#0#;
    LW_RISCV_CSR_SSPM_SPLM_LEVEL0 : constant LwU4 := 16#1#;
    LW_RISCV_CSR_SSPM_SPLM_LEVEL1 : constant LwU4 := 16#3#;
    LW_RISCV_CSR_SSPM_SPLM_LEVEL2 : constant LwU4 := 16#5#;
    LW_RISCV_CSR_SSPM_SPLM_LEVEL3 : constant LwU4 := 16#f#;
    LW_RISCV_CSR_SSPM_UPLM_LEVEL0 : constant LwU4 := 16#1#;
    LW_RISCV_CSR_SSPM_UPLM_LEVEL1 : constant LwU4 := 16#3#;
    LW_RISCV_CSR_SSPM_UPLM_LEVEL2 : constant LwU4 := 16#5#;
    LW_RISCV_CSR_SSPM_UPLM_LEVEL3 : constant LwU4 := 16#f#;

    type LW_RISCV_CSR_SSPM_Register is record
        Wpri1 : LwU46;
        Ssecm : LW_RISCV_CSR_SSPM_SSECM_Field;
        Usecm : LW_RISCV_CSR_SSPM_USECM_Field;
        Wpri0 : LwU8;
        Splm  : LwU4;
        Uplm  : LwU4;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SSPM_Register use record
        Wpri1 at 0 range 18 .. 63;
        Ssecm at 0 range 17 .. 17;
        Usecm at 0 range 16 .. 16;
        Wpri0 at 0 range  8 .. 15;
        Splm  at 0 range  4 ..  7;
        Uplm  at 0 range  0 ..  3;
    end record;

    LW_RISCV_CSR_SRSP_WPRI1_RST : constant LwU54 := 16#0#;
    type LW_RISCV_CSR_SRSP_SRSEC_Field is (SRSEC_INSEC, SRSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_SRSP_SRSEC_Field use (SRSEC_INSEC => 16#0#, SRSEC_SEC => 16#1#);
    type LW_RISCV_CSR_SRSP_URSEC_Field is (URSEC_INSEC, URSEC_SEC) with
        Size => 1;
    for LW_RISCV_CSR_SRSP_URSEC_Field use (URSEC_INSEC => 16#0#, URSEC_SEC => 16#1#);
    LW_RISCV_CSR_SRSP_WPRI0_RST : constant LwU4 := 16#0#;
    type LW_RISCV_CSR_SRSP_SRPL_Field is (SRPL_LEVEL0, SRPL_LEVEL1, SRPL_LEVEL2, SRPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_SRSP_SRPL_Field use (SRPL_LEVEL0 => 16#0#, SRPL_LEVEL1 => 16#1#, SRPL_LEVEL2 => 16#2#, SRPL_LEVEL3 => 16#3#);
    type LW_RISCV_CSR_SRSP_URPL_Field is (URPL_LEVEL0, URPL_LEVEL1, URPL_LEVEL2, URPL_LEVEL3) with
        Size => 2;
    for LW_RISCV_CSR_SRSP_URPL_Field use (URPL_LEVEL0 => 16#0#, URPL_LEVEL1 => 16#1#, URPL_LEVEL2 => 16#2#, URPL_LEVEL3 => 16#3#);

    type LW_RISCV_CSR_SRSP_Register is record
        Wpri1 : LwU54;
        Srsec : LW_RISCV_CSR_SRSP_SRSEC_Field;
        Ursec : LW_RISCV_CSR_SRSP_URSEC_Field;
        Wpri0 : LwU4;
        Srpl  : LW_RISCV_CSR_SRSP_SRPL_Field;
        Urpl  : LW_RISCV_CSR_SRSP_URPL_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SRSP_Register use record
        Wpri1 at 0 range 10 .. 63;
        Srsec at 0 range  9 ..  9;
        Ursec at 0 range  8 ..  8;
        Wpri0 at 0 range  4 ..  7;
        Srpl  at 0 range  2 ..  3;
        Urpl  at 0 range  0 ..  1;
    end record;

    LW_RISCV_CSR_SLWRRUID_WPRI_RST     : constant LwU48 := 16#0#;
    LW_RISCV_CSR_SLWRRUID_SLWRRUID_RST : constant LwU8  := 16#0#;
    LW_RISCV_CSR_SLWRRUID_ULWRRUID_RST : constant LwU8  := 16#0#;

    type LW_RISCV_CSR_SLWRRUID_Register is record
        Wpri     : LwU48;
        Slwrruid : LwU8;
        Ulwrruid : LwU8;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SLWRRUID_Register use record
        Wpri     at 0 range 16 .. 63;
        Slwrruid at 0 range  8 .. 15;
        Ulwrruid at 0 range  0 ..  7;
    end record;

    LW_RISCV_CSR_SCFG_WPRI4_RST : constant LwU50 := 16#0#;
    type LW_RISCV_CSR_SCFG_SPOSTIO_Field is (SPOSTIO_FALSE, SPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_SPOSTIO_Field use (SPOSTIO_FALSE => 16#0#, SPOSTIO_TRUE => 16#1#);
    type LW_RISCV_CSR_SCFG_UPOSTIO_Field is (UPOSTIO_FALSE, UPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_UPOSTIO_Field use (UPOSTIO_FALSE => 16#0#, UPOSTIO_TRUE => 16#1#);
    LW_RISCV_CSR_SCFG_WPRI3_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_SCFG_SSBC_Field is (SSBC_FALSE, SSBC_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_SSBC_Field use (SSBC_FALSE => 16#0#, SSBC_TRUE => 16#1#);
    type LW_RISCV_CSR_SCFG_USBC_Field is (USBC_FALSE, USBC_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_USBC_Field use (USBC_FALSE => 16#0#, USBC_TRUE => 16#1#);
    LW_RISCV_CSR_SCFG_WPRI2_RST : constant LwU3 := 16#0#;
    type LW_RISCV_CSR_SCFG_USSE_Field is (USSE_FALSE, USSE_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_USSE_Field use (USSE_FALSE => 16#0#, USSE_TRUE => 16#1#);
    LW_RISCV_CSR_SCFG_WPRI1_RST : constant LwU2 := 16#0#;
    type LW_RISCV_CSR_SCFG_SWFP_DIS_Field is (DIS_FALSE, DIS_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_SCFG_SWFP_DIS_Field use (DIS_FALSE => 16#0#, DIS_TRUE => 16#1#);
    LW_RISCV_CSR_SCFG_UWFP_DIS_FALSE : constant LwU1 := 16#0#;

    type LW_RISCV_CSR_SCFG_Register is record
        Wpri4    : LwU50;
        Spostio  : LW_RISCV_CSR_SCFG_SPOSTIO_Field;
        Upostio  : LW_RISCV_CSR_SCFG_UPOSTIO_Field;
        Wpri3    : LwU2;
        Ssbc     : LW_RISCV_CSR_SCFG_SSBC_Field;
        Usbc     : LW_RISCV_CSR_SCFG_USBC_Field;
        Wpri2    : LwU3;
        Usse     : LW_RISCV_CSR_SCFG_USSE_Field;
        Wpri1    : LwU2;
        Swfp_Dis : LW_RISCV_CSR_SCFG_SWFP_DIS_Field;
        Uwfp_Dis : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SCFG_Register use record
        Wpri4    at 0 range 14 .. 63;
        Spostio  at 0 range 13 .. 13;
        Upostio  at 0 range 12 .. 12;
        Wpri3    at 0 range 10 .. 11;
        Ssbc     at 0 range  9 ..  9;
        Usbc     at 0 range  8 ..  8;
        Wpri2    at 0 range  5 ..  7;
        Usse     at 0 range  4 ..  4;
        Wpri1    at 0 range  2 ..  3;
        Swfp_Dis at 0 range  1 ..  1;
        Uwfp_Dis at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_SBPCFG_WPRI1_RST       : constant LwU46 := 16#0#;
    LW_RISCV_CSR_SBPCFG_BTB_FLUSHS_TRUE : constant LwU1  := 16#1#;
    LW_RISCV_CSR_SBPCFG_BTB_FLUSHU_TRUE : constant LwU1  := 16#1#;
    LW_RISCV_CSR_SBPCFG_WPRI0_RST       : constant LwU16 := 16#0#;

    type LW_RISCV_CSR_SBPCFG_Register is record
        Wpri1      : LwU46;
        Btb_Flushs : LwU1;
        Btb_Flushu : LwU1;
        Wpri0      : LwU16;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SBPCFG_Register use record
        Wpri1      at 0 range 18 .. 63;
        Btb_Flushs at 0 range 17 .. 17;
        Btb_Flushu at 0 range 16 .. 16;
        Wpri0      at 0 range  0 .. 15;
    end record;

    LW_RISCV_CSR_SMISCOPEN_WPRI_RST : constant LwU63 := 16#0#;
    type LW_RISCV_CSR_SMISCOPEN_DCACHEOP_Field is (DCACHEOP_DISABLE, DCACHEOP_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SMISCOPEN_DCACHEOP_Field use (DCACHEOP_DISABLE => 16#0#, DCACHEOP_ENABLE => 16#1#);

    type LW_RISCV_CSR_SMISCOPEN_Register is record
        Wpri     : LwU63;
        Dcacheop : LW_RISCV_CSR_SMISCOPEN_DCACHEOP_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMISCOPEN_Register use record
        Wpri     at 0 range 1 .. 63;
        Dcacheop at 0 range 0 ..  0;
    end record;

    LW_RISCV_CSR_SSCRATCH2_VALUE_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SSCRATCH2_Register is record
        Value : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SSCRATCH2_Register use record
        Value at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SMPUCTL_WPRI1_RST       : constant LwU40 := 16#0#;
    LW_RISCV_CSR_SMPUCTL_ENTRY_COUNT_RST : constant LwU8  := 16#0#;
    LW_RISCV_CSR_SMPUCTL_WPRI0_RST       : constant LwU16 := 16#0#;

    type LW_RISCV_CSR_SMPUCTL_Register is record
        Wpri1       : LwU40;
        Entry_Count : LwU8;
        Wpri0       : LwU16;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUCTL_Register use record
        Wpri1       at 0 range 24 .. 63;
        Entry_Count at 0 range 16 .. 23;
        Wpri0       at 0 range  0 .. 15;
    end record;

    LW_RISCV_CSR_SMPUIDX_WPRI_RST  : constant LwU57 := 16#0#;
    LW_RISCV_CSR_SMPUIDX_INDEX_RST : constant LwU7  := 16#0#;

    type LW_RISCV_CSR_SMPUIDX_Register is record
        Wpri  : LwU57;
        Index : LwU7;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUIDX_Register use record
        Wpri  at 0 range 7 .. 63;
        Index at 0 range 0 ..  6;
    end record;

    LW_RISCV_CSR_SMPUVA_BASE_RST  : constant LwU54 := 16#0#;
    LW_RISCV_CSR_SMPUVA_WPRI1_RST : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SMPUVA_D_RST     : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUVA_A_RST     : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUVA_WPRI_RST  : constant LwU5  := 16#0#;
    LW_RISCV_CSR_SMPUVA_VLD_RST   : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_SMPUVA_Register is record
        Base  : LwU54;
        Wpri1 : LwU2;
        D     : LwU1;
        A     : LwU1;
        Wpri  : LwU5;
        Vld   : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUVA_Register use record
        Base  at 0 range 10 .. 63;
        Wpri1 at 0 range  8 ..  9;
        D     at 0 range  7 ..  7;
        A     at 0 range  6 ..  6;
        Wpri  at 0 range  1 ..  5;
        Vld   at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_SMPUHP_V0_RST   : constant LwU16 := 16#0#;
    LW_RISCV_CSR_SMPUHP_V1_RST   : constant LwU16 := 16#0#;
    LW_RISCV_CSR_SMPUHP_V2_RST   : constant LwU16 := 16#0#;
    LW_RISCV_CSR_SMPUHP_WPRI_RST : constant LwU16 := 16#0#;

    type LW_RISCV_CSR_SMPUHP_Register is record
        V0   : LwU16;
        V1   : LwU16;
        V2   : LwU16;
        Wpri : LwU16;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUHP_Register use record
        V0   at 0 range  0 .. 15;
        V1   at 0 range 16 .. 31;
        V2   at 0 range 32 .. 47;
        Wpri at 0 range 48 .. 63;
    end record;

    LW_RISCV_CSR_SMPUPA_DATA_RST           : constant LwU57 := 16#0#;
    LW_RISCV_CSR_SMPUPA_WPRI_RST           : constant LwU7  := 16#0#;
    LW_RISCV_CSR_SMPUPA_NAPOT_BASE_1K_INIT : constant       := 16#7f#;

    type LW_RISCV_CSR_SMPUPA_Register is record
        Data : LwU57;
        Wpri : LwU7;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUPA_Register use record
        Data at 0 range 7 .. 63;
        Wpri at 0 range 0 ..  6;
    end record;

    LW_RISCV_CSR_SMPURNG_SMPURNG_RANGE_RST : constant LwU54 := 16#0#;
    LW_RISCV_CSR_SMPURNG_WPRI_RST          : constant LwU10 := 16#0#;

    type LW_RISCV_CSR_SMPURNG_Register is record
        Smpurng_Range : LwU54;
        Wpri          : LwU10;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPURNG_Register use record
        Smpurng_Range at 0 range 10 .. 63;
        Wpri          at 0 range  0 ..  9;
    end record;

    LW_RISCV_CSR_SMPUATTR_WPRI2_RST     : constant LwU34 := 16#0#;
    LW_RISCV_CSR_SMPUATTR_VPR_RST       : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_WPR_RST       : constant LwU5  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_L2C_RD_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_L2C_WR_RST    : constant LwU2  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_COHERENT_RST  : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_CACHEABLE_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_WPRI1_RST     : constant LwU9  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_WPRI0_RST     : constant LwU3  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_SX_RST        : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_SW_RST        : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_SR_RST        : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_UX_RST        : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_UW_RST        : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SMPUATTR_UR_RST        : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_SMPUATTR_Register is record
        Wpri2     : LwU34;
        Vpr       : LwU1;
        Wpr       : LwU5;
        L2c_Rd    : LwU2;
        L2c_Wr    : LwU2;
        Coherent  : LwU1;
        Cacheable : LwU1;
        Wpri1     : LwU9;
        Wpri0     : LwU3;
        Sx        : LwU1;
        Sw        : LwU1;
        Sr        : LwU1;
        Ux        : LwU1;
        Uw        : LwU1;
        Ur        : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUATTR_Register use record
        Wpri2     at 0 range 30 .. 63;
        Vpr       at 0 range 29 .. 29;
        Wpr       at 0 range 24 .. 28;
        L2c_Rd    at 0 range 22 .. 23;
        L2c_Wr    at 0 range 20 .. 21;
        Coherent  at 0 range 19 .. 19;
        Cacheable at 0 range 18 .. 18;
        Wpri1     at 0 range  9 .. 17;
        Wpri0     at 0 range  6 ..  8;
        Sx        at 0 range  5 ..  5;
        Sw        at 0 range  4 ..  4;
        Sr        at 0 range  3 ..  3;
        Ux        at 0 range  2 ..  2;
        Uw        at 0 range  1 ..  1;
        Ur        at 0 range  0 ..  0;
    end record;

    LW_RISCV_CSR_SMPUIDX2_WPRI_RST : constant LwU62 := 16#0#;
    LW_RISCV_CSR_SMPUIDX2_IDX_RST  : constant LwU2  := 16#0#;

    type LW_RISCV_CSR_SMPUIDX2_Register is record
        Wpri : LwU62;
        Idx  : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUIDX2_Register use record
        Wpri at 0 range 2 .. 63;
        Idx  at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_SMPUVLD_VLD_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SMPUVLD_Register is record
        Vld : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUVLD_Register use record
        Vld at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SMPUDTY_DTY_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SMPUDTY_Register is record
        Dty : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUDTY_Register use record
        Dty at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SMPUACC_ACC_RST : constant LwU64 := 16#0#;

    type LW_RISCV_CSR_SMPUACC_Register is record
        Acc : LwU64;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMPUACC_Register use record
        Acc at 0 range 0 .. 63;
    end record;

    LW_RISCV_CSR_SMTE_WPRI2_RST : constant LwU59 := 16#0#;
    type LW_RISCV_CSR_SMTE_SPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SMTE_SPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_SMTE_WPRI1_RST : constant LwU1 := 16#0#;
    type LW_RISCV_CSR_SMTE_UPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_SMTE_UPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_SMTE_WPRI0_RST : constant LwU2 := 16#0#;

    type LW_RISCV_CSR_SMTE_Register is record
        Wpri2  : LwU59;
        Spm_En : LW_RISCV_CSR_SMTE_SPM_EN_Field;
        Wpri1  : LwU1;
        Upm_En : LW_RISCV_CSR_SMTE_UPM_EN_Field;
        Wpri0  : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SMTE_Register use record
        Wpri2  at 0 range 5 .. 63;
        Spm_En at 0 range 4 ..  4;
        Wpri1  at 0 range 3 ..  3;
        Upm_En at 0 range 2 ..  2;
        Wpri0  at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_SPMMASK_VAL_RST  : constant LwU16 := 16#0#;
    LW_RISCV_CSR_SPMMASK_WPRI_RST : constant LwU48 := 16#0#;

    type LW_RISCV_CSR_SPMMASK_Register is record
        Val  : LwU16;
        Wpri : LwU48;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SPMMASK_Register use record
        Val  at 0 range 48 .. 63;
        Wpri at 0 range  0 .. 47;
    end record;

    LW_RISCV_CSR_SSYSOPEN_WPRI_RST        : constant LwU56 := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_L2FLHDTY_RST    : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_L2CLNCOMP_RST   : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_L2PEERILW_RST   : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_L2SYSILW_RST    : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_FLUSH_RST       : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_TLBILWOP_RST    : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_TLBILWDATA1_RST : constant LwU1  := 16#0#;
    LW_RISCV_CSR_SSYSOPEN_BIND_RST        : constant LwU1  := 16#0#;

    type LW_RISCV_CSR_SSYSOPEN_Register is record
        Wpri        : LwU56;
        L2flhdty    : LwU1;
        L2clncomp   : LwU1;
        L2peerilw   : LwU1;
        L2sysilw    : LwU1;
        Flush       : LwU1;
        Tlbilwop    : LwU1;
        Tlbilwdata1 : LwU1;
        Bind        : LwU1;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_SSYSOPEN_Register use record
        Wpri        at 0 range 8 .. 63;
        L2flhdty    at 0 range 7 ..  7;
        L2clncomp   at 0 range 6 ..  6;
        L2peerilw   at 0 range 5 ..  5;
        L2sysilw    at 0 range 4 ..  4;
        Flush       at 0 range 3 ..  3;
        Tlbilwop    at 0 range 2 ..  2;
        Tlbilwdata1 at 0 range 1 ..  1;
        Bind        at 0 range 0 ..  0;
    end record;

    type LW_RISCV_CSR_DCACHEOP_ADDR_MODE_Field is (MODE_VA, MODE_PA) with
        Size => 1;
    for LW_RISCV_CSR_DCACHEOP_ADDR_MODE_Field use (MODE_VA => 16#0#, MODE_PA => 16#1#);
    type LW_RISCV_CSR_DCACHEOP_MODE_Field is (MODE_ILW_ALL, MODE_ILW_LINE, MODE_CLR_ALL, MODE_CLR_LINE) with
        Size => 2;
    for LW_RISCV_CSR_DCACHEOP_MODE_Field use (MODE_ILW_ALL => 16#0#, MODE_ILW_LINE => 16#1#, MODE_CLR_ALL => 16#2#, MODE_CLR_LINE => 16#3#);

    type LW_RISCV_CSR_DCACHEOP_Register is record
        Addr      : LwU58;
        Rs0       : LwU3;
        Addr_Mode : LW_RISCV_CSR_DCACHEOP_ADDR_MODE_Field;
        Mode      : LW_RISCV_CSR_DCACHEOP_MODE_Field;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_DCACHEOP_Register use record
        Addr      at 0 range 6 .. 63;
        Rs0       at 0 range 3 ..  5;
        Addr_Mode at 0 range 2 ..  2;
        Mode      at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_CFG_WIRI0_RST : constant LwU51 := 16#0#;
    type LW_RISCV_CSR_CFG_UPOSTIO_Field is (UPOSTIO_FALSE, UPOSTIO_TRUE) with
        Size => 1;
    for LW_RISCV_CSR_CFG_UPOSTIO_Field use (UPOSTIO_FALSE => 16#0#, UPOSTIO_TRUE => 16#1#);
    LW_RISCV_CSR_CFG_WIRI1_RST : constant LwU12 := 16#0#;

    type LW_RISCV_CSR_CFG_Register is record
        Wiri0   : LwU51;
        Upostio : LW_RISCV_CSR_CFG_UPOSTIO_Field;
        Wiri1   : LwU12;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_CFG_Register use record
        Wiri0   at 0 range 13 .. 63;
        Upostio at 0 range 12 .. 12;
        Wiri1   at 0 range  0 .. 11;
    end record;

    LW_RISCV_CSR_LWRRUID_WPRI_RST     : constant LwU56 := 16#0#;
    LW_RISCV_CSR_LWRRUID_ULWRRUID_RST : constant LwU8  := 16#0#;

    type LW_RISCV_CSR_LWRRUID_Register is record
        Wpri     : LwU56;
        Ulwrruid : LwU8;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_LWRRUID_Register use record
        Wpri     at 0 range 8 .. 63;
        Ulwrruid at 0 range 0 ..  7;
    end record;

    LW_RISCV_CSR_UMTE_WPRI1_RST : constant LwU61 := 16#0#;
    type LW_RISCV_CSR_UMTE_UPM_EN_Field is (EN_DISABLE, EN_ENABLE) with
        Size => 1;
    for LW_RISCV_CSR_UMTE_UPM_EN_Field use (EN_DISABLE => 16#0#, EN_ENABLE => 16#1#);
    LW_RISCV_CSR_UMTE_WPRI0_RST : constant LwU2 := 16#0#;

    type LW_RISCV_CSR_UMTE_Register is record
        Wpri1  : LwU61;
        Upm_En : LW_RISCV_CSR_UMTE_UPM_EN_Field;
        Wpri0  : LwU2;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_UMTE_Register use record
        Wpri1  at 0 range 3 .. 63;
        Upm_En at 0 range 2 ..  2;
        Wpri0  at 0 range 0 ..  1;
    end record;

    LW_RISCV_CSR_UPMMASK_VAL_RST  : constant LwU16 := 16#0#;
    LW_RISCV_CSR_UPMMASK_WPRI_RST : constant LwU48 := 16#0#;

    type LW_RISCV_CSR_UPMMASK_Register is record
        Val  : LwU16;
        Wpri : LwU48;
    end record with
        Size        => 64,
        Object_Size => 64;

    for LW_RISCV_CSR_UPMMASK_Register use record
        Val  at 0 range 48 .. 63;
        Wpri at 0 range  0 .. 47;
    end record;

    -- offsets of registers --
    LW_RISCV_CSR_MISA_Address           : constant := 16#301#;
    LW_RISCV_CSR_MVENDORID_Address      : constant := 16#f11#;
    LW_RISCV_CSR_MARCHID_Address        : constant := 16#f12#;
    LW_RISCV_CSR_MIMPID_Address         : constant := 16#f13#;
    LW_RISCV_CSR_MHARTID_Address        : constant := 16#f14#;
    LW_RISCV_CSR_MSTATUS_Address        : constant := 16#300#;
    LW_RISCV_CSR_MTVEC_Address          : constant := 16#305#;
    LW_RISCV_CSR_MEDELEG_Address        : constant := 16#302#;
    LW_RISCV_CSR_MIDELEG_Address        : constant := 16#303#;
    LW_RISCV_CSR_MIP_Address            : constant := 16#344#;
    LW_RISCV_CSR_MIE_Address            : constant := 16#304#;
    LW_RISCV_CSR_MHPMCOUNTER_0_Address  : constant := 16#b00#;
    LW_RISCV_CSR_MHPMCOUNTER_1_Address  : constant := 16#b01#;
    LW_RISCV_CSR_MHPMCOUNTER_2_Address  : constant := 16#b02#;
    LW_RISCV_CSR_MHPMCOUNTER_3_Address  : constant := 16#b03#;
    LW_RISCV_CSR_MHPMCOUNTER_4_Address  : constant := 16#b04#;
    LW_RISCV_CSR_MHPMCOUNTER_5_Address  : constant := 16#b05#;
    LW_RISCV_CSR_MHPMCOUNTER_6_Address  : constant := 16#b06#;
    LW_RISCV_CSR_MHPMCOUNTER_7_Address  : constant := 16#b07#;
    LW_RISCV_CSR_MHPMCOUNTER_8_Address  : constant := 16#b08#;
    LW_RISCV_CSR_MHPMCOUNTER_9_Address  : constant := 16#b09#;
    LW_RISCV_CSR_MHPMCOUNTER_10_Address : constant := 16#b0a#;
    LW_RISCV_CSR_MHPMCOUNTER_11_Address : constant := 16#b0b#;
    LW_RISCV_CSR_MHPMCOUNTER_12_Address : constant := 16#b0c#;
    LW_RISCV_CSR_MHPMCOUNTER_13_Address : constant := 16#b0d#;
    LW_RISCV_CSR_MHPMCOUNTER_14_Address : constant := 16#b0e#;
    LW_RISCV_CSR_MHPMCOUNTER_15_Address : constant := 16#b0f#;
    LW_RISCV_CSR_MHPMCOUNTER_16_Address : constant := 16#b10#;
    LW_RISCV_CSR_MHPMCOUNTER_17_Address : constant := 16#b11#;
    LW_RISCV_CSR_MHPMCOUNTER_18_Address : constant := 16#b12#;
    LW_RISCV_CSR_MHPMCOUNTER_19_Address : constant := 16#b13#;
    LW_RISCV_CSR_MHPMCOUNTER_20_Address : constant := 16#b14#;
    LW_RISCV_CSR_MHPMCOUNTER_21_Address : constant := 16#b15#;
    LW_RISCV_CSR_MHPMCOUNTER_22_Address : constant := 16#b16#;
    LW_RISCV_CSR_MHPMCOUNTER_23_Address : constant := 16#b17#;
    LW_RISCV_CSR_MHPMCOUNTER_24_Address : constant := 16#b18#;
    LW_RISCV_CSR_MHPMCOUNTER_25_Address : constant := 16#b19#;
    LW_RISCV_CSR_MHPMCOUNTER_26_Address : constant := 16#b1a#;
    LW_RISCV_CSR_MHPMCOUNTER_27_Address : constant := 16#b1b#;
    LW_RISCV_CSR_MHPMCOUNTER_28_Address : constant := 16#b1c#;
    LW_RISCV_CSR_MHPMCOUNTER_29_Address : constant := 16#b1d#;
    LW_RISCV_CSR_MHPMCOUNTER_30_Address : constant := 16#b1e#;
    LW_RISCV_CSR_MHPMCOUNTER_31_Address : constant := 16#b1f#;
    LW_RISCV_CSR_MCOUNTEREN_Address     : constant := 16#306#;
    LW_RISCV_CSR_MHPMEVENT_0_Address    : constant := 16#320#;
    LW_RISCV_CSR_MHPMEVENT_1_Address    : constant := 16#321#;
    LW_RISCV_CSR_MHPMEVENT_2_Address    : constant := 16#322#;
    LW_RISCV_CSR_MHPMEVENT_3_Address    : constant := 16#323#;
    LW_RISCV_CSR_MHPMEVENT_4_Address    : constant := 16#324#;
    LW_RISCV_CSR_MHPMEVENT_5_Address    : constant := 16#325#;
    LW_RISCV_CSR_MHPMEVENT_6_Address    : constant := 16#326#;
    LW_RISCV_CSR_MHPMEVENT_7_Address    : constant := 16#327#;
    LW_RISCV_CSR_MHPMEVENT_8_Address    : constant := 16#328#;
    LW_RISCV_CSR_MHPMEVENT_9_Address    : constant := 16#329#;
    LW_RISCV_CSR_MHPMEVENT_10_Address   : constant := 16#32a#;
    LW_RISCV_CSR_MHPMEVENT_11_Address   : constant := 16#32b#;
    LW_RISCV_CSR_MHPMEVENT_12_Address   : constant := 16#32c#;
    LW_RISCV_CSR_MHPMEVENT_13_Address   : constant := 16#32d#;
    LW_RISCV_CSR_MHPMEVENT_14_Address   : constant := 16#32e#;
    LW_RISCV_CSR_MHPMEVENT_15_Address   : constant := 16#32f#;
    LW_RISCV_CSR_MHPMEVENT_16_Address   : constant := 16#330#;
    LW_RISCV_CSR_MHPMEVENT_17_Address   : constant := 16#331#;
    LW_RISCV_CSR_MHPMEVENT_18_Address   : constant := 16#332#;
    LW_RISCV_CSR_MHPMEVENT_19_Address   : constant := 16#333#;
    LW_RISCV_CSR_MHPMEVENT_20_Address   : constant := 16#334#;
    LW_RISCV_CSR_MHPMEVENT_21_Address   : constant := 16#335#;
    LW_RISCV_CSR_MHPMEVENT_22_Address   : constant := 16#336#;
    LW_RISCV_CSR_MHPMEVENT_23_Address   : constant := 16#337#;
    LW_RISCV_CSR_MHPMEVENT_24_Address   : constant := 16#338#;
    LW_RISCV_CSR_MHPMEVENT_25_Address   : constant := 16#339#;
    LW_RISCV_CSR_MHPMEVENT_26_Address   : constant := 16#33a#;
    LW_RISCV_CSR_MHPMEVENT_27_Address   : constant := 16#33b#;
    LW_RISCV_CSR_MHPMEVENT_28_Address   : constant := 16#33c#;
    LW_RISCV_CSR_MHPMEVENT_29_Address   : constant := 16#33d#;
    LW_RISCV_CSR_MHPMEVENT_30_Address   : constant := 16#33e#;
    LW_RISCV_CSR_MHPMEVENT_31_Address   : constant := 16#33f#;
    LW_RISCV_CSR_MSCRATCH_Address       : constant := 16#340#;
    LW_RISCV_CSR_MEPC_Address           : constant := 16#341#;
    LW_RISCV_CSR_MCAUSE_Address         : constant := 16#342#;
    LW_RISCV_CSR_MCAUSE2_Address        : constant := 16#7d4#;
    LW_RISCV_CSR_MTVAL_Address          : constant := 16#343#;
    LW_RISCV_CSR_MMTE_Address           : constant := 16#7f4#;
    LW_RISCV_CSR_MPMMASK_Address        : constant := 16#7f5#;
    LW_RISCV_CSR_PMPCFG0_Address        : constant := 16#3a0#;
    LW_RISCV_CSR_PMPCFG2_Address        : constant := 16#3a2#;
    LW_RISCV_CSR_PMPADDR_0_Address      : constant := 16#3b0#;
    LW_RISCV_CSR_PMPADDR_1_Address      : constant := 16#3b1#;
    LW_RISCV_CSR_PMPADDR_2_Address      : constant := 16#3b2#;
    LW_RISCV_CSR_PMPADDR_3_Address      : constant := 16#3b3#;
    LW_RISCV_CSR_PMPADDR_4_Address      : constant := 16#3b4#;
    LW_RISCV_CSR_PMPADDR_5_Address      : constant := 16#3b5#;
    LW_RISCV_CSR_PMPADDR_6_Address      : constant := 16#3b6#;
    LW_RISCV_CSR_PMPADDR_7_Address      : constant := 16#3b7#;
    LW_RISCV_CSR_PMPADDR_8_Address      : constant := 16#3b8#;
    LW_RISCV_CSR_PMPADDR_9_Address      : constant := 16#3b9#;
    LW_RISCV_CSR_PMPADDR_10_Address     : constant := 16#3ba#;
    LW_RISCV_CSR_PMPADDR_11_Address     : constant := 16#3bb#;
    LW_RISCV_CSR_PMPADDR_12_Address     : constant := 16#3bc#;
    LW_RISCV_CSR_PMPADDR_13_Address     : constant := 16#3bd#;
    LW_RISCV_CSR_PMPADDR_14_Address     : constant := 16#3be#;
    LW_RISCV_CSR_PMPADDR_15_Address     : constant := 16#3bf#;
    LW_RISCV_CSR_MEXTPMPCFG0_Address    : constant := 16#7f0#;
    LW_RISCV_CSR_MEXTPMPCFG2_Address    : constant := 16#7f2#;
    LW_RISCV_CSR_MEXTPMPADDR_0_Address  : constant := 16#7e0#;
    LW_RISCV_CSR_MEXTPMPADDR_1_Address  : constant := 16#7e1#;
    LW_RISCV_CSR_MEXTPMPADDR_2_Address  : constant := 16#7e2#;
    LW_RISCV_CSR_MEXTPMPADDR_3_Address  : constant := 16#7e3#;
    LW_RISCV_CSR_MEXTPMPADDR_4_Address  : constant := 16#7e4#;
    LW_RISCV_CSR_MEXTPMPADDR_5_Address  : constant := 16#7e5#;
    LW_RISCV_CSR_MEXTPMPADDR_6_Address  : constant := 16#7e6#;
    LW_RISCV_CSR_MEXTPMPADDR_7_Address  : constant := 16#7e7#;
    LW_RISCV_CSR_MEXTPMPADDR_8_Address  : constant := 16#7e8#;
    LW_RISCV_CSR_MEXTPMPADDR_9_Address  : constant := 16#7e9#;
    LW_RISCV_CSR_MEXTPMPADDR_10_Address : constant := 16#7ea#;
    LW_RISCV_CSR_MEXTPMPADDR_11_Address : constant := 16#7eb#;
    LW_RISCV_CSR_MEXTPMPADDR_12_Address : constant := 16#7ec#;
    LW_RISCV_CSR_MEXTPMPADDR_13_Address : constant := 16#7ed#;
    LW_RISCV_CSR_MEXTPMPADDR_14_Address : constant := 16#7ee#;
    LW_RISCV_CSR_MEXTPMPADDR_15_Address : constant := 16#7ef#;
    LW_RISCV_CSR_MSCRATCH2_Address      : constant := 16#7c0#;
    LW_RISCV_CSR_MTIMECMP_Address       : constant := 16#7db#;
    LW_RISCV_CSR_MCFG_Address           : constant := 16#7d5#;
    LW_RISCV_CSR_MBPCFG_Address         : constant := 16#7dd#;
    LW_RISCV_CSR_MDCACHEOP_Address      : constant := 16#bd0#;
    LW_RISCV_CSR_MMPUCTL_Address        : constant := 16#7c9#;
    LW_RISCV_CSR_MLWRRUID_Address       : constant := 16#7d7#;
    LW_RISCV_CSR_MOPT_Address           : constant := 16#7d8#;
    LW_RISCV_CSR_MMISCOPEN_Address      : constant := 16#7f9#;
    LW_RISCV_CSR_MLDSTATTR_Address      : constant := 16#7c8#;
    LW_RISCV_CSR_MFETCHATTR_Address     : constant := 16#7df#;
    LW_RISCV_CSR_FFLAGS_Address         : constant := 16#1#;
    LW_RISCV_CSR_FRM_Address            : constant := 16#2#;
    LW_RISCV_CSR_FCSR_Address           : constant := 16#3#;
    LW_RISCV_CSR_HPMCOUNTER_0_Address   : constant := 16#c00#;
    LW_RISCV_CSR_HPMCOUNTER_1_Address   : constant := 16#c01#;
    LW_RISCV_CSR_HPMCOUNTER_2_Address   : constant := 16#c02#;
    LW_RISCV_CSR_HPMCOUNTER_3_Address   : constant := 16#c03#;
    LW_RISCV_CSR_HPMCOUNTER_4_Address   : constant := 16#c04#;
    LW_RISCV_CSR_HPMCOUNTER_5_Address   : constant := 16#c05#;
    LW_RISCV_CSR_HPMCOUNTER_6_Address   : constant := 16#c06#;
    LW_RISCV_CSR_HPMCOUNTER_7_Address   : constant := 16#c07#;
    LW_RISCV_CSR_HPMCOUNTER_8_Address   : constant := 16#c08#;
    LW_RISCV_CSR_HPMCOUNTER_9_Address   : constant := 16#c09#;
    LW_RISCV_CSR_HPMCOUNTER_10_Address  : constant := 16#c0a#;
    LW_RISCV_CSR_HPMCOUNTER_11_Address  : constant := 16#c0b#;
    LW_RISCV_CSR_HPMCOUNTER_12_Address  : constant := 16#c0c#;
    LW_RISCV_CSR_HPMCOUNTER_13_Address  : constant := 16#c0d#;
    LW_RISCV_CSR_HPMCOUNTER_14_Address  : constant := 16#c0e#;
    LW_RISCV_CSR_HPMCOUNTER_15_Address  : constant := 16#c0f#;
    LW_RISCV_CSR_HPMCOUNTER_16_Address  : constant := 16#c10#;
    LW_RISCV_CSR_HPMCOUNTER_17_Address  : constant := 16#c11#;
    LW_RISCV_CSR_HPMCOUNTER_18_Address  : constant := 16#c12#;
    LW_RISCV_CSR_HPMCOUNTER_19_Address  : constant := 16#c13#;
    LW_RISCV_CSR_HPMCOUNTER_20_Address  : constant := 16#c14#;
    LW_RISCV_CSR_HPMCOUNTER_21_Address  : constant := 16#c15#;
    LW_RISCV_CSR_HPMCOUNTER_22_Address  : constant := 16#c16#;
    LW_RISCV_CSR_HPMCOUNTER_23_Address  : constant := 16#c17#;
    LW_RISCV_CSR_HPMCOUNTER_24_Address  : constant := 16#c18#;
    LW_RISCV_CSR_HPMCOUNTER_25_Address  : constant := 16#c19#;
    LW_RISCV_CSR_HPMCOUNTER_26_Address  : constant := 16#c1a#;
    LW_RISCV_CSR_HPMCOUNTER_27_Address  : constant := 16#c1b#;
    LW_RISCV_CSR_HPMCOUNTER_28_Address  : constant := 16#c1c#;
    LW_RISCV_CSR_HPMCOUNTER_29_Address  : constant := 16#c1d#;
    LW_RISCV_CSR_HPMCOUNTER_30_Address  : constant := 16#c1e#;
    LW_RISCV_CSR_HPMCOUNTER_31_Address  : constant := 16#c1f#;
    LW_RISCV_CSR_MSPM_Address           : constant := 16#7d9#;
    LW_RISCV_CSR_MDBGCTL_Address        : constant := 16#7de#;
    LW_RISCV_CSR_MRSP_Address           : constant := 16#7da#;
    LW_RISCV_CSR_SPM_Address            : constant := 16#899#;
    LW_RISCV_CSR_RSP_Address            : constant := 16#89a#;
    LW_RISCV_CSR_TSELECT_Address        : constant := 16#7a0#;
    LW_RISCV_CSR_TDATA1_Address         : constant := 16#7a1#;
    LW_RISCV_CSR_TDATA2_Address         : constant := 16#7a2#;
    LW_RISCV_CSR_DCSR_Address           : constant := 16#7b0#;
    LW_RISCV_CSR_MBIND_Address          : constant := 16#bc0#;
    LW_RISCV_CSR_MTLBILWDATA1_Address   : constant := 16#bc1#;
    LW_RISCV_CSR_MTLBILWOP_Address      : constant := 16#bc2#;
    LW_RISCV_CSR_MFLUSH_Address         : constant := 16#bc3#;
    LW_RISCV_CSR_ML2SYSILW_Address      : constant := 16#bc4#;
    LW_RISCV_CSR_ML2PEERILW_Address     : constant := 16#bc5#;
    LW_RISCV_CSR_ML2CLNCOMP_Address     : constant := 16#bc6#;
    LW_RISCV_CSR_ML2FLHDTY_Address      : constant := 16#bc7#;
    LW_RISCV_CSR_MSYSOPEN_Address       : constant := 16#7f8#;
    LW_RISCV_CSR_MROMPROT0_Address      : constant := 16#7c4#;
    LW_RISCV_CSR_MROMPROT1_Address      : constant := 16#7c5#;
    LW_RISCV_CSR_MROMPROT2_Address      : constant := 16#7c6#;
    LW_RISCV_CSR_MROMPROT3_Address      : constant := 16#7c7#;
    LW_RISCV_CSR_BIND_Address           : constant := 16#8c0#;
    LW_RISCV_CSR_TLBILWDATA1_Address    : constant := 16#8c1#;
    LW_RISCV_CSR_TLBILWOP_Address       : constant := 16#8c2#;
    LW_RISCV_CSR_FLUSH_Address          : constant := 16#8c3#;
    LW_RISCV_CSR_L2SYSILW_Address       : constant := 16#8c4#;
    LW_RISCV_CSR_L2PEERILW_Address      : constant := 16#8c5#;
    LW_RISCV_CSR_L2CLNCOMP_Address      : constant := 16#8c6#;
    LW_RISCV_CSR_L2FLHDTY_Address       : constant := 16#8c7#;
    LW_RISCV_CSR_LWFENCEIO_Address      : constant := 16#800#;
    LW_RISCV_CSR_LWFENCEMEM_Address     : constant := 16#801#;
    LW_RISCV_CSR_LWFENCEALL_Address     : constant := 16#802#;
    LW_RISCV_CSR_SSTATUS_Address        : constant := 16#100#;
    LW_RISCV_CSR_STVEC_Address          : constant := 16#105#;
    LW_RISCV_CSR_SSCRATCH_Address       : constant := 16#140#;
    LW_RISCV_CSR_SIP_Address            : constant := 16#144#;
    LW_RISCV_CSR_SIE_Address            : constant := 16#104#;
    LW_RISCV_CSR_STVAL_Address          : constant := 16#143#;
    LW_RISCV_CSR_SEPC_Address           : constant := 16#141#;
    LW_RISCV_CSR_SCAUSE_Address         : constant := 16#142#;
    LW_RISCV_CSR_SCAUSE2_Address        : constant := 16#5d4#;
    LW_RISCV_CSR_SATP_Address           : constant := 16#180#;
    LW_RISCV_CSR_SCOUNTEREN_Address     : constant := 16#106#;
    LW_RISCV_CSR_SSPM_Address           : constant := 16#5d9#;
    LW_RISCV_CSR_SRSP_Address           : constant := 16#5da#;
    LW_RISCV_CSR_SLWRRUID_Address       : constant := 16#5d7#;
    LW_RISCV_CSR_SCFG_Address           : constant := 16#5d5#;
    LW_RISCV_CSR_SBPCFG_Address         : constant := 16#5dd#;
    LW_RISCV_CSR_SMISCOPEN_Address      : constant := 16#5f9#;
    LW_RISCV_CSR_SSCRATCH2_Address      : constant := 16#5c0#;
    LW_RISCV_CSR_SMPUCTL_Address        : constant := 16#5c9#;
    LW_RISCV_CSR_SMPUIDX_Address        : constant := 16#5ca#;
    LW_RISCV_CSR_SMPUVA_Address         : constant := 16#5cb#;
    LW_RISCV_CSR_SMPUHP_Address         : constant := 16#5cd#;
    LW_RISCV_CSR_SMPUPA_Address         : constant := 16#5cc#;
    LW_RISCV_CSR_SMPURNG_Address        : constant := 16#5ce#;
    LW_RISCV_CSR_SMPUATTR_Address       : constant := 16#5cf#;
    LW_RISCV_CSR_SMPUIDX2_Address       : constant := 16#5d0#;
    LW_RISCV_CSR_SMPUVLD_Address        : constant := 16#5d1#;
    LW_RISCV_CSR_SMPUDTY_Address        : constant := 16#5d2#;
    LW_RISCV_CSR_SMPUACC_Address        : constant := 16#5d3#;
    LW_RISCV_CSR_SMTE_Address           : constant := 16#5f4#;
    LW_RISCV_CSR_SPMMASK_Address        : constant := 16#5f5#;
    LW_RISCV_CSR_SSYSOPEN_Address       : constant := 16#5f8#;
    LW_RISCV_CSR_DCACHEOP_Address       : constant := 16#8d0#;
    LW_RISCV_CSR_CFG_Address            : constant := 16#895#;
    LW_RISCV_CSR_LWRRUID_Address        : constant := 16#897#;
    LW_RISCV_CSR_UMTE_Address           : constant := 16#8b4#;
    LW_RISCV_CSR_UPMMASK_Address        : constant := 16#8b5#;
end Lw_Ref_Dev_Riscv_Csr_64;
