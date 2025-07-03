-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;
with Types; use Types;
with Error_Handling; use Error_Handling;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Riscv_Pri; use Dev_Riscv_Pri;
with Dev_Fbif_Addendum; use Dev_Fbif_Addendum;
with Dev_Fbif_V4; use Dev_Fbif_V4;
with Dev_Falcon_V4; use Dev_Falcon_V4;
with Dev_Pwr_Pri; use Dev_Pwr_Pri;
with Lw_Riscv_Address_Map; use Lw_Riscv_Address_Map;

with Riscv_Core; use Riscv_Core;
with Trap; use Trap;
with SBI; use SBI;
with Engine_Config;
with Debug_Control;
with Fusing;

package body Separation_Kernel
is
   function Get_Imem_Start_Address return LwU64 with Inline_Always;

   function Get_Imem_Start_Address return LwU64 with SPARK_Mode => Off
   is
      Imem_Start_Address : LwU64 with
        Import        => True,
        Convention    => C,
        External_Name => "_IMEM_START_ADDRESS";

      Imem_Start_Address_Val : aliased constant System.Address := Imem_Start_Address'Address;

      function System_Addr_To_LwU64 is new Ada.Unchecked_Colwersion(Source => System.Address,
                                                                    Target => LwU64);
   begin
      return System_Addr_To_LwU64(Imem_Start_Address_Val);
   end Get_Imem_Start_Address;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 1 - Verify HW state
-- -----------------------------------------------------------------------------
-- The only thing that will be verified is that Mcause match the expected value.
-- * mtvec == Start_Address
-- * Mcause == 1
-- * mepc < FMC start address (BROM) - make sure exception oclwred in BROM, before exception handler install
-- This makes sure that an exception didn't occur before the actual M-mode trap vector was installed
-- as the value of mtvec set by the bootrom is the start of the FMC.
   procedure Verify_HW_State(Err_Code : in out Error_Codes) is
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MTVEC_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);

      Mepc : LW_RISCV_CSR_MEPC_Register;
      Mtvec : LW_RISCV_CSR_MTVEC_Register;
      Mcause : LW_RISCV_CSR_MCAUSE_Register;
   begin
      Operation_Done:
      for Unused_Loop_Var in 1 .. 1 loop
         exit Operation_Done when Err_Code /= OK;

         Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEPC_Address,
                      Data => Mepc);
         Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MTVEC_Address,
                      Data => Mtvec);
         Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                      Data => Mcause);

         if Mepc.Epc >= Get_Imem_Start_Address then
            Err_Code := CRITICAL_ERROR;
            exit Operation_Done;
         end if;

         if LwU64 (Lw_Shift_Left (Value  => LwU64 (Mtvec.Base), Amount => 2)) /= Get_Imem_Start_Address then
            Err_Code := CRITICAL_ERROR;
            exit Operation_Done;
         end if;

         if Mcause.Excode /= LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT then
            Err_Code := CRITICAL_ERROR;
            exit Operation_Done;
         end if;

      end loop Operation_Done;
   end Verify_HW_State;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 2 - Initialize SK state
-- -----------------------------------------------------------------------------
-- Program CSRs not set by manifest
-- * mtvec      - Build Time defined - Set to FMC start address by BR. SK will set to its trap handler
-- * mscratch   - Build Time defined - Set to SK data block used during trap handling -- SET in startup to hold SK SP
-- * mhpmeventN - Build Time Defined - If enabled at build time initial profiling of early SK exelwtion can be enabled here.

   procedure Initialize_SK_State(Err_Code : Error_Codes) is
   begin

      Riscv_Core.Setup_Trap_Handler(Get_Address_Of_Trap_Entry);

   end Initialize_SK_State;

