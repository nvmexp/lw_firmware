-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;


package Dev_Fpf is

   type LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER is record
      Data      : LwU8;
   end record
     with Size => 32, Object_Size => 32;

   for LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_REGISTER use record
      Data      at 0 range  0 .. 7;
   end record;

   ------------------------------------ Offsets of various registers --------------------------------
     LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_ADDR : constant BAR0_ADDR := 16#0082_0250#;

end Dev_Fpf;
