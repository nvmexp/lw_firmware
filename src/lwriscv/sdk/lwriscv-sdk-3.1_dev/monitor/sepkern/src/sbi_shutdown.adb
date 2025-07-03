-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Inst;  use Riscv_Core.Inst;
with Lw_Types;         use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Types;            use Types;

package body SBI_Shutdown
is

    procedure Shutdown
    is
        procedure Csr_Reg_Clr64 is new Riscv_Core.Rv_Csr.Clr64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
    begin
        -- Disable S-mode and M-mode interrupts
        Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIE_Address,
                    Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => LW_RISCV_CSR_MIE_WPRI3_RST,
                                                        Meie  => 1,
                                                        Wpri2 => LW_RISCV_CSR_MIE_WPRI2_RST,
                                                        Seie  => 1,
                                                        Ueie  => 1,
                                                        Mtie  => 1,
                                                        Wpri1 => LW_RISCV_CSR_MIE_WPRI1_RST,
                                                        Stie  => 1,
                                                        Utie  => 1,
                                                        Msie  => 1,
                                                        Wpri0 => LW_RISCV_CSR_MIE_WPRI0_RST,
                                                        Ssie  => 1,
                                                        Usie  => 1));

        -- “FENCE.IO is exactly the same as lwfenceio
        Riscv_Core.Inst.Fence_Io;

        -- Halt the core
        Riscv_Core.Inst.Halt;

    end Shutdown;

end SBI_Shutdown;
