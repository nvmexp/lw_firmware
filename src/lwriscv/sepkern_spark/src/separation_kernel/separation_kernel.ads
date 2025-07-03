-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Error_Handling; use Error_Handling;
with Riscv_Core; use Riscv_Core;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Riscv_Pri; use Dev_Riscv_Pri;

package Separation_Kernel
is

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 1 - Verify HW state
-- -----------------------------------------------------------------------------
-- The only thing that will be verified is that mcause and mepc match the expected values defined by the bootrom.
-- * mcause == ???
-- * mepc == FMC start - 0x1000
-- This makes sure that an exception didn't occur before the actual M-mode trap vector was installed
-- as the value of mtvec set by the bootrom is the start of the FMC.

   procedure Verify_HW_State(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 2 - Initialize SK state
-- -----------------------------------------------------------------------------
-- Program CSRs not set by manifest
-- * mtvec      - Build Time defined - Set to FMC start address by BR. SK will set to its trap handler
-- * mscratch   - Build Time defined - Set to SK data block used during trap handling
-- * mhpmeventN - Build Time Defined - If enabled at build time initial profiling of early SK exelwtion can be enabled here.

   procedure Initialize_SK_State(Err_Code : Error_Codes) with
     Pre => Err_Code = OK,
     Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 3 - Verify Correct Fusing
-- -----------------------------------------------------------------------------
-- If WPRID is set and we are not in debug_mode:
-- 1. Make sure privsec is on and
-- 2. RISCV is not in devmode and
-- 3. br_error is disabled
-- 4. DCLS is on with action halt
   procedure Verify_Fusing(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Global => (Input => Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State),
     Inline_Always;

   package Partition is

      procedure Setup_Supervisor_Trap_Vector with
        Inline_Always;
      procedure Setup_Interrupt_Delegation with
        Inline_Always;
      procedure Allow_S_Mode_To_Flush_D_Cache with
        Inline_Always;
      procedure Enable_Partition_Memory_Operations with
        Inline_Always;
      procedure Set_Partition_Start_Address(Err_Code : in out Error_Codes) with
        Pre => Err_Code = OK,
        Inline_Always;
      procedure Enable_All_Branch_Predictors with
        Inline_Always;
      procedure Enable_ICD_In_S_Mode with
        Inline_Always;

      procedure Define_S_And_U_Mode_Privilege(Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER;
                                              Mspm           : in out LW_RISCV_CSR_MSPM_Register;
                                              Mrsp           : in out LW_RISCV_CSR_MRSP_Register;
                                              Err_Code       : in out Error_Codes)
        with
          Pre => Err_Code = OK,
          Post => (if Mspm.Mplm /= LW_RISCV_CSR_MSPM_MPLM_LEVEL0 and then Bcr_DmaCfg_Sec.F_Wprid = 0 then
                     Err_Code = CRITICAL_ERROR
                       else
                     ((Err_Code = OK and then
                        ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                             (Mrsp.Srpl = SRPL_LEVEL3 and then Mrsp.Urpl = URPL_LEVEL3) and then
                             Mspm.Mplm = LW_RISCV_CSR_MSPM_MPLM_LEVEL3)) or else

                        ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                             (Mrsp.Srpl = SRPL_LEVEL2 and then Mrsp.Urpl = URPL_LEVEL2) and then
                             Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL2 or LW_RISCV_CSR_MSPM_MPLM_LEVEL1)) or else

                          ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                             (Mrsp.Srpl = SRPL_LEVEL2 and then Mrsp.Urpl = URPL_LEVEL2) and then
                                 Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL2)) or else

                        ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                             (Mrsp.Srpl = SRPL_LEVEL1 and then Mrsp.Urpl = URPL_LEVEL1) and then
                             Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL1)) or else

                          ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                             (Mrsp.Srpl = SRPL_LEVEL0 and then Mrsp.Urpl = URPL_LEVEL0)))),

          Global => null,
          Depends => (Mspm =>+ (Bcr_DmaCfg_Sec, Err_Code),
                      Mrsp =>+ (Bcr_DmaCfg_Sec, Mspm, Err_Code),
                      Err_Code =>+ (Bcr_DmaCfg_Sec, Mspm)),
          Inline_Always;


      procedure Set_S_And_U_Mode_Privilege(Mspm : LW_RISCV_CSR_MSPM_Register;
                                           Mrsp : LW_RISCV_CSR_MRSP_Register)
        with
          Pre => (((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                    (Mrsp.Srpl = SRPL_LEVEL3 and then Mrsp.Urpl = URPL_LEVEL3) and then
                    Mspm.Mplm = LW_RISCV_CSR_MSPM_MPLM_LEVEL3) or else

                      ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                         (Mrsp.Srpl = SRPL_LEVEL2 and then Mrsp.Urpl = URPL_LEVEL2) and then
                             Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL2 or LW_RISCV_CSR_MSPM_MPLM_LEVEL1)) or else

                    ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                         (Mrsp.Srpl = SRPL_LEVEL2 and then Mrsp.Urpl = URPL_LEVEL2) and then
                         Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL2)) or else

                      ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                         (Mrsp.Srpl = SRPL_LEVEL1 and then Mrsp.Urpl = URPL_LEVEL1) and then
                             Mspm.Mplm = (LW_RISCV_CSR_MSPM_MPLM_LEVEL1)) or else

                    ((Mspm.Splm = Mspm.Mplm and then Mspm.Uplm = Mspm.Mplm) and then
                         (Mrsp.Srpl = SRPL_LEVEL0 and then Mrsp.Urpl = URPL_LEVEL0))),

          Global => (Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
          Inline_Always;

      procedure Allow_L1_Caching(Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER) with
        Inline_Always;
      procedure Allow_Time_CSR_Access with
        Inline_Always;
      procedure Setup_MStatus with
        Inline_Always;
      procedure Enable_Timer_Interrupt with
        Inline_Always;
   end Partition;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 4- Initialize Partition CSR
-- -----------------------------------------------------------------------------
-- * stvec      -       Temporary Trap Vector address           - Will use a stub trap handler until the partition can load its own.
-- * medeleg    -       All delegated except ECALL from S-mode  -
-- * mmiscopen  -       DCACHEOP = 1                            - S-mode allowed to flush dCache
-- * msysopen   -       All fields = 1                          - Enable all memory operations for partition
-- * mepc       -       Start address of partition              - Will be used by eret instruction when leaving SK
-- * mbpcfg     -       Enable All Branch Predictors            - Allowing branch prediction, but will clear state before transferring to partition
-- * mdbgctl    -       Build Defined Value                     - Enable ICD in S-mode.
-- * sspm       -       mspm.MPLM                               - Manifest sets M-mode level, FMC will copy that into S-mode.
-- * srsp       -       SRPL = sspm.SPLM                        - Default it to the max allowed privilege level for S-mode so early bootloader can run out of external memory
-- * mfetchattr -       Cacheable=1; WPR ID = <boot arg>        - Allowing L1 caching to accelerate the early bootloader that runs out of external memory prior to programming MPU
-- * mldstattr  -       Cacheable=1; WPR ID = <boot arg>        - Allowing L1 caching to accelerate the early bootloader that runs out of external memory prior to programming MPU
-- * mcounteren -       TM = 1                                  - Allow access to time CSR (PTIMER sourced). Do not allow access by default to cycle/instret

   procedure Initialize_Partition_CSR(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Global => (Output => (Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                           Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State)),
     Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 5 - Initialize Peregrine State
-- -----------------------------------------------------------------------------
-- Beyond the LWRISCV core itself the SK is responsible for setting up state inside Peregrine.
-- Everything except interrupt delegation will be done by the manifest. The SK itself does not touch any peripherals so they can be simply passed through.
-- REGIONCFG(0) will hold WPR_ID from BCR
-- TODO - SHA_CTRL_FB_DIS

   procedure Initialize_Peregrine_State(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Post => Err_Code = OK,
     Global => (In_Out => Riscv_Core.Bar0_Reg.Bar0_Reg_Hw_Model.Mmio_State,
                Output => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 6 - Load Initial Arguments
-- -----------------------------------------------------------------------------
-- There are certain constant values only accessible or known at first boot to M-mode that may also be needed by the partition when it boots.
-- In the standard SBI many of these are exposed as runtime SBI calls that can be made.
-- This would lead to unnecessary code size and attack surface,
-- so instead LWRISCV SK will pass them as part of the initial CPU state when starting the partition.

--   Value                      - Location
-- SBI Version                  - a0
-- misa                         - a1
-- marchid                      - a2    Riscv_Core
-- Reserved for mimpid          - a3 - Hardwired to 0 for GA10x
-- Reserved for mhartid         - a4 - Hardwired to 0 for GA10x
-- mfetchattr                   - a5
-- mldstattr                    - a6
-- Reserved, guaranteed to be 0 - a7
   procedure Load_Initial_Arguments(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Post => Err_Code = OK,
     Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                Output => Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
     Inline_Always;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 7 - Do Fence IO and Check for Pending Interrupts
-- -----------------------------------------------------------------------------
-- Issue a Fence.IO and check for any pending interrupts to ensure all transactions are completed.

   procedure Do_Fence_IO_And_Check_For_Pending_Interrupts(Err_Code : in out Error_Codes) with
     Pre => Err_Code = OK,
     Global => (Input => Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State),
     Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 8 - Clear GPRs
-- -----------------------------------------------------------------------------
-- Clear all GPRs to 0 to avoid leaking any information about SK exelwtion into partition.

   procedure Clear_GPRs(Err_Code : in out Error_Codes) with
     Pre => (Err_Code = OK and then Riscv_Core.Ghost_Lwrrent_Stack = SK_Stack),
     Post => (Err_Code = OK and then Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP),
     Global => (Output => (Riscv_Core.Rv_Gpr.Gpr_Reg_Hw_Model.Gpr_State),
                In_Out => Riscv_Core.Ghost_Lwrrent_Stack),
     Inline_Always;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 9 - Transfer to S mode
-- -----------------------------------------------------------------------------
   -- Transfer to S mode - 'mret'
   procedure Transfer_to_S_Mode with
     Pre => (Riscv_Core.Ghost_Lwrrent_Stack = Zero_SP),
     Inline_Always,
     No_Return;

end Separation_Kernel;
