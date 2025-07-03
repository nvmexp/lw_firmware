-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Falcon_Selwrebusdio_Falc_Specific_Tu10x; use Falcon_Selwrebusdio_Falc_Specific_Tu10x;

package body Falcon_Selwrebusdio
with SPARK_Mode => On
is

   procedure Selwre_Bus_Read_Register( Reg_Addr : LW_SSE_BAR0_ADDR;
                                       Reg_Val : out LwU32;
                                       Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is

      pragma Suppress(All_Checks);
   begin

      -- Initialize with an invalid value
      Reg_Val := LwU32'Last;

      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop


         Selwre_Bus_Send_Request( True, Reg_Addr, 0, Status );
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Selwre_Bus_Get_Data( Reg_Val, Status );
      end loop Main_Block;

   end Selwre_Bus_Read_Register;

   procedure Selwre_Bus_Write_Register( Reg_Addr : LW_SSE_BAR0_ADDR;
                                        Reg_Val : LwU32;
                                        Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      pragma Suppress(All_Checks);
   begin

      Selwre_Bus_Send_Request( False, Reg_Addr, Reg_Val, Status );

   end Selwre_Bus_Write_Register;



   procedure Selwre_Bus_Send_Request( Read_Request : Boolean;
                                      Reg_Addr : LW_SSE_BAR0_ADDR;
                                      Write_Val : LwU32;
                                      Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      pragma Suppress(All_Checks);

      Timestamp_Initial : LwU32;
      Timestamp         : LwU32;
      Error             : Boolean;
      Rd_Bit_Reg_Addr   : LwU32;

   begin

      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         -- Initialize Ghost variable to NO SE ERROR, this cannot be a precondition
         Ghost_Se_Status := NO_SE_ERROR;

         Read_Ptimer0(Val    => Timestamp_Initial,
                      Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         loop
            pragma Loop_Ilwariant (Status = UCODE_ERR_CODE_NOERROR);
            pragma Loop_Ilwariant (Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE);

            Read_Doc_Ctrl_Error(Error      => Error,
                                Error_Type => Error1,
                                Status     => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            Read_Ptimer0(Val    => Timestamp,
                         Status => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            -- Is it better to use a if condition instead of this??
            pragma Assume( Timestamp > Timestamp_Initial );
            exit when Error or else
              ( ( Timestamp - Timestamp_Initial ) > FALCON_DIO_TIMEOUT );

         end loop;

         if ( Timestamp  - Timestamp_Initial ) > FALCON_DIO_TIMEOUT then
            -- Ghost variable Ghost_Se_Status updated to corresponding error value
            Ghost_Se_Status := SE_ERROR;
            Status := SEBUS_SEND_REQUEST_TIMEOUT_CHANNEL_EMPTY;
            exit Main_Block;
         end if;

         if Read_Request then
            Rd_Bit_Reg_Addr := "or"( LwU32(Reg_Addr), 16#0001_0000#);
         else
            Rd_Bit_Reg_Addr := "and"( LwU32(Reg_Addr), 16#FFFE_FFFF#);

            Write_LwU32_Doc_D1(Val    => Write_Val,
                               Status => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         end if;

         Write_LwU32_Doc_D0(Val    => Rd_Bit_Reg_Addr,
                            Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Read_Request = False then

            Read_Ptimer0(Val    => Timestamp_Initial,
                         Status => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            loop
               pragma Loop_Ilwariant (Status = UCODE_ERR_CODE_NOERROR and then
                                      Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE);

               Read_Doc_Ctrl_Error(Error      => Error,
                                   Error_Type => Error2,
                                   Status     => Status);
               exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

               Read_Ptimer0(Val    => Timestamp,
                            Status => Status);
               exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

               -- Is it better to use a if condition instead of this??
               pragma Assume( Timestamp > Timestamp_Initial );
               exit when Error or else
                 ( ( Timestamp - Timestamp_Initial ) > FALCON_DIO_TIMEOUT );

            end loop;

            if ( Timestamp - Timestamp_Initial ) > FALCON_DIO_TIMEOUT then
               -- Ghost variable Ghost_Se_Status updated to corresponding error value
               Ghost_Se_Status := SE_ERROR;
               Status := SEBUS_SEND_REQUEST_TIMEOUT_WRITE_COMPLETE;
               exit Main_Block;
            end if;

         end if;
         pragma Assert (Ghost_Se_Status = NO_SE_ERROR);
      end loop Main_Block;

   end Selwre_Bus_Send_Request;

   procedure Selwre_Bus_Get_Data( Reg_Val : out LwU32;
                                  Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      pragma Suppress(All_Checks);

      Timestamp_Initial : LwU32;
      Timestamp         : LwU32;
      Count             : LwU32;
      Error             : Boolean;
   begin
      Main_Block :
      for Unused_Loop_Var in 1 .. 1 loop

         -- Initialize Ghost variable to NO SE ERROR, this cannot be a precondition
         Ghost_Se_Status := NO_SE_ERROR;

         -- Initialize with an invalid value to make flow analysis happy
         Reg_Val := LwU32'Last;

         Read_Ptimer0(Val    => Timestamp_Initial,
                      Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         loop

            pragma Loop_Ilwariant (Status = UCODE_ERR_CODE_NOERROR and then
                                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE);

            Read_Count(Count  => Count,
                       Status => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            Read_Ptimer0(Val    => Timestamp,
                         Status => Status);
            exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

            -- Is it better to use a if condition instead of this??
            pragma Assume(  Timestamp  > Timestamp_Initial );
            exit when Count /= 0 or else
              ( ( Timestamp - Timestamp_Initial ) > FALCON_DIO_TIMEOUT );
         end loop;

         if ( Timestamp - Timestamp_Initial ) > FALCON_DIO_TIMEOUT then
            -- Ghost variable Ghost_Se_Status updated to corresponding error value
            Ghost_Se_Status := SE_ERROR;
            Status := SEBUS_GET_DATA_TIMEOUT;
            exit Main_Block;
         end if;

         Write_Dic_Ctrl(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Read_Doc_Ctrl_Error(Error      => Error,
                             Error_Type => Error3,
                             Status     => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         if Error then
            -- Ghost variable Ghost_Se_Status updated to corresponding error value
            Ghost_Se_Status := SE_ERROR;
            Status := SEBUS_GET_DATA_BUS_REQUEST_FAIL;
            exit Main_Block;
         end if;

         Read_LwU32_Dic_D0(Val    => Reg_Val,
                           Status => Status);

      end loop Main_Block;

   end Selwre_Bus_Get_Data;
end Falcon_Selwrebusdio;

