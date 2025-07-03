--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Nv_Types; use Nv_Types;
with Riscv_Core.Csb_Reg;

package TFBIF_Cfg
is
    procedure TFBIF_Write_Transcfg (Region : NvU32;
                                    Swid   : NvU2)
    with
        Pre => Region < 8,
        Global => (In_Out => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

    procedure TFBIF_Write_Regioncfg (Region : NvU32;
                                     Vpr    : NvU1;
                                     Apert  : NvU5)
    with
        Pre => Region < 8,
        Global => (In_Out => Riscv_Core.Csb_Reg.Csb_Reg_Hw_Model.Mmio_State);

end TFBIF_Cfg;
