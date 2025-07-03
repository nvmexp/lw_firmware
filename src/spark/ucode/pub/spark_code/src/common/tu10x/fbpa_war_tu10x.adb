-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_


with Dev_Pri_Ringstation_Sys_Ada; use Dev_Pri_Ringstation_Sys_Ada;
with Pub_Fbpa_War_Reg_Rd_Wr_Instances; use Pub_Fbpa_War_Reg_Rd_Wr_Instances;
-- with Update_Mailbox_Falc_Specific_Tu10x; use Update_Mailbox_Falc_Specific_Tu10x;
with Se_Common_Mutex_Tu10x; use Se_Common_Mutex_Tu10x;
with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;

package body Fbpa_War_tu10x
is
   procedure Setting_Decode_Trap12(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Decode_Trap12_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER;
      Decode_Trap12_Match_Cfg_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_CFG_REGISTER;
      Decode_Trap12_Data1_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA1_REGISTER;
      Decode_Trap12_Match_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_REGISTER;
      Decode_Trap12_Mask_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_REGISTER;
      Decode_Trap12_Data2_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_DATA2_REGISTER;
      Trap12_Match_Addr : constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MATCH_ADDR_FIELD  := 16#0090_0100#;
      Trap12_Mask_Addr: constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_ADDR_FIELD  := 16#0001_C000#;
      Trap12_Mask_Subid: constant LW_PPRIV_SYS_PRI_DECODE_TRAP12_MASK_SUBID_FIELD  := 16#0000_003F#;
   begin
       Main_Block:
       for Unused_Loop_Var in 1 .. 1 loop
         Decode_Trap12_Match_Cfg_Reg :=
           (F_Ignore_Read             => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Read_Matched,
            F_Ignore_Write            => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Matched,
            F_Ignore_Write_Ack        => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Write_Ack_Matched,
            F_Ignore_Word_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Word_Access_Matched,
            F_Ignore_Byte_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ignore_Byte_Access_Matched,
            F_Ilwert_Addr_Match       => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Addr_Match_Normal,
            F_Ilwert_Subid_Match      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Ilwert_Subid_Match_Normal,
            F_Trap_Application_Level0 => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level0_Enable,
            F_Trap_Application_Level1 => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level1_Disable,
            F_Trap_Application_Level2 => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Trap_Application_Level2_Disable,
            F_Subid_Ctl               => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Subid_Ctl_Masked,
            F_Compare_Ctl             => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg_Compare_Ctl_Masked
            );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg(Reg => Decode_Trap12_Match_Cfg_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Cfg,
                                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap12_Data1_Reg :=(
         F_V_Rs => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Rs_All,
                                    F_V_Mc => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_Mc_Bc,
                                    F_PAD  => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_Padding_0,
         F_V_For_Action_Set_Priv_Level =>Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1_V_For_Action_Set_Priv_Level_Value_Level_2
         );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data1(Reg => Decode_Trap12_Data1_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data1,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap12_Match_Reg := (F_Addr  => Trap12_Match_Addr,
                                     F_Subid => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match_Subid_I
                                    );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Match(Reg => Decode_Trap12_Match_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Match,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap12_Mask_Reg := ( F_Addr  => Trap12_Mask_Addr,
                                     F_Subid => Trap12_Mask_Subid
                                   );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Mask( Reg => Decode_Trap12_Mask_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Mask,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap12_Data2_Reg := (F_V => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data2_V_I);
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Data2( Reg => Decode_Trap12_Data2_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Data2,
                                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap12_Action_Reg :=
         (
            F_Drop_Transaction        => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Drop_Transaction_Disable,
            F_Force_Decode_Physical   => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Physical_Disable,
            F_Force_Decode_Virtual    => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Decode_Virtual_Disable,
            F_Force_Error_Return      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Force_Error_Return_Disable,
            F_Return_Specified_Data   => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Return_Specified_Data_Disable,
            F_Log_Request             => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Log_Request_Disable,
            F_Redirect_To_Address     => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Address_Disable,
            F_Redirect_To_Ringstation => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Redirect_To_Ringstation_Disable,
            F_Override_Ack            => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Ack_Disable,
            F_Override_Without_Ack    => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Without_Ack_Disable,
            F_Bc_Read_And             => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_And_Disable,
            F_Bc_Read_Or              => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Bc_Read_Or_Disable,
            F_Override_Decode_Error   => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Decode_Error_Disable,
            F_Set_Local_Ordering      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Local_Ordering_Disable,
            F_Clear_Local_Ordering    => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Clear_Local_Ordering_Disable,
            F_Ucode_Write_Intercept   => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Intercept_Disable,
            F_Ucode_Read_Intercept    => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Intercept_Disable,
            F_Ucode_Write_Observe     => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Write_Observe_Disable,
            F_Ucode_Read_Observe      => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Ucode_Read_Observe_Disable,
            F_Set_Priv_Level          => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Set_Priv_Level_Enable,
            F_Override_Wrbe_Error     => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action_Override_Wrbe_Error_Disable
         );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap12_Action(Reg  =>   Decode_Trap12_Action_Reg,
                                                          Addr =>   Lw_Ppriv_Sys_Pri_Decode_Trap12_Action,
                                                          Status => Status);
       end loop Main_Block;
   end Setting_Decode_Trap12;

   procedure Setting_Decode_Trap13(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Decode_Trap13_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER;
      Decode_Trap13_Match_Cfg_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_CFG_REGISTER;
      Decode_Trap13_Data1_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA1_REGISTER;
      Decode_Trap13_Match_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_REGISTER;
      Decode_Trap13_Mask_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_REGISTER;
      Decode_Trap13_Data2_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_DATA2_REGISTER;
      Trap13_Match_Addr : constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MATCH_ADDR_FIELD  := 16#0098_0100#;
      Trap13_Mask_Addr: constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_ADDR_FIELD  := 16#0000_C000#;
      Trap13_Mask_Subid: constant LW_PPRIV_SYS_PRI_DECODE_TRAP13_MASK_SUBID_FIELD  := 16#0000_003F#;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Decode_Trap13_Match_Cfg_Reg :=
           (F_Ignore_Read             => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Read_Matched,
            F_Ignore_Write            => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Matched,
            F_Ignore_Write_Ack        => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Write_Ack_Matched,
            F_Ignore_Word_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Word_Access_Matched,
            F_Ignore_Byte_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ignore_Byte_Access_Matched,
            F_Ilwert_Addr_Match       => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Addr_Match_Normal,
            F_Ilwert_Subid_Match      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Ilwert_Subid_Match_Normal,
            F_Trap_Application_Level0 => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level0_Enable,
            F_Trap_Application_Level1 => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level1_Disable,
            F_Trap_Application_Level2 => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Trap_Application_Level2_Disable,
            F_Subid_Ctl               => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Subid_Ctl_Masked,
            F_Compare_Ctl             => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg_Compare_Ctl_Masked
           );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg(Reg => Decode_Trap13_Match_Cfg_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Cfg,
                                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap13_Data1_Reg :=(
         F_V_Rs => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Rs_All,
         F_V_Mc => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_Mc_Bc,
         F_PAD  => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_Padding_0,
         F_V_For_Action_Set_Priv_Level =>Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1_V_For_Action_Set_Priv_Level_Value_Level_2
         );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data1(Reg => Decode_Trap13_Data1_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data1,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap13_Match_Reg := (F_Addr  => Trap13_Match_Addr,
                                     F_Subid => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match_Subid_I
                                    );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Match(Reg => Decode_Trap13_Match_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Match,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;




         Decode_Trap13_Mask_Reg := ( F_Addr  => Trap13_Mask_Addr,
                                     F_Subid => Trap13_Mask_Subid
                                    );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Mask( Reg => Decode_Trap13_Mask_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Mask,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;



         Decode_Trap13_Data2_Reg := (F_V => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data2_V_I);
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Data2( Reg => Decode_Trap13_Data2_Reg,
                                                          Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Data2,
                                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap13_Action_Reg :=
           (
            F_Drop_Transaction        => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Drop_Transaction_Disable,
            F_Force_Decode_Physical   => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Physical_Disable,
            F_Force_Decode_Virtual    => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Decode_Virtual_Disable,
            F_Force_Error_Return      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Force_Error_Return_Disable,
            F_Return_Specified_Data   => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Return_Specified_Data_Disable,
            F_Log_Request             => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Log_Request_Disable,
            F_Redirect_To_Address     => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Address_Disable,
            F_Redirect_To_Ringstation => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Redirect_To_Ringstation_Disable,
            F_Override_Ack            => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Ack_Disable,
            F_Override_Without_Ack    => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Without_Ack_Disable,
            F_Bc_Read_And             => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_And_Disable,
            F_Bc_Read_Or              => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Bc_Read_Or_Disable,
            F_Override_Decode_Error   => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Decode_Error_Disable,
            F_Set_Local_Ordering      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Local_Ordering_Disable,
            F_Clear_Local_Ordering    => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Clear_Local_Ordering_Disable,
            F_Ucode_Write_Intercept   => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Intercept_Disable,
            F_Ucode_Read_Intercept    => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Intercept_Disable,
            F_Ucode_Write_Observe     => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Write_Observe_Disable,
            F_Ucode_Read_Observe      => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Ucode_Read_Observe_Disable,
            F_Set_Priv_Level          => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Set_Priv_Level_Enable,
            F_Override_Wrbe_Error     => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action_Override_Wrbe_Error_Disable
           );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap13_Action(Reg  =>Decode_Trap13_Action_Reg,
                                                          Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action,
                                                          Status => Status);
      end loop Main_Block;
   end Setting_Decode_Trap13;


   procedure Setting_Decode_Trap14(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Decode_Trap14_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER;
      Decode_Trap14_Match_Cfg_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_CFG_REGISTER;
      Decode_Trap14_Data1_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA1_REGISTER;
      Decode_Trap14_Match_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_REGISTER;
      Decode_Trap14_Mask_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_REGISTER;
      Decode_Trap14_Data2_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_DATA2_REGISTER;
      Trap14_Match_Addr : constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MATCH_ADDR_FIELD  := 16#009A_0100#;
      -- TODO RahulB: Need to investigate if Ada.Unchecked_Colwersion can be used insteaf of defining Trap14_Match_Addr
      Trap14_Mask_Addr: constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_ADDR_FIELD  := 16#0000_0000#;
      Trap14_Mask_Subid: constant LW_PPRIV_SYS_PRI_DECODE_TRAP14_MASK_SUBID_FIELD  := 16#0000_003F#;
   begin
       Main_Block:
       for Unused_Loop_Var in 1 .. 1 loop
         Decode_Trap14_Match_Cfg_Reg :=
           (F_Ignore_Read             => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Read_Matched,
            F_Ignore_Write            => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Matched,
            F_Ignore_Write_Ack        => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Write_Ack_Matched,
            F_Ignore_Word_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Word_Access_Matched,
            F_Ignore_Byte_Access      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ignore_Byte_Access_Matched,
            F_Ilwert_Addr_Match       => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Addr_Match_Normal,
            F_Ilwert_Subid_Match      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Ilwert_Subid_Match_Normal,
            F_Trap_Application_Level0 => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level0_Enable,
            F_Trap_Application_Level1 => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level1_Disable,
            F_Trap_Application_Level2 => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Trap_Application_Level2_Disable,
            F_Subid_Ctl               => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Subid_Ctl_Masked,
            F_Compare_Ctl             => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg_Compare_Ctl_Masked
            );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg(Reg => Decode_Trap14_Match_Cfg_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Cfg,
                                                             Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap14_Data1_Reg := (
         F_V_Rs => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Rs_All,
         F_V_Mc => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_Mc_Bc,
         F_PAD  => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_Padding_0,
         F_V_For_Action_Set_Priv_Level =>Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1_V_For_Action_Set_Priv_Level_Value_Level_2
         );

         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data1(Reg => Decode_Trap14_Data1_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data1,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap14_Match_Reg := (F_Addr  => Trap14_Match_Addr,
                                     F_Subid => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match_Subid_I
                                    );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Match(Reg => Decode_Trap14_Match_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Match,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;




         Decode_Trap14_Mask_Reg := ( F_Addr  => Trap14_Mask_Addr,
                                     F_Subid => Trap14_Mask_Subid
                                   );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Mask( Reg => Decode_Trap14_Mask_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Mask,
                                                         Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;



         Decode_Trap14_Data2_Reg := (F_V => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data2_V_I);
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Data2( Reg => Decode_Trap14_Data2_Reg,
                                                         Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Data2,
                                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Decode_Trap14_Action_Reg :=
         (
            F_Drop_Transaction        => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Drop_Transaction_Disable,
            F_Force_Decode_Physical   => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Physical_Disable,
            F_Force_Decode_Virtual    => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Decode_Virtual_Disable,
            F_Force_Error_Return      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Force_Error_Return_Disable,
            F_Return_Specified_Data   => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Return_Specified_Data_Disable,
            F_Log_Request             => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Log_Request_Disable,
            F_Redirect_To_Address     => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Address_Disable,
            F_Redirect_To_Ringstation => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Redirect_To_Ringstation_Disable,
            F_Override_Ack            => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Ack_Disable,
            F_Override_Without_Ack    => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Without_Ack_Disable,
            F_Bc_Read_And             => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_And_Disable,
            F_Bc_Read_Or              => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Bc_Read_Or_Disable,
            F_Override_Decode_Error   => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Decode_Error_Disable,
            F_Set_Local_Ordering      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Local_Ordering_Disable,
            F_Clear_Local_Ordering    => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Clear_Local_Ordering_Disable,
            F_Ucode_Write_Intercept   => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Intercept_Disable,
            F_Ucode_Read_Intercept    => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Intercept_Disable,
            F_Ucode_Write_Observe     => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Write_Observe_Disable,
            F_Ucode_Read_Observe      => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Ucode_Read_Observe_Disable,
            F_Set_Priv_Level          => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Set_Priv_Level_Enable,
            F_Override_Wrbe_Error     => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action_Override_Wrbe_Error_Disable
          );
         Bar0_Reg_Wr32_Ppriv_Sys_Pri_Decode_Trap14_Action(Reg  =>Decode_Trap14_Action_Reg,
                                                          Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action,
                                                          Status => Status);
       end loop Main_Block;
   end Setting_Decode_Trap14;

   procedure Provide_Access_Fbpa_To_Cpu_Bug_2369597(Status: in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Decode_Trap12_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER;
      Decode_Trap13_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER;
      Decode_Trap14_Action_Reg :LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER;
      Status_Error : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;

      Mutex_Token              : LwU8                                          := LW_MUTEX_OWNER_ID_ILWALID;

      function UC_LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                            (Source => LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER,
                                                             Target => LwU32);
      function UC_LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                            (Source => LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER,
                                                             Target => LwU32);
      function UC_LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER_TO_LwU32 is new Ada.Unchecked_Colwersion
                                                            (Source => LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER,
                                                             Target => LwU32);
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         -- acquire mutex before accessing decode trap
         Se_Acquire_Common_Mutex( MutexId => SEC2_MUTEX_DECODE_TRAPS_WAR_TU10X_BUG_2369597,
                                  Mutex_Token => Mutex_Token,
                                  Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         Decode_Trap:
         for Unused_Loop_Var in 1 .. 1 loop

            Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap12_Action(Reg  =>Decode_Trap12_Action_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap12_Action,
                                                             Status => Status);

            exit Decode_Trap when Status /= UCODE_ERR_CODE_NOERROR;

            Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap13_Action(Reg =>Decode_Trap13_Action_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap13_Action,
                                                             Status => Status);

            exit Decode_Trap when Status /= UCODE_ERR_CODE_NOERROR;

            Bar0_Reg_Rd32_Ppriv_Sys_Pri_Decode_Trap14_Action(Reg =>Decode_Trap14_Action_Reg,
                                                             Addr => Lw_Ppriv_Sys_Pri_Decode_Trap14_Action,
                                                             Status => Status);

            exit Decode_Trap when Status /= UCODE_ERR_CODE_NOERROR;

            if UC_LW_PPRIV_SYS_PRI_DECODE_TRAP12_ACTION_REGISTER_TO_LwU32(Decode_Trap12_Action_Reg) /= 0 or else
               UC_LW_PPRIV_SYS_PRI_DECODE_TRAP13_ACTION_REGISTER_TO_LwU32(Decode_Trap13_Action_Reg) /= 0 or else
               UC_LW_PPRIV_SYS_PRI_DECODE_TRAP14_ACTION_REGISTER_TO_LwU32(Decode_Trap14_Action_Reg) /= 0
            then
               Status := DEV_FBPA_WAR_FAILED;
--               Update_Status_In_Mailbox(Status => DEV_FBPA_WAR_FAILED);
               exit Decode_Trap;
            end if;

            Setting_Decode_Trap12(Status => Status);
            exit Decode_Trap when Status /= UCODE_ERR_CODE_NOERROR;

            Setting_Decode_Trap13(Status => Status);
            exit Decode_Trap when Status /= UCODE_ERR_CODE_NOERROR;

            Setting_Decode_Trap14(Status => Status);
         end loop Decode_Trap;
         -- Relase the mutex acquire before
         if Mutex_Token /= LW_MUTEX_OWNER_ID_ILWALID then
            Se_Release_Common_Mutex( MutexId => SEC2_MUTEX_DECODE_TRAPS_WAR_TU10X_BUG_2369597,
                                     HwTokenFromCaller=> Mutex_Token,
                                     Status => Status_Error);
            if Status_Error /= UCODE_ERR_CODE_NOERROR then
               Status := Status_Error;
            end if;
         end if;
      end loop Main_Block;
   end Provide_Access_Fbpa_To_Cpu_Bug_2369597;
end Fbpa_War_tu10x;
