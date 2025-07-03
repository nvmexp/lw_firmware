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

package Iopmp_Policy_Types
is

    -- Partition Policy IO-PMP types
    type Io_Pmp_Master is (DISABLE, ENABLE)
    with
        Size => 1;
    for Io_Pmp_Master use (DISABLE => 16#0#, ENABLE => 16#1#);

    type Plcy_Io_Pmp_Cfg is record
        Read         : LW_PRGNLCL_RISCV_IOPMP_CFG_READ_Field;
        Write        : LW_PRGNLCL_RISCV_IOPMP_CFG_WRITE_Field;
        Fbdma_Imem   : Io_Pmp_Master;
        Fbdma_Dmem   : Io_Pmp_Master;
        Sha          : Io_Pmp_Master;
        Cpdma        : Io_Pmp_Master;
        PMB0         : Io_Pmp_Master;
        PMB1         : Io_Pmp_Master;
        PMB2         : Io_Pmp_Master;
        PMB3         : Io_Pmp_Master;
        Gdma_Chan0_0 : Io_Pmp_Master;
        Gdma_Chan0_1 : Io_Pmp_Master;
        Gdma_Chan1_0 : Io_Pmp_Master;
        Gdma_Chan1_1 : Io_Pmp_Master;
        Gdma_Chan2_0 : Io_Pmp_Master;
        Gdma_Chan2_1 : Io_Pmp_Master;
        Gdma_Chan3_0 : Io_Pmp_Master;
        Gdma_Chan3_1 : Io_Pmp_Master;
        SE_AES_0     : Io_Pmp_Master;
        SE_AES_1     : Io_Pmp_Master;
        SE_HASH      : Io_Pmp_Master;
        Lock         : LW_PRGNLCL_RISCV_IOPMP_CFG_LOCK_Field;
    end record
    with
        Size        => 32,
        Object_Size => 32;

    for Plcy_Io_Pmp_Cfg use record
        Read         at 0 range 0 .. 0;
        Write        at 0 range 1 .. 1;
        Fbdma_Imem   at 0 range 4 .. 4;
        Fbdma_Dmem   at 0 range 5 .. 5;
        Sha          at 0 range 6 .. 6;
        Cpdma        at 0 range 7 .. 7;
        PMB0         at 0 range 8 .. 8;
        PMB1         at 0 range 9 .. 9;
        PMB2         at 0 range 10 .. 10;
        PMB3         at 0 range 11 .. 11;
        Gdma_Chan0_0 at 0 range 12 .. 12;
        Gdma_Chan0_1 at 0 range 13 .. 13;
        Gdma_Chan1_0 at 0 range 14 .. 14;
        Gdma_Chan1_1 at 0 range 15 .. 15;
        Gdma_Chan2_0 at 0 range 16 .. 16;
        Gdma_Chan2_1 at 0 range 17 .. 17;
        Gdma_Chan3_0 at 0 range 18 .. 18;
        Gdma_Chan3_1 at 0 range 19 .. 19;
        SE_AES_0     at 0 range 20 .. 20;
        SE_AES_1     at 0 range 21 .. 21;
        SE_HASH      at 0 range 22 .. 22;
        Lock         at 0 range 31 .. 31;
    end record;

    type Io_Pmp_Address_Mode is (OFF, NAPOT)
    with
        Size => 2;
    for Io_Pmp_Address_Mode use (OFF => 16#0#, NAPOT => 16#3#);

    type Io_Pmp_Entry is record
        Cfg             : Plcy_Io_Pmp_Cfg;
        Mode            : Io_Pmp_Address_Mode;
        Addr_Lo_1k      : LwU7;
        Addr_Lo_Above1k : LwU25;
        Addr_Hi         : LwU29;
    end record
    with
        Size        => 128,
        Object_Size => 128;

end Iopmp_Policy_Types;
