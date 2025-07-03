with System; use System;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Lw_Types; use Lw_Types;
with Lw_Types.Array_Types; use Lw_Types.Array_Types;
with Ada.Unchecked_Colwersion;
with Dev_Falc_Spr; use Dev_Falc_Spr;
with Reg_Rd_Wr_Csb_Falc_Specific_Tu10x; use Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;

package Ns_Section
with
SPARK_Mode => On
is
   package Csb_Reg_Wr32_Falcon_Mailbox0 is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                      LW_CSEC_FALCON_MAILBOX0_REGISTER);
   package Csb_Reg_Wr32_Falcon_Brom_Paraaddr is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                           LW_CSEC_FALCON_BROM_PARAADDR_REGISTER);
   package Csb_Reg_Wr32_Falcon_Engidmask is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                       LW_CSEC_FALCON_BROM_ENGIDMASK_REGISTER);
   package Csb_Reg_Wr32_Falcon_Brom_Ucode_Id is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                           LW_CSEC_FALCON_BROM_LWRR_UCODE_ID_REGISTER);
   package Csb_Reg_Wr32_Falcon_Mod_Sel is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                     LW_CSEC_FALCON_MOD_SEL_REGISTER);
   package Csb_Reg_Wr32_Falcon_Csberrstat is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x (GENERIC_REGISTER =>
                                                                                        LW_CSEC_FALCON_CSBERRSTAT_REGISTER);

   --  Requirement: This procedure shall do the following:
   --  Setup BROM to authenticate HS signature of secure code.
   --
   procedure Ns_Entry
     with
       Inline_Always;

private

   function To_Raw is new Ada.Unchecked_Colwersion (Source => LW_FALC_SEC_SPR_Register,
                                                    Target => LwU32);

   procedure Stack_Check_Fail
     with
       Export        => True,
       Convention    => C,
       External_Name => "__stack_chk_fail",
       Linker_Section => ".imem_pub",
       No_Inline;

   procedure Ns_Canary_Setup
     with
       Import        => True,
       Convention    => C,
       External_Name => "__canary_setup";

   Size_4k_DWord : constant LwU32 := 128;

   --  Represents size of RSA3K signature as an array of dwords.
   subtype PKC_RSA_SIG is ARR_LwU32_IDX32 (0 .. Size_4k_DWord - 1);

   --  PKC Params record parsed by BROM.
   --  @field F_Pkc_Signature        Holds PKC signature for secure ucode.
   --  @field F_Pkc_Pk               Pointer to signature if used.
   --  @field F_Hash_Saving          Denotes whether BROM should save hash in VHR.
   type PKC_PARAMS_TYPE is record
      F_Pkc_Signature     : PKC_RSA_SIG;
      F_Pkc_Pk            : LwU32;
      F_Hash_Saving       : LwU32;
   end record
     with Alignment => 16;

   Pkc_Params : PKC_PARAMS_TYPE
     with
       Export     => True,
       Convention => C,
       External_Name => "g_signature",
       Linker_Section => ".dataEntry",
       Alignment => 16;

   --  This procedure returns the address of linker variable _pub_code_start.
   --
   --  @param Selwre_Blk_Start_From_Linker Output parameter : Address of the linker variable _pub_code_start.
   procedure Get_Selwre_Blk_Start_Addr( Selwre_Blk_Start_From_Linker : out LwU32 )
     with
       Global => null,
       Depends => ( Selwre_Blk_Start_From_Linker => null ),
     Inline_Always;

   --  Requirement: This procedure shall do the following:
   --  Return the address of the global struct which contains PKC parameters which needs to be parsed by BROM.
   --
   --  @param Pkc_Params_Addr Output parameter : Address of the global record Pkc_Params.
   procedure Get_Pkc_Param_Addr( Pkc_Params_Addr : out LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD)
     with
       Global => null,
       Depends => ( Pkc_Params_Addr => null ),
     Inline_Always;

   pragma Assert (System.Address'Size = LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD'Size);
   function UC_SYSTEM_ADDRESS_PARAADDR is new
     Ada.Unchecked_Colwersion (Source => System.Address,
                               Target => LW_CSEC_FALCON_BROM_PARAADDR_VAL_FIELD);

end Ns_Section;
