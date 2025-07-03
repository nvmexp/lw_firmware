-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core;
with Riscv_Core.Csb_Reg;
with Types;                     use Types;
with Policy_Types;              use Policy_Types;
with Nv_Types;                  use Nv_Types;
with Dev_Riscv_Csr_64;          use Dev_Riscv_Csr_64;
with Error_Handling;            use Error_Handling;
with SBI_Shutdown;              use SBI_Shutdown;
with SBI_Switch_To;             use SBI_Switch_To;
with SBI_Release_Priv_Lockdown; use SBI_Release_Priv_Lockdown;
with SBI_Update_Tracectl;       use SBI_Update_Tracectl;
with SBI_FBIF_Cfg;              use SBI_FBIF_Cfg;
with SBI_TFBIF_Cfg;             use SBI_TFBIF_Cfg;
with Separation_Kernel;         use Separation_Kernel;
with Separation_Kernel.Policies; use Separation_Kernel.Policies;
with SBI_Types;                 use SBI_Types;
with SBI_Invalid_Extension;     use SBI_Invalid_Extension;

package body SBI
is
    procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MTIMECMP_Register);
    procedure Csr_Reg_Set64 is new Riscv_Core.Rv_Csr.Set64_Generic (Generic_Csr => LW_RISCV_CSR_MIE_Register);
    procedure Csr_Reg_Clr64 is new Riscv_Core.Rv_Csr.Clr64_Generic (Generic_Csr => LW_RISCV_CSR_MIP_Register);

    procedure Dispatch(arg0        : in out NvU64;
                       arg1        : in out NvU64;
                       arg2        : NvU64;
                       arg3        : NvU64;
                       arg4        : NvU64;
                       arg5        : NvU64;
                       functionId  : NvU64;
                       extensionId : NvU64)
    is
        Mtimecmp : LW_RISCV_CSR_MTIMECMP_Register;
        SBI_RC   : SBI_Return_Type := SBI_Return_Type'(Error => SBI_SUCCESS, Value => 0);
        SBI_Access : Plcy_SBI_Access := Partition_Policies(Get_Current_Partition_ID).SBI_Access_Config;
    begin

        case extensionId is
            when NvU64(SBI_EXTENSION_SET_TIMER) =>
                if functionId /= 0 then
                    arg0 := SBI_Error_Code_To_NvU64(SBI_ERR_INVALID_PARAM);
                    arg1 := 0;
                    return;
                end if;

                -- Write to mtimecmp
                Mtimecmp.Time := arg0;
                Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MTIMECMP_Address,
                             Data => Mtimecmp);

                -- Clear Supervisor timer interrupt pending
                Csr_Reg_Clr64(Addr => LW_RISCV_CSR_MIP_Address,
                              Data => LW_RISCV_CSR_MIP_Register'(Wiri3 => LW_RISCV_CSR_MIP_WIRI3_RST,
                                                                 Meip  => LW_RISCV_CSR_MIP_MEIP_RST,
                                                                 Wiri2 => LW_RISCV_CSR_MIP_WIRI2_RST,
                                                                 Seip  => LW_RISCV_CSR_MIP_SEIP_RST,
                                                                 Ueip  => LW_RISCV_CSR_MIP_UEIP_RST,
                                                                 Mtip  => LW_RISCV_CSR_MIP_MTIP_RST,
                                                                 Wiri1 => LW_RISCV_CSR_MIP_WIRI1_RST,
                                                                 Stip  => 1,
                                                                 Utip  => LW_RISCV_CSR_MIP_UTIP_RST,
                                                                 Msip  => LW_RISCV_CSR_MIP_MSIP_RST,
                                                                 Wiri0 => LW_RISCV_CSR_MIP_WIRI0_RST,
                                                                 Ssip  => LW_RISCV_CSR_MIP_SSIP_RST,
                                                                 Usip  => LW_RISCV_CSR_MIP_USIP_RST));

                -- Re-enable machine timer interrupt enable
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

            when NvU64(SBI_EXTENSION_LWIDIA) =>
                -- Default rc to be denied and have each SBI call to go through access control
                -- Switch_To is always allowed
                SBI_RC.Error := SBI_ERR_DENIED;
                case functionId is
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_SWITCH_PARTITION) =>
                        Switch_To (arg0, arg1, arg2, arg3, arg4, arg5);
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_RELEASE_PRIV_LOCKDOWN) =>
                        if SBI_Access.Release_Priv_Lockdown = PERMITTED then
                            Release_Priv_Lockdown(SBI_RC);
                        end if;
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_UPDATE_TRACECTL) =>
                        if SBI_Access.TraceCtl = PERMITTED then
                            Update_Tracectl (arg0, SBI_RC);
                        end if;
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_FBIF_TRANSCFG) =>
                        if SBI_Access.FBIF_TransCfg = PERMITTED then
                            FBIF_Transcfg (arg0, arg1, SBI_RC);
                        end if;
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_FBIF_REGIONCFG) =>
                        if SBI_Access.FBIF_RegionCfg = PERMITTED then
                            FBIF_Regioncfg (arg0, arg1, SBI_RC);
                        end if;
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_TFBIF_TRANSCFG) =>
                        if SBI_Access.TFBIF_TransCfg = PERMITTED then
                            TFBIF_Transcfg (arg0, arg1, SBI_RC);
                        end if;
                    when NvU64(SBI_LWIDIA_EXTENSION_FUNC_TFBIF_REGIONCFG) =>
                        if SBI_Access.TFBIF_RegionCfg = PERMITTED then
                            TFBIF_Regioncfg (arg0, arg1, arg2, SBI_RC);
                        end if;
                    when others =>
                        Throw_Critical_Error(Pz_Code  => TRAP_HANDLER_PHASE,
                                             Err_Code => SBI_LWIDIA_INVALID_FUNCTION_ID);
                end case;

            when NvU64(SBI_EXTENSION_SHUTDOWN) =>
                if functionId = 0 then
                    Shutdown;
                    raise Program_Error with "This should never happen!";
                end if;
                SBI_RC.Error := SBI_ERR_INVALID_PARAM;

            when others =>
                Handle_Invalid_Extension(arg0, arg1, extensionId, functionId);
                return;

        end case;
        arg0 := SBI_Error_Code_To_NvU64(SBI_RC.Error);
        arg1 := SBI_RC.Value;

    end Dispatch;

end SBI;
