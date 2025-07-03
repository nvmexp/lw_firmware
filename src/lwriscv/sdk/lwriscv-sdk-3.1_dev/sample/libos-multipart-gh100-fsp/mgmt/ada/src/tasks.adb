-- _LWRM_COPYRIGHT_BEGIN_
--
-- Copyright 2021 by LWPU Corporation.  All rights reserved.  All
-- information contained herein is proprietary and confidential to LWPU
-- Corporation.  Any use, reproduction, or disclosure without the written
-- permission of LWPU Corporation is prohibited.
--
-- _LWRM_COPYRIGHT_END_

with Ada.Unchecked_Colwersion;
with System; use System;
with libos_h; use libos_h;
with libos_port_h; use libos_port_h;
with libos_log_h; use libos_log_h;
with libos_status_h; use libos_status_h;
with lwtypes_h; use lwtypes_h;

package body Tasks
    with
        SPARK_Mode => ON
is
    function Init_Task (Worker_Task_Id : LibosPortId) return int is
        I : int;
        Status : LibosStatus;
    begin
        I := LibosPrintf ("Hello from Ada/SPARK init task. ");
        Status := LibosPortSyncSend (Worker_Task_Id, System.Null_Address, 0, null, 0);
        I := LibosPrintf ("Goodbye from Ada/SPARK init task. ");
        return 0;
    end Init_Task;

    function Worker_Task return int is
        I : int;
        Out_Arg_1 : LwU64;
        Out_Arg_2 : LwU64;
        Out_Arg_3 : LwU64;
        Out_Arg_4 : LwU64;
        Out_Arg_5 : LwU64;
    begin
        I := LibosPrintf ("Hello from Ada/SPARK worker task. ");
        Partition_Switch_Wrapper (
            0,
            16#00000000deadbeef#, 0, 0, 0, 0,
            Out_Arg_1, Out_Arg_2, Out_Arg_3, Out_Arg_4, Out_Arg_5
        );
        I := LibosPrintf ("Hello again from Ada/SPARK worker task. ");
        if Test_Gdma then
            I := LibosPrintf ("GDMA test passed. ");
        else
            I := LibosPrintf ("GDMA test failed. ");
        end if;
        return 0;
    end Worker_Task;

    procedure Partition_Switch_Wrapper (
        Partition_ID : LwU64;

        In_Arg_1 : LwU64;
        In_Arg_2 : LwU64;
        In_Arg_3 : LwU64;
        In_Arg_4 : LwU64;
        In_Arg_5 : LwU64;

        Out_Arg_1 : out LwU64;
        Out_Arg_2 : out LwU64;
        Out_Arg_3 : out LwU64;
        Out_Arg_4 : out LwU64;
        Out_Arg_5 : out LwU64
    )
        with
            SPARK_Mode => OFF
    is
        type Partition_Switch_Args is array (0 .. 4) of aliased LwU64;
        In_Args : Partition_Switch_Args := (
            In_Arg_1, In_Arg_2, In_Arg_3, In_Arg_4, In_Arg_5);
        Out_Args : Partition_Switch_Args := (0, 0, 0, 0, 0);
    begin
        LibosPartitionSwitch (Partition_ID, In_Args (0)'Access, Out_Args (0)'Access);
        Out_Arg_1 := Out_Args (0);
        Out_Arg_2 := Out_Args (1);
        Out_Arg_3 := Out_Args (2);
        Out_Arg_4 := Out_Args (3);
        Out_Arg_5 := Out_Args (4);
    end Partition_Switch_Wrapper;

    function Test_Gdma return Boolean
        with
            SPARK_Mode => OFF
    is
        I : int;
        Status: LibosStatus;
        Gdma_Dst : aliased LwU32 := 0;
        Gdma_Src : aliased LwU32 := 16#deadbeef#;
        function Colwert is new Ada.Unchecked_Colwersion (System.Address, LwU64);
    begin
        Status := LibosGdmaTransfer (Colwert (Gdma_Dst'Address), Colwert (Gdma_Src'Address), 4, 0);
        if Status /= LibosOk then
            I := LibosPrintf ("LibosGdmaTransfer failed. ");
            return False;
        end if;
        Status := LibosGdmaFlush;
        if Status /= LibosOk then
            I := LibosPrintf ("LibosGdmaFlush failed. ");
            return False;
        end if;
        return Gdma_Dst = Gdma_Src;
    end Test_Gdma;

end Tasks;
