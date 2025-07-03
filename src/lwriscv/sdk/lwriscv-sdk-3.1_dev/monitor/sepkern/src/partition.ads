-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Lw_Types;           use Lw_Types;
with Error_Handling;     use Error_Handling;
with Dev_Riscv_Csr_64;   use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;        use Dev_Prgnlcl;
with Policy;             use Policy;

package Partition
is

    procedure Initialize_Partition_Per_Policy(Partition_Policy : Policy.Policy)
    with
        Global => (Output => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                              Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State));

    procedure Clear_Partition_State
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    procedure Clear_MPU
    with
        Global => (In_Out => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

end Partition;
