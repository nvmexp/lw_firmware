pragma Ada_2012;
pragma Style_Checks (Off);

package libos_status_h is

  -- _LWRM_COPYRIGHT_BEGIN_
  -- *
  -- * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
  -- * information contained herein is proprietary and confidential to LWPU
  -- * Corporation.  Any use, reproduction, or disclosure without the written
  -- * permission of LWPU Corporation is prohibited.
  -- *
  -- * _LWRM_COPYRIGHT_END_
  --  

  --*
  -- *  @brief Status codes for Libos kernel syscalls and standard daemons.
  -- * 
  --  

   type LibosStatus is 
     (LibosOk,
      LibosErrorArgument,
      LibosErrorAccess,
      LibosErrorTimeout,
      LibosErrorIncomplete,
      LibosErrorFailed)
   with Convention => C;  -- /home/jerzhou/scratch/p4/jerzhou_scratch_sw0/sw/lwriscv/main/os/libos/include/libos_status.h:25

end libos_status_h;
