-- _LWRM_COPYRIGHT_BEGIN_
--
-- Copyright 2021 by LWPU Corporation.  All rights reserved.  All
-- information contained herein is proprietary and confidential to LWPU
-- Corporation.  Any use, reproduction, or disclosure without the written
-- permission of LWPU Corporation is prohibited.
--
-- _LWRM_COPYRIGHT_END_

with Interfaces.C; use Interfaces.C;
with libos_manifest_h; use libos_manifest_h;
with lwtypes_h; use lwtypes_h;

package Tasks
    with
        SPARK_Mode => ON
is
    function Init_Task (Worker_Task_Id : LibosPortId) return int
        with
            Export        => True,
            Convention    => C,
            External_Name => "adaInitTask";

    function Worker_Task return int
        with
            Export        => True,
            Convention    => C,
            External_Name => "adaWorkerTask";

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
    );

    function Test_Gdma return Boolean;

end Tasks;
