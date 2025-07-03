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
with Lw_Riscv_Address_Map;     use Lw_Riscv_Address_Map;
with Lw_Types.Shift_Right_Op;  use Lw_Types.Shift_Right_Op;
with Types;                    use Types;
with Typecast;                 use Typecast;
with Policy_Types;             use Policy_Types;
with Mpu_Control;              use Mpu_Control;
with Debug_Control;            use Debug_Control;
with Riscv_Core.Inst;          use Riscv_Core.Inst;
with Policies;                 use Policies;
with Device_Map_Policy_Types;  use Device_Map_Policy_Types;
with Policy_External_To_Internal_Colwersions; use Policy_External_To_Internal_Colwersions;
with Partition_Entry_Priv_Lockdown; use Partition_Entry_Priv_Lockdown;

package body Partition
is

    -- S-mode allowed to flush dCache
    -- mmiscopen => DCACHEOP = 1
    procedure Allow_S_Mode_To_Flush_D_Cache
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MMISCOPEN_Register);
    begin
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MMISCOPEN_Address,
                     Data => LW_RISCV_CSR_MMISCOPEN_Register'(Wpri     => LW_RISCV_CSR_MMISCOPEN_WPRI_RST,
                                                              Dcacheop => DCACHEOP_ENABLE));

    end Allow_S_Mode_To_Flush_D_Cache;

    -- Enable all memory operations for partition
    -- msysopen => All fields = 1
    procedure Enable_Partition_Memory_Operations
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_LWCONFIG_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSYSOPEN_Register);

        LwConfig : LW_PRGNLCL_RISCV_LWCONFIG_Register;
    begin
        -- First check if MSYSOPEN is enabled
        Csb_Reg_Rd32(Addr => LwU32(LW_PRGNLCL_RISCV_LWCONFIG_Address),
                     Data => LwConfig);

        if LwConfig.Sysop_Csr_En = 1 then
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSYSOPEN_Address,
                         Data => LW_RISCV_CSR_MSYSOPEN_Register'(Wpri        => LW_RISCV_CSR_MSYSOPEN_WPRI_RST,
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

    -- Allowing L1 caching to accelerate the early bootloader that runs out of external memory prior to programming MPU
    procedure Allow_L1_Caching
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register);

        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MFETCHATTR_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MLDSTATTR_Register);

        Bcr_DmaCfg_Sec : LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register;
    begin
        Csb_Reg_Rd32(Addr => LwU32(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Address),
                    Data => Bcr_DmaCfg_Sec);

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MFETCHATTR_Address,
                     Data => LW_RISCV_CSR_MFETCHATTR_Register'(Rs0       => LW_RISCV_CSR_MFETCHATTR_RS0_RST,
                                                               Vpr       => LW_RISCV_CSR_MFETCHATTR_VPR_RST,
                                                               Wpr       => BCR_DMACFG_SEC_to_WPR(Bcr_DmaCfg_Sec), -- WPRID or GSCID
                                                               L2c_Rd    => LW_RISCV_CSR_MFETCHATTR_L2C_RD_RST,
                                                               L2c_Wr    => LW_RISCV_CSR_MFETCHATTR_L2C_WR_RST,
                                                               Coherent  => LW_RISCV_CSR_MFETCHATTR_COHERENT_RST, -- 1
                                                               Cacheable => 1,
                                                               Rs1       => LW_RISCV_CSR_MFETCHATTR_RS1_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MLDSTATTR_Address,
                     Data => LW_RISCV_CSR_MLDSTATTR_Register'(Rs0       => LW_RISCV_CSR_MLDSTATTR_RS0_RST,
                                                              Vpr       => LW_RISCV_CSR_MLDSTATTR_VPR_RST,
                                                              Wpr       => BCR_DMACFG_SEC_to_WPR(Bcr_DmaCfg_Sec), -- WPRID or GSCID
                                                              L2c_Rd    => LW_RISCV_CSR_MLDSTATTR_L2C_RD_RST,
                                                              L2c_Wr    => LW_RISCV_CSR_MLDSTATTR_L2C_WR_RST,
                                                              Coherent  => LW_RISCV_CSR_MLDSTATTR_COHERENT_RST, -- 1
                                                              Cacheable => 1,
                                                              Rs1       => LW_RISCV_CSR_MLDSTATTR_RS1_RST));

    end Allow_L1_Caching;

    procedure Setup_MStatus
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSTATUS_Register);
    begin
        -- Set MPP (Machine Previous Privilege) to S-mode and disable interrupts
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSTATUS_Address,
                     Data => LW_RISCV_CSR_MSTATUS_Register'(Sd    => 0,
                                                            Wpri4 => LW_RISCV_CSR_MSTATUS_WPRI4_RST,
                                                            Sxl   => LW_RISCV_CSR_MSTATUS_SXL_RST,
                                                            Uxl   => LW_RISCV_CSR_MSTATUS_UXL_RST,
                                                            Wpri3 => LW_RISCV_CSR_MSTATUS_WPRI3_RST,
                                                            Mmi   => MMI_DISABLE,
                                                            Tsr   => TSR_DISABLE,
                                                            Tw    => TW_DISABLE,
                                                            Tvm   => TVM_DISABLE,
                                                            Mxr   => MXR_DISABLE,
                                                            Sum   => LW_RISCV_CSR_MSTATUS_SUM_DISABLE,
                                                            Mprv  => MPRV_DISABLE,
                                                            Xs    => LW_RISCV_CSR_MSTATUS_XS_RST,
                                                            Fs    => FS_OFF,
                                                            Mpp   => MPP_SUPERVISOR,
                                                            Wpri2 => LW_RISCV_CSR_MSTATUS_WPRI2_RST,
                                                            Spp   => SPP_USER,
                                                            Mpie  => MPIE_DISABLE,
                                                            Wpri1 => LW_RISCV_CSR_MSTATUS_WPRI1_RST,
                                                            Spie  => SPIE_DISABLE,
                                                            Upie  => LW_RISCV_CSR_MSTATUS_UPIE_DISABLE,
                                                            Mie   => MIE_DISABLE,
                                                            Wpri0 => LW_RISCV_CSR_MSTATUS_WPRI0_RST,
                                                            Sie   => SIE_DISABLE,
                                                            Uie   => LW_RISCV_CSR_MSTATUS_UIE_DISABLE));

    end Setup_MStatus;

    procedure Program_Entry_Point_Address(Entry_Point_Address : Plcy_Entry_Point_Addr_Type)
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEPC_Register);
    begin
        -- Set partition entry point
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEPC_Address,
                     Data => LW_RISCV_CSR_MEPC_Register'(Epc => Entry_Point_Address));

    end Program_Entry_Point_Address;

    procedure Program_Ucode_Id(Ucode_Id : Plcy_Ucode_Id_Type)
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MLWRRUID_Register);
    begin

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MLWRRUID_Address,
                     Data => LW_RISCV_CSR_MLWRRUID_Register'(Rs0      => LW_RISCV_CSR_MLWRRUID_RS0_RST,
                                                             Mlwrruid => LW_RISCV_CSR_MLWRRUID_MLWRRUID_RST,
                                                             Wpri0    => LW_RISCV_CSR_MLWRRUID_WPRI0_RST,
                                                             Slwrruid => Ucode_Id,
                                                             Ulwrruid => Ucode_Id));

    end Program_Ucode_Id;

    procedure Program_S_And_U_Mode_Privilege(Policy_Sspm : LW_RISCV_CSR_SSPM_Register)
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);

        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);

        Mspm : LW_RISCV_CSR_MSPM_Register;
        Mrsp : LW_RISCV_CSR_MRSP_Register;
    begin

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MSPM_Address,
                     Data => Mspm);
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MRSP_Address,
                     Data => Mrsp);

        Mspm.Splm  := Policy_Sspm.Splm;
        Mspm.Ssecm := (if Policy_Sspm.Ssecm = SSECM_SEC then SSECM_SEC else SSECM_INSEC);

        -- Start S-mode partition with LEVEL0 privilege
        Mrsp.Srpl  := SRPL_LEVEL0;
        Mrsp.Srsec := (if Policy_Sspm.Ssecm = SSECM_SEC then SRSEC_SEC else SRSEC_INSEC);

        -- Simply write default values for U-mode
        Mspm.Uplm  := LW_RISCV_CSR_MSPM_UPLM_LEVEL0;
        Mspm.Usecm := USECM_INSEC;

        Mrsp.Urpl  := URPL_LEVEL0;
        Mrsp.Ursec := URSEC_INSEC;

        -- And now write into CSR registers
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MSPM_Address,
                     Data => Mspm);

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MRSP_Address,
                     Data => Mrsp);

    end Program_S_And_U_Mode_Privilege;

    procedure Program_SCP_Lockdown(Policy_Sspm : LW_RISCV_CSR_SSPM_Register; Device_Map : Device_Map_Cfg)
    is
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_SCP_CTL_CFG_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_SCP_CTL_CFG_Register);
        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MRSP_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MSPM_Register);

        Mspm        : LW_RISCV_CSR_MSPM_Register;
        Mrsp_save   : LW_RISCV_CSR_MRSP_Register;
        Mrsp        : LW_RISCV_CSR_MRSP_Register;
        Scp         : LW_PRGNLCL_SCP_CTL_CFG_Register;
        MmodeReg    : LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_Register;
        MmodeScp    : MMODE_GROUP;
        SubmmodeScp : SUBMMODE_GROUP;
    begin
        -- Read MMode SCP group enable setting
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE_2_Address,
                     Data => MmodeReg);

        MmodeScp := LW_PRGNLCL_RISCV_DEVICEMAP_MMODE_Register_To_Plcy_Device_Map_Group_2(MmodeReg).SCP;

        -- Read Mspm for msecm setting
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MSPM_Address,
                     Data => Mspm);

        -- Check SCP MMODE access is enabled, and msecm is SEC, otherwise r/w for SCP_CTL_CFG register will fail
        if MmodeScp.Readable = MMODE_READ_ENABLE and then
           MmodeScp.Writable = MMODE_WRITE_ENABLE and then
           Mspm.Msecm = MSECM_SEC 
        then
            -- Disabling scp lockdown requires mrsec to be set to SEC
            -- Set temporarily here and restore after configuring scp lockdown
            Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MRSP_Address,
                         Data => Mrsp_save);
            Mrsp := Mrsp_save;
            Mrsp.Mrsec := MRSEC_SEC;
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MRSP_Address,
                         Data => Mrsp);

            Csb_Reg_Rd32(Addr => LW_PRGNLCL_SCP_CTL_CFG_Address,
                         Data => Scp);

            -- Enable scp lock down unless partition requires non-secure access
            SubmmodeScp := LW_PRGNLCL_RISCV_DEVICEMAP_SUBMMODE_Register_To_Plcy_Device_Map_Group_2(Device_Map.Cfg (2)).SCP;
            Scp.Lockdown_Scp := (if Policy_Sspm.Ssecm = SSECM_SEC or else
                                    (SubmmodeScp.Readable = SUBMMODE_READ_DISABLE or else
                                     SubmmodeScp.Writable = SUBMMODE_WRITE_DISABLE) then SCP_ENABLE else SCP_DISABLE);

            Csb_Reg_Wr32(Addr => LW_PRGNLCL_SCP_CTL_CFG_Address,
                         Data => Scp);

            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MRSP_Address,
                         Data => Mrsp_save);

        end if;
    end Program_SCP_Lockdown;

    procedure Program_Secret_Mask(Secret_Mask : Plcy_Secret_Mask)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK_Register);
    begin

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_0_Address,
                     Data => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_Register'(Val => Secret_Mask.Scp_Secret_Mask0));

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_1_Address,
                     Data => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_Register'(Val => Secret_Mask.Scp_Secret_Mask1));

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK_0_Address,
                     Data => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK_Register'(Val => Secret_Mask.Scp_Secret_Mask_lock0));

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK_1_Address,
                     Data => LW_PRGNLCL_RISCV_SCP_SECRET_MASK_LOCK_Register'(Val => Secret_Mask.Scp_Secret_Mask_lock1));

    end Program_Secret_Mask;

    procedure Program_Device_Map(Device_Map : Device_Map_Cfg)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_Register);
    begin
        for I in Device_Map.Cfg'Range loop
            Csb_Reg_Wr32 (Addr => LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_0_Address +
                            LwU32 (I) * (LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_1_Address - LW_PRGNLCL_RISCV_DEVICEMAP_RISCVSUBMMODE_0_Address),
                        Data => Device_Map.Cfg (I));
        end loop;
    end Program_Device_Map;

    procedure Program_Core_Pmp(Core_Pmp_Config : Core_Pmp_Cfg)
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_PMPCFG2_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_PMPADDR_Register);

        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPCFG0_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MEXTPMPADDR_Register);
    begin

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_8_Address,
                     Data => Core_Pmp_Config.Addr(0));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_9_Address,
                     Data => Core_Pmp_Config.Addr(1));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_10_Address,
                     Data => Core_Pmp_Config.Addr(2));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_11_Address,
                     Data => Core_Pmp_Config.Addr(3));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_12_Address,
                     Data => Core_Pmp_Config.Addr(4));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_13_Address,
                     Data => Core_Pmp_Config.Addr(5));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_14_Address,
                     Data => Core_Pmp_Config.Addr(6));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPADDR_15_Address,
                     Data => Core_Pmp_Config.Addr(7));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_PMPCFG2_Address,
                     Data => Core_Pmp_Config.Cfg2);

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_0_Address,
                     Data => Core_Pmp_Config.Ext_Addr(0));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_1_Address,
                     Data => Core_Pmp_Config.Ext_Addr(1));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_2_Address,
                     Data => Core_Pmp_Config.Ext_Addr(2));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_3_Address,
                     Data => Core_Pmp_Config.Ext_Addr(3));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_4_Address,
                     Data => Core_Pmp_Config.Ext_Addr(4));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_5_Address,
                     Data => Core_Pmp_Config.Ext_Addr(5));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_6_Address,
                     Data => Core_Pmp_Config.Ext_Addr(6));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPADDR_7_Address,
                     Data => Core_Pmp_Config.Ext_Addr(7));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MEXTPMPCFG0_Address,
                     Data => Core_Pmp_Config.Ext_Cfg0);

    end Program_Core_Pmp;

    procedure Program_Io_Pmp(Io_Pmp_Config : Io_Pmp_Cfg;
                             Io_Pmp_Mode   : Io_Pmp_Mode_Cfg)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_INDEX_Register);

        procedure Csb_Reg_Rd32 is new Riscv_Core.Csb_Reg.Rd32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_MODE_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_MODE_Register);

        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Register);
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => LW_PRGNLCL_RISCV_IOPMP_CFG_Register);

        Io_Pmp_Mode_Reg : LW_PRGNLCL_RISCV_IOPMP_MODE_Register;
    begin
        Csb_Reg_Rd32(Addr => LW_PRGNLCL_RISCV_IOPMP_MODE_0_Address,
                     Data => Io_Pmp_Mode_Reg);

        -- Need to preserve values for the first 8 entries that're set via manifest
        Io_Pmp_Mode_Reg.Val := (Io_Pmp_Mode_Reg.Val and IOPMP_MODE_CLEAR_MASK) or Io_Pmp_Mode.Io_Pmp_Mode0;

        Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_IOPMP_MODE_0_Address,
                     Data => Io_Pmp_Mode_Reg);

        for I in Io_Pmp_Cfg_Array'Range loop
            -- Set Index
            Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_IOPMP_INDEX_Address,
                         Data => LW_PRGNLCL_RISCV_IOPMP_INDEX_Register'(Val => LwU5 (I + IOPMP_START_ENTRY)));

            -- Set addr lo
            Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_IOPMP_ADDR_LO_Address,
                         Data => Io_Pmp_Config.Addr_Lo (I));

            -- Set addr hi
            Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_IOPMP_ADDR_HI_Address,
                         Data => Io_Pmp_Config.Addr_Hi (I));

            -- Cfg configuration must be the last step, otherwise the lock bit will Lock the entry causing the
            -- address/mode can't be updated.
            Csb_Reg_Wr32(Addr => LW_PRGNLCL_RISCV_IOPMP_CFG_Address,
                        Data => Io_Pmp_Config.Cfg (I));
        end loop;
    end Program_Io_Pmp;

    procedure Initialize_Partition_Per_Policy(Partition_Policy : Policy.Policy)
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_STVEC_Register);
    begin

        Program_Entry_Point_Address(Partition_Policy.Entry_Point_Address);
        Program_Ucode_Id(Partition_Policy.Ucode_Id);
        Program_Secret_Mask(Partition_Policy.Secret_Mask);
        Program_Debug_Control(Partition_Policy.Debug_Control);
        Program_Mmpu_Ctl(Partition_Policy.Mpu_Control);
        Program_Device_Map(Partition_Policy.Device_Map);
        Program_Core_Pmp(Partition_Policy.Core_Pmp);
        Program_Io_Pmp(Partition_Policy.Io_Pmp, Partition_Policy.Io_Pmp_Mode);

        -- Set Supervisor trap handler to HW default value (0)
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_STVEC_Address,
                     Data => LW_RISCV_CSR_STVEC_Register'(Base => LW_RISCV_CSR_STVEC_BASE_RST,
                                                          Mode => MODE_DIRECT));

        Program_S_And_U_Mode_Privilege(Partition_Policy.Sspm);

        Program_SCP_Lockdown(Partition_Policy.Sspm, Partition_Policy.Device_Map);

        Allow_S_Mode_To_Flush_D_Cache;
        Enable_Partition_Memory_Operations;
        Debug_Control.Configure_ICD;
        Allow_L1_Caching;
        Setup_MStatus;
        Enforce_Priv_Lockdown(Partition_Policy);
    end Initialize_Partition_Per_Policy;

    procedure Clear_MPU
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MMPUCTL_Register);
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LW_RISCV_CSR_MCFG_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCFG_Register);

        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUIDX_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUVA_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUPA_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPURNG_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUATTR_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUIDX2_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUVLD_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUDTY_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SMPUACC_Register);

        Mcfg : LW_RISCV_CSR_MCFG_Register;
        Mmpuctl : LW_RISCV_CSR_MMPUCTL_Register;
    begin
        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MCFG_Address,
                     Data => Mcfg);
        Mcfg.Mwfp_Dis := DIS_TRUE;
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCFG_Address,
                     Data => Mcfg);

        Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MMPUCTL_Address,
                     Data => Mmpuctl);

        -- Loop over all possible indices
        for I in Mmpuctl.Start_Index .. LwU7'Last loop
            -- Set Index for smpuva, smpupa, smpurng, smpuattr
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUIDX_Address,
                         Data => LW_RISCV_CSR_SMPUIDX_Register'(Wpri  => LW_RISCV_CSR_SMPUIDX_WPRI_RST,
                                                                Index => LwU7(I)));

            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUVA_Address,
                         Data => LW_RISCV_CSR_SMPUVA_Register'(Base  => LW_RISCV_CSR_SMPUVA_BASE_RST,
                                                               Wpri1 => LW_RISCV_CSR_SMPUVA_WPRI1_RST,
                                                               D     => LW_RISCV_CSR_SMPUVA_D_RST,
                                                               A     => LW_RISCV_CSR_SMPUVA_A_RST,
                                                               Wpri  => LW_RISCV_CSR_SMPUVA_WPRI_RST,
                                                               Vld   => LW_RISCV_CSR_SMPUVA_VLD_RST));
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUPA_Address,
                         Data => LW_RISCV_CSR_SMPUPA_Register'(Base => LW_RISCV_CSR_SMPUPA_BASE_RST,
                                                               Wpri => LW_RISCV_CSR_SMPUPA_WPRI_RST));

            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPURNG_Address,
                         Data => LW_RISCV_CSR_SMPURNG_Register'(Smpurng_Range => LW_RISCV_CSR_SMPURNG_SMPURNG_RANGE_RST,
                                                                Wpri          => LW_RISCV_CSR_SMPURNG_WPRI_RST));
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUATTR_Address,
                         Data => LW_RISCV_CSR_SMPUATTR_Register'(Wpri2     => LW_RISCV_CSR_SMPUATTR_WPRI2_RST,
                                                                 Vpr       => LW_RISCV_CSR_SMPUATTR_VPR_RST,
                                                                 Wpr       => LW_RISCV_CSR_SMPUATTR_WPR_RST,
                                                                 L2c_Rd    => LW_RISCV_CSR_SMPUATTR_L2C_RD_RST,
                                                                 L2c_Wr    => LW_RISCV_CSR_SMPUATTR_L2C_WR_RST,
                                                                 Coherent  => LW_RISCV_CSR_SMPUATTR_COHERENT_RST,
                                                                 Cacheable => LW_RISCV_CSR_SMPUATTR_CACHEABLE_RST,
                                                                 Wpri1     => LW_RISCV_CSR_SMPUATTR_WPRI1_RST,
                                                                 Wpri0     => LW_RISCV_CSR_SMPUATTR_WPRI0_RST,
                                                                 Sx        => LW_RISCV_CSR_SMPUATTR_SX_RST,
                                                                 Sw        => LW_RISCV_CSR_SMPUATTR_SW_RST,
                                                                 Sr        => LW_RISCV_CSR_SMPUATTR_SR_RST,
                                                                 Ux        => LW_RISCV_CSR_SMPUATTR_UX_RST,
                                                                 Uw        => LW_RISCV_CSR_SMPUATTR_UW_RST,
                                                                 Ur        => LW_RISCV_CSR_SMPUATTR_UR_RST));
        end loop;

        -- Loop over all possible indices
        for I in LwU2'Range loop
            -- Set Index for smpuvld/smpudty/smpuacc
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUIDX2_Address,
                         Data => LW_RISCV_CSR_SMPUIDX2_Register'(Wpri  => LW_RISCV_CSR_SMPUIDX2_WPRI_RST,
                                                                 Idx => LwU2(I)));
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUVLD_Address,
                         Data => LW_RISCV_CSR_SMPUVLD_Register'(Vld => LW_RISCV_CSR_SMPUVLD_VLD_RST));
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUDTY_Address,
                         Data => LW_RISCV_CSR_SMPUDTY_Register'(Dty => LW_RISCV_CSR_SMPUDTY_DTY_RST));
            Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SMPUACC_Address,
                         Data => LW_RISCV_CSR_SMPUACC_Register'(Acc => LW_RISCV_CSR_SMPUACC_ACC_RST));
        end loop;

        Mcfg.Mwfp_Dis := DIS_FALSE;
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCFG_Address,
                     Data => Mcfg);

        Riscv_Core.Inst.Fence_VMA;
    end Clear_MPU;

    procedure Clear_Partition_State
    is
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MHPMEVENT_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MHPMCOUNTER_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SSTATUS_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SIE_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCOUNTEREN_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SSCRATCH_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SSCRATCH2_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SEPC_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCAUSE_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_STVAL_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SATP_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCAUSE2_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SCFG_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SPM_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_RSP_Register);
        procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_SBPCFG_Register);

    begin
        -- Clear performance events and counters
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_3_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_4_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_5_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_6_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_7_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_8_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_9_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_10_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_11_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_12_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_13_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_14_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_15_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_16_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_17_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_18_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_19_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_20_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_21_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_22_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_23_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_24_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_25_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_26_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_27_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_28_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_29_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_30_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMEVENT_31_Address,
                     Data => LW_RISCV_CSR_MHPMEVENT_Register'(Value => LW_RISCV_CSR_MHPMEVENT_VALUE_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_3_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_4_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_5_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_6_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_7_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_8_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_9_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_10_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_11_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_12_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_13_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_14_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_15_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_16_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_17_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_18_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_19_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_20_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_21_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_22_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_23_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_24_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_25_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_26_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_27_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_28_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_29_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_30_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MHPMCOUNTER_31_Address,
                     Data => LW_RISCV_CSR_MHPMCOUNTER_Register'(Value => LW_RISCV_CSR_MHPMCOUNTER_VALUE_RST));

        -- S-Mode reset CSRs
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SSTATUS_Address,
                     Data => LW_RISCV_CSR_SSTATUS_Register'(Sd    => 0,
                                                            Wpri5 => LW_RISCV_CSR_SSTATUS_WPRI5_RST,
                                                            Uxl   => LW_RISCV_CSR_SSTATUS_UXL_RST,
                                                            Wpri4 => LW_RISCV_CSR_SSTATUS_WPRI4_RST,
                                                            Mxr   => LW_RISCV_CSR_SSTATUS_MXR_RST,
                                                            Sum   => SUM_DISABLE,
                                                            Wpri3 => LW_RISCV_CSR_SSTATUS_WPRI3_RST,
                                                            Xs    => LW_RISCV_CSR_SSTATUS_XS_OFF,
                                                            Fs    => FS_OFF,
                                                            Wpri2 => LW_RISCV_CSR_SSTATUS_WPRI2_RST,
                                                            Spp   => SPP_USER,
                                                            Wpri1 => LW_RISCV_CSR_SSTATUS_WPRI1_RST,
                                                            Spie  => SPIE_DISABLE,
                                                            Upie  => LW_RISCV_CSR_SSTATUS_UPIE_DISABLE,
                                                            Wpri0 => LW_RISCV_CSR_SSTATUS_WPRI0_RST,
                                                            Sie   => SIE_DISABLE,
                                                            Uie   => LW_RISCV_CSR_SSTATUS_UIE_DISABLE));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SIE_Address,
                     Data => LW_RISCV_CSR_SIE_Register'(Wpri2 => LW_RISCV_CSR_SIE_WPRI2_RST,
                                                        Seie  => LW_RISCV_CSR_SIE_SEIE_RST,
                                                        Ueie  => LW_RISCV_CSR_SIE_UEIE_RST,
                                                        Wpri1 => LW_RISCV_CSR_SIE_WPRI1_RST,
                                                        Stie  => LW_RISCV_CSR_SIE_STIE_RST,
                                                        Utie  => LW_RISCV_CSR_SIE_UTIE_RST,
                                                        Wpri0 => LW_RISCV_CSR_SIE_WPRI0_RST,
                                                        Ssie  => LW_RISCV_CSR_SIE_SSIE_RST,
                                                        Usie  => LW_RISCV_CSR_SIE_USIE_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCOUNTEREN_Address,
                     Data => LW_RISCV_CSR_SCOUNTEREN_Register'(Rs0 => LW_RISCV_CSR_SCOUNTEREN_RS0_RST,
                                                               Hpm => LW_RISCV_CSR_SCOUNTEREN_HPM_DISABLE,
                                                               Ir  => IR_DISABLE,
                                                               Tm  => TM_DISABLE,
                                                               Cy  => CY_DISABLE));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SSCRATCH_Address,
                     Data => LW_RISCV_CSR_SSCRATCH_Register'(Scratch => LW_RISCV_CSR_SSCRATCH_SCRATCH_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SSCRATCH2_Address,
                     Data => LW_RISCV_CSR_SSCRATCH2_Register'(Value => LW_RISCV_CSR_SSCRATCH2_VALUE_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SEPC_Address,
                     Data => LW_RISCV_CSR_SEPC_Register'(Epc => LW_RISCV_CSR_SEPC_EPC_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCAUSE_Address,
                     Data => LW_RISCV_CSR_SCAUSE_Register'(Int    => LW_RISCV_CSR_SCAUSE_INT_RST,
                                                           Wlrl   => LW_RISCV_CSR_SCAUSE_WLRL_RST,
                                                           Excode => LW_RISCV_CSR_SCAUSE_EXCODE_RST));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_STVAL_Address,
                     Data => LW_RISCV_CSR_STVAL_Register'(Value => LW_RISCV_CSR_STVAL_VALUE_RST));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SATP_Address,
                     Data => LW_RISCV_CSR_SATP_Register'(Mode => LW_RISCV_CSR_SATP_MODE_BARE,
                                                         Asid => LW_RISCV_CSR_SATP_ASID_BARE,
                                                         Ppn  => LW_RISCV_CSR_SATP_PPN_BARE));

        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCAUSE2_Address,
                     Data => LW_RISCV_CSR_SCAUSE2_Register'(Wpri  => LW_RISCV_CSR_SCAUSE2_WPRI_RST,
                                                            Cause => LW_RISCV_CSR_SCAUSE2_CAUSE_NO,
                                                            Error => LW_RISCV_CSR_SCAUSE2_ERROR_NO));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SCFG_Address,
                     Data => LW_RISCV_CSR_SCFG_Register'(Wpri4    => LW_RISCV_CSR_SCFG_WPRI4_RST,
                                                         Spostio  => SPOSTIO_FALSE,
                                                         Upostio  => UPOSTIO_FALSE,
                                                         Wpri3    => LW_RISCV_CSR_SCFG_WPRI3_RST,
                                                         Ssbc     => SSBC_FALSE,
                                                         Usbc     => USBC_FALSE,
                                                         Wpri2    => LW_RISCV_CSR_SCFG_WPRI2_RST,
                                                         Usse     => USSE_FALSE,
                                                         Wpri1    => LW_RISCV_CSR_SCFG_WPRI1_RST,
                                                         Swfp_Dis => DIS_FALSE,
                                                         Uwfp_Dis => LW_RISCV_CSR_SCFG_UWFP_DIS_FALSE));

        -- U-Mode reset CSRs
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SPM_Address,
                     Data => LW_RISCV_CSR_SPM_Register'(Wiri1 => LW_RISCV_CSR_SPM_WIRI1_RST,
                                                        Usecm => USECM_INSEC,
                                                        Wiri0 => LW_RISCV_CSR_SPM_WIRI0_RST,
                                                        Uplm  => LW_RISCV_CSR_SPM_UPLM_LEVEL0));
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_RSP_Address,
                     Data => LW_RISCV_CSR_RSP_Register'(Wpri1 => LW_RISCV_CSR_RSP_WPRI1_RST,
                                                        Ursec => URSEC_INSEC,
                                                        Wpri0 => LW_RISCV_CSR_RSP_WPRI0_RST,
                                                        Urpl  => URPL_LEVEL0));
        -- Flush BTB U-mode and S-mode entries
        Csr_Reg_Wr64(Addr => LW_RISCV_CSR_SBPCFG_Address,
                     Data => LW_RISCV_CSR_SBPCFG_Register'(Wpri1      => LW_RISCV_CSR_SBPCFG_WPRI1_RST,
                                                           Btb_Flushs => LW_RISCV_CSR_SBPCFG_BTB_FLUSHS_TRUE,
                                                           Btb_Flushu => LW_RISCV_CSR_SBPCFG_BTB_FLUSHU_TRUE,
                                                           Wpri0      => LW_RISCV_CSR_SBPCFG_WPRI0_RST));

        -- Floating point unit is disabled on all ucode. No FS related registers need to be cleaned
        -- Otherwise FFLAGS/FRM/FCSR needs to be cleaned as well.

    end Clear_Partition_State;

end Partition;
