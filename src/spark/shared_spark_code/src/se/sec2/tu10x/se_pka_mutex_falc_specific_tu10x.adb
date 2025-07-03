-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Csb_Reg_Rd_Wr_Instances; use Csb_Reg_Rd_Wr_Instances;
with Ada.Unchecked_Colwersion;

package body Se_Pka_Mutex_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   function UC_FALCON_PTIMER0_TO_LwU32 is new Ada.Unchecked_Colwersion(Source => LW_CSEC_FALCON_PTIMER0_REGISTER,
                                                                       Target => LwU32);

   procedure Read_Timestamp(Data : out LwU32; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Timestamp_Initial          : LW_CSEC_FALCON_PTIMER0_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Initialized to please prover, however have also specified this as a special case in
         -- post condition. Reviewer please assess if this is enough ?
         Data := 0;
         Csb_Reg_Rd32_Timestamp(Reg    => Timestamp_Initial,
                                Addr   => Lw_Csec_Falcon_Ptimer0,
                                Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Data := UC_FALCON_PTIMER0_TO_LwU32(Timestamp_Initial);
      end loop Main_Block;

   end Read_Timestamp;


end Se_Pka_Mutex_Falc_Specific_Tu10x;
