-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;
with Lw_Types;           use Lw_Types;

package Error_Handling
is

    type Phase_Codes is (
        START_PHASE,
        TRAP_HANDLER_PHASE
    )
    with Size => 8;

    for Phase_Codes use (
        START_PHASE        => 16#1#,
        TRAP_HANDLER_PHASE => 16#2#
    );

    type Error_Codes is (
        OK,
        ILWALID_MEPC,
        ILWALID_MTVEC,
        UNEXPECTED_INTERRUPT,
        SBI_LWIDIA_ILWALID_FUNCTION_ID,
        SBI_SWITCH_TO_ILWALID_PARTITION_ID,
        SBI_SWITCH_TO_DENIED,
        UNEXPECTED_INTERRUPT_CAUSE,
        UNEXPECTED_EXCEPTION_CAUSE,
        HW_CHECK_DMA_NOT_IDLE,
        HW_CHECK_DMA_ERROR,
        HW_CHECK_IOPMP_ERROR,
        HW_CHECK_SHA_NOT_IDLE,
        HW_CHECK_SHA_ERROR,
        HW_CHECK_GDMA_NOT_IDLE,
        HW_CHECK_GDMA_ERROR,
        HW_CHECK_SCP_NOT_IDLE,
        HW_CHECK_SCP_ERROR,
        CRITICAL_ERROR
    )
    with Size => 8;

    for Error_Codes use (
        OK                                 => 16#0#,
        ILWALID_MEPC                       => 16#1#,
        ILWALID_MTVEC                      => 16#2#,
        UNEXPECTED_INTERRUPT               => 16#3#,
        SBI_LWIDIA_ILWALID_FUNCTION_ID     => 16#4#,
        SBI_SWITCH_TO_ILWALID_PARTITION_ID => 16#5#,
        SBI_SWITCH_TO_DENIED               => 16#6#,
        UNEXPECTED_INTERRUPT_CAUSE         => 16#7#,
        UNEXPECTED_EXCEPTION_CAUSE         => 16#8#,
        HW_CHECK_DMA_NOT_IDLE              => 16#9#,
        HW_CHECK_DMA_ERROR                 => 16#A#,
        HW_CHECK_IOPMP_ERROR               => 16#B#,
        HW_CHECK_SHA_NOT_IDLE              => 16#C#,
        HW_CHECK_SHA_ERROR                 => 16#D#,
        HW_CHECK_GDMA_NOT_IDLE             => 16#E#,
        HW_CHECK_GDMA_ERROR                => 16#F#,
        HW_CHECK_SCP_NOT_IDLE              => 16#10#,
        HW_CHECK_SCP_ERROR                 => 16#11#,
        CRITICAL_ERROR                     => 16#FF#
    );

    type SK_ERROR_Register is record
        Phase    : Phase_Codes;
        Err_Code : Error_Codes;
        Reserved : LwU16;
    end record with
        Size        => 32,
        Object_Size => 32;

    for SK_ERROR_Register use record
        Phase    at 0 range  0 ..  7;
        Err_Code at 0 range  8 .. 15;
        Reserved at 0 range 16 .. 31;
    end record;
    ---------------------------------------------------------------------------

    procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes)
    with
        Pre => Err_Code /= OK,
        Global => (Output => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                              Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State)),
        Inline_Always,
        No_Return;
    ---------------------------------------------------------------------------

    -- Overriding Last Chance Handler (LCH)
    procedure Last_Chance_Handler (Source_Location : System.Address; Line : Integer)
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        No_Return;
    pragma Export (C, Last_Chance_Handler, "__gnat_last_chance_handler");
    ---------------------------------------------------------------------------

end Error_Handling;
