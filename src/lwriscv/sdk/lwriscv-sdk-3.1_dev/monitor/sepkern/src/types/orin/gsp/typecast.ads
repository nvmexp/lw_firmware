-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Nv_Types;         use Nv_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;      use Dev_Prgnlcl;

package Typecast
is

    function BCR_DMACFG_SEC_to_WPR(Bcr_DmaCfg_Sec : LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register) return NvU5 is
        (NvU5(Bcr_DmaCfg_Sec.Wprid))
    with Inline;

end Typecast;