--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System.Machine_Code; use System.Machine_Code;
with SBI_Types; use SBI_Types;

package body SBI_Interface
is

    --  r6: extension id
    --  r7: function id -- right now only function id 0 is supported

    function SBI_Set_Timer (Next_Event_Time : LwU64) return SBI_Return_Type
    is
        rc_error : SBI_Error_Code;
        rc_value : LwU64;
        use ASCII;
    begin
        Asm ("li a6, %2" & LF &
             "li a7, 0"  & LF &
             "mv a0, %3" & LF &
             "ecall"     & LF &
             "mv %0, a0" & LF &
             "mv %1, a1",

             --  Output clause is numbered first
             Outputs => (SBI_Error_Code'Asm_Output ("=r", rc_error),
                         LwU64'Asm_Output ("=r", rc_value)),

             Inputs => (SBI_Extension_ID'Asm_Input ("i", SBI_EXTENSION_SET_TIMER),
                        LwU64'Asm_Input ("r", Next_Event_Time)),

             Clobber => "s0, s1, t0, t1, t2, t3, t4, t5, t6, ra, a2, a3, a4, a5, a6, a7, memory",

             Volatile => True
        );

        return SBI_Return_Type'(error => rc_error, value => rc_value);

    end SBI_Set_Timer;

    function SBI_Switch_To (Param0 : in LwU64;
                            Param1 : in LwU64;
                            Param2 : in LwU64;
                            Param3 : in LwU64;
                            Param4 : in LwU64;
                            Param5 : in LwU64) return SBI_Return_Type
    is
        rc_error : SBI_Error_Code;
        rc_value : LwU64;
        use ASCII;
    begin
        Asm ("li a6, %2" & LF &
             "li a7, 0"  & LF &
             "mv a0, %3" & LF &
             "mv a1, %4" & LF &
             "mv a2, %5" & LF &
             "mv a3, %6" & LF &
             "mv a4, %7" & LF &
             "mv a5, %8" & LF &
             "ecall"     & LF &
             "mv %0, a0" & LF &
             "mv %1, a1",

             Outputs => (SBI_Error_Code'Asm_Output ("=r", rc_error),
                         LwU64'Asm_Output ("=r", rc_value)),

             Inputs => (SBI_Extension_ID'Asm_Input ("i", SBI_EXTENSION_LWIDIA),
                        LwU64'Asm_Input ("r", Param0),
                        LwU64'Asm_Input ("r", Param1),
                        LwU64'Asm_Input ("r", Param2),
                        LwU64'Asm_Input ("r", Param3),
                        LwU64'Asm_Input ("r", Param4),
                        LwU64'Asm_Input ("r", Param5)),

             Clobber => "s0, s1, t0, t1, t2, t3, t4, t5, t6, ra, a2, a3, a4, a5, a6, a7, memory",

             Volatile => True
        );

        return SBI_Return_Type'(error => rc_error, value => rc_value);

    end SBI_Switch_To;

    function SBI_Shutdown return SBI_Return_Type
    is
        rc_error : SBI_Error_Code := SBI_ERR_FAILURE;
        rc_value : LwU64 := 0;
        use ASCII;
    begin
        Asm ("li a6, %0" & LF &
             "li a7, 0"  & LF &
             "ecall",

             Inputs => SBI_Extension_ID'Asm_Input ("i", SBI_EXTENSION_SHUTDOWN),

             Volatile => True
        );

        return SBI_Return_Type'(error => rc_error, value => rc_value);

    end SBI_Shutdown;

end SBI_Interface;
