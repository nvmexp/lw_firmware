-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Scp_Rand_Falc_Specific_Tu10x; use Scp_Rand_Falc_Specific_Tu10x;
with Utility; use Utility;
with Scp_Cci_Intrinsics; use Scp_Cci_Intrinsics;


package body Scp_Rand
with SPARK_Mode => On
is


   procedure Scp_Get_Rand128( Random_Num_Addr : UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
   is
   begin

      Scp_Start_Rand;

      -- load Random_Num_Addr to SCP
      Falc_Scp_Trap_C( TC_INFINITY );
      Falc_Trapped_Dmwrite_C( Falc_Sethi_I_C( LwU32(Random_Num_Addr), SCP_R5 ) );

      -- improve RNG quality by collecting entropy across
      -- two conselwtive random numbers
      Falc_Scp_Rand_C( SCP_R4 );
      Falc_Scp_Rand_C( SCP_R3 );

      -- trapped dmwait
      Falc_Dmwait_C;

      -- mixing in previous whitened rand data
      Falc_Scp_Xor_C( SCP_R5, SCP_R4 );

      -- use AES cipher to whiten random data
      Falc_Scp_Key_C( SCP_R4 );
      Falc_Scp_Encrypt_C( SCP_R3, SCP_R3 );

      -- retrieve random data and update DMEM buffer.
      Falc_Trapped_Dmread_C( Falc_Sethi_I_C( LwU32(Random_Num_Addr), SCP_R3 ) );
      Falc_Dmwait_C;

      -- Reset trap to 0
      Falc_Scp_Trap_C(0);

      Scp_Stop_Rand;

   end Scp_Get_Rand128;

end Scp_Rand;
