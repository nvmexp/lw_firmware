--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with Lw_Types;                  use Lw_Types;

package SBI_Types
is

    type SBI_Error_Code is (
        SBI_ERR_ILWALID_ADDRESS,
        SBI_ERR_DENIED,
        SBI_ERR_ILWALID_PARAM,
        SBI_ERR_NOT_SUPPORTED,
        SBI_ERR_FAILURE,
        SBI_SUCCESS
    ) with
        Size => 64,
        Object_Size => 64;

    for SBI_Error_Code use (
        SBI_ERR_ILWALID_ADDRESS => -5,
        SBI_ERR_DENIED          => -4,
        SBI_ERR_ILWALID_PARAM   => -3,
        SBI_ERR_NOT_SUPPORTED   => -2,
        SBI_ERR_FAILURE         => -1,
        SBI_SUCCESS             =>  0
    );

    type SBI_Return_Type is record
        Error : SBI_Error_Code;
        Value : LwU64;
    end record;

    function SBI_Error_Code_To_LwU64 is new Ada.Unchecked_Colwersion
        (Source => SBI_Error_Code,
         Target => LwU64);

    type SBI_Extension_ID is new LwU64;

    SBI_EXTENSION_SET_TIMER : constant SBI_Extension_ID := 16#0#;
    SBI_EXTENSION_SHUTDOWN  : constant SBI_Extension_ID := 16#8#;
    SBI_EXTENSION_LWIDIA    : constant SBI_Extension_ID := 16#90001EB#;

    type SBI_Lwidia_Extension_Function_ID is new LwU64;

    SBI_LWIDIA_EXTENSION_FUNC_SWITCH_PARTITION      : constant SBI_Lwidia_Extension_Function_ID := 0;
    SBI_LWIDIA_EXTENSION_FUNC_RELEASE_PRIV_LOCKDOWN : constant SBI_Lwidia_Extension_Function_ID := 1;
    SBI_LWIDIA_EXTENSION_FUNC_UPDATE_TRACECTL       : constant SBI_Lwidia_Extension_Function_ID := 2;
    SBI_LWIDIA_EXTENSION_FUNC_FBIF_TRANSCFG         : constant SBI_Lwidia_Extension_Function_ID := 3;
    SBI_LWIDIA_EXTENSION_FUNC_FBIF_REGIONCFG        : constant SBI_Lwidia_Extension_Function_ID := 4;
    SBI_LWIDIA_EXTENSION_FUNC_TFBIF_TRANSCFG        : constant SBI_Lwidia_Extension_Function_ID := 5;
    SBI_LWIDIA_EXTENSION_FUNC_TFBIF_REGIONCFG       : constant SBI_Lwidia_Extension_Function_ID := 6;

end SBI_Types;
