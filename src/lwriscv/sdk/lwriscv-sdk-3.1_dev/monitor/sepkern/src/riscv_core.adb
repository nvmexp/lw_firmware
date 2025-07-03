-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Types;                    use Types;
with Dev_Riscv_Csr_64;         use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;              use Dev_Prgnlcl;
with System.Machine_Code;      use System.Machine_Code;
with Lw_Riscv_Address_Map;     use Lw_Riscv_Address_Map;

with Riscv_Core.Inst;

package body Riscv_Core
is

    -- Ghost code
    procedure Ghost_Switch_To_SK_Stack
    is
    begin
        Ghost_Lwrrent_Stack := SK_Stack;
    end Ghost_Switch_To_SK_Stack;

    procedure Ghost_Switch_To_S_Stack
    is
    begin
        Ghost_Lwrrent_Stack := S_Stack;
    end Ghost_Switch_To_S_Stack;

end Riscv_Core;
