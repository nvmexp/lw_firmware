-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  SCP CCI intrinsics
--
--  @description
--  This package contains constants of SCP CCI intrinsics
package Scp_Cci_Intrinsics
with SPARK_Mode => On
is

   SCP_R0 : constant := 16#0#;
   SCP_R1 : constant := 16#1#;
   SCP_R2 : constant := 16#2#;
   SCP_R3 : constant := 16#3#;
   SCP_R4 : constant := 16#4#;
   SCP_R5 : constant := 16#5#;
   SCP_R6 : constant := 16#6#;
   SCP_R7 : constant := 16#7#;

   procedure Clear_SCP_Registers
     with
       Pre => (Ghost_Hs_Init_States_Tracker = STALE_SIGNATURE_CLEARED or else
          Ghost_Hs_Init_States_Tracker >= VHR_EMPTY_CHECKED),
       Global => (In_Out => Ghost_Hs_Init_States_Tracker),
     Post => Ghost_Hs_Init_States_Tracker = SCP_REGISTERS_CLEARED,
     Inline_Always;

   --  Wrapper which calls falc_scp_xor.
   --  TODO : Add pramams information
   procedure Falc_Scp_Xor_C( Rx : LwU32;
                             Ry: LwU32 )
     with
       Global => null,
       Depends => ( null => ( Rx, Ry ) ),
       Inline_Always;

   --  Wrapper which calls falc_scp_trap.
   --  TODO : Add pramams information
   procedure Falc_Scp_Trap_C( Tc : LwU32 )
     with
       Global => null,
       Depends => ( null => ( Tc ) ),
       Inline_Always;

   --  Wrapper which calls falc_scp_rand.
   --  TODO : Add pramams information
   procedure Falc_Scp_Rand_C( Ry: LwU32 )
     with
       Global => null,
       Depends => ( null => ( Ry ) ),
       Inline_Always;

   --  Wrapper which calls falc_scp_key.
   --  TODO : Add pramams information
   procedure Falc_Scp_Key_C( Ry: LwU32 )
     with
       Global => null,
       Depends => ( null => ( Ry ) ),
       Inline_Always;

   --  Wrapper which calls falc_scp_encrypt.
   --  TODO : Add pramams information
   procedure Falc_Scp_Encrypt_C( Rx : LwU32;
                                 Ry: LwU32 )
     with
       Global => null,
       Depends => ( null => ( Rx, Ry ) ),
       Inline_Always;

   --  Wrapper which calls falc_trapped_dmread.
   --  TODO : Add pramams information
   procedure Falc_Trapped_Dmread_C( Addr : LwU32 )
     with
       Global => null,
       Depends => ( null => ( Addr ) ),
       Inline_Always;

   --  Wrapper which calls falc_trapped_dmwrite.
   --  TODO : Add pramams information
   procedure Falc_Trapped_Dmwrite_C( Addr : LwU32 )
     with
       Global => null,
       Depends => ( null => ( Addr ) ),
       Inline_Always;

private

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_scp_xor( Rx : LwU32;
                           Ry: LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Rx, Ry ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_scp_trap( Tc : LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Tc ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_scp_rand( Ry: LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Ry ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_scp_key( Ry: LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Ry ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_scp_encrypt( Rx : LwU32;
                               Ry: LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Rx, Ry ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_trapped_dmread( Addr : LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Addr ) ),
       Inline_Always;

   --  This function corresponds to scp cci intrinsic function cci.
   --  This will be imported to C and the actual scp-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_trapped_dmwrite( Addr : LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( null => ( Addr ) ),
       Inline_Always;

end Scp_Cci_Intrinsics;
