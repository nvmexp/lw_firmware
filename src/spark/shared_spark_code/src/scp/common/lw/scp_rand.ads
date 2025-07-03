-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;

--  @summary
--  HW and Arch Indpendent SCP procedures to generate Random numbers
--
--  @description
--  This package contains procedure to program SCP to generate random number
--  required for setting guard variable in SSP.

package Scp_Rand
with SPARK_Mode => On
is

   TC_INFINITY                : constant := 16#001F#;

   procedure Scp_Get_Rand128( Random_Num_Addr : UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
     with
       Pre => Random_Num_Addr mod 16 = 0,-- Random_Num_Addr must be 16 byte aligned
       Global => null,
       Inline_Always;
end Scp_Rand;
