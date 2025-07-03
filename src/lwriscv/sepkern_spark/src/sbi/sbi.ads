-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;

with Lw_Types; use Lw_Types;
with Riscv_Core;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;

package SBI is

   -- TODO: Check this value
   SBI_Version : constant LwU64 := 0;

   type SBI_Error_Code is
     (
      SBI_ERR_ILWALID_ADDRESS,
      SBI_ERR_DENIED,
      SBI_ERR_ILWALID_PARAM,
      SBI_ERR_NOT_SUPPORTED,
      SBI_ERR_FAILURE,
      SBI_SUCCESS
     ) with Size => 64, Object_Size => 64;

   for SBI_Error_Code use
     (
      SBI_ERR_ILWALID_ADDRESS => -5,
      SBI_ERR_DENIED => -4,
      SBI_ERR_ILWALID_PARAM => -3,
      SBI_ERR_NOT_SUPPORTED => -2,
      SBI_ERR_FAILURE => -1,
      SBI_SUCCESS => 0
     );
   function SBI_Error_Code_To_LwU64 is new Ada.Unchecked_Colwersion (Source => SBI_Error_Code,
                                                                     Target => LwU64) with Inline_Always;

   type SBI_Extension_ID is new LwU64;

   SBI_EXTENSION_SET_TIMER : constant SBI_Extension_ID := 16#0#;
   SBI_EXTENSION_SHUTDOWN  : constant SBI_Extension_ID := 16#8#;

   procedure SBI_Dispatch(arg0 : in out LwU64; -- Holds the new mtimecmp for timer SBI, ignored otherwise; On OUT holds return value
                          arg1 : out LwU64;    -- Holds return value 
                          arg2 : LwU64;
                          arg3 : LwU64;
                          arg4 : LwU64;
                          arg5 : LwU64;
                          functionId : LwU64;  -- Holds the function ID. Must be 0 for SOL implementation
                          extensionId : LwU64) -- Holds the extension ID. Must be 0 or 8 for SOL implementation
     with
       Annotate => (GNATprove, Might_Not_Return),  -- in case of SBI_EXTENSION_SHUTDOWN Dispatch won't return, but will for timer SBI_EXTENSION_SET_TIMER
       Annotate => (GNATProve, Intentional,
                     "might not be written",
                     "Csr_Reg_Hw_Model.Csr_State might not be written in case of invalid functionId, so this warning is justified."),
     Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State), -- Since SBI_Dispatch only does CSR writes
     Inline_Always;

end SBI;