-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 3 - Verify Correct Fusing
-- -----------------------------------------------------------------------------
-- If WPRID is set and we are not in debug_mode:
-- 1. Make sure privsec is on and
-- 2. RISCV is not in devmode and
-- 3. br_error is disabled
-- 4. DCLS is on with action halt
   procedure Verify_Fusing(Err_Code : in out Error_Codes) is
      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER);

      Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER;
   begin
      Riscv_Reg_Rd32 (Offset => LwU32(Lw_Priscv_Riscv_Bcr_Dmacfg_Sec),
                      Data => Bcr_DmaCfg_Sec);

      if Bcr_DmaCfg_Sec.F_Wprid /= Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Wprid_Init then
         Fusing.Check_Fusing(Err_Code => Err_Code);
      end if;

   end Verify_Fusing;


   package body Partition is

      -- Setup a stub trap handler until the partition can load its own.
      -- If interrupts are delegated and the partition is in external memory then
      -- not having a trap handler programmed means we could generate errors that cannot be handled.
      -- stvec => Temporary Trap Vector Address
      procedure Setup_Supervisor_Trap_Vector is
      begin

         Riscv_Core.Setup_S_Mode_Trap_Handler(Get_Address_Of_Temp_S_Mode_Trap_Handler);

      end Setup_Supervisor_Trap_Vector;


      -- medeleg => All delegated except ECALL from S-Mode
      -- RISCV_IRQDELEG => All interrupts delegated to S-mode
      procedure Setup_Interrupt_Delegation is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEDELEG_Register);
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MIDELEG_Register);
         procedure Riscv_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Wr32_Generic (Generic_Register => LW_PRISCV_RISCV_IRQDELEG_REGISTER);
      begin

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEDELEG_Address,
                      Data => LW_RISCV_CSR_MEDELEG_Register'(Wpri3               => 0,
                                                             Wpri2               => 0,
                                                             Wpri1               => 0,
                                                             Wpri0               => 0,
                                                             Uss                 => USS_DELEG,
                                                             Store_Page_Fault    => FAULT_DELEG,
                                                             Load_Page_Fault     => FAULT_DELEG,
                                                             Fetch_Page_Fault    => FAULT_DELEG,
                                                             User_Ecall          => ECALL_DELEG,
                                                             Fault_Store         => STORE_DELEG,
                                                             Misaligned_Store    => STORE_DELEG,
                                                             Fault_Load          => LOAD_DELEG,
                                                             Misaligned_Load     => LOAD_DELEG,
                                                             Breakpoint          => BREAKPOINT_DELEG,
                                                             Illegal_Instruction => INSTRUCTION_DELEG,
                                                             Fault_Fetch         => FETCH_DELEG,
                                                             Misaligned_Fetch    => FETCH_DELEG));

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MIDELEG_Address,
                      Data => LW_RISCV_CSR_MIDELEG_Register'(Wpri2 => 0,
                                                             Seid  => SEID_DELEG,
                                                             Ueid  => UEID_NODELEG,
                                                             Wpri1 => 0,
                                                             Stid  => STID_DELEG,
                                                             Utid  => UTID_NODELEG,
                                                             Wpri0 => 0,
                                                             Ssid  => SSID_DELEG,
                                                             Usid  => USID_NODELEG));

         Riscv_Reg_Wr32(Offset => LwU32(Lw_Priscv_Riscv_Irqdeleg),
                      Data => LW_PRISCV_RISCV_IRQDELEG_REGISTER'(F_Gptmr         => Lw_Priscv_Riscv_Irqdeleg_Gptmr_Riscv_Seip,
                                                                 F_Wdtmr         => Lw_Priscv_Riscv_Irqdeleg_Wdtmr_Riscv_Seip,
                                                                 F_Mthd          => Lw_Priscv_Riscv_Irqdeleg_Mthd_Riscv_Seip,
                                                                 F_Ctxsw         => Lw_Priscv_Riscv_Irqdeleg_Ctxsw_Riscv_Seip,
                                                                 F_Halt          => Lw_Priscv_Riscv_Irqdeleg_Halt_Riscv_Seip,
                                                                 F_Exterr        => Lw_Priscv_Riscv_Irqdeleg_Exterr_Riscv_Seip,
                                                                 F_Swgen0        => Lw_Priscv_Riscv_Irqdeleg_Swgen0_Riscv_Seip,
                                                                 F_Swgen1        => Lw_Priscv_Riscv_Irqdeleg_Swgen1_Riscv_Seip,
                                                                 F_Ext_Extirq1   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq1_Riscv_Seip,
                                                                 F_Ext_Extirq2   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq2_Riscv_Seip,
                                                                 F_Ext_Extirq3   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq3_Riscv_Seip,
                                                                 F_Ext_Extirq4   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq4_Riscv_Seip,
                                                                 F_Ext_Extirq5   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq5_Riscv_Seip,
                                                                 F_Ext_Extirq6   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq6_Riscv_Seip,
                                                                 F_Ext_Extirq7   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq7_Riscv_Seip,
                                                                 F_Ext_Extirq8   => Lw_Priscv_Riscv_Irqdeleg_Ext_Extirq8_Riscv_Seip,
                                                                 F_Dma           => Lw_Priscv_Riscv_Irqdeleg_Dma_Riscv_Seip,
                                                                 F_Sha           => Lw_Priscv_Riscv_Irqdeleg_Sha_Riscv_Seip,
                                                                 F_Memerr        => Lw_Priscv_Riscv_Irqdeleg_Memerr_Riscv_Seip,
                                                                 F_Icd           => Lw_Priscv_Riscv_Irqdeleg_Icd_Riscv_Seip,
                                                                 F_Iopmp         => Lw_Priscv_Riscv_Irqdeleg_Iopmp_Riscv_Seip,
                                                                 F_Core_Mismatch => Lw_Priscv_Riscv_Irqdeleg_Core_Mismatch_Riscv_Seip));
      end Setup_Interrupt_Delegation;


      -- S-mode allowed to flush dCache
      -- mmiscopen => DCACHEOP = 1
      procedure Allow_S_Mode_To_Flush_D_Cache is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MMISCOPEN_Register);
      begin
         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MMISCOPEN_Address,
                      Data => LW_RISCV_CSR_MMISCOPEN_Register'(Wpri     => 0,
                                                               Dcacheop => DCACHEOP_ENABLE));

      end Allow_S_Mode_To_Flush_D_Cache;


      -- Enable all memory operations for partition
      -- msysopen => All fields = 1
      procedure Enable_Partition_Memory_Operations is
         procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_LWCONFIG_REGISTER);
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSYSOPEN_Register);

         LwConfig : LW_PRISCV_RISCV_LWCONFIG_REGISTER;
      begin
         -- First check if MSYSOPEN exists
         Riscv_Reg_Rd32(Offset => LwU32(Lw_Priscv_Riscv_Lwconfig),
                      Data => LwConfig);

         if LwConfig.F_Sysop_Csr_En = Lw_Priscv_Riscv_Lwconfig_Sysop_Csr_En_Rst then
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSYSOPEN_Address,
                         Data => LW_RISCV_CSR_MSYSOPEN_Register'(Wpri => 0,
                                                                 L2flhdty    => L2FLHDTY_ENABLE,
                                                                 L2clncomp   => L2CLNCOMP_ENABLE,
                                                                 L2peerilw   => L2PEERILW_ENABLE,
                                                                 L2sysilw    => L2SYSILW_ENABLE,
                                                                 Flush       => FLUSH_ENABLE,
                                                                 Tlbilwop    => TLBILWOP_ENABLE,
                                                                 Tlbilwdata1 => TLBILWDATA1_ENABLE,
                                                                 Bind        => BIND_ENABLE));
         end if;


      end Enable_Partition_Memory_Operations;


      -- Will be used by mret instruction when leaving SK
      -- mepc => Start address of partition
      procedure Set_Partition_Start_Address(Err_Code : in out Error_Codes) is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);

         Mepc : LW_RISCV_CSR_MEPC_Register;
      begin
         Engine_Config.Get_Partition_Entry_Point(Mepc.Epc, Err_Code);

         if Err_Code = OK then
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEPC_Address,
                         Data => Mepc);
         end if;
      end Set_Partition_Start_Address;

      -- Allowing branch prediction, but will clear state before transferring to partition
      -- mbpcfg => Enable All Branch Predictors
      procedure Enable_All_Branch_Predictors is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MBPCFG_Register);
      begin

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MBPCFG_Address,
                      Data => LW_RISCV_CSR_MBPCFG_Register'(Wpri2 => 0,
                                                            Wpri1 => 0,
                                                            Wpri0 => 0,
                                                            Ras_Flush  => 1,
                                                            Bht_Flush  => 1,
                                                            Btb_Flushm => 1,
                                                            Btb_Flushs => 1,
                                                            Btb_Flushu => 1,
                                                            Ras_En     => EN_TRUE,
                                                            Bht_En     => EN_TRUE,
                                                            Btb_En     => EN_TRUE));
      end Enable_All_Branch_Predictors;

      -- Enable ICD in S-mode.
      -- mdbgctl => Build Defined Value
      procedure Enable_ICD_In_S_Mode is
      begin

         Debug_Control.Configure_ICD;

      end Enable_ICD_In_S_Mode;

      -- Manifest sets M-mode level, FMC will copy that into S-mode. !!! And U-mode. Added later to match C-SepKern
      -- sspm => mspm.MPLM
      -- Default it to the max allowed privilege level for S-mode so early bootloader can run out of external memory
      -- srsp => SRPL = sspm.SPLM
      procedure Define_S_And_U_Mode_Privilege(Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER;
                                              Mspm           : in out LW_RISCV_CSR_MSPM_Register;
                                              Mrsp           : in out LW_RISCV_CSR_MRSP_Register;
                                              Err_Code       : in out Error_Codes) is
      begin
         Operation_Done:
         for Unused_Loop_Var in 1 .. 1 loop

         -- SK need to halt if BCR_SEC_WPRID == 0 but manifest (mspm) indicates non-zero privilege
            if Mspm.Mplm /= LW_RISCV_CSR_MSPM_MPLM_LEVEL0 and then Bcr_DmaCfg_Sec.F_Wprid = Lw_Priscv_Riscv_Bcr_Dmacfg_Sec_Wprid_Init then
               Err_Code := CRITICAL_ERROR;
            end if;

            exit Operation_Done when Err_Code /= OK;

            Mspm.Splm := Mspm.Mplm;
            Mspm.Uplm := Mspm.Mplm;

            case Mspm.Mplm is
            when LW_RISCV_CSR_MSPM_MPLM_LEVEL3 => -- 1111
               Mrsp.Srpl := SRPL_LEVEL3;
               Mrsp.Urpl := URPL_LEVEL3;
            when (LW_RISCV_CSR_MSPM_MPLM_LEVEL2 or LW_RISCV_CSR_MSPM_MPLM_LEVEL1) => -- 0111
               Mrsp.Urpl := URPL_LEVEL2;
               Mrsp.Srpl := SRPL_LEVEL2;
            when LW_RISCV_CSR_MSPM_MPLM_LEVEL2 => -- 0101
               Mrsp.Srpl := SRPL_LEVEL2;
               Mrsp.Urpl := URPL_LEVEL2;
            when LW_RISCV_CSR_MSPM_MPLM_LEVEL1 => -- 0011
               Mrsp.Srpl := SRPL_LEVEL1;
               Mrsp.Urpl := URPL_LEVEL1;
            when LW_RISCV_CSR_MSPM_MPLM_LEVEL0 => -- 0001
               Mrsp.Srpl := SRPL_LEVEL0;
               Mrsp.Urpl := URPL_LEVEL0;
            when others => -- all other values are invalid
               Mrsp.Srpl := SRPL_LEVEL0;
               Mrsp.Urpl := URPL_LEVEL0;
               Err_Code := CRITICAL_ERROR;
            end case;

            case Mspm.Msecm is
            when MSECM_SEC   =>
               Mspm.Ssecm := SSECM_SEC;
               Mspm.Usecm := USECM_SEC;
               Mrsp.Srsec := SRSEC_SEC;
               Mrsp.Ursec := URSEC_SEC;
            when MSECM_INSEC =>
               Mspm.Ssecm := SSECM_INSEC;
               Mspm.Usecm := USECM_INSEC;
               Mrsp.Srsec := SRSEC_INSEC;
               Mrsp.Ursec := URSEC_INSEC;
            end case;

         end loop Operation_Done;

      end Define_S_And_U_Mode_Privilege;


      procedure Set_S_And_U_Mode_Privilege(Mspm : LW_RISCV_CSR_MSPM_Register;
                                           Mrsp : LW_RISCV_CSR_MRSP_Register) is

         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);

      begin
         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSPM_Address,
                      Data => Mspm);

         Csr_Reg_Wr64 (Addr => LW_RISCV_CSR_MRSP_Address,
                       Data => Mrsp);

      end Set_S_And_U_Mode_Privilege;

      -- Allowing L1 caching to accelerate the early bootloader that runs out of external memory prior to programming MPU
      -- mfetchattr => Cacheable=1
      -- mfetchattr => WPR ID = <boot arg>

      -- Allowing L1 caching to accelerate the early bootloader that runs out of external memory prior to programming MPU
      -- mldstattr => Cacheable=1
      -- mldstattr => WPR ID = <boot arg>
      procedure Allow_L1_Caching(Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER) is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MFETCHATTR_Register);
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MLDSTATTR_Register);
      begin

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MFETCHATTR_Address,
                      Data => LW_RISCV_CSR_MFETCHATTR_Register'(Rs0       => 0,
                                                                Vpr       => 0,
                                                                Wpr       => LwU5(Bcr_DmaCfg_Sec.F_Wprid),
                                                                L2c_Rd    => 0,
                                                                L2c_Wr    => 0,
                                                                Coherent  => 1,
                                                                Cacheable => 1,
                                                                Rs1       => 0));

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MLDSTATTR_Address,
                      Data => LW_RISCV_CSR_MLDSTATTR_Register'(Rs0       => 0,
                                                               Vpr       => 0,
                                                               Wpr       => LwU5(Bcr_DmaCfg_Sec.F_Wprid),
                                                               L2c_Rd    => 0,
                                                               L2c_Wr    => 0,
                                                               Coherent  => 1,
                                                               Cacheable => 1,
                                                               Rs1       => 0));

      end Allow_L1_Caching;

      -- Allow access to time CSR (PTIMER sourced). Do not allow access by default to cycle/instret
      -- mcounteren => TM = 1
      procedure Allow_Time_CSR_Access is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCOUNTEREN_Register);

         Mcounteren : LW_RISCV_CSR_MCOUNTEREN_Register;
      begin
         Mcounteren.Rs0 := LW_RISCV_CSR_MCOUNTEREN_RS0_RST;
         Mcounteren.Hpm := LW_RISCV_CSR_MCOUNTEREN_HPM_DISABLE;
         Mcounteren.Ir := IR_DISABLE;
         Mcounteren.Cy := CY_DISABLE;
         Mcounteren.Tm := TM_ENABLE;

         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCOUNTEREN_Address,
                      Data => Mcounteren);
      end Allow_Time_CSR_Access;

      procedure Setup_MStatus is
         procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSTATUS_Register);
      begin
         Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSTATUS_Address,
                      Data => LW_RISCV_CSR_MSTATUS_Register'(Sd    => 0,
                                                             Wpri4 => 0,
                                                             Sxl   => 0,
                                                             Uxl   => 0,
                                                             Wpri3 => 0,
                                                             Mmi   => MMI_DISABLE,
                                                             Tsr   => TSR_DISABLE,
                                                             Tw    => TW_DISABLE,
                                                             Tvm   => TVM_DISABLE,
                                                             Mxr   => MXR_DISABLE,
                                                             Sum   => 0,
                                                             Mprv  => MPRV_DISABLE,
                                                             Xs    => 0,
                                                             Fs    => FS_OFF,
                                                             Mpp   => MPP_SUPERVISOR, -- !!!
                                                             Wpri2 => 0,
                                                             Spp   => SPP_USER,
                                                             Mpie  => MPIE_DISABLE,
                                                             Wpri1 => 0,
                                                             Spie  => SPIE_DISABLE,
                                                             Upie  => 0,
                                                             Mie   => MIE_DISABLE,    -- !!!
                                                             Wpri0 => 0,
                                                             Sie   => SIE_DISABLE,
                                                             Uie   => 0));

      end Setup_MStatus;

      procedure Enable_Timer_Interrupt is
         procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
      begin
         Csr_Reg_Set64(Addr => LW_RISCV_CSR_MIE_Address,
                       Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => 0,
                                                          Meie  => 0,
                                                          Wpri2 => 0,
                                                          Seie  => 0,
                                                          Ueie  => 0,
                                                          Mtie  => 1,
                                                          Wpri1 => 0,
                                                          Stie  => 0,
                                                          Utie  => 0,
                                                          Msie  => 0,
                                                          Wpri0 => 0,
                                                          Ssie  => 0,
                                                          Usie  => 0));
      end Enable_Timer_Interrupt;
   end Partition;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 4 - Initialize Partition CSR
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

   procedure Initialize_Partition_CSR(Err_Code : in out Error_Codes) is
      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);

      Bcr_DmaCfg_Sec : LW_PRISCV_RISCV_BCR_DMACFG_SEC_REGISTER;
      Mspm : LW_RISCV_CSR_MSPM_Register;
      Mrsp : LW_RISCV_CSR_MRSP_Register;

   begin
      Operation_Done:
      for Unused_Loop_Var in 1 .. 1 loop
         Partition.Setup_Supervisor_Trap_Vector;
         Partition.Setup_Interrupt_Delegation;
         Partition.Allow_S_Mode_To_Flush_D_Cache;
         Partition.Enable_Partition_Memory_Operations;
         Partition.Set_Partition_Start_Address(Err_Code => Err_Code);
         exit Operation_Done when Err_Code /= OK;

         Partition.Enable_All_Branch_Predictors;
         Partition.Enable_ICD_In_S_Mode;

         Riscv_Reg_Rd32 (Offset => LwU32(Lw_Priscv_Riscv_Bcr_Dmacfg_Sec),
                         Data => Bcr_DmaCfg_Sec);
         Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MSPM_Address,
             Data => Mspm);
         Csr_Reg_Rd64 (Addr => LW_RISCV_CSR_MRSP_Address,
                       Data => Mrsp);

         Partition.Define_S_And_U_Mode_Privilege(Bcr_DmaCfg_Sec, Mspm, Mrsp, Err_Code);
         exit Operation_Done when Err_Code /= OK;

         Partition.Set_S_And_U_Mode_Privilege(Mspm, Mrsp);
         Partition.Allow_L1_Caching(Bcr_DmaCfg_Sec);

         Partition.Allow_Time_CSR_Access;
         Partition.Setup_MStatus;
         Partition.Enable_Timer_Interrupt;
      end loop Operation_Done;

   end Initialize_Partition_CSR;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 5 - Initialize Peregrine State
