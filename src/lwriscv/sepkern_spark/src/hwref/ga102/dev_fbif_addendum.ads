-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_FBIF_ADDENDUM  ******************************--

package Dev_Fbif_Addendum
with SPARK_MODE => On
is

---------- Register Declaration ----------

    Lw_Pfalcon_Fbif_Ctl2    : constant Bar0_Addr := 16#0000_0084#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_PFALCON_FBIF_CTL2_NACK_MODE_FIELD is
    (
        Lw_Pfalcon_Fbif_Ctl2_Nack_Mode_Init,
        Lw_Pfalcon_Fbif_Ctl2_Nack_Mode_Nack_As_Ack
    ) with size => 1;

    for LW_PFALCON_FBIF_CTL2_NACK_MODE_FIELD use
    (
        Lw_Pfalcon_Fbif_Ctl2_Nack_Mode_Init => 0,
        Lw_Pfalcon_Fbif_Ctl2_Nack_Mode_Nack_As_Ack => 1
    );

---------- Record Declaration ----------

    type LW_PFALCON_FBIF_CTL2_REGISTER is
    record
        F_Nack_Mode    : LW_PFALCON_FBIF_CTL2_NACK_MODE_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_PFALCON_FBIF_CTL2_REGISTER use
    record
        F_Nack_Mode at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Fbif_Addendum;
