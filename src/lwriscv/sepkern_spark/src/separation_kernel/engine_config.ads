-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Error_Handling; use Error_Handling;
with Riscv_Core;

package Engine_Config is

   procedure Configure with
     Global => (Output => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                           Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State)),
     Inline_Always;

   procedure Get_Partition_Entry_Point(Partition_Entry_Point : out LwU64; Err_Code : in out Error_Codes)
     with
       Pre => Err_Code = OK,
       Post => (if Err_Code = OK then Partition_Entry_Point mod 4 = 0),
     Inline_Always;

end Engine_Config;
