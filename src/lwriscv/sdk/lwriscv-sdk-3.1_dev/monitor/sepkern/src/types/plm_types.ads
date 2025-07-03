-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package PLM_Types
is

    LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_DEFAULT_PRIV_LEVEL   : constant LwU4  := 16#8#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED   : constant LwU4  := 16#f#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_DISABLED  : constant LwU4  := 16#0#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ONLY_LEVEL3_ENABLED  : constant LwU4  := 16#8#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED       : constant LwU4  := 16#c#;

    LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_DEFAULT_PRIV_LEVEL  : constant LwU4  := 16#8#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_ENABLED  : constant LwU4  := 16#f#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_DISABLED : constant LwU4  := 16#0#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ONLY_LEVEL3_ENABLED : constant LwU4  := 16#8#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_ENABLED      : constant LwU4  := 16#c#;

    LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_DISABLED   : constant LwU20 := 16#0#;
    LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED    : constant LwU20 := 16#f_ffff#;

    type LW_PRGNLCL_PRIV_LEVEL_MASK_READ_VIOLATION_Field is (VIOLATION_SOLDIER_ON, VIOLATION_REPORT_ERROR) with
        Size => 1;
    for LW_PRGNLCL_PRIV_LEVEL_MASK_READ_VIOLATION_Field use (VIOLATION_SOLDIER_ON => 16#0#, VIOLATION_REPORT_ERROR => 16#1#);
    type LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_VIOLATION_Field is (VIOLATION_SOLDIER_ON, VIOLATION_REPORT_ERROR) with
        Size => 1;
    for LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_VIOLATION_Field use (VIOLATION_SOLDIER_ON => 16#0#, VIOLATION_REPORT_ERROR => 16#1#);
    type LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_Field is (CONTROL_LOWERED, CONTROL_BLOCKED) with
        Size => 1;
    for LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_Field use (CONTROL_LOWERED => 16#0#, CONTROL_BLOCKED => 16#1#);
    type LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_Field is (CONTROL_LOWERED, CONTROL_BLOCKED) with
        Size => 1;
    for LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_Field use (CONTROL_LOWERED => 16#0#, CONTROL_BLOCKED => 16#1#);

    type LW_PRGNLCL_PRIV_LEVEL_MASK_Register is record
        Read_Protection      : LwU4;
        Write_Protection     : LwU4;
        Read_Violation       : LW_PRGNLCL_PRIV_LEVEL_MASK_READ_VIOLATION_Field;
        Write_Violation      : LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_VIOLATION_Field;
        Source_Read_Control  : LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_READ_CONTROL_Field;
        Source_Write_Control : LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_WRITE_CONTROL_Field;
        Source_Enable        : LwU20;
    end record with
        Size        => 32,
        Object_Size => 32;

    for LW_PRGNLCL_PRIV_LEVEL_MASK_Register use record
        Read_Protection      at 0 range  0 ..  3;
        Write_Protection     at 0 range  4 ..  7;
        Read_Violation       at 0 range  8 ..  8;
        Write_Violation      at 0 range  9 ..  9;
        Source_Read_Control  at 0 range 10 .. 10;
        Source_Write_Control at 0 range 11 .. 11;
        Source_Enable        at 0 range 12 .. 31;
    end record;

    type PLM_Pair is record
       Address : LwU32;
       Data    : LW_PRGNLCL_PRIV_LEVEL_MASK_Register;
    end record;

    type PLM_Array_Type is array(Natural range <>) of PLM_Pair;

    PRIV_LEVEL_MASK_ALL_DISABLE : constant LW_PRGNLCL_PRIV_LEVEL_MASK_Register := (
        Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_DISABLED,
        Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_DISABLED,
        Read_Violation       => VIOLATION_REPORT_ERROR,
        Write_Violation      => VIOLATION_REPORT_ERROR,
        Source_Read_Control  => CONTROL_BLOCKED,
        Source_Write_Control => CONTROL_BLOCKED,
        Source_Enable        => LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_DISABLED
    );

    PRIV_LEVEL_MASK_L0R_L3W_ALL_SOURCE : constant LW_PRGNLCL_PRIV_LEVEL_MASK_Register := (
        Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
        Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ONLY_LEVEL3_ENABLED,
        Read_Violation       => VIOLATION_REPORT_ERROR,
        Write_Violation      => VIOLATION_REPORT_ERROR,
        Source_Read_Control  => CONTROL_BLOCKED,
        Source_Write_Control => CONTROL_BLOCKED,
        Source_Enable        => LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED
    );

    PRIV_LEVEL_MASK_L2R_L2W_ALL_SOURCE : constant LW_PRGNLCL_PRIV_LEVEL_MASK_Register := (
        Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL2_ENABLED,
        Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL2_ENABLED,
        Read_Violation       => VIOLATION_REPORT_ERROR,
        Write_Violation      => VIOLATION_REPORT_ERROR,
        Source_Read_Control  => CONTROL_BLOCKED,
        Source_Write_Control => CONTROL_BLOCKED,
        Source_Enable        => LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED
    );

    PRIV_LEVEL_MASK_RO_ALL_SOURCE : constant LW_PRGNLCL_PRIV_LEVEL_MASK_Register := (
        Read_Protection      => LW_PRGNLCL_PRIV_LEVEL_MASK_READ_PROTECTION_ALL_LEVELS_ENABLED,
        Write_Protection     => LW_PRGNLCL_PRIV_LEVEL_MASK_WRITE_PROTECTION_ALL_LEVELS_DISABLED,
        Read_Violation       => VIOLATION_REPORT_ERROR,
        Write_Violation      => VIOLATION_REPORT_ERROR,
        Source_Read_Control  => CONTROL_BLOCKED,
        Source_Write_Control => CONTROL_BLOCKED,
        Source_Enable        => LW_PRGNLCL_PRIV_LEVEL_MASK_SOURCE_ENABLE_ALL_SOURCES_ENABLED
    );

    Null_PLM_Pair : constant PLM_Pair := PLM_Pair'(
        Address => 0,
        Data    => PRIV_LEVEL_MASK_ALL_DISABLE
    );

end PLM_Types;
