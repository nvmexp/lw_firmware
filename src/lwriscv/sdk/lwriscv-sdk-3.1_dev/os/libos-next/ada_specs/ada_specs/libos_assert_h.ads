pragma Ada_2012;
pragma Style_Checks (Off);

with Interfaces.C; use Interfaces.C;

package libos_assert_h is

   --  arg-macro: procedure LibosValidate (x)
   --    if (not(x)) { LibosPanic(); }
  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

   procedure LibosBreakpoint  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_assert.h:16
   with Import => True, 
        Convention => C, 
        External_Name => "LibosBreakpoint";

end libos_assert_h;
