-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Utility; use Utility;
with Dev_Falc_Spr; use Dev_Falc_Spr;
with Ada.Unchecked_Colwersion;
with Scp_Cci_Intrinsics; use Scp_Cci_Intrinsics;
with Lw_Types.Boolean; use Lw_Types.Boolean;
with Reset_Plm_Falc_Specific_Tu10x; use Reset_Plm_Falc_Specific_Tu10x;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Dump_Coverage; use Dump_Coverage;

package body Cleanup
with SPARK_Mode => On
is

   procedure Fetch_Bss_Limits( S_Addr : out LwU32;
                               E_Addr : out LwU32)
     with SPARK_Mode => Off
   is

      Bss_Begin : LwU32
        with Import => True,
        Convention => C,
        External_Name => "_bss_begin";
      Bss_End : LwU32
        with Import => True,
        Convention => C,
        External_Name => "_bss_end";
   begin

      S_Addr := UC_ADDRESS_TO_LwU32( Bss_Begin'Address );
      E_Addr := UC_ADDRESS_TO_LwU32( Bss_End'Address );

   end Fetch_Bss_Limits;


   procedure Scrub_Bss
   is

      Start_Addr : LwU32 ;
      End_Addr   : LwU32 ;

   begin

      Fetch_Bss_Limits( Start_Addr, End_Addr );

      while Start_Addr < End_Addr loop

         Scrub_Helper( UCODE_DMEM_OFFSET_IN_FALCON_BYTES(Start_Addr) );
         Start_Addr := Start_Addr + 4;

      end loop;

   end Scrub_Bss;

   procedure Scrub_Dmem(Start_Addr : UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES;
                        End_Addr : UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES)
   is
      pragma Suppress(All_Checks);

      Addr : UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES := Start_Addr;
   begin

      while Addr < End_Addr loop
         Scrub_Helper(Addr);
         Addr := Addr + 4;
      end loop;

      -- Update HS Init State in ghost variable "Ghost_Hs_Init_States_Tracker" for SPARK prover
      Ghost_Hs_Init_States_Tracker := DMEM_SCRUBBED;

   end Scrub_Dmem;

   procedure Ucode_Halt
   is
   begin
      Falc_Halt_C;
   end Ucode_Halt;

   procedure Ucode_Cleanup_And_Halt
   is
      -- Suppress all checks is needed since this procedure is also ilwoked from Last_Chance Handler
      -- Hence to avoid relwrsion from happening we need below pragma
      pragma Suppress(All_Checks);

      Sec_Spr : LW_FALC_SEC_SPR_Register;
      Status : LW_UCODE_UNIFIED_ERROR_TYPE;

      function From_Raw is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                         Target => LW_FALC_SEC_SPR_Register);
      function To_Raw is new Ada.Unchecked_Colwersion (Source => LW_FALC_SEC_SPR_Register,
                                                       Target => LwU32);
   begin

      -- clear all SCP registers
      Clear_SCP_Registers;


      -- Restore Falcon Reset protection such that PL0 can write to it
      Update_Falcon_Reset_PLM(LW_FALSE, Status);

      --VCAST_DONT_INSTRUMENT_START
      Dump_Coverage_Data;
      --VCAST_DONT_INSTRUMENT_END
      Scrub_Dmem(0,
                 UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES(MAX_FALCON_DMEM_OFFSET_BYTES));

      --Set Disable Exceptions
      pragma Warnings(Off,"unused assignment",Reason => "Expected as it is RMW");
      Sec_Spr := From_Raw( Read_Sec_Spr_C );
      pragma Warnings(On,"unused assignment",Reason => "Expected as it is RMW");
      Sec_Spr.Disable_Exceptions := Set;
      Write_Sec_Spr_C(To_Raw(Sec_Spr));

      Clear_Falcon_Gprs;

      -- Update HS Exit State
      Ghost_Hs_Exit_State_Tracker := CLEANUP_COMPLETED;

      Ucode_Halt;

   end Ucode_Cleanup_And_Halt;

end Cleanup;
