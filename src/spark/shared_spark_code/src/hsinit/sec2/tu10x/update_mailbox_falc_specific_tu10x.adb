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

package body Update_Mailbox_Falc_Specific_Tu10x is

   procedure Update_Status_In_Mailbox( Status : LW_UCODE_UNIFIED_ERROR_TYPE )
   is
      Status_Of_Write : LW_UCODE_UNIFIED_ERROR_TYPE := UCODE_ERR_CODE_NOERROR;
      csec_mailbox0 : LW_CSEC_FALCON_MAILBOX0_REGISTER;
      function To_Mailbox is new Ada.Unchecked_Colwersion(Source => LW_UCODE_UNIFIED_ERROR_TYPE,
                                                          Target => LW_CSEC_FALCON_MAILBOX0_DATA_FIELD);
   begin
      csec_mailbox0.F_Data := To_Mailbox(Status);

      -- This Status is not tracked but we need to satisfy Spark
      pragma Warnings(Off, """Status_Of_Write"" modified by call, but value might not be referenced");
      Csb_Reg_Wr32_Mailbox(Reg  => csec_mailbox0,
                           Addr => Lw_Csec_Falcon_Mailbox0,
                           Status => Status_Of_Write);
      pragma Warnings(On, """Status_Of_Write"" modified by call, but value might not be referenced");

      -- Update HS Exit State
      Ghost_Hs_Exit_State_Tracker := MAILBOX_UPDATED;

   end Update_Status_In_Mailbox;


end Update_Mailbox_Falc_Specific_Tu10x;
