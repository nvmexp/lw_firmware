-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--******************************  DEV_FUSE  ******************************--

package Dev_Fuse
with SPARK_MODE => On
is


---------- Register Range Subtype Declaration ----------
    subtype LW_FUSE_BAR0_ADDR is Bar0_Addr range 16#0082_0000# .. 16#0082_5FFF#;
------------------------------------------------------------------------------------------------------

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Priv_Sec_En    : constant LW_FUSE_BAR0_ADDR := 16#0082_0434#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_PRIV_SEC_EN_DATA_FIELD is
    (
        Lw_Fuse_Opt_Priv_Sec_En_Data_Init,
        Lw_Fuse_Opt_Priv_Sec_En_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_PRIV_SEC_EN_DATA_FIELD use
    (
        Lw_Fuse_Opt_Priv_Sec_En_Data_Init => 0,
        Lw_Fuse_Opt_Priv_Sec_En_Data_Yes => 1
    );

    Lw_Fuse_Opt_Priv_Sec_En_Data_No :
        constant LW_FUSE_OPT_PRIV_SEC_EN_DATA_FIELD
        := Lw_Fuse_Opt_Priv_Sec_En_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_PRIV_SEC_EN_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_PRIV_SEC_EN_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_PRIV_SEC_EN_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt    : constant LW_FUSE_BAR0_ADDR := 16#0082_1788#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_DATA_FIELD is
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Init,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Enable
    ) with size => 1;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_DATA_FIELD use
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Init => 0,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Enable => 1
    );

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Disable :
        constant LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_DATA_FIELD
        := Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert    : constant LW_FUSE_BAR0_ADDR := 16#0082_178C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_DATA_FIELD is
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Init,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Enable
    ) with size => 1;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_DATA_FIELD use
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Init => 0,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Enable => 1
    );

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Disable :
        constant LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_DATA_FIELD
        := Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Assert_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt    : constant LW_FUSE_BAR0_ADDR := 16#0082_1790#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_DATA_FIELD is
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Init,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Enable
    ) with size => 1;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_DATA_FIELD use
    (
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Init => 0,
        Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Enable => 1
    );

    Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Disable :
        constant LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_DATA_FIELD
        := Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Interrupt_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis    : constant LW_FUSE_BAR0_ADDR := 16#0082_0640#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_DATA_FIELD is
    (
        Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Init,
        Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_DATA_FIELD use
    (
        Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Init => 0,
        Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Yes => 1
    );

    Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_No :
        constant LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_DATA_FIELD
        := Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis    : constant LW_FUSE_BAR0_ADDR := 16#0082_1784#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_PMU_RISCV_DEV_DIS_DATA_FIELD is
    (
        Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Init,
        Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_PMU_RISCV_DEV_DIS_DATA_FIELD use
    (
        Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Init => 0,
        Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Yes => 1
    );

    Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_No :
        constant LW_FUSE_OPT_PMU_RISCV_DEV_DIS_DATA_FIELD
        := Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_PMU_RISCV_DEV_DIS_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_PMU_RISCV_DEV_DIS_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_PMU_RISCV_DEV_DIS_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;
------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En    : constant LW_FUSE_BAR0_ADDR := 16#0082_4398#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_DATA_FIELD is
    (
        Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_Init,
        Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_DATA_FIELD use
    (
        Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_Init => 0,
        Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_Yes => 1
    );

    Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_No :
        constant LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_DATA_FIELD
        := Lw_Fuse_Opt_Fpf_Pmu_Riscv_Br_Error_Info_En_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_FPF_PMU_RISCV_BR_ERROR_INFO_EN_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis    : constant LW_FUSE_BAR0_ADDR := 16#0082_074C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_DATA_FIELD is
    (
        Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_Init,
        Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_DATA_FIELD use
    (
        Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_Init => 0,
        Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_Yes => 1
    );

    Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_No :
        constant LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_DATA_FIELD
        := Lw_Fuse_Opt_Selwre_Gsp_Debug_Dis_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_SELWRE_GSP_DEBUG_DIS_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis    : constant LW_FUSE_BAR0_ADDR := 16#0082_177C#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_GSP_RISCV_DEV_DIS_DATA_FIELD is
    (
        Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_Init,
        Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_GSP_RISCV_DEV_DIS_DATA_FIELD use
    (
        Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_Init => 0,
        Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_Yes => 1
    );

    Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_No :
        constant LW_FUSE_OPT_GSP_RISCV_DEV_DIS_DATA_FIELD
        := Lw_Fuse_Opt_Gsp_Riscv_Dev_Dis_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_GSP_RISCV_DEV_DIS_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_GSP_RISCV_DEV_DIS_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_GSP_RISCV_DEV_DIS_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
---------- Register Declaration ----------

    Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En    : constant LW_FUSE_BAR0_ADDR := 16#0082_4390#;

------------------------------------------------------------------------------------------------------
---------- Enum Declaration ----------

    type LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_DATA_FIELD is
    (
        Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_Init,
        Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_Yes
    ) with size => 1;

    for LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_DATA_FIELD use
    (
        Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_Init => 0,
        Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_Yes => 1
    );

    Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_No :
        constant LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_DATA_FIELD
        := Lw_Fuse_Opt_Fpf_Gsp_Riscv_Br_Error_Info_En_Data_Init;

---------- Record Declaration ----------

    type LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_REGISTER is
    record
        F_Data    : LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_DATA_FIELD;
    end record
        with  Size => 32, Object_size => 32, Alignment => 4;

    for LW_FUSE_OPT_FPF_GSP_RISCV_BR_ERROR_INFO_EN_REGISTER use
    record
        F_Data at 0 range 0 .. 0;
    end record;

------------------------------------------------------------------------------------------------------
end Dev_Fuse;
