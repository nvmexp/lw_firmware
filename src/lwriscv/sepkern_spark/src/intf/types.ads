-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package Types is

   type Callee_Saved_Registers is array (LwU4 range 0 .. 11) of LwU64;
   type Argument_Registers is array (LwU3 range 0 .. 7) of LwU64;

   type Phys_Addr is new LwU47;

   Fmc_4k_Alignment : constant LwU64 := 16#1000#;
   subtype Offset_4K_Aligned is LwU64 with
     Dynamic_Predicate => Offset_4K_Aligned mod Fmc_4k_Alignment = 0;

   type Csr_Addr is new LwU12;

end Types;
