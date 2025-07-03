-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Riscv_Core;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;

package body Debug_Control is

   -- Minimized ICD privilege: Mem(Match core) CSR(Supervisor) PLM(Match core). No halt in M.
   -- Typical use case: Production LS
   procedure Configure_ICD is
      procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MDBGCTL_Register);
   begin

      Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MDBGCTL_Address,
                   Data => LW_RISCV_CSR_MDBGCTL_Register'(Wiri                 => 0,
                                                          Micd_Dis             => DIS_TRUE,
                                                          Icdcsrprv_S_Override => OVERRIDE_TRUE,
                                                          Icdmemprv            => ICDMEMPRV_S,
                                                          Icdmemprv_Override   => OVERRIDE_TRUE,
                                                          Icdpl                => ICDPL_USE_LWR));
   end Configure_ICD;

end Debug_Control;
