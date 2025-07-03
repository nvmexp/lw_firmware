-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  HW and Arch specific SCP procedures to generate Random numbers
--
--  @description
--  This package contains helper procedures to program SCP to generate random number
--  required for setting guard variable in SSP.

package Scp_Rand_Falc_Specific_Tu10x is

   SCP_HOLDOFF_INIT_LOWER_VAL : constant := 16#7FFF#;
   SCP_HOLDOFF_INTRA_MASK     : constant := 16#03FF#;
   SCP_AUTOCAL_STATIC_TAPA    : constant := 16#000F#;
   SCP_AUTOCAL_STATIC_TAPB    : constant := 16#000F#;

   procedure Scp_Start_Rand
     with
       Global => null,
       Inline_Always;

   procedure Scp_Stop_Rand
     with
       Global => null,
       Inline_Always;

end Scp_Rand_Falc_Specific_Tu10x;
