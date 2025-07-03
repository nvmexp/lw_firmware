-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with Types;             use Types;

package Riscv_Core
is

    -- Ghost code
    type Stacks is (SK_Stack, S_Stack, Zero_SP)
    with
        Ghost;
    Ghost_Lwrrent_Stack : Stacks
    with
        Ghost;

    procedure Ghost_Switch_To_SK_Stack
    with
        Ghost,
        Post => Ghost_Lwrrent_Stack = SK_Stack;

    procedure Ghost_Switch_To_S_Stack
    with
        Ghost,
        Post => Ghost_Lwrrent_Stack = S_Stack;

end Riscv_Core;