-- -----------------------------------------------------------------------------
-- Beyond the LWRISCV core itself the SK is responsible for setting up state inside Peregrine.
-- Everything except interrupt delegation will be done by the manifest. The SK itself does not touch any peripherals so they can be simply passed through.
   procedure Initialize_Peregrine_State(Err_Code : in out Error_Codes) is
      procedure Flcn_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Rd32_Generic (Generic_Register => LW_PFALCON_FBIF_CTL_REGISTER);
      procedure Flcn_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Rd32_Generic (Generic_Register => LW_PFALCON_FALCON_MAILBOX0_REGISTER);
      procedure Flcn_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Rd32_Generic (Generic_Register => LW_PFALCON_FALCON_MAILBOX1_REGISTER);
      procedure Flcn_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Wr32_Generic (Generic_Register => LW_PFALCON_FBIF_CTL_REGISTER);
      procedure Flcn_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Wr32_Generic (Generic_Register => LW_PFALCON_FBIF_CTL2_REGISTER);

      procedure Riscv_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Wr32_Generic (Generic_Register => LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_REGISTER);

      Fbif_Ctl : LW_PFALCON_FBIF_CTL_REGISTER;
   begin

      Flcn_Reg_Rd32 (Offset => LwU32(Lw_Pfalcon_Fbif_Ctl),
                     Data => Fbif_Ctl);
      Fbif_Ctl.F_Enable := Lw_Pfalcon_Fbif_Ctl_Enable_True;
      Fbif_Ctl.F_Allow_Phys_No_Ctx := Lw_Pfalcon_Fbif_Ctl_Allow_Phys_No_Ctx_Allow;

      Flcn_Reg_Wr32(Offset => LwU32(Lw_Pfalcon_Fbif_Ctl),
                    Data => Fbif_Ctl);

      Flcn_Reg_Wr32(Offset => LwU32(Lw_Pfalcon_Fbif_Ctl2),
                    Data => LW_PFALCON_FBIF_CTL2_REGISTER'(F_Nack_Mode => Lw_Pfalcon_Fbif_Ctl2_Nack_Mode_Nack_As_Ack));

      Engine_Config.Configure;

      -- Release lockdown
      Riscv_Reg_Wr32(Offset => LwU32(Lw_Priscv_Riscv_Br_Priv_Lockdown),
                   Data => LW_PRISCV_RISCV_BR_PRIV_LOCKDOWN_REGISTER'(F_Lock => Lw_Priscv_Riscv_Br_Priv_Lockdown_Lock_Unlocked));

      Err_Code := OK;

   end Initialize_Peregrine_State;


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
-- marchid                      - a2
-- Reserved for mimpid          - a3 - Hardwired to 0 for GA10x
-- Reserved for mhartid         - a4 - Hardwired to 0 for GA10x
-- mfetchattr                   - a5
-- mldstattr                    - a6
-- Reserved, guaranteed to be 0 - a7

   procedure Load_Initial_Arguments(Err_Code : in out Error_Codes)
   is
      Misa : LW_RISCV_CSR_MISA_Register;
      MarchId : LW_RISCV_CSR_MARCHID_Register;
      Mfetchattr : LW_RISCV_CSR_MFETCHATTR_Register;
      Mldstattr : LW_RISCV_CSR_MLDSTATTR_Register;

      a : Argument_Registers;

      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MISA_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MARCHID_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MFETCHATTR_Register);
      procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MLDSTATTR_Register);

      function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => LW_RISCV_CSR_MISA_Register,
                                                                 Target => LwU64);
      function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => LW_RISCV_CSR_MARCHID_Register,
                                                                 Target => LwU64);
      function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => LW_RISCV_CSR_MFETCHATTR_Register,
                                                                 Target => LwU64);
      function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion (Source => LW_RISCV_CSR_MLDSTATTR_Register,
                                                                 Target => LwU64);

   begin
      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MISA_Address,
                   Data => Misa);
      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MARCHID_Address,
                   Data => MarchId);
      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MFETCHATTR_Address,
                   Data => Mfetchattr);
      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MLDSTATTR_Address,
                   Data => Mldstattr);

      a(0) := SBI_Version;
      a(1) := Colwert_To_LwU64(Misa);
      a(2) := Colwert_To_LwU64(MarchId);
      a(3) := 0;
      a(4) := 0;
      a(5) := Colwert_To_LwU64(Mfetchattr);
      a(6) := Colwert_To_LwU64(Mldstattr);
      a(7) := 0;

      Riscv_Core.Rv_Gpr.Save_To_Argument_Registers(a);
      pragma Annotate(GNATProve, False_Positive, """a"" might not be initialized", "Clearly false_positive. 'a' is initialized");

      Err_Code := OK;

   end Load_Initial_Arguments;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 7 - Do Fence IO and Check for Pending Interrupts
-- -----------------------------------------------------------------------------
-- Issue a Fence.IO and check for any pending interrupts to ensure all transactions are completed.

   procedure Do_Fence_IO_And_Check_For_Pending_Interrupts(Err_Code : in out Error_Codes) is
     Mip : LW_RISCV_CSR_MIP_Register;

     procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MIP_Register);
   begin
      Riscv_Core.Inst.Fence_Io;
      Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MIP_Address,
                   Data => Mip);

      if Mip.Meip  = LW_RISCV_CSR_MIP_MEIP_RST and then
        Mip.Seip  = LW_RISCV_CSR_MIP_SEIP_RST and then
        Mip.Ueip  = LW_RISCV_CSR_MIP_UEIP_RST and then
        Mip.Mtip  = LW_RISCV_CSR_MIP_MTIP_RST and then
        Mip.Stip  = LW_RISCV_CSR_MIP_STIP_RST and then
        Mip.Utip  = LW_RISCV_CSR_MIP_UTIP_RST and then
        Mip.Msip  = LW_RISCV_CSR_MIP_MSIP_RST and then
        Mip.Ssip  = LW_RISCV_CSR_MIP_SSIP_RST and then
        Mip.Usip  = LW_RISCV_CSR_MIP_USIP_RST
      then
            Err_Code := OK;
            return;
      end if;

      Err_Code := CRITICAL_ERROR;

   end Do_Fence_IO_And_Check_For_Pending_Interrupts;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 8 - Clear GPRs
-- -----------------------------------------------------------------------------
-- Clear all GPRs to 0 to avoid leaking any information about SK exelwtion into partition.
   procedure Clear_GPRs(Err_Code : in out Error_Codes) is
   begin
      Riscv_Core.Rv_Gpr.Clear_Gpr;
      Err_Code := OK;
   end Clear_GPRs;


-- -----------------------------------------------------------------------------
-- *** FMC/SK - Step 9 - Transfer to S-Mode
-- -----------------------------------------------------------------------------
   procedure Transfer_to_S_Mode
   is
   begin
      Riscv_Core.Inst.Mret;
   end Transfer_to_S_Mode;

end Separation_Kernel;
