--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Riscv_Core.Csb_Reg;
with Nv_Types; use Nv_Types;

package FBIF_Cfg
is
    procedure FBIF_Write_Transcfg (Region : NvU32;
                                   Value  : NvU32)
    with
        Pre => Region < 8,
        Global => (Output => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

    procedure FBIF_Write_Regioncfg (Region : NvU32;
                                    Value  : NvU4)
    with
        Pre => Region < 8,
        Global => (In_Out => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State),
        Inline_Always;

end FBIF_Cfg;
