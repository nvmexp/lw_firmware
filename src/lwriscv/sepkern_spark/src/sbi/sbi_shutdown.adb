-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Fbif_V4; use Dev_Fbif_V4;
with Dev_Falcon_V4; use Dev_Falcon_V4;
with Types; use Types;
with Riscv_Core;

package body SBI_Shutdown is

   procedure Shutdown
   is
      procedure Csr_Reg_Clr64 is new Riscv_Core.Rv_Csr.Clr64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
   begin
      -- Disable S-mode and M-mode interrupts
      Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIE_Address,
                    Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => 0,
                                                       Meie  => 1,
                                                       Wpri2 => 0,
                                                       Seie  => 1,
                                                       Ueie  => 1,
                                                       Mtie  => 1,
                                                       Wpri1 => 0,
                                                       Stie  => 1,
                                                       Utie  => 1,
                                                       Msie  => 1,
                                                       Wpri0 => 0,
                                                       Ssie  => 1,
                                                       Usie  => 1));

      -- Flush IO
      Riscv_Core.Inst.Fence_Io;

      -- Halt the core
      Riscv_Core.Halt;
   end Shutdown;

end SBI_Shutdown;
