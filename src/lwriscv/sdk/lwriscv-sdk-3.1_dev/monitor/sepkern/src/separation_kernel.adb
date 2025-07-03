-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Ada.Unchecked_Colwersion;
with Lw_Types;                   use Lw_Types;
with Lw_Types.Shift_Left_Op;     use Lw_Types.Shift_Left_Op;
with Lw_Types.Shift_Right_Op;    use Lw_Types.Shift_Right_Op;
with Types;                      use Types;
with Error_Handling;             use Error_Handling;

with Dev_Riscv_Csr_64;           use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;                use Dev_Prgnlcl;
with Lw_Riscv_Address_Map;       use Lw_Riscv_Address_Map;

with Riscv_Core.Inst;            use Riscv_Core.Inst;
with Riscv_Core.Rv_Gpr;          use Riscv_Core.Rv_Gpr;

with Types;                      use Types;
with Policy_Types;               use Policy_Types;
with Trap;                       use Trap;
with SBI;                        use SBI;
with Partition;
with Engine_Constants;
with PLM_Types;                  use PLM_Types;
with PLM_List;                   use PLM_List;

with Separation_Kernel.Policies; use Separation_Kernel.Policies;

package body Separation_Kernel
with
   Refined_State => (Separation_Kernel_State => Lwrrent_Partition_ID)
is

    Lwrrent_Partition_ID : aliased Partition_ID;

    function Get_Lwrrent_Partition_ID return Partition_ID is (Lwrrent_Partition_ID);

    -- -----------------------------------------------------------------------------
    -- *** Verify HW state
    -- -----------------------------------------------------------------------------
    -- If transfered from bootrom or bootplugin, check:
    -- * mtvec == Start_Address
    -- * Mcause == 1
    -- * mepc < FMC start address (BROM) - make sure exception oclwred in BROM, before exception handler install
    -- This makes sure that an exception didn't occur before the actual M-mode trap vector was installed
    -- as the value of mtvec set by the bootrom is the start of the FMC.
    --
    -- If SK is first code to run, check same register set against their reset values
    procedure Verify_HW_State(Err_Code : in out Error_Codes)
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MTVEC_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCAUSE_Register);

        Mepc   : LW_RISCV_CSR_MEPC_Register;
        Mtvec  : LW_RISCV_CSR_MTVEC_Register;
        Mcause : LW_RISCV_CSR_MCAUSE_Register;
    begin

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEPC_Address,
                        Data => Mepc);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MTVEC_Address,
                        Data => Mtvec);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCAUSE_Address,
                        Data => Mcause);
#if INITIATOR = "bootrom" or else INITIATOR = "bootplugin" then
        if Mepc.Epc >= LW_RISCV_AMAP_IMEM_START then
            Err_Code := ILWALID_MEPC;
            return;
        end if;

        if LwU64 (Lw_Shift_Left (Value  => LwU64 (Mtvec.Base), Amount => 2)) /= LW_RISCV_AMAP_IMEM_START then
            Err_Code := ILWALID_MTVEC;
            return;
        end if;

        if Mcause.Int = 1 then
            Err_Code := UNEXPECTED_INTERRUPT;
            return;
        end if;

        if Mcause.Excode /= LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT then
            Err_Code := UNEXPECTED_EXCEPTION_CAUSE;
            return;
        end if;
#else
        if Mepc.Epc /= LW_RISCV_CSR_MEPC_EPC_RST then
            Err_Code := ILWALID_MEPC;
            return;
        end if;

        if Mtvec.Base /= LW_RISCV_CSR_MTVEC_BASE_RST then
            Err_Code := ILWALID_MTVEC;
            return;
        end if;

        if Mcause.Int /= LW_RISCV_CSR_MCAUSE_INT_RST then
            Err_Code := UNEXPECTED_INTERRUPT;
            return;
        end if;

        if Mcause.Excode /= LW_RISCV_CSR_MCAUSE_EXCODE_RST then
            Err_Code := UNEXPECTED_EXCEPTION_CAUSE;
            return;
        end if;
