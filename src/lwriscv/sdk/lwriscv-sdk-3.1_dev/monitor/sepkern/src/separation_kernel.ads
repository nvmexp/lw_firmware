-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with Types;             use Types;
with Error_Handling;    use Error_Handling;
with Dev_Riscv_Csr_64;  use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;       use Dev_Prgnlcl;
with Partition;         use Partition;
with Riscv_Core;        use Riscv_Core;
with Riscv_Core.Rv_Gpr;
with Riscv_Core.Rv_Csr;
with Riscv_Core.Csb_Reg;

package Separation_Kernel
with
   Abstract_State => Separation_Kernel_State
is

    function Get_Lwrrent_Partition_ID return Partition_ID;

    -- -----------------------------------------------------------------------------
    -- *** Verify HW state
    -- -----------------------------------------------------------------------------
    procedure Verify_HW_State (Err_Code : in out Error_Codes)
    with
        Pre    => Err_Code = OK,
        Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- -----------------------------------------------------------------------------
    -- *** Initialize SK state
    -- -----------------------------------------------------------------------------
    procedure Initialize_SK_State
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- -----------------------------------------------------------------------------
    -- *** Clear Current Partition State
    -- -----------------------------------------------------------------------------
    procedure Clear_Lwrrent_Partition_State
    with
        Global => (In_Out => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State);

    -- -----------------------------------------------------------------------------
    -- *** Check if the current active partition can switch to partition with Id=Id
    -- -----------------------------------------------------------------------------
    function Is_Switchable_To (Id : Partition_ID) return Boolean
    with
        Global => (Input => Separation_Kernel_State);

    -- -----------------------------------------------------------------------------
    -- *** Initialize Partition With ID
    -- -----------------------------------------------------------------------------
    procedure Initialize_Partition_With_ID (Id : Partition_ID)
    with
        Global => (Output => (Separation_Kernel_State,
                              Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                              Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State));

    -- -----------------------------------------------------------------------------
    -- *** Initialize Peregrine State
    -- -----------------------------------------------------------------------------
    procedure Initialize_Peregrine_State
    with
        Global => (Output => (Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State,
                              Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State));

    -- -----------------------------------------------------------------------------
    -- *** Load Initial Arguments
    -- -----------------------------------------------------------------------------
    procedure Load_Initial_Arguments
    with
        Global => (Input  => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                   Output => Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** Load Parameters to Argument Registers
    -- -----------------------------------------------------------------------------
    procedure Load_Parameters_To_Argument_Registers(Param1 : LwU64;
                                                    Param2 : LwU64;
                                                    Param3 : LwU64;
                                                    Param4 : LwU64;
                                                    Param5 : LwU64;
                                                    Param6 : LwU64)
    with
        Global => (Output => Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** Clear SK State
    -- -----------------------------------------------------------------------------
    -- Clear perf counters, ilwalidate branch predictors and
    -- issue a Fence.IO to ensure all transactions are completed
    procedure Clear_SK_State
    with
        Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** Check for Pending Interrupts
    -- -----------------------------------------------------------------------------
    -- Halt the core if any interrupts are pending
    procedure Check_For_Pending_Interrupts (Err_Code : in out Error_Codes)
    with
        Pre    => Err_Code = OK,
        Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** Clear GPRs
    -- -----------------------------------------------------------------------------
    procedure Clear_GPRs
    with
        Pre    => Riscv_Core.Ghost_Lwrrent_Stack = SK_Stack,
        Post   => Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP,
        Global => (Output => (Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
                   In_Out => Riscv_Core.Ghost_Lwrrent_Stack),
        Inline_Always;

    -- -----------------------------------------------------------------------------
    -- *** Transfer to S mode
    -- -----------------------------------------------------------------------------
    procedure Transfer_to_S_Mode
    with
        Pre => (Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP),
        Inline_Always,
        No_Return;

end Separation_Kernel;
