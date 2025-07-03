-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;

--  @summary
--  Falcon clean up actions.
--
--  @description
--  This package contains procedures for falcon clean up before halting the falcon.

package Cleanup
with SPARK_Mode => On
is


   FLCN_DMEM_SIZE_IN_BLK    : constant LwU32 := 256;
   FLCN_DMEM_BLK_ALIGN_BITS : constant Natural := 8;

   -- Scrubs BSS section marked by Linker variables from "bss_begin" to "bss_end"
   procedure Scrub_Bss
     with
       Pre => (Ghost_Hs_Init_States_Tracker = CMEMBASE_APERTURE_DISABLED),
     Global => (Proof_In => Ghost_Hs_Init_States_Tracker),
     Inline_Always;

   -- Fetches BSS section limits marked by Linker variables "bss_begin" and "bss_end"
   -- @param S_Addr The start of Bss Section. Out Arg.
   -- @param E_Addr The end of Bss Section. Out Arg.
   procedure Fetch_Bss_Limits( S_Addr : out LwU32;
                               E_Addr : out LwU32)
     with
       Post => S_Addr < MAX_FALCON_DMEM_OFFSET_BYTES and then
       E_Addr < MAX_FALCON_DMEM_OFFSET_BYTES,
       Inline_Always;

   -- Procedure to Halt Ucode
   procedure Ucode_Halt
     with
       Pre => (Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                 Ghost_Hs_Exit_State_Tracker = CLEANUP_COMPLETED),
     Global => (Proof_In => (Ghost_Halt_Action_State,
                             Ghost_Hs_Exit_State_Tracker)),
     Inline_Always;

   -- Procedure has Ucode Cleanup Code and call to Ucode_Halt
   procedure Ucode_Cleanup_And_Halt
     with
       Pre => (Ghost_Hs_Init_States_Tracker >= VHR_EMPTY_CHECKED and then
                 Ghost_Hs_Exit_State_Tracker = MAILBOX_UPDATED),
     Post => (Ghost_Hs_Exit_State_Tracker = CLEANUP_COMPLETED),
     Global => ( In_Out => (Ghost_Hs_Init_States_Tracker,
                            Ghost_Hs_Exit_State_Tracker)),
     Depends => (Ghost_Hs_Init_States_Tracker => null,
                 Ghost_Hs_Exit_State_Tracker => null,
                 null => (Ghost_Hs_Init_States_Tracker,
                          Ghost_Hs_Exit_State_Tracker)),
     Linker_Section => LINKER_SECTION_NAME;

   -- Clears Falcon Gprs
   procedure Clear_Falcon_Gprs
     with
       Pre           => (Ghost_Hs_Init_States_Tracker = RESET_PLM_PROTECTED or else
                           Ghost_Hs_Init_States_Tracker = SCP_REGISTERS_CLEARED or else
                             Ghost_Hs_Init_States_Tracker = DMEM_SCRUBBED),
       Post          => Ghost_Hs_Init_States_Tracker = GPRS_SCRUBBED,
       Global        => (Proof_In => Ghost_Hs_Init_States_Tracker),
     Import        => True,
     Convention    => C,
     External_Name => "clearFalconGprs",
     Inline_Always;

   -- Scrubs Dmem from Start of Dmem to Stack Bottom (48Kb)
   procedure Scrub_Dmem(Start_Addr : UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES;
                        End_Addr : UCODE_DMEM_OFFSET_IN_FALCON_4B_ALIGNED_BYTES)
     with
       Pre => (if Start_Addr = 0 then
                 Ghost_Hs_Init_States_Tracker = RESET_PLM_PROTECTED else
                   Ghost_Hs_Init_States_Tracker = GPRS_SCRUBBED and then
                     LwU32 (End_Addr) <= MAX_FALCON_DMEM_OFFSET_BYTES),
       Post => ( Ghost_Hs_Init_States_Tracker = DMEM_SCRUBBED),
     Global => (In_Out => Ghost_Hs_Init_States_Tracker),
     Inline_Always;
end Cleanup;
