-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;         use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;      use Dev_Prgnlcl;

package Device_Map_Policy_Types
is

    -- Partition Policy Device Map types
    SUBMMODE_READ_ENABLE   : constant := 16#1#;
    SUBMMODE_READ_DISABLE  : constant := 16#0#;
    SUBMMODE_WRITE_ENABLE  : constant := 16#1#;
    SUBMMODE_WRITE_DISABLE : constant := 16#0#;

    MMODE_READ_ENABLE   : constant := 16#1#;
    MMODE_READ_DISABLE  : constant := 16#0#;
    MMODE_WRITE_ENABLE  : constant := 16#1#;
    MMODE_WRITE_DISABLE : constant := 16#0#;

    type SUBMMODE_GROUP is record
        Readable : LwU1;
        Writable : LwU1;
        Reserved : LwU2;
    end record
    with
        Size => 4;

    for SUBMMODE_GROUP use record
        Readable at 0 range 0 .. 0;
        Writable at 0 range 1 .. 1;
        Reserved at 0 range 2 .. 3;
    end record;

    subtype MMODE_GROUP is SUBMMODE_GROUP;

    type Plcy_Device_Map_Group_0 is record
        MMODE       : SUBMMODE_GROUP;
        RISCV_CTL   : SUBMMODE_GROUP;
        PIC         : SUBMMODE_GROUP;
        TIMER       : SUBMMODE_GROUP;
        HOSTIF      : SUBMMODE_GROUP;
        DMA         : SUBMMODE_GROUP;
        PMB         : SUBMMODE_GROUP;
        DIO         : SUBMMODE_GROUP;
    end record
    with
        Size        => 32,
        Object_Size => 32;

    for Plcy_Device_Map_Group_0 use record
        MMODE       at 0 range 0 .. 3;
        RISCV_CTL   at 0 range 4 .. 7;
        PIC         at 0 range 8 .. 11;
        TIMER       at 0 range 12 .. 15;
        HOSTIF      at 0 range 16 .. 19;
        DMA         at 0 range 20 .. 23;
        PMB         at 0 range 24 .. 27;
        DIO         at 0 range 28 .. 31;
    end record;

    type Plcy_Device_Map_Group_1 is record
        KEY         : SUBMMODE_GROUP;
        DEBUG       : SUBMMODE_GROUP;
        SHA         : SUBMMODE_GROUP;
        KMEM        : SUBMMODE_GROUP;
        BROM        : SUBMMODE_GROUP;
        ROM_PATCH   : SUBMMODE_GROUP;
        IOPMP       : SUBMMODE_GROUP;
        NOACCESS    : SUBMMODE_GROUP;
    end record
    with
        Size        => 32,
        Object_Size => 32;

    for Plcy_Device_Map_Group_1 use record
        KEY         at 0 range 0 .. 3;
        DEBUG       at 0 range 4 .. 7;
        SHA         at 0 range 8 .. 11;
        KMEM        at 0 range 12 .. 15;
        BROM        at 0 range 16 .. 19;
        ROM_PATCH   at 0 range 20 .. 23;
        IOPMP       at 0 range 24 .. 27;
        NOACCESS    at 0 range 28 .. 31;
    end record;

    type Plcy_Device_Map_Group_2 is record
        SCP         : SUBMMODE_GROUP;
        FBIF        : SUBMMODE_GROUP;
        FALCON_ONLY : SUBMMODE_GROUP;
        PRGN_CTL    : SUBMMODE_GROUP;
        SCR_GRP0    : SUBMMODE_GROUP;
        SCR_GRP1    : SUBMMODE_GROUP;
        SCR_GRP2    : SUBMMODE_GROUP;
        SCR_GRP3    : SUBMMODE_GROUP;
    end record
    with
        Size        => 32,
        Object_Size => 32;

    for Plcy_Device_Map_Group_2 use record
        SCP         at 0 range 0 .. 3;
        FBIF        at 0 range 4 .. 7;
        FALCON_ONLY at 0 range 8 .. 11;
        PRGN_CTL    at 0 range 12 .. 15;
        SCR_GRP0    at 0 range 16 .. 19;
        SCR_GRP1    at 0 range 20 .. 23;
        SCR_GRP2    at 0 range 24 .. 27;
        SCR_GRP3    at 0 range 28 .. 31;
    end record;

    type Plcy_Device_Map_Group_3 is record
        PLM         : SUBMMODE_GROUP;
        HUB_DIO     : SUBMMODE_GROUP;
        RESET       : SUBMMODE_GROUP;
        READ_ONLY   : SUBMMODE_GROUP;
        Reserved    : LwU16;
    end record
    with
        Size        => 32,
        Object_Size => 32;

    for Plcy_Device_Map_Group_3 use record
        PLM         at 0 range 0 .. 3;
        HUB_DIO     at 0 range 4 .. 7;
        RESET       at 0 range 8 .. 11;
        READ_ONLY   at 0 range 12 .. 15;
        Reserved    at 0 range 16 .. 31;
    end record;

end Device_Map_Policy_Types;
