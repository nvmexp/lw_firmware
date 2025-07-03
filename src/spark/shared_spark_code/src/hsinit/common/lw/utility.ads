-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;
with Falcon_Utility_Whitelist; use Falcon_Utility_Whitelist;
with Ada.Unchecked_Colwersion;
with System;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Register_Whitelist_Shared; use Register_Whitelist_Shared;
with Register_Whitelist_Ucode_Specific; use Register_Whitelist_Ucode_Specific;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;

--  @summary
--  Falcon intrinsic and generic packages.
--
--  @description
--  This package contains procedures and required wrappers to perform falcon intrinsics and also generic procedures
--  like Last Chance Handler and null Initialize to please prover procedures.
package Utility
with SPARK_Mode => On
is

   --  Functions to do unchecked colwersion from Address to LwU32 and vice-versa
   --  For falcon architecture Address is of 32 bits and hence
   --  added //sw/pvt/gfw_ucode/spark/poc/target32.atp for this
   pragma Assert (System.Address'Size = LwU32'Size);

   function UC_ADDRESS_TO_LwU32 is new Ada.Unchecked_Colwersion (Source => System.Address,
                                                                 Target => LwU32);

   function UC_LwU32_TO_ADDRESS is new Ada.Unchecked_Colwersion (Source => LwU32,
                                                                 Target => System.Address);
   procedure Infinite_Loop
     with
       Global => null,
       No_Return,
       Inline_Always;

   --  Procedure to write 0 to the input address
   --  @param Addr Address to scrub
   procedure Scrub_Helper( Addr : UCODE_DMEM_OFFSET_IN_FALCON_BYTES )
     with
       Global => null,
       Depends => ( ( null ) => Addr ),
       Inline_Always;

   --  Wrapper which calls falc_jmp.
   --  @param Addr Address of the instruction to jump to.
   procedure Falc_Jmp_C(Addr : LwU32)
     with
       Pre => Addr <= MAX_FALCON_IMEM_OFFSET_BYTES_FOR_LAST_DWORD_DATUM,
       Global => null,
       Depends => ( ( null ) => Addr ),
       Inline_Always;

   --  Wrapper which calls falc_sclrb.
   --  @param Bitnum Bit position of the bit to clear in CSW.
   procedure Falc_Sclrb_C(Bitnum : LwU32)
     with
       Pre => ( Falc_Sclrb_Bitnum_Check_Success( Bitnum ) ),
       Global => null,
       Depends => ( ( null ) => Bitnum ),
       Inline_Always;

   --  Wrapper which calls falc_halt.
   procedure Falc_Halt_C
     with
       Pre => (Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                 Ghost_Hs_Exit_State_Tracker = CLEANUP_COMPLETED),
     Global => (Proof_In => (Ghost_Halt_Action_State,
                             Ghost_Hs_Exit_State_Tracker)),
     Inline_Always;

   --  Wrapper which calls falc_ssetb_i.
   --  @param Bitnum Bit position of the bit to set in CSW.
   procedure Falc_Ssetb_I_C(Bitnum : LwU32)
     with
       Pre => ( Falc_Ssetb_Bitnum_Check_Success( Bitnum ) ),
       Global => null,
       Depends => ( ( null ) => Bitnum ),
       Inline_Always;

   --  Wrapper which calls falc_ldx_i_wrapper.
   --  @param Addr CSB address of register to read from.
   --  @param Bitnum Bit position of the bit to set in CSW.
   --  @param Val Value read from register.
   procedure Falc_Ldx_I_C(Addr   : CSB_ADDR;
                          Bitnum : LwU32;
                          Val    : out LwU32)
     with
       Pre => ( Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                  Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                    ( Is_Valid_Shared_Csb_Read_Address( LwU32(Addr) )  or else
                       Is_Valid_Ucode_Csb_Register_Write_Address ) and then
                    ( Bitnum = 0 ) ),
       Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                               Ghost_Csb_Pri_Error_State)),
     Depends => ( Val => ( Addr, Bitnum ) ),
     Inline_Always;

   --  Wrapper which calls falc_stx_wrapper.
   --  @param Addr CSB address of register to write.
   --  @param Val Value to write to CSB register.
   procedure Falc_Stx_C(Addr : CSB_ADDR;
                        Val  : LwU32)
     with
       Pre => ( Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                  Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                    (Is_Valid_Shared_Csb_Write_Address( LwU32(Addr) )  or else
                       Is_Valid_Ucode_Csb_Register_Write_Address)),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Csb_Pri_Error_State)),
     Depends => ( null => ( Addr, Val ) ),
     Inline_Always;

   --  Wrapper which calls falc_stxb_wrapper.
   --  @param Addr Address of CSB register to write to.
   --  @param Val Value to write to CS register.
   procedure Falc_Stxb_C(Addr : CSB_ADDR;
                         Val  : LwU32)
     with
       Pre => ( Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                  Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                    (Is_Valid_Shared_Csb_Write_Address( LwU32(Addr) ) or else
                       Is_Valid_Ucode_Csb_Register_Write_Address)),
     Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                             Ghost_Csb_Pri_Error_State)),
     Depends => ( null => ( Addr, Val ) ),
     Inline_Always;

   procedure Falc_Imblk_C(Blk_Status : out LwU32;
                          Blk_No     : LwU32)
     with
       Global => null,
       Depends => ( Blk_Status => null,
                    null => Blk_No ),
     Inline_Always;

   --  Wrapper which implements custom Last Chance Handler.
   procedure Last_Chance_Handler
     with
       Global => ( Proof_In => Ghost_Halt_Action_State,
                   In_Out => (Ghost_Hs_Init_States_Tracker,
                              Ghost_Hs_Exit_State_Tracker)),
     Export => True,
     Convention => C,
     External_Name => "__lwstom_gnat_last_chance_handler",
     Depends => ( Ghost_Hs_Init_States_Tracker => null,
                  Ghost_Hs_Exit_State_Tracker => null,
                  null => (Ghost_Hs_Init_States_Tracker,
                           Ghost_Hs_Exit_State_Tracker)),
     Linker_Section => LINKER_SECTION_NAME;

   --  Wrapper which calls falc_rspr_sec_spr_wrapper.
   function Read_Sec_Spr_C return LwU32
     with
       Global => null,
       Depends => ( Read_Sec_Spr_C'Result => ( null ) ),
       Inline_Always;

   --  Wrapper which calls falc_wspr_sec_spr_wrapper.
   --  @param Sec_Spr value to write.
   procedure Write_Sec_Spr_C(Sec_Spr : LwU32)
     with
       Global => null,
       Depends => ( null => ( Sec_Spr ) ),
       Inline_Always;

   --  Wrapper which calls falc_rspr_sec_spr_wrapper.
   function Read_Csw_Spr_C return LwU32
     with
       Global => null,
       Depends => ( Read_Csw_Spr_C'Result => null ),
     Inline_Always;

   --  Wrapper which calls falc_wspr_csw_spr_wrapper.
   --  @param Csw_Spr value to write.
   procedure Write_Csw_Spr_C( Csw_Spr: LwU32 )
     with
       Global => null,
       Depends => ( null => ( Csw_Spr ) ),
       Inline_Always;

   --  Wrapper which calls falc_sethi_i.
   --  TODO : Add pramams information
   function Falc_Sethi_I_C( Lo : LwU32;
                            Hi : LwU32 )
                           return LwU32
     with
       Global => null,
       Depends => ( Falc_Sethi_I_C'Result => ( Lo, Hi ) ),
       Inline_Always;

   --  Wrapper which calls falc_dmwait.
   procedure Falc_Dmwait_C
     with
       Global => null,
       Depends => null,
       Inline_Always;

   --  Wrapper which calls falc_rspr_sp_spr_wrapper.
   function Read_Sp_Spr_C return LwU32
     with
       Global => null,
       Depends => ( Read_Sp_Spr_C'Result => ( null ) ),
       Post => Read_Sp_Spr_C'Result < LwU32'Last -4,
       Inline_Always;

   procedure Falc_Imilw_C(Blk_No  : LwU32)
     with
       Global => null,
       Depends => ( null => Blk_No ),
     Inline_Always;

private

   --  This function corresponds to falcon intrinsic function jmp.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of the instruction to jump to.
   procedure falc_jmp(Addr : LwU32)
     with
       Pre => Addr <= MAX_FALCON_IMEM_OFFSET_BYTES_FOR_LAST_DWORD_DATUM,
       Import => True,
       Convention => C,
       Global => null,
       Depends => ( ( null ) => Addr ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function sclrb.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param BitNum Bit position of the bit to clear in CSW.
   procedure falc_sclrb(Bitnum : LwU32)
     with
       Pre => ( Falc_Sclrb_Bitnum_Check_Success( Bitnum ) ),
       Import => True,
       Convention => C,
       Global => null,
       Depends => ( ( null ) => Bitnum ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function halt.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   procedure falc_halt
     with
       Pre => (Ghost_Halt_Action_State = HALT_ACTION_ALLOWED and then
                 Ghost_Hs_Exit_State_Tracker = CLEANUP_COMPLETED),
     Global => (Proof_In => (Ghost_Halt_Action_State,
                             Ghost_Hs_Exit_State_Tracker)),
     Import => True,
     Convention => C,
     Depends => null,
     Inline_Always;

   --  This function corresponds to falcon intrinsic function ssetb.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Bitnum Bit position of the bit to set in CSW.
   procedure falc_ssetb_i(Bitnum : LwU32)
     with
       Pre => ( Falc_Ssetb_Bitnum_Check_Success( Bitnum ) ),
       Import => True,
       Convention => C,
       Global => null,
       Depends => ( ( null ) => Bitnum ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function ssetb.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of CSB register to read from.
   --  @param Bitnum Bit position of the bit to set in CSW.
   function falc_ldx_i_wrapper(Addr : LwU32; Bitnum : LwU32) return LwU32
     with
       Pre => ( (Is_Valid_Shared_Csb_Read_Address(Addr) or else
                  Is_Valid_Ucode_Csb_Register_Write_Address) and then
                    ( Bitnum = 0 ) ),
       Import => True,
       Convention => C,
       Global => null,
       Depends => ( falc_ldx_i_wrapper'Result => ( Addr, Bitnum ) ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function stx.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of CSB register to write to.
   --  @param Val Value to write to CSB register.
   procedure falc_stx_wrapper(Addr : LwU32; Val : LwU32)
     with
       Pre => ( Is_Valid_Shared_Csb_Read_Address( Addr ) or else
                      Is_Valid_Ucode_Csb_Register_Read_Address),
         Import => True,
         Convention => C,
         Global => null,
         Depends => ( null => ( Addr, Val ) ),
         Inline_Always;

   --  This function corresponds to falcon intrinsic function stxb.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of CSB register to write to.
   --  @param Val Value to write to CSB register.
   procedure falc_stxb_wrapper(Addr : LwU32; Val : LwU32)
     with
       Pre => ( Is_Valid_Shared_Csb_Read_Address( Addr ) or else
                      Is_Valid_Ucode_Csb_Register_Read_Address),
         Import => True,
         Convention => C,
         Global => null,
         Depends => ( null => ( Addr, Val ) ),
         Inline_Always;

   --  This function corresponds to falcon intrinsic function rspr.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   function falc_rspr_sec_spr_wrapper return LwU32
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( falc_rspr_sec_spr_wrapper'Result => ( null ) ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function wspr.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_wspr_sec_spr_wrapper(Sec_Spr: LwU32)
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Inline_Always;

   --  This function corresponds to falcon intrinsic function rspr.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   function falc_rspr_csw_spr_wrapper return LwU32
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( falc_rspr_csw_spr_wrapper'Result => ( null ) ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function wspr.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   procedure falc_wspr_csw_spr_wrapper( Csw_Spr: LwU32 )
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Inline_Always;

   --  This function corresponds to falcon intrinsic function sethi.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  Add params information
   function falc_sethi_i( Lo : LwU32;
                          Hi : LwU32 )
                         return LwU32
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( falc_sethi_i'Result => ( Lo, Hi ) ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function dmwait.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   procedure falc_dmwait
     with
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => null,
       Inline_Always;

   --  This function corresponds to falcon intrinsic function rspr.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   function falc_rspr_sp_spr_wrapper return LwU32
     with
       Post => falc_rspr_sp_spr_wrapper'Result < LwU32'Last -4,
       Import        => True,
       Convention    => C,
       Global        => null,
       Depends => ( falc_rspr_sp_spr_wrapper'Result => ( null ) ),
       Inline_Always;

   --  This function corresponds to falcon intrinsic function imblk.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of CSB register to write to.
   --  @param Val Value to write to CSB register.
   procedure falc_imblk(Blk_Status : out LwU32; Blk_No : LwU32)
     with
       Import => True,
       Convention => C,
       Global => null,
       Depends => ( Blk_Status => null,
                    null => Blk_No ),
     Inline_Always;

   --  This function corresponds to falcon intrinsic function imilw.
   --  This will be imported to C and the actual falcon-instruction will be
   --  implemented in C. This is because we do not have visibility of
   --  falcon instructions here. This function is called with Global attribute = null
   --  to tell flow analysis that this has no global effects.
   --  @param Addr Address of CSB register to write to.
   --  @param Val Value to write to CSB register.
   procedure falc_imilw(Blk_No : LwU32)
     with
       Import => True,
       Convention => C,
       Global => null,
       Depends => (null => Blk_No),
     Inline_Always;

end Utility;
