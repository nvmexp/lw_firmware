-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;
with Separation_Kernel;
with Error_Handling;    use Error_Handling;
with Trace_Buffer;      use Trace_Buffer;
with Hw_State_Check;

package body SBI_Switch_To
is

    procedure Switch_To(Param1    : LwU64;
                        Param2    : LwU64;
                        Param3    : LwU64;
                        Param4    : LwU64;
                        Param5    : LwU64;
                        Param6    : LwU64)
    is
        Source_Partition_ID : Partition_ID;
        Target_Id : Partition_ID;
        Err_Code : Error_Codes := OK;
    begin
        -- Check if target id is valid
        if Param6 > LwU64(Partition_ID'Last) then
            Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                 Err_Code => SBI_SWITCH_TO_ILWALID_PARTITION_ID);
        end if;

        Target_Id := Partition_ID(Param6);

        -- Check if switch is allowed
        if Separation_Kernel.Is_Switchable_To(Id => Target_Id) = False then
            Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                 Err_Code => SBI_SWITCH_TO_DENIED);
        end if;

        -- Check if hw engines are idle and clean
        Hw_State_Check.Check_Hw_Engine_State(Err_Code);
        if Err_Code /= OK then
            Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                 Err_Code => Err_Code);
        end if;

        Source_Partition_ID := Separation_Kernel.Get_Lwrrent_Partition_ID;

        Separation_Kernel.Clear_Lwrrent_Partition_State;

        Separation_Kernel.Initialize_Partition_With_ID(Id => Target_Id);

        Restore_Trace_Buffer;

        Separation_Kernel.Clear_SK_State;

        Separation_Kernel.Load_Parameters_To_Argument_Registers(Param1 => Param1,
                                                                Param2 => Param2,
                                                                Param3 => Param3,
                                                                Param4 => Param4,
                                                                Param5 => Param5,
                                                                Param6 => LwU64(Source_Partition_ID));
        Separation_Kernel.Clear_GPRs;

        Separation_Kernel.Transfer_to_S_Mode;

    end Switch_To;

end SBI_Switch_To;
