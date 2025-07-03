/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file: config_ls.c handles and initializes security
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "config_ls.h"
#include "config.h"
#include "reg_access.h"
#include <lwtypes.h>
#include "rmlsfm.h"
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */

static LwBool
_selwrityIsLsInitializationRequired(void)
{
    // TODO: Add check against BR programmed values of MSPM
    // if WPR ID is not set then manifest with LS permissions should not be used
    LwU32 wprId = 0;

    wprId = intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC);
    wprId = DRF_VAL(_PRGNLCL, _RISCV_BCR_DMACFG_SEC, _WPRID, wprId);

    return wprId == LSF_WPR_EXPECTED_REGION_ID;
}

static void
_selwrityInitCorePrivilege(void)
{
    // TODO: Add check against BR programmed value
    // hardcode to level 2 for now
    LwU64 data64 = csr_read(LW_RISCV_CSR_MSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _UPLM, _LEVEL2, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _SPLM, _LEVEL2, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _MPLM, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_MSPM, data64);

    data64 = csr_read(LW_RISCV_CSR_SSPM);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SSPM, _SPLM, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SSPM, data64);

    data64 = csr_read(LW_RISCV_CSR_RSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _RSP, _URPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_RSP, data64);

    data64 = csr_read(LW_RISCV_CSR_MRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL2, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MRSP, _SRPL, _LEVEL2, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _MRSP, _MRPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_MRSP, data64);

    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL2, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);
}

 void
_selwrityInitPLM(void)
{
    LwU32 data = 0;
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_SCTL, _RESET_LVLM_EN, _TRUE, 0);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_SCTL, _STALLREQ_CLR_EN, _TRUE, data);
    data = FLD_SET_DRF(_PRGNLCL, _FALCON_SCTL, _AUTH_EN, _TRUE, data);
    intioWrite(LW_PRGNLCL_FALCON_SCTL, data);

    data = FLD_SET_DRF(_PRGNLCL, _FALCON_SCTL, _LSMODE, _TRUE, data);
    data = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_SCTL, _LSMODE_LEVEL, 0x2, data);
    intioWrite(LW_PRGNLCL_FALCON_SCTL, data);

}

/*
 * @brief: Initializes LS security.
 * TODO: replace LwU32 with meaningful error codes
 */
LwU32
selwrityInitLS(void)
{
    if(!_selwrityIsLsInitializationRequired())
    {
        // TODO: replace with proper status code
        return 0;
    }

    _selwrityInitCorePrivilege();
    _selwrityInitPLM();

    return 0;
}
