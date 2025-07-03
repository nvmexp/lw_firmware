--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with SBI_Types; use SBI_Types;
with Lw_Types;  use Lw_Types;

package SBI_Interface is

    --------------------- Types -----------------------------------------------

    type SBI_Return_Type is record
        error : SBI_Error_Code;
        value : LwU64;
    end record;

    --------------------- Funcs -----------------------------------------------

    function SBI_Set_Timer (Next_Event_Time : in LwU64) return SBI_Return_Type
    with
       Inline_Always;

    function SBI_Switch_To (Param0 : in LwU64;
                            Param1 : in LwU64;
                            Param2 : in LwU64;
                            Param3 : in LwU64;
                            Param4 : in LwU64;
                            Param5 : in LwU64) return SBI_Return_Type
    with
       Inline_Always,
       Annotate => (GNATprove, Might_Not_Return);

    function SBI_Shutdown return SBI_Return_Type
    with
       Inline_Always,
       Annotate => (GNATprove, Might_Not_Return);

end SBI_Interface;
