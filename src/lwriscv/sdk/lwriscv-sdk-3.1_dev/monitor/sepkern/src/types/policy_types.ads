-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;             use Lw_Types;
with Types;                use Types;
with Mpu_Policy_Types;     use Mpu_Policy_Types;
with Iopmp_Policy_Types;   use Iopmp_Policy_Types;
with Dev_Riscv_Csr_64;     use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;          use Dev_Prgnlcl;
with Lw_Riscv_Address_Map; use Lw_Riscv_Address_Map;

package Policy_Types
is

    -- Partition Policy Entry Point Address type
    -- Doesn't take into account SK size, consider to add IMEM_LIMIT
    PARTITION_ENTRY_POINT_ADDRESS_MIN : constant := LW_RISCV_AMAP_IMEM_START;
    PARTITION_ENTRY_POINT_ADDRESS_MAX : constant := LW_RISCV_AMAP_IMEM_END - 4;
    subtype Plcy_Entry_Point_Addr_Type is LwU64 range PARTITION_ENTRY_POINT_ADDRESS_MIN .. PARTITION_ENTRY_POINT_ADDRESS_MAX;
    ---------------------------------------------------------------------------

    -- Partition Policy Ucode ID type
    subtype Plcy_Ucode_Id_Type is LwU8 range 16#0# .. 16#10#;
    ---------------------------------------------------------------------------

    -- Partition Policy Switchable_To type
    type Switchable_To_Type is (DENIED, PERMITTED)
    with
        Size => 1;
    for Switchable_To_Type use (DENIED => 16#0#, PERMITTED => 16#1#);

    type Plcy_Switchable_To_Array is array(Partition_ID) of Switchable_To_Type;
    ---------------------------------------------------------------------------

    subtype Plcy_Splm_Type is LwU4 range 2#0001# .. 2#1111#;

    -- Partition Policy Supervisor mode System Privilege Mask type
    type Plcy_SSPM is record
        Splm  : Plcy_Splm_Type;
        Ssecm : LW_RISCV_CSR_SSPM_SSECM_Field;
    end record;

    for Plcy_SSPM use record
        Splm  at 0 range 0 .. 7;
        Ssecm at 0 range 8 .. 15;
    end record;
    ---------------------------------------------------------------------------

    -- Partition Policy SCP HW Secret Mask type
    type Plcy_Secret_Mask is record
        Scp_Secret_Mask0      : LwU32;
        Scp_Secret_Mask1      : LwU32;
        Scp_Secret_Mask_lock0 : LwU32;
        Scp_Secret_Mask_lock1 : LwU32;
    end record
    with
        Size => 128,
        Object_Size => 128;

    for Plcy_Secret_Mask use record
        Scp_Secret_Mask0      at 0 range 0  .. 31;
        Scp_Secret_Mask1      at 0 range 32 .. 63;
        Scp_Secret_Mask_lock0 at 0 range 64 .. 95;
        Scp_Secret_Mask_lock1 at 0 range 96 .. 127;
    end record;
    ---------------------------------------------------------------------------

    -- Partition Policy Debug Control type
    type Plcy_Debug_Control is record
        Debug_Ctrl      : LW_PRGNLCL_RISCV_DBGCTL_Register;
        Debug_Ctrl_Lock : LW_PRGNLCL_RISCV_DBGCTL_LOCK_Register;
    end record
    with
        Size => 64,
        Object_Size => 64;

    for Plcy_Debug_Control use record
        Debug_Ctrl      at 0 range 0 .. 31;
        Debug_Ctrl_Lock at 0 range 32 .. 63;
    end record;
    ---------------------------------------------------------------------------

    -- Partition Policy Core PMP types
    type PMP_Address_Mode is (OFF, TOR, NA4, NAPOT)
    with
        Size => 2;
    for PMP_Address_Mode use (OFF => 16#0#, TOR => 16#1#, NA4 => 16#2#, NAPOT => 16#3#);

    type PMP_Access is (DENIED, PERMITTED)
    with
        Size => 1;
    for PMP_Access use (DENIED => 16#0#, PERMITTED => 16#1#);

    type PMP_Access_Mode is record
        Reserved          : LwU1;
        Exelwtion_Access  : PMP_Access;
        Write_Access      : PMP_Access;
        Read_Access       : PMP_Access;
    end record
    with
        Size => 4;

    for PMP_Access_Mode use record
        Reserved          at 0 range  3 ..  3;
        Exelwtion_Access  at 0 range  2 ..  2;
        Write_Access      at 0 range  1 ..  1;
        Read_Access       at 0 range  0 ..  0;
    end record;

    type PMP_Address is new LwU64;

    type Core_Pmp_Entry is record
        Access_Mode       : PMP_Access_Mode;
        Reserved          : LwU2;
        Address_Mode      : PMP_Address_Mode;
        Addr              : PMP_Address;
    end record
    with
        Size        => 72,
        Object_Size => 72;

    for Core_Pmp_Entry use record
        Access_Mode  at 0 range 68 ..  71;
        Reserved     at 0 range 66 ..  67;
        Address_Mode at 0 range 64 .. 65;
        Addr         at 0 range 0 ..  63;
    end record;

    CORE_PMP_NUM_OF_PMP_ENTRY     : constant := 16;
    CORE_PMP_PMP_ARRAY_LAST_INDEX : constant := CORE_PMP_NUM_OF_PMP_ENTRY - 1;

    type Plcy_Core_Pmp_Array_Index is range 0 .. CORE_PMP_PMP_ARRAY_LAST_INDEX;
    type Plcy_Core_Pmp is array (Plcy_Core_Pmp_Array_Index) of Core_Pmp_Entry
    with
        Size => 1152,
        Object_Size => 1152;
    ---------------------------------------------------------------------------

    -- Partition Policy IO-PMP types
    IOPMP_NUM_OF_IO_PMP_CFG_ENTRY : constant := 8;
    IOPMP_IO_PMP_ARRAY_LAST_INDEX : constant := IOPMP_NUM_OF_IO_PMP_CFG_ENTRY - 1;

    type Io_Pmp_Array_Index is range 0 .. IOPMP_IO_PMP_ARRAY_LAST_INDEX;

    type Plcy_Io_Pmp is array (Io_Pmp_Array_Index) of Io_Pmp_Entry
    with
        Size => 1024,
        Object_Size => 1024;
    ---------------------------------------------------------------------------

    type SBI_Access is (DENIED, PERMITTED)
    with
        Size => 1;
    for SBI_Access use (DENIED => 16#0#, PERMITTED => 16#1#);

    type Plcy_SBI_Access is record
        Release_Priv_Lockdown : SBI_Access;
        TraceCtl              : SBI_Access;
        FBIF_TransCfg         : SBI_Access;
        FBIF_RegionCfg        : SBI_Access;
        TFBIF_TransCfg        : SBI_Access;
        TFBIF_RegionCfg       : SBI_Access;
    end record;
    for Plcy_SBI_Access use record
        Release_Priv_Lockdown at 0 range 0 .. 0;
        TraceCtl              at 0 range 1 .. 1;
        FBIF_TransCfg         at 0 range 2 .. 2;
        FBIF_RegionCfg        at 0 range 3 .. 3;
        TFBIF_TransCfg        at 0 range 4 .. 4;
        TFBIF_RegionCfg       at 0 range 5 .. 5;
    end record;

    ALLOW_ALL_SBI : constant Plcy_SBI_Access := Plcy_SBI_Access'(others => PERMITTED);

end Policy_Types;
