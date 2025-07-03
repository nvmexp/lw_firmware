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

package Mpu_Policy_Types
is

    -- Partition Policy MPU Control types
    subtype Mpu_Entry_Count_Type is LwU8 range 0 .. 128;

    type Plcy_Mpu_Control is record
        Entry_Count : aliased Mpu_Entry_Count_Type;
        Start_Index : aliased LwU7;
    end record;

    function Plcy_Mpu_Control_to_LW_RISCV_CSR_MMPUCTL_Register(Plcy_Mpu_Ctl : Plcy_Mpu_Control) return LW_RISCV_CSR_MMPUCTL_Register is
        (LW_RISCV_CSR_MMPUCTL_Register'(Wpri1       => LW_RISCV_CSR_MMPUCTL_WPRI1_RST,
                                        Entry_Count => Plcy_Mpu_Ctl.Entry_Count,
                                        Wpri0       => LW_RISCV_CSR_MMPUCTL_WPRI0_RST,
                                        Start_Index => Plcy_Mpu_Ctl.Start_Index))
    with Inline;

end Mpu_Policy_Types;