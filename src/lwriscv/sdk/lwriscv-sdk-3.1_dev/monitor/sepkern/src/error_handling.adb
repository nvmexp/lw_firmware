-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Prgnlcl;        use Dev_Prgnlcl;
with Riscv_Core.Inst;

package body Error_Handling
is

    procedure Throw_Critical_Error (Pz_Code : Phase_Codes; Err_Code : Error_Codes)
    is
        procedure Csb_Reg_Wr32 is new Riscv_Core.Csb_Reg.Wr32_Generic (Generic_Register => SK_ERROR_Register);
    begin
        Csb_Reg_Wr32(Addr => LW_PRGNLCL_FALCON_MAILBOX0_Address,
                     Data => SK_ERROR_Register'(Phase    => Pz_Code,
                                                Err_Code => Err_Code,
                                                Reserved => 0));
        Riscv_Core.Inst.Halt;
    end Throw_Critical_Error;

    procedure Last_Chance_Handler (Source_Location : System.Address; Line : Integer)
    is
        pragma Unreferenced (Line, Source_Location);
    begin
        Riscv_Core.Inst.Halt;
    end Last_Chance_Handler;

end Error_Handling;
