/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   config_pmu.c
 * @brief  PMU specific configuration
 */

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include <lwtypes.h>
#include "sepkern.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global variables ------------------------------- */
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Local Functions  ------------------------------- */
void engineConfigApply(void)
{
    LwU32 reg = 0;

    if (FLD_TEST_DRF64(_RISCV_CSR, _MSPM, _MSECM, _SEC, csr_read(LW_RISCV_CSR_MSPM)))
    {
        //
        // Give PMU S-mode access to secure SCP commands if MSECM has already been set
        // (by the manifest). We have to set SSECM here because it is not writeable from S-mode.
        //
        csr_set(LW_RISCV_CSR_MSPM, DRF_DEF64(_RISCV_CSR, _MSPM, _SSECM, _SEC));
        csr_set(LW_RISCV_CSR_MRSP, DRF_DEF64(_RISCV_CSR, _MRSP, _SRSEC, _SEC));
    }

    // Release PMB lock (it is set up correctly now by IOPMP)
    intioWrite(LW_PRGNLCL_FALCON_LOCKPMB,
                DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _IMEM, _LOCK) |
                DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _DMEM, _UNLOCK)
                // @todo  yitianc: check why _FALCON_LOCKPMB not there on manual
                // | DRF_DEF(_PRGNLCL, _FALCON_LOCKPMB, _EXT2MEM, _LOCK)
                );

    // Block access to PMB group
    reg = intioRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(LW_PRGNLCL_DEVICE_MAP_GROUP_PMB / 8));
    reg = FLD_IDX_SET_DRF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _READ, LW_PRGNLCL_DEVICE_MAP_GROUP_PMB, _DISABLE, reg);
    reg = FLD_IDX_SET_DRF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _WRITE, LW_PRGNLCL_DEVICE_MAP_GROUP_PMB, _DISABLE,reg);
    reg = FLD_IDX_SET_DRF(_PRGNLCL, _RISCV_DEVICEMAP_RISCVMMODE, _LOCK, LW_PRGNLCL_DEVICE_MAP_GROUP_PMB, _LOCKED, reg);
    intioWrite(LW_PRGNLCL_RISCV_DEVICEMAP_RISCVMMODE(LW_PRGNLCL_DEVICE_MAP_GROUP_PMB / 8), reg);

    // Give PMU access to performance counters
    csr_write(LW_RISCV_CSR_MCOUNTEREN,
              DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _RS0, _RST) |
              DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _HPM, _DISABLE) |
              DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _IR, _ENABLE) |
              DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _TM, _ENABLE) |
              DRF_DEF64(_RISCV_CSR, _MCOUNTEREN, _CY, _ENABLE));

    //
    // Disable idle check during context switch - it is required so RM can do
    // context switch.
    //
    reg = intioRead(LW_PRGNLCL_FALCON_DEBUG1);
    reg = FLD_SET_DRF(_PRGNLCL, _FALCON_DEBUG1, _CTXSW_MODE1, _BYPASS_IDLE_CHECKS, reg);
    intioWrite(LW_PRGNLCL_FALCON_DEBUG1, reg);

    // Enable CTXSW FSM
    reg = intioRead(LW_PRGNLCL_FALCON_ITFEN);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _ITFEN, _CTXEN, _ENABLE, reg);
    intioWrite(LW_PRGNLCL_FALCON_ITFEN, reg);
}
