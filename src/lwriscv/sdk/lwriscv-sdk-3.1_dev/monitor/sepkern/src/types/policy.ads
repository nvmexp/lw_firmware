-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;         use Lw_Types;
with Types;            use Types;
with Policy_Types;     use Policy_Types;
with Mpu_Policy_Types; use Mpu_Policy_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;      use Dev_Prgnlcl;

package Policy
is

    -- Partition Policy Device Map types
    DEVICEMAP_NUM_OF_CFG_ENTRY : constant := 4;
    DEVICEMAP_CFG_ARRAY_LAST_INDEX : constant := DEVICEMAP_NUM_OF_CFG_ENTRY - 1; -- 3

    subtype Device_Map_Cfg_Array_Index is LwU8 range 0 .. DEVICEMAP_CFG_ARRAY_LAST_INDEX;

    type Device_Map_Cfg_Array is array (Device_Map_Cfg_Array_Index) of LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_Register;

    type Device_Map_Cfg is record
        Cfg : Device_Map_Cfg_Array;
    end record
    with
        Size => 128,
        Object_Size => 128;
    ---------------------------------------------------------------------------

    -- Partition Policy Core PMP types
    COREPMP_NUM_OF_COREPMP_CFG_ENTRY     : constant := 8;
    COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY : constant := 8;

    COREPMP_CORE_PMP_ADDR_ARRAY_LAST_INDEX     : constant := COREPMP_NUM_OF_COREPMP_CFG_ENTRY - 1;
    COREPMP_EXT_CORE_PMP_ADDR_ARRAY_LAST_INDEX : constant := COREPMP_NUM_OF_EXT_COREPMP_CFG_ENTRY - 1;

    type Core_Pmp_Addr_Cfg_Array_Index is range 0 .. COREPMP_CORE_PMP_ADDR_ARRAY_LAST_INDEX;
    type Core_Pmp_Cfg_Addr_Array is array (Core_Pmp_Addr_Cfg_Array_Index) of LW_RISCV_CSR_PMPADDR_Register
    with
        Size => 512,
        Object_Size => 512;

    type Ext_Core_Pmp_Cfg_Addr_Array_Index is range 0 .. COREPMP_EXT_CORE_PMP_ADDR_ARRAY_LAST_INDEX;
    type Ext_Core_Pmp_Cfg_Addr_Array is array (Ext_Core_Pmp_Cfg_Addr_Array_Index) of LW_RISCV_CSR_MEXTPMPADDR_Register
    with
        Size => 512,
        Object_Size => 512;

    type Core_Pmp_Cfg is record
        Cfg2     : LW_RISCV_CSR_PMPCFG2_Register;
        Addr     : Core_Pmp_Cfg_Addr_Array;
        Ext_Cfg0 : LW_RISCV_CSR_MEXTPMPCFG0_Register;
        Ext_Addr : Ext_Core_Pmp_Cfg_Addr_Array;
    end record
    with
        Size => 1152,
        Object_Size => 1152;

    for Core_Pmp_Cfg use record
        Cfg2     at 0 range 0 .. 63;
        Addr     at 0 range 64 .. 575;
        Ext_Cfg0 at 0 range 576 .. 639;
        Ext_Addr at 0 range 640 .. 1151;
    end record;
    ---------------------------------------------------------------------------

    -- Partition Policy IO-PMP types
    type Io_Pmp_Mode_Cfg is record
        Io_Pmp_Mode0      : LwU32;
    end record
    with
        Size => 32,
        Object_Size => 32;

    for Io_Pmp_Mode_Cfg use record
        Io_Pmp_Mode0      at 0 range 0 .. 31;
    end record;

    -- Policy IO PMP definition
    IOPMP_START_ENTRY : constant := 8;

    -- MPSK owned entries clearing mask in Mode register
    -- 2bits per entry, starting at entry index 8, with 8 used entries
    IOPMP_MODE_CLEAR_MASK : constant LwU32 := 16#0000_FFFF#;

    type Io_Pmp_Cfg_Array is array (Io_Pmp_Array_Index) of LW_PRGNLCL_RISCV_IOPMP_CFG_Register
    with
        Size => 256,
        Object_Size => 256;

    type Io_Pmp_Addr_Lo_Array is array (Io_Pmp_Array_Index) of LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Register
    with
        Size => 256,
        Object_Size => 256;

    type Io_Pmp_Addr_Hi_Array is array (Io_Pmp_Array_Index) of LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Register
    with
        Size => 256,
        Object_Size => 256;

    type Io_Pmp_Cfg is record
        Cfg     : Io_Pmp_Cfg_Array;
        Addr_Lo : Io_Pmp_Addr_Lo_Array;
        Addr_Hi : Io_Pmp_Addr_Hi_Array;
    end record
    with
        Size => 768,
        Object_Size => 768;

    for Io_Pmp_Cfg use record
        Cfg     at 0 range 0 .. 255;
        Addr_Lo at 0 range 256 .. 511;
        Addr_Hi at 0 range 512 .. 767;
    end record;
    ---------------------------------------------------------------------------

    -- SK Internal Partition Policy type
    type Policy is record
        Switchable_To       : aliased Plcy_Switchable_To_Array;
        Entry_Point_Address : aliased Plcy_Entry_Point_Addr_Type;
        Ucode_Id            : aliased Plcy_Ucode_Id_Type;
        Sspm                : aliased LW_RISCV_CSR_SSPM_Register;
        Secret_Mask         : aliased Plcy_Secret_Mask;
        Debug_Control       : aliased Plcy_Debug_Control;
        Mpu_Control         : aliased Plcy_Mpu_Control;
        Device_Map          : aliased Device_Map_Cfg;
        Core_Pmp            : aliased Core_Pmp_Cfg;
        Io_Pmp_Mode         : aliased Io_Pmp_Mode_Cfg;
        Io_Pmp              : aliased Io_Pmp_Cfg;
#if POLICY_CONFIG_SBI
        SBI_Access_Config   : aliased Plcy_SBI_Access;
#end if;
#if POLICY_CONFIG_PRIV_LOCKDOWN
        Priv_Lockdown       : aliased LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN_Register;
#end if;
    end record;

    -------------------------------------------------------------------------

    -- SK Internal Partition Policy Array type
    type Partition_Policy_Array is array(Partition_ID) of Policy;
    -------------------------------------------------------------------------

end Policy;
