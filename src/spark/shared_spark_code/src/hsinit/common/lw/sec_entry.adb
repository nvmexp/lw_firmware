-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Hs_Init; use Hs_Init;
with Hs_Init_Falc_Specific_Tu10x; use Hs_Init_Falc_Specific_Tu10x;
with Exceptions; use Exceptions;
with Exceptions_Falc_Specific_Tu10x; use Exceptions_Falc_Specific_Tu10x;
with Cleanup; use Cleanup;
with Ada.Unchecked_Colwersion;
with Utility; use Utility;
with Scp_Rand; use Scp_Rand;
with Scp_Cci_Intrinsics; use Scp_Cci_Intrinsics;
with Lw_Types.Boolean; use Lw_Types.Boolean;
with Update_Mailbox_Falc_Specific_Tu10x; use Update_Mailbox_Falc_Specific_Tu10x;
with Se_Vhr; use Se_Vhr;
with Reset_Plm_Falc_Specific_Tu10x; use Reset_Plm_Falc_Specific_Tu10x;


package body Sec_Entry
with SPARK_Mode => On
is

   -- Spark mode is OFF since we access System Address and is justified
   procedure Get_Rand_Num16_Byte_Addr( Rand_Num16_Byte_Addr : out UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
     with SPARK_Mode => Off
   is
   begin
      Rand_Num16_Byte_Addr :=
        UCODE_DMEM_OFFSET_IN_FALCON_BYTES( UC_ADDRESS_TO_LwU32( Rand_Num16_Byte.Data(0)'Address ) );
   end Get_Rand_Num16_Byte_Addr;

   procedure Enable_Ssp
   is

      pragma Suppress(All_Checks);

      Rand_Num_Offset : UCODE_DMEM_OFFSET_IN_FALCON_BYTES;
   begin

      -- Initialize to make prover happy
      Rand_Num16_Byte.Data := (0, 0, 0, 0);

      Get_Rand_Num16_Byte_Addr(Rand_Num16_Byte_Addr => Rand_Num_Offset);

      Scp_Get_Rand128( Rand_Num_Offset );

      Stack_Check_Guard := UC_LwU32_TO_ADDRESS( Rand_Num16_Byte.Data(0) );

      -- Update HS Init Tracker
      Ghost_Hs_Init_States_Tracker := SSP_ENABLED;
   end Enable_Ssp;

   procedure Hs_Entry_Inline
   is
      pragma Suppress(All_Checks);
   begin

      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         -- Step 1: Set SP
         -- This is done in wrapper function itself since if done here
         -- the callwlation done for accessing local variables goes beyond dmem range.
         -- EG. SP = 0xFFFC; then accessing value at [SP + 0x20] results in CSB error.


         -- Step 2: Clear SCP signature.
         Falc_Scp_Forget_Sig;

         -- Step 3: Clear SCP registers.
         Clear_SCP_Registers;

         -- ******************* NO USAGE OF SCP GPR'S BEFORE THIS POINT. ************************************

         -- Step 4: Clear falcon GPR's.
         Clear_Falcon_Gprs;

         -- ******************* NO USAGE OF FALCON GPR'S BEFORE THIS POINT. ************************************

         declare
            -- TODO : Change this to parameter
            Status      :  LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
            Bss_Begin   : LwU32
              with Import => True,
              Convention => C,
              External_Name => "_bss_begin";
         begin

            -- Step 5: Scrub DMEM from End of BSS to current stack pointer.
            Scrub_Dmem(UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES (Bss_Begin),
                       UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES(Read_Sp_Spr_C) );

            -- ******************* NO USAGE OF CSB TRANSACTION BEFORE THIS POINT. ************************************

            -- Step 6: CSBERRSTAT Check.
            Enable_Csb_Err_Reporting(Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 7: Disable CMEMBASE
            Disable_Cmembase_Aperture(Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 8: Disable TRACEPC
            Disable_Trace_Pc_Buffer(Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 9: NS Restart from HS mitigation.
            Prevent_Halted_Cpu_From_Being_Restarted(Status => Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 10: Setup Minimum Exception Handler.
            Exception_Installer_Hang(Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 11: Setup STACKCFG.BOTTOM.
            -- TODO : Resolve STACKCFG write issue
            Stack_Underflow_Installer(Status);
            if Status /= UCODE_ERR_CODE_NOERROR then
               Infinite_Loop;
            end if;

            -- Step 12: SSP Enablement.
            Enable_Ssp;
         end;
         -- ******************* NO CALL TO NON-INLINED FUNCTIONS BEFORE THIS POINT ********************************

      end loop Main_Block;

   end Hs_Entry_Inline;

   procedure Hs_Entry_NonInline(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin

      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         -- Step 13: Ilwalidate NS Blocks
         Ilwalidate_Ns_Blocks( Status);
         if Status /= UCODE_ERR_CODE_NOERROR then
            Infinite_Loop;
         end if;

         -- Step 14: VHR Empty Check.
         Se_Vhr_Empty_Check( Status );
         if Status /= UCODE_ERR_CODE_NOERROR then
            Infinite_Loop;
         end if;

         -- Step 15: Raise falcon reset to PL3.
         Update_Falcon_Reset_PLM(LW_TRUE, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Step 16: Set BAR0 timeout
         -- By default it is set to 0, and no BAR0 reg rd/wr will work before this is set
         Set_Bar0_Timeout(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- ******************* NO BAR0 ACCESS BEFORE THIS POINT ******************

         -- Step 17: BOOT_42 check.
         -- Check if we're exelwting on the expected chip
         Is_Valid_Chip(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Step 18: Revocation check.
         -- Check if uCode rev is >= rev fuse
         Is_Valid_Ucode_Rev(Status => Status);

      end loop Main_Block;

   end Hs_Entry_NonInline;

   pragma Annotate (GNATprove, Intentional, "call to nonreturning subprogram might be exelwted", "Expected");

   procedure Hs_Exit(Status : LW_UCODE_UNIFIED_ERROR_TYPE)
   is
   begin
      Update_Status_In_Mailbox(Status);
      Ucode_Cleanup_And_Halt;
   end Hs_Exit;

end Sec_Entry;
