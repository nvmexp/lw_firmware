-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;         use Lw_Types;
with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Prgnlcl;      use Dev_Prgnlcl;

package Typecast
is

    function BCR_DMACFG_SEC_to_WPR(Bcr_DmaCfg_Sec : LW_PRGNLCL_RISCV_BCR_DMACFG_SEC_Register) return LwU5 is
        (LwU5(Bcr_DmaCfg_Sec.Wprid))
    with Inline;

end Typecast;