-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Error_Handling; use Error_Handling;
with Types; use Types;
with Riscv_Core;
with SBI_Shutdown; use SBI_Shutdown;

package body SBI is

   procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MTIMECMP_Register);
   procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
   procedure Csr_Reg_Clr64 is new Riscv_Core.Rv_Csr.Clr64_Generic (Generic_Csr => LW_RISCV_CSR_MIP_Register);

   procedure SBI_Dispatch(arg0 : in out LwU64; -- Holds the new mtimecmp for timer SBI, ignored otherwise
                          arg1 : out LwU64;   -- Holds return value 
                          arg2 : LwU64;
                          arg3 : LwU64;
                          arg4 : LwU64;
                          arg5 : LwU64;
                          functionId : LwU64;  -- Holds the function ID. Must be 0 for SOL implementation
                          extensionId : LwU64) -- Holds the extension ID. Must be 0 or 8 for SOL implementation
   is
      Mtimecmp : LW_RISCV_CSR_MTIMECMP_Register;
   begin
      pragma Unreferenced(arg2, arg3, arg4, arg5);

      if functionId /= 0 then
         arg0 := SBI_Error_Code_To_LwU64(SBI_ERR_ILWALID_PARAM);
         arg1 := 0;
         return;

      end if;

      case extensionId is
       when LwU64(SBI_EXTENSION_SET_TIMER) =>
            -- Write to mtimecmp
            Mtimecmp.Time := arg0;
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MTIMECMP_Address, 
                         Data => Mtimecmp);

            -- Clear Supervisor timer interrupt pending
            Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIP_Address,
                          Data => LW_RISCV_CSR_MIP_Register'(Wiri3 => 0,
                                                             Meip  => 0,
                                                             Wiri2 => 0,
                                                             Seip  => 0,
                                                             Ueip  => 0,
                                                             Mtip  => 0,
                                                             Wiri1 => 0,
                                                             Stip  => 1,
                                                             Utip  => 0,
                                                             Msip  => 0,
                                                             Wiri0 => 0,
                                                             Ssip  => 0,
                                                             Usip  => 0));

            -- Re-enable machine timer interrupt enable
            Csr_Reg_Set64(Addr => LW_RISCV_CSR_MIE_Address,
                          Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => 0,
                                                             Meie  => 0,
                                                             Wpri2 => 0,
                                                             Seie  => 0,
                                                             Ueie  => 0,
                                                             Mtie  => 1,
                                                             Wpri1 => 0,
                                                             Stie  => 0,
                                                             Utie  => 0,
                                                             Msie  => 0,
                                                             Wpri0 => 0,
                                                             Ssie  => 0,
                                                             Usie  => 0));

            arg0 := SBI_Error_Code_To_LwU64(SBI_SUCCESS);
            arg1 := 0;

--         For all other values, including SBI_EXTENSION_SHUTDOWN, just Shutdown the core
           when others =>

            Shutdown;

            raise Program_Error with "This should never happen!";
      end case;

   end SBI_Dispatch;

end SBI;
