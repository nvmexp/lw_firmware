-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

package Nv_Riscv_Address_Map
is

    LW_RISCV_AMAP_IROM_START    : constant := 16#0000000000080000#;
    LW_RISCV_AMAP_IROM_SIZE     : constant := 16#0000000000080000#;
    LW_RISCV_AMAP_IROM_END      : constant := 16#0000000000100000#;
    LW_RISCV_AMAP_IMEM_START    : constant := 16#0000000000100000#;
    LW_RISCV_AMAP_IMEM_SIZE     : constant := 16#0000000000080000#;
    LW_RISCV_AMAP_IMEM_END      : constant := 16#0000000000180000#;
    LW_RISCV_AMAP_DMEM_START    : constant := 16#0000000000180000#;
    LW_RISCV_AMAP_DMEM_SIZE     : constant := 16#0000000000080000#;
    LW_RISCV_AMAP_DMEM_END      : constant := 16#0000000000200000#;
    LW_RISCV_AMAP_EMEM_START    : constant := 16#0000000001200000#;
    LW_RISCV_AMAP_EMEM_SIZE     : constant := 16#0000000000080000#;
    LW_RISCV_AMAP_EMEM_END      : constant := 16#0000000001280000#;
    LW_RISCV_AMAP_INTIO_START   : constant := 16#0000000001400000#;
    LW_RISCV_AMAP_INTIO_SIZE    : constant := 16#0000000000200000#;
    LW_RISCV_AMAP_INTIO_END     : constant := 16#0000000001600000#;
    LW_RISCV_AMAP_EXTMEM1_START : constant := 16#0000040000000000#;
    LW_RISCV_AMAP_EXTMEM1_SIZE  : constant := 16#0000040000000000#;
    LW_RISCV_AMAP_EXTMEM1_END   : constant := 16#0000080000000000#;

    LW_RISCV_AMAP_FBGPA_START   : constant := LW_RISCV_AMAP_EXTMEM1_START;
    LW_RISCV_AMAP_FBGPA_SIZE    : constant := LW_RISCV_AMAP_EXTMEM1_SIZE;
    LW_RISCV_AMAP_FBGPA_END     : constant := LW_RISCV_AMAP_EXTMEM1_END;

end Nv_Riscv_Address_Map;
