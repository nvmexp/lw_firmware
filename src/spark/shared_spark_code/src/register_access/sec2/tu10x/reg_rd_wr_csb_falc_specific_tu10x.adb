-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Utility; use Utility;
with Pri_Err_Handling_Falc_Specific_Tu10x; use Pri_Err_Handling_Falc_Specific_Tu10x;

package body Reg_Rd_Wr_Csb_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   procedure Csb_Reg_Rd32_Generic(Reg  : out GENERIC_REGISTER;
                                  Addr : CSB_ADDR;
                                  Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is

      Val : LwU32;
      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
      function UC_GENERIC_REGISTER is new Ada.Unchecked_Colwersion ( Source => LwU32,
                                                                     Target => GENERIC_REGISTER );
   begin

      Falc_Ldx_I_C( Addr   => Addr,
                    Bitnum => 0,
                    Val    => Val );
      Reg := UC_GENERIC_REGISTER(Val);

      -- Pri Err Handling
      Check_Csb_Err_Value(Status => Status);
   end Csb_Reg_Rd32_Generic;

   procedure Csb_Reg_Wr32_Generic(Reg  : GENERIC_REGISTER;
                                  Addr : CSB_ADDR;
                                  Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)

   is

      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
      function UC_LwU32 is new Ada.Unchecked_Colwersion (Source => GENERIC_REGISTER,
                                                         Target => LwU32);

      Val : constant LwU32 := UC_LwU32(Reg);
   begin

      Falc_Stxb_C(Addr => Addr,
                  Val  => Val);

      Check_Csb_Err_Value(Status);

   end Csb_Reg_Wr32_Generic;

   package body Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
   with SPARK_Mode => On
   is

      pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);

      function UC_GENERIC_REGISTER is new Ada.Unchecked_Colwersion ( Source => LwU32,
                                                                     Target => GENERIC_REGISTER );

      function UC_LwU32 is new Ada.Unchecked_Colwersion (Source => GENERIC_REGISTER,
                                                         Target => LwU32);


      procedure Csb_Reg_Rd32_Generic_Inline(Reg  : out GENERIC_REGISTER;
                                     Addr : CSB_ADDR;
                                     Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
      is

         Val : LwU32;
      begin

         Falc_Ldx_I_C( Addr   => Addr,
                       Bitnum => 0,
                       Val    => Val );
         Reg := UC_GENERIC_REGISTER(Val);

         -- Pri Err Handling
         Check_Csb_Err_Value(Status => Status);
      end Csb_Reg_Rd32_Generic_Inline;

      procedure Csb_Reg_Wr32_Generic_Inline(Reg  : GENERIC_REGISTER;
                                     Addr : CSB_ADDR;
                                     Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)

      is

         Val :  constant LwU32 := UC_LwU32(Reg);
      begin

         Falc_Stxb_C(Addr => Addr,
                     Val  => Val);

         -- Pri Err Handling
         Check_Csb_Err_Value(Status);

      end Csb_Reg_Wr32_Generic_Inline;

      procedure Csb_Reg_Wr32_Generic_No_Peh(Reg  : GENERIC_REGISTER;
                                            Addr : CSB_ADDR)
      is
         pragma Assert (LwU32'Size = GENERIC_REGISTER'Size);
         Val : constant LwU32 := UC_LwU32(Reg);
      begin

         Falc_Stxb_C(Addr => Addr,
                     Val  => Val);

      end Csb_Reg_Wr32_Generic_No_Peh;

   end Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x;

end Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;
