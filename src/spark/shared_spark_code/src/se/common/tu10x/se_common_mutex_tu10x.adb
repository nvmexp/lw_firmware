-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Falcon_Selwrebusdio; use Falcon_Selwrebusdio;
with Ada.Unchecked_Colwersion;
with Se_Pka_Mutex_Falc_Specific_Tu10x; use Se_Pka_Mutex_Falc_Specific_Tu10x;

package  body Se_Common_Mutex_Tu10x
is
   procedure Se_Acquire_Common_Mutex( MutexId     : SEC2_MUTEX_RESERVATION;
                                      Status      : out LW_UCODE_UNIFIED_ERROR_TYPE;
                                      Mutex_Token : out LwU8)
   is
      GroupIndex                         : LwU8                   := 0;
      MutexIndex                         : LwU8                   := 0;
      Reg_Val                            : LwU32                  := 0;
      MutexOwner                         : LwU32                  := 0;
      Timestamp_Initial                  : LwU32                  := 0;
      Timestamp                          : LwU32                  := 0;
      Mutex_Id_Reg                       : LW_SSE_SE_COMMON_MUTEX_ID_REGISTER;
      Mutex_Mutex_Reg                    : LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER;
      Mutex_Release_Readback_Reg         : LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER;
      function UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_ID is new Ada.Unchecked_Colwersion
                                                            (Source => LwU32,
                                                             Target => LW_SSE_SE_COMMON_MUTEX_ID_REGISTER);
      function UC_LW_SSE_SE_COMMON_MUTEX_ID_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                            (Source => LW_SSE_SE_COMMON_MUTEX_ID_REGISTER,
                                                             Target => LwU32);
      function UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX is new Ada.Unchecked_Colwersion
                                                               (Source => LwU32,
                                                                Target => LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER);
       function UC_LW_SSE_SE_COMMON_MUTEX_MUTEX_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                               (Source => LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER,
                                                                Target => LwU32);
      function UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8 is new Ada.Unchecked_Colwersion
                                                               (Source => SEC2_MUTEX_RESERVATION,
                                                                Target => LwU8);

      function UC_LW_SSE_SE_COMMON_MUTEX_ID_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX is new Ada.Unchecked_Colwersion
                                                            (Source => LW_SSE_SE_COMMON_MUTEX_ID_REGISTER,
                                                             Target => LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER);

   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Mutex_Release_Readback_Reg.F_Value := Lw_Sse_Se_Common_Mutex_Mutex_Value_Init;
         -- Check validity of group and mutex
         GroupIndex := Selwre_Mutex_Derive_GroupId(UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8(MutexId));

         MutexIndex := Selwre_Mutex_Derive_MutexId(UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8(MutexId));

         if Selwre_Mutex_Group_Index_Is_Valid(Group_Index => GroupIndex, Mutex_Index => MutexIndex) /= True
         then
            Status := SE_COMMON_MUTEX_INDEX_ILWALID;
            exit Main_Block;
         end if;
         -- ToDo: RahulB add timeout for while 1 loop
         -- Get HW generated token
         Selwre_Bus_Read_Register(
           Lw_Sse_Se_Common_Mutex_Id_Addr(LW_SSE_SE_COMMON_MUTEX_ID_INDEX(GroupIndex)), Reg_Val, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         Mutex_Id_Reg := UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_ID(Reg_Val);
         if Mutex_Id_Reg.F_Value = Lw_Sse_Se_Common_Mutex_Id_Value_Init or else
            Mutex_Id_Reg.F_Value = Lw_Sse_Se_Common_Mutex_Id_Value_Not_Avail
         then
             Status := SE_COMMON_MUTEX_ACQUIRE_FAILED;
             exit Main_Block;
         end if;
         Mutex_Mutex_Reg := UC_LW_SSE_SE_COMMON_MUTEX_ID_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX(Mutex_Id_Reg);

         Read_Timestamp(Timestamp_Initial, Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         Mutex_Acquire:
         loop
            -- Write generated token to Mutex register
            Selwre_Bus_Write_Register(
                                      Reg_Addr => Lw_Sse_Se_Common_Mutex_Mutex_Addr
                                        (LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX(GroupIndex),
                                         LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX(MutexIndex)),
                                      Reg_Val => UC_LW_SSE_SE_COMMON_MUTEX_MUTEX_TO_LwU32(Mutex_Mutex_Reg),
                                      Status =>  Status);

           exit Mutex_Acquire when Status /= UCODE_ERR_CODE_NOERROR;

            -- Read back Mutex register
            Selwre_Bus_Read_Register(
                                     Reg_Addr => Lw_Sse_Se_Common_Mutex_Mutex_Addr(
                                       LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX(GroupIndex),
                                       LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX(MutexIndex)),
                                     Reg_Val => MutexOwner,
                                     Status => Status);
            exit Mutex_Acquire when Status /= UCODE_ERR_CODE_NOERROR;

            Read_Timestamp(Timestamp, Status);
            exit Mutex_Acquire when Status /= UCODE_ERR_CODE_NOERROR;

            Mutex_Release_Readback_Reg := UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX(MutexOwner);
            pragma Assume(Timestamp > Timestamp_Initial );
            exit Mutex_Acquire when Mutex_Mutex_Reg.F_Value = Mutex_Release_Readback_Reg.F_Value or else
              (( Timestamp - Timestamp_Initial ) > SE_COMMON_MUTEX_TIMEOUT_VAL_USEC);
         end loop Mutex_Acquire;
         pragma Assume(Timestamp > Timestamp_Initial );
         if Status = UCODE_ERR_CODE_NOERROR and then
           ( ( Timestamp - Timestamp_Initial ) < SE_COMMON_MUTEX_TIMEOUT_VAL_USEC )
         then
            Mutex_Token := LwU8(Mutex_Release_Readback_Reg.F_Value);
            exit Main_Block;
         end if;
         -- If code reach here means error was observed while acquiring mutex
         if Mutex_Id_Reg.F_Value /= Lw_Sse_Se_Common_Mutex_Id_Value_Init and then
           Mutex_Id_Reg.F_Value /= Lw_Sse_Se_Common_Mutex_Id_Value_Not_Avail
         then
            -- Release the Mutex_Token generated by this function
            Mutex_Token := 0;
            Selwre_Bus_Write_Register(
             Reg_Addr => Lw_Sse_Se_Common_Mutex_Id_Release_Addr(
                                  LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_INDEX(GroupIndex)),
             Reg_Val => UC_LW_SSE_SE_COMMON_MUTEX_ID_TO_LwU32(Mutex_Id_Reg),
             Status => Status);
         end if;
         Status := SE_COMMON_MUTEX_ACQUIRE_FAILED;
      end loop Main_Block;
   end Se_Acquire_Common_Mutex;


   procedure Se_Release_Common_Mutex( MutexId: SEC2_MUTEX_RESERVATION;
                                      HwTokenFromCaller: LwU8;
                                      Status : out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      MutexOwnerInHw                     : LwU8                        := 0;
      GroupIndex                         : LwU8                        := 0;
      MutexIndex                         : LwU8                        := 0;
      Reg_Val                            : LwU32                       := 0;
      Status2                            : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
      Mutex_Release_Reg                  : LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_REGISTER;
      Mutex_Mutex_Reg                    : LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER;
      function UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX is new Ada.Unchecked_Colwersion
                                                               (Source => LwU32,
                                                                Target => LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER);
      function UC_LW_SSE_SE_COMMON_MUTEX_MUTEX_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                               (Source => LW_SSE_SE_COMMON_MUTEX_MUTEX_REGISTER,
                                                                Target => LwU32);
      function UC_LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                                (Source => LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_REGISTER,
                                                                 Target => LwU32);
       function UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8 is new Ada.Unchecked_Colwersion
                                                                (Source => SEC2_MUTEX_RESERVATION,
                                                                 Target => LwU8);
   begin
        Main_Block:
        for Unused_Loop_Var in 1 .. 1 loop
         -- Check validity of group and mutex
         GroupIndex := Selwre_Mutex_Derive_GroupId(UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8(MutexId));

         MutexIndex := Selwre_Mutex_Derive_MutexId(UC_LW_SEC2_MUTEX_RESERVATION_TO_LwU8(MutexId));

         if Selwre_Mutex_Group_Index_Is_Valid(Group_Index => GroupIndex, Mutex_Index => MutexIndex) /= True
         then
            Status := SE_COMMON_MUTEX_INDEX_ILWALID;
            exit Main_Block;
         end if;

        -- Read back mutex register and confirm ownership against hwToken passed by caller
         Selwre_Bus_Read_Register(
          Reg_Addr => Lw_Sse_Se_Common_Mutex_Mutex_Addr(LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX(GroupIndex),
                                                        LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX(MutexIndex)),
          Reg_Val => Reg_Val,
          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Mutex_Mutex_Reg := UC_LwU32_TO_LW_SSE_SE_COMMON_MUTEX_MUTEX(Reg_Val);
         MutexOwnerInHw :=  LwU8(Mutex_Mutex_Reg.F_Value);
         if  HwTokenFromCaller /=  MutexOwnerInHw
         then
            Status := SE_COMMON_MUTEX_OWNERSHIP_MATCH_FAILED;
             exit Main_Block;
         end if;
         -- Write generated token to Mutex register
         Mutex_Mutex_Reg.F_Value := Lw_Sse_Se_Common_Mutex_Mutex_Value_Init;
         Selwre_Bus_Write_Register(
           Reg_Addr => Lw_Sse_Se_Common_Mutex_Mutex_Addr(LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_1_INDEX(GroupIndex),
                                                         LW_SSE_SE_COMMON_MUTEX_MUTEX_SIZE_2_INDEX(MutexIndex)),
           Reg_Val => UC_LW_SSE_SE_COMMON_MUTEX_MUTEX_TO_LwU32(Mutex_Mutex_Reg),
           Status => Status);

         Mutex_Release_Reg.F_Value := LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_VALUE_FIELD(HwTokenFromCaller);
         Selwre_Bus_Write_Register(
           Reg_Addr => Lw_Sse_Se_Common_Mutex_Id_Release_Addr(
                               LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_INDEX(GroupIndex)),
           Reg_Val => UC_LW_SSE_SE_COMMON_MUTEX_ID_RELEASE_TO_LwU32(Mutex_Release_Reg),
           Status => Status2);

         if Status = UCODE_ERR_CODE_NOERROR
         then
            Status := Status2;
         end if;
        end loop Main_Block;
   end Se_Release_Common_Mutex;
end Se_Common_Mutex_Tu10x;
