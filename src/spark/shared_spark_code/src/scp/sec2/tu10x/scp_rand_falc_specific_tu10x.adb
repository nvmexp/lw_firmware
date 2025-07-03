-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Lw_Types; use Lw_Types;
with Utility; use Utility;

package body Scp_Rand_Falc_Specific_Tu10x is

   function From_Raw_Scp_Rndctl1 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                                  Target => LW_CSEC_SCP_RNDCTL1_REGISTER);
   function To_Raw_Scp_Rndctl1 is new Ada.Unchecked_Colwersion (Source => LW_CSEC_SCP_RNDCTL1_REGISTER,
                                                                Target => LwU32);
   function From_Raw_Scp_Rndctl11 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                                   Target => LW_CSEC_SCP_RNDCTL11_REGISTER);
   function To_Raw_Scp_Rndctl11 is new Ada.Unchecked_Colwersion (Source => LW_CSEC_SCP_RNDCTL11_REGISTER,
                                                                 Target => LwU32);
   function From_Raw_Scp_Ctl1 is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                               Target => LW_CSEC_SCP_CTL1_REGISTER);
   function To_Raw_Scp_Ctl1 is new Ada.Unchecked_Colwersion (Source => LW_CSEC_SCP_CTL1_REGISTER,
                                                             Target => LwU32);
   procedure Scp_Start_Rand
   is

      Val              : LwU32;
      Scp_Rndctl1_Reg  : LW_CSEC_SCP_RNDCTL1_REGISTER;
      Scp_Rndctl11_Reg : LW_CSEC_SCP_RNDCTL11_REGISTER;
      Scp_Ctl1_Reg     : LW_CSEC_SCP_CTL1_REGISTER;

   begin

      Falc_Stxb_C(Addr => Lw_Csec_Scp_Rndctl0,
                  Val  => SCP_HOLDOFF_INIT_LOWER_VAL);

      Falc_Ldx_I_C( Addr   => Lw_Csec_Scp_Rndctl1,
                    Bitnum => 0,
                    Val    => Val );
      Scp_Rndctl1_Reg := From_Raw_Scp_Rndctl1( Val );
      Scp_Rndctl1_Reg.F_Holdoff_Intra_Mask := SCP_HOLDOFF_INTRA_MASK;
      Falc_Stxb_C(Addr => Lw_Csec_Scp_Rndctl1,
                  Val  => To_Raw_Scp_Rndctl1( Scp_Rndctl1_Reg ));

      Falc_Ldx_I_C( Addr   => Lw_Csec_Scp_Rndctl11,
                    Bitnum => 0,
                    Val    => Val );
      Scp_Rndctl11_Reg := From_Raw_Scp_Rndctl11( Val );
      Scp_Rndctl11_Reg.F_Autocal_Static_Tap_A := SCP_AUTOCAL_STATIC_TAPA;
      Scp_Rndctl11_Reg.F_Autocal_Static_Tap_B := SCP_AUTOCAL_STATIC_TAPB;
      Falc_Stxb_C(Addr => Lw_Csec_Scp_Rndctl11,
                  Val  => To_Raw_Scp_Rndctl11( Scp_Rndctl11_Reg ));

      Falc_Ldx_I_C( Addr   => Lw_Csec_Scp_Ctl1,
                    Bitnum => 0,
                    Val    => Val );
      Scp_Ctl1_Reg := From_Raw_Scp_Ctl1( Val );
      Scp_Ctl1_Reg.F_Rng_En := Lw_Csec_Scp_Ctl1_Rng_En_Enabled;
      Falc_Stxb_C(Addr => Lw_Csec_Scp_Ctl1,
                  Val  => To_Raw_Scp_Ctl1( Scp_Ctl1_Reg ));

   end Scp_Start_Rand;

   procedure Scp_Stop_Rand
   is

      Val          : LwU32;
      Scp_Ctl1_Reg : LW_CSEC_SCP_CTL1_REGISTER;

   begin

      Falc_Ldx_I_C( Addr   => Lw_Csec_Scp_Ctl1,
                    Bitnum => 0,
                    Val    => Val );
      Scp_Ctl1_Reg := From_Raw_Scp_Ctl1( Val );
      Scp_Ctl1_Reg.F_Rng_En := Lw_Csec_Scp_Ctl1_Rng_En_Disabled;
      Falc_Stxb_C(Addr => Lw_Csec_Scp_Ctl1,
                  Val  => To_Raw_Scp_Ctl1( Scp_Ctl1_Reg ));

   end Scp_Stop_Rand;

end Scp_Rand_Falc_Specific_Tu10x;
