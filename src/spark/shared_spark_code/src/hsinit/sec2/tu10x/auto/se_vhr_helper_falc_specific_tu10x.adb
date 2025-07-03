-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_


with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;

package body Se_Vhr_Helper_Falc_Specific_Tu10x is

   -- TODO : CCG generates pelwliar code. Need to raise ticket with adacore.
--   function Base_Addr_Helper( Base_Addr : LW_CSEC_FALCON_VHRCFG_BASE_REGISTER)
--                             return LW_SSE_BAR0_ADDR
--   is
--      Ret_Val : LW_SSE_BAR0_ADDR;

--      case Base_Addr.F_Val is
--         when Lw_Csec_Falcon_Vhrcfg_Base_Val_Deft =>
--            Ret_Val := 0;
--         when Lw_Csec_Falcon_Vhrcfg_Base_Val_Sec =>
--            Ret_Val := 33280;
--         when Lw_Csec_Falcon_Vhrcfg_Base_Val_Pwr_Pmu =>
--            Ret_Val := 33776;
--         when Lw_Csec_Falcon_Vhrcfg_Base_Val_Lwdec =>
--            Ret_Val := 34272;
--         when Lw_Csec_Falcon_Vhrcfg_Base_Val_Gsp =>
--            Ret_Val := 34528;
--      end case;
--      return Ret_Val;
--   end Base_Addr_Helper;

   function Shift_Left_Helper( Value : LwU32;
                               Amount : LW_CSEC_FALCON_VHRCFG_ENTNUM_VAL_FIELD )
                              return LwU32
   is

      pragma Suppress(All_Checks);

      Ret_Val : LwU32;

   begin

      case Amount is

         when Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Deft =>
            Ret_Val := Value;

         when Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Gsp =>
            Ret_Val := Safe_Shift_Left( Value, 3 );

         when Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Lwdec =>
            Ret_Val := Safe_Shift_Left( Value, 5 );

         when Lw_Csec_Falcon_Vhrcfg_Entnum_Val_Sec =>
            Ret_Val := Safe_Shift_Left( Value, 10 );

      end case;

      return Ret_Val;

   end Shift_Left_Helper;

   procedure Callwlate_Sse_Addr(Sse_Addr : out LW_SSE_BAR0_ADDR; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Base_Addr : LW_CSEC_FALCON_VHRCFG_BASE_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_Vhrcfg_Base(Reg    => Base_Addr,
                                  Addr   => Lw_Csec_Falcon_Vhrcfg_Base,
                                  Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- TODO CCG generates pelwliar code. Need to raise ticket with adacore.
         Sse_Addr := 33280; --Base_Addr_Helper( Base_Addr );
       --  if LwU32( Sse_Addr ) = LwU32( LW_SSE_BAR0_ADDR'Last ) then
       --     Status := BAD_VHRCFG_BASE_ADDR;
       --  end if;
      end loop Main_Block;
   end Callwlate_Sse_Addr;


   procedure Callwlate_Ent_Num_Mask(Ent_Num_Mask : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Ent_Num        : LW_CSEC_FALCON_VHRCFG_ENTNUM_REGISTER;
   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Csb_Reg_Rd32_Vhrcfg_Entnum(Reg    => Ent_Num,
                                    Addr   => Lw_Csec_Falcon_Vhrcfg_Entnum,
                                    Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Shift_Left_Helper is needed as the shift Amount is variable
         Ent_Num_Mask := Shift_Left_Helper( 1, Ent_Num.F_Val ) - 1 ;

      end loop Main_Block;

   end Callwlate_Ent_Num_Mask;


end Se_Vhr_Helper_Falc_Specific_Tu10x;
