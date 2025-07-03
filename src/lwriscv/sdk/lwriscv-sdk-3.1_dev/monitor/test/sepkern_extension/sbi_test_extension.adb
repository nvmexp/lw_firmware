--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Types;              use Types;
with Lw_Types;           use Lw_Types;
with Error_Handling;     use Error_Handling;
with SBI_Switch_To;      use SBI_Switch_To;
with SBI_Types;          use SBI_Types;
with Ada.Unchecked_Colwersion;
with Lw_Riscv_Address_Map;     use Lw_Riscv_Address_Map;
with System.Machine_Code; use System.Machine_Code;
with Riscv_Core.Rv_Csr;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;

package body SBI_Ilwalid_Extension
is
    procedure Test_Read_Csr(arg0 : in out LwU64;
                            arg1 : out LwU64)
    is
        procedure Csr_Reg_Rd64 is new Riscv_Core.Rv_Csr.Rd64_Generic (Generic_Csr => LwU64);
    begin
        case arg0 is
            when LwU64(LW_RISCV_CSR_MTIMECMP_Address) => 
                Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MTIMECMP_Address,
                             Data => arg1);
                arg0 := SBI_Error_Code_To_LwU64(SBI_SUCCESS);
            when LwU64(LW_RISCV_CSR_MMPUCTL_Address) =>
                Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MMPUCTL_Address,
                             Data => arg1);
                arg0 := SBI_Error_Code_To_LwU64(SBI_SUCCESS);
            when LwU64(LW_RISCV_CSR_PMPCFG2_Address) =>
                Csr_Reg_Rd64(Addr => LW_RISCV_CSR_PMPCFG2_Address,
                             Data => arg1);
                arg0 := SBI_Error_Code_To_LwU64(SBI_SUCCESS);
            when LwU64(LW_RISCV_CSR_MEXTPMPCFG0_Address) =>
                Csr_Reg_Rd64(Addr => LW_RISCV_CSR_MEXTPMPCFG0_Address,
                             Data => arg1);
                arg0 := SBI_Error_Code_To_LwU64(SBI_SUCCESS);
            when others =>
                arg0 := SBI_Error_Code_To_LwU64(SBI_ERR_ILWALID_PARAM);
        end case;
    end Test_Read_Csr;

    procedure Test_Extension(arg0        : in out LwU64;
                             arg1        : in out LwU64;
                             functionId  : LwU64)
    is
        EXTENSION_READ_CSR : constant LwU64 := 0;
    begin
        case functionId is
            when EXTENSION_READ_CSR =>
                Test_Read_Csr(arg0, arg1);
            when others =>
                arg0 := SBI_Error_Code_To_LwU64(SBI_ERR_ILWALID_PARAM);
                arg1 := functionId;
        end case;

    end Test_Extension;

    procedure Handle_Ilwalid_Extension(arg0        : in out LwU64;
                                       arg1        : in out LwU64;
                                       extensionId : LwU64;
                                       functionId  : LwU64)
    is
        TEST_EXTENSION_ID : constant LwU64 := 16#74657374#;
    begin
        case extensionId is
            when TEST_EXTENSION_ID =>
                Test_Extension(arg0, arg1, functionId);
            when others =>
                arg0 := SBI_Error_Code_To_LwU64(SBI_ERR_NOT_SUPPORTED);
                arg1 := extensionId;
        end case;

    end Handle_Ilwalid_Extension;

end SBI_Ilwalid_Extension;
