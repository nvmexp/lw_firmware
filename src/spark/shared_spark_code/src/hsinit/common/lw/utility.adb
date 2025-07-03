-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Cleanup; use Cleanup;

package body Utility
with SPARK_Mode => On
is

   ---------------------------------------------------------------------------
   --------------------------- subprogram implementation ---------------------
   ---------------------------------------------------------------------------

   procedure Scrub_Helper( Addr : UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
     with SPARK_Mode => Off
   is

      Val : LwU32
        with Address => System'To_Address(Addr), Volatile;

   begin

      Val := 0;

   end Scrub_Helper;

   procedure Falc_Jmp_C(Addr : LwU32)
   is
   begin

      falc_jmp(Addr);

   end Falc_Jmp_C;

   procedure Falc_Dmwait_C
   is
   begin

      falc_dmwait;

   end Falc_Dmwait_C;

   procedure Falc_Sclrb_C(Bitnum : LwU32)
   is
   begin

      falc_sclrb(Bitnum);

   end Falc_Sclrb_C;

   procedure Falc_Halt_C
   is
   begin

      falc_halt;

   end Falc_Halt_C;

   procedure Falc_Ssetb_I_C(Bitnum : LwU32)
   is
   begin

      falc_ssetb_i(Bitnum);

   end Falc_Ssetb_I_C;

   procedure Falc_Ldx_I_C(Addr   : CSB_ADDR;
                          Bitnum : LwU32;
                          Val    : out LwU32)
   is
   begin

      Val := falc_ldx_i_wrapper(Addr   => LwU32(Addr),
                                Bitnum => Bitnum);

   end Falc_Ldx_I_C;

   procedure Falc_Stx_C(Addr : CSB_ADDR;
                        Val  : LwU32)
   is
   begin

      falc_stx_wrapper(Addr => LwU32(Addr),
                       Val  => Val);

   end Falc_Stx_C;

   procedure Falc_Stxb_C(Addr : CSB_ADDR;
                         Val  : LwU32)
   is
   begin

      falc_stxb_wrapper(Addr => LwU32(Addr),
                        Val  => Val);

   end Falc_Stxb_C;

   procedure Falc_Imblk_C(Blk_Status : out LwU32;
                          Blk_No     : LwU32)
   is

   begin

      falc_imblk(Blk_Status => Blk_Status,
                 Blk_No     => Blk_No);

   end Falc_Imblk_C;

   procedure Falc_Imilw_C(Blk_No : LwU32)
   is
   begin

      falc_imilw(Blk_No => Blk_No);

   end Falc_Imilw_C;

   procedure Last_Chance_Handler
   is
   begin

      -- The call to this is added by CCG even before VHR Check (aka before HALT is allowed)
      --
      pragma Assume(Ghost_Halt_Action_State = HALT_ACTION_ALLOWED);
      pragma Assume(Ghost_Hs_Init_States_Tracker >= VHR_EMPTY_CHECKED);
      pragma Assume(Ghost_Hs_Exit_State_Tracker = MAILBOX_UPDATED);

      Ucode_Cleanup_And_Halt;

   end Last_Chance_Handler;

   function Read_Sec_Spr_C return LwU32
   is
   begin

      return falc_rspr_sec_spr_wrapper;

   end Read_Sec_Spr_C;

   procedure Write_Sec_Spr_C(Sec_Spr : LwU32)
   is
   begin

      falc_wspr_sec_spr_wrapper(Sec_Spr);

   end Write_Sec_Spr_C;

   function Read_Sp_Spr_C return LwU32
   is
   begin

      return falc_rspr_sp_spr_wrapper;

   end Read_Sp_Spr_C;

   function Read_Csw_Spr_C return LwU32
   is
   begin

      return falc_rspr_csw_spr_wrapper;

   end Read_Csw_Spr_C;

   procedure Write_Csw_Spr_C( Csw_Spr: LwU32 )
   is
   begin

      falc_wspr_csw_spr_wrapper( Csw_Spr );

   end Write_Csw_Spr_C;

   function Falc_Sethi_I_C( Lo : LwU32;
                            Hi : LwU32 )
                           return LwU32
   is
   begin

      return falc_sethi_i(Lo, Hi);

   end Falc_Sethi_I_C;

   procedure Infinite_Loop
   is
   begin

      loop
         null;
      end loop;

   end Infinite_Loop;

end Utility;
