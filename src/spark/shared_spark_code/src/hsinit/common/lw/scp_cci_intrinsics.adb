-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

package body Scp_Cci_Intrinsics
with SPARK_Mode => On
is

   procedure Falc_Scp_Xor_C( Rx : LwU32;
                             Ry: LwU32 )
   is
   begin

      falc_scp_xor(Rx, Ry);

   end Falc_Scp_Xor_C;

   procedure Falc_Scp_Trap_C( Tc : LwU32 )
   is
   begin

      falc_scp_trap(Tc);

   end Falc_Scp_Trap_C;

   procedure Falc_Scp_Rand_C( Ry: LwU32 )
   is
   begin

      falc_scp_rand(Ry);

   end Falc_Scp_Rand_C;

   procedure Falc_Scp_Key_C( Ry: LwU32 )
   is
   begin

      falc_scp_key(Ry);

   end Falc_Scp_Key_C;

   procedure Falc_Scp_Encrypt_C( Rx : LwU32;
                                 Ry: LwU32 )
   is
   begin

      falc_scp_encrypt(Rx, Ry);

   end Falc_Scp_Encrypt_C;

   procedure Falc_Trapped_Dmread_C( Addr : LwU32 )
   is
   begin

      falc_trapped_dmread(Addr);

   end Falc_Trapped_Dmread_C;

   procedure Falc_Trapped_Dmwrite_C( Addr : LwU32 )
   is
   begin

      falc_trapped_dmwrite(Addr);

   end Falc_Trapped_Dmwrite_C;

   procedure Clear_SCP_Registers
   is
   begin

      Falc_Scp_Xor_C(SCP_R0,SCP_R0);

      Falc_Scp_Xor_C(SCP_R1,SCP_R1);

      Falc_Scp_Xor_C(SCP_R2,SCP_R2);

      Falc_Scp_Xor_C(SCP_R3,SCP_R3);

      Falc_Scp_Xor_C(SCP_R4,SCP_R4);

      Falc_Scp_Xor_C(SCP_R5,SCP_R5);

      Falc_Scp_Xor_C(SCP_R6,SCP_R6);

      Falc_Scp_Xor_C(SCP_R7,SCP_R7);

      Ghost_Hs_Init_States_Tracker := SCP_REGISTERS_CLEARED;

   end Clear_SCP_Registers;

end Scp_Cci_Intrinsics;