#end if;
    end Verify_HW_State;

    -- -----------------------------------------------------------------------------
    -- *** Initialize SK state
    -- -----------------------------------------------------------------------------
    procedure Initialize_SK_State
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MTVEC_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCOUNTEREN_Register);
        procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);
    begin
        -- Pogramm mtvec
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MTVEC_Address,
                     Data => LW_RISCV_CSR_MTVEC_Register'(Base => LwU62 (Lw_Shift_Right (Get_Address_Of_Trap_Entry, 2)),
                                                          Mode => MODE_DIRECT));

        -- Set priv level to 0 (lowest)
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MRSP_Address,                                                                              
                     Data => LW_RISCV_CSR_MRSP_Register'(Wpri2 => LW_RISCV_CSR_MRSP_WPRI2_RST,
                                                         Mrsec => MRSEC_INSEC,
                                                         Wpri1 => LW_RISCV_CSR_MRSP_WPRI1_RST,
                                                         Srsec => SRSEC_INSEC,
                                                         Ursec => URSEC_INSEC,
                                                         Mrpl  => MRPL_LEVEL0,
                                                         Wpri0 => LW_RISCV_CSR_MRSP_WPRI0_RST,
                                                         Srpl  => SRPL_LEVEL0,
                                                         Urpl  => URPL_LEVEL0));  

        -- Allow all temporarily for testing
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCOUNTEREN_Address,
                     Data => LW_RISCV_CSR_MCOUNTEREN_Register'(Rs0 => LW_RISCV_CSR_MCOUNTEREN_RS0_RST,
                                                               Hpm => LW_RISCV_CSR_MCOUNTEREN_HPM_DISABLE,
                                                               Ir => IR_ENABLE,
                                                               Cy => CY_ENABLE,
                                                               Tm => TM_ENABLE));

        -- TODO: Allow access to time CSR (PTIMER sourced). Do not allow access by default to cycle/instret
        -- mcounteren => TM = 1
        -- Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCOUNTEREN_Address,
        --              Data => LW_RISCV_CSR_MCOUNTEREN_Register'(Rs0 => LW_RISCV_CSR_MCOUNTEREN_RS0_RST,
        --                                                        Hpm => LW_RISCV_CSR_MCOUNTEREN_HPM_DISABLE,
        --                                                        Ir => IR_DISABLE,
        --                                                        Cy => CY_DISABLE,
        --                                                        Tm => TM_ENABLE));

        -- Disable all interrupts except timer. Mtimecmp is not programmed so no interrupts will arise
        Csr_Reg_Set64(Addr => LW_RISCV_CSR_MIE_Address,
                      Data => LW_RISCV_CSR_MIE_Register'(Wpri3 => LW_RISCV_CSR_MIE_WPRI3_RST,
                                                         Meie  => LW_RISCV_CSR_MIE_MEIE_RST,
                                                         Wpri2 => LW_RISCV_CSR_MIE_WPRI2_RST,
                                                         Seie  => LW_RISCV_CSR_MIE_SEIE_RST,
                                                         Ueie  => LW_RISCV_CSR_MIE_UEIE_RST,
                                                         Mtie  => 1,
                                                         Wpri1 => LW_RISCV_CSR_MIE_WPRI1_RST,
                                                         Stie  => LW_RISCV_CSR_MIE_STIE_RST,
                                                         Utie  => LW_RISCV_CSR_MIE_UTIE_RST,
                                                         Msie  => LW_RISCV_CSR_MIE_MSIE_RST,
                                                         Wpri0 => LW_RISCV_CSR_MIE_WPRI0_RST,
                                                         Ssie  => LW_RISCV_CSR_MIE_SSIE_RST,
                                                         Usie  => LW_RISCV_CSR_MIE_USIE_RST));

    end Initialize_SK_State;

    -- -----------------------------------------------------------------------------
    -- *** Check if the current active partition can switch to partition with id=Id
    -- -----------------------------------------------------------------------------
    function Is_Switchable_To(Id: Partition_ID) return Boolean
    is
    begin

        if Policies.Partition_Policies(Lwrrent_Partition_ID).Switchable_To(Id) = PERMITTED then
            return True;
        else
            return False;
        end if;

    end Is_Switchable_To;

    -- -----------------------------------------------------------------------------
    -- *** Initialize Partition
    -- -----------------------------------------------------------------------------
    procedure Initialize_Partition_With_ID(Id : Partition_ID)
    with
        Refined_Global => (Output => (Lwrrent_Partition_ID,
                                    Riscv_Core.Rv_Csr.Csr_Reg_Hw_Model.Csr_State,
                                    Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State))
    is
    begin

        Partition.Initialize_Partition_Per_Policy(Policies.Partition_Policies(Id));

        Lwrrent_Partition_ID := Id;

    end Initialize_Partition_With_ID;

    -- -----------------------------------------------------------------------------
    -- *** Clear Current Partition State
    -- -----------------------------------------------------------------------------
    procedure Clear_Lwrrent_Partition_State
    is
    begin

        -- Given policies are static, we can assert there are no MPU entry overlaps between partitions.
        -- It is only ok to have overlaps if Clear_MPU_State is enabled, i.e. SK will clear the whole MPU on every switch
        pragma Assert(for all I in Partition_ID =>
                        (for all J in Partition_ID =>
                            (if Separation_Kernel.Policies.Clear_MPU_State = 1 then True
                            elsif I /= J and then
                                Policies.Partition_Policies(I).Mpu_Control.Entry_Count > 0 and then
                                Policies.Partition_Policies(J).Mpu_Control.Entry_Count > 0
                            then
                                (LwU8(Policies.Partition_Policies(I).Mpu_Control.Start_Index) + Policies.Partition_Policies(I).Mpu_Control.Entry_Count - 1) < LwU8(Policies.Partition_Policies(J).Mpu_Control.Start_Index)
                             or else
                                (LwU8(Policies.Partition_Policies(J).Mpu_Control.Start_Index) + Policies.Partition_Policies(J).Mpu_Control.Entry_Count - 1) < LwU8(Policies.Partition_Policies(I).Mpu_Control.Start_Index))));

        -- Clear MPU if required
        if Separation_Kernel.Policies.Clear_MPU_State = 1 then
            Partition.Clear_MPU;
        end if;

        -- Clear Partition State
        Partition.Clear_Partition_State;

    end Clear_Lwrrent_Partition_State;

    -- -----------------------------------------------------------------------------
    -- *** SK Initialize Peregrine State
    -- -----------------------------------------------------------------------------
    procedure Initialize_Peregrine_State
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEDELEG_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MIDELEG_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IRQDELEG_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_PRIV_LEVEL_MASK_Register);
    begin

        -- Delegate all exceptions
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEDELEG_Address,
                     Data => LW_RISCV_CSR_MEDELEG_Register'(Wpri3               => LW_RISCV_CSR_MEDELEG_WPRI3_RST,
                                                            Wpri2               => LW_RISCV_CSR_MEDELEG_WPRI2_RST,
                                                            Wpri1               => LW_RISCV_CSR_MEDELEG_WPRI1_RST,
                                                            Wpri0               => LW_RISCV_CSR_MEDELEG_WPRI0_RST,
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
        -- Delegate all interupts
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MIDELEG_Address,
                     Data => LW_RISCV_CSR_MIDELEG_Register'(Wpri2 => LW_RISCV_CSR_MIDELEG_WPRI2_RST,
                                                            Seid  => SEID_DELEG,
                                                            Ueid  => UEID_NODELEG,
                                                            Wpri1 => LW_RISCV_CSR_MIDELEG_WPRI1_RST,
                                                            Stid  => STID_DELEG,
                                                            Utid  => UTID_NODELEG,
                                                            Wpri0 => LW_RISCV_CSR_MIDELEG_WPRI0_RST,
                                                            Ssid  => SSID_DELEG,
                                                            Usid  => USID_NODELEG));

        Csb_Reg_Wr32(Addr => LwU32(LW_PRGNLCL_RISCV_IRQDELEG_Address),
                     Data => Engine_Constants.Riscv_Irqdeleg_All_Routed_To_S_Mode);

        -- Pogram PLMs
        if PLM_AD_Pair'Length > 0 then
            for I in PLM_AD_Pair'range loop
                Csb_Reg_Wr32(Addr => PLM_AD_Pair(I).Address,
                             Data => PLM_AD_Pair(I).Data);
            end loop;
        end if;

    end Initialize_Peregrine_State;

    -- -----------------------------------------------------------------------------
    -- *** Load Initial Arguments
    -- -----------------------------------------------------------------------------
    procedure Load_Initial_Arguments
    is
        Misa       : LW_RISCV_CSR_MISA_Register;
        MarchId    : LW_RISCV_CSR_MARCHID_Register;
        Mimpd      : LW_RISCV_CSR_MIMPID_Register;
        Mvendorid  : LW_RISCV_CSR_MVENDORID_Register;
        Mfetchattr : LW_RISCV_CSR_MFETCHATTR_Register;
        Mldstattr  : LW_RISCV_CSR_MLDSTATTR_Register;

        a : Argument_Registers_Array;

        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MISA_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MARCHID_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MIMPID_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MVENDORID_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MFETCHATTR_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MLDSTATTR_Register);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MISA_Register,
             Target => LwU64);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MARCHID_Register,
             Target => LwU64);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MIMPID_Register,
             Target => LwU64);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MVENDORID_Register,
             Target => LwU64);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MFETCHATTR_Register,
             Target => LwU64);

        function Colwert_To_LwU64 is new Ada.Unchecked_Colwersion
            (Source => LW_RISCV_CSR_MLDSTATTR_Register,
             Target => LwU64);

    begin
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MISA_Address,
                     Data => Misa);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MARCHID_Address,
                     Data => MarchId);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MIMPID_Address,
                     Data => Mimpd);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MVENDORID_Address,
                     Data => Mvendorid);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MFETCHATTR_Address,
                     Data => Mfetchattr);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MLDSTATTR_Address,
                     Data => Mldstattr);

        a(0) := SBI_Version;
        a(1) := Colwert_To_LwU64(Misa);
        a(2) := Colwert_To_LwU64(MarchId);
        a(3) := Colwert_To_LwU64(Mimpd);
        a(4) := Colwert_To_LwU64(Mvendorid);
        a(5) := Colwert_To_LwU64(Mfetchattr);
        a(6) := Colwert_To_LwU64(Mldstattr);
        a(7) := 0;

        Riscv_Core.Rv_Gpr.Restore_Argument_Registers_From(a);
        pragma Annotate(GNATProve, False_Positive, """a"" might not be initialized", "Clearly false_positive. 'a' is initialized");

    end Load_Initial_Arguments;

    -- -----------------------------------------------------------------------------
    -- *** Load Parameters to Argument Registers
    -- -----------------------------------------------------------------------------
    procedure Load_Parameters_To_Argument_Registers(Param1 : LwU64;
                                                    Param2 : LwU64;
                                                    Param3 : LwU64;
                                                    Param4 : LwU64;
                                                    Param5 : LwU64;
                                                    Param6 : LwU64)
    is
        a : Argument_Registers_Array;
    begin
        a(0) := Param1;
        a(1) := Param2;
        a(2) := Param3;
        a(3) := Param4;
        a(4) := Param5;
        a(5) := Param6;
        a(6) := 0;
        a(7) := 0;

        Riscv_Core.Rv_Gpr.Restore_Argument_Registers_From(a);
        pragma Annotate(GNATProve, False_Positive, """a"" might not be initialized", "Clearly false_positive. 'a' is initialized");

    end Load_Parameters_To_Argument_Registers;

    -- -----------------------------------------------------------------------------
    -- *** Clear SK State
    -- -----------------------------------------------------------------------------
    procedure Clear_SK_State
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MBPCFG_Register);
    begin
        -- Allowing branch prediction, but clear state before transferring to partition
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MBPCFG_Address,
                     Data => LW_RISCV_CSR_MBPCFG_Register'( Wpri2      => LW_RISCV_CSR_MBPCFG_WPRI2_RST,
                                                            Wpri1      => LW_RISCV_CSR_MBPCFG_WPRI1_RST,
                                                            Wpri0      => LW_RISCV_CSR_MBPCFG_WPRI0_RST,
                                                            Ras_Flush  => LW_RISCV_CSR_MBPCFG_RAS_FLUSH_TRUE,
                                                            Bht_Flush  => LW_RISCV_CSR_MBPCFG_BHT_FLUSH_TRUE,
                                                            Btb_Flushm => LW_RISCV_CSR_MBPCFG_BTB_FLUSHM_TRUE,
                                                            Btb_Flushs => LW_RISCV_CSR_MBPCFG_BTB_FLUSHS_TRUE,
                                                            Btb_Flushu => LW_RISCV_CSR_MBPCFG_BTB_FLUSHU_TRUE,
                                                            Ras_En     => EN_TRUE,
                                                            Bht_En     => EN_TRUE,
                                                            Btb_En     => EN_TRUE));

        -- Issue a Fence.IO and check for any pending interrupts to ensure all transactions are completed.
        Riscv_Core.Inst.Fence_Io;

    end Clear_SK_State;

    procedure Check_For_Pending_Interrupts(Err_Code : in out Error_Codes)
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MIP_Register);
        Mip : LW_RISCV_CSR_MIP_Register;
    begin
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MIP_Address,
                    Data => Mip);

        if Mip.Meip = LW_RISCV_CSR_MIP_MEIP_RST and then
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

        Err_Code := UNEXPECTED_INTERRUPT;

    end Check_For_Pending_Interrupts;

    -- -----------------------------------------------------------------------------
    -- *** Clear GPRs
    -- -----------------------------------------------------------------------------
    procedure Clear_GPRs
    is
    begin
        Riscv_Core.Rv_Gpr.Clear_Gpr;
    end Clear_GPRs;

    -- -----------------------------------------------------------------------------
    -- *** Transfer to S-Mode
    -- -----------------------------------------------------------------------------
    procedure Transfer_to_S_Mode
    is
    begin
        Riscv_Core.Inst.Mret;
    end Transfer_to_S_Mode;

end Separation_Kernel;
