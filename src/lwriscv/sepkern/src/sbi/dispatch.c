/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include <riscv_sbi.h>
#include "sepkern.h"

/*!
 * @brief SBI call handler.
 *
 * @param[in]   arg0    SBI call argument 0.
 * @param[in]   arg1    SBI call argument 1.
 * @param[in]   arg2    SBI call argument 2.
 * @param[in]   arg3    SBI call argument 3.
 * @param[in]   arg4    SBI call argument 4.
 * @param[in]   arg5    SBI call argument 5.
 * @param[in]   funcId  SBI call function ID.
 * @param[in]   extId   SBI call extension ID.
 *
 * @return Standardized SBI return structure indicating an error code and a return value.
 */
SBI_RETURN_VALUE sbiDispatch
(
    LwU64 arg0,
    LwU64 arg1,
    LwU64 arg2,
    LwU64 arg3,
    LwU64 arg4,
    LwU64 arg5,
    LwS32 funcId,
    LwS32 extId
)
{
    SBI_RETURN_VALUE ret = {
        .error = SBI_ERR_NOT_SUPPORTED,
        .value = 0,
    };

    switch (extId)
    {
        case SBI_EXTENSION_SET_TIMER:
            // Adjust timer compare
            csr_write(LW_RISCV_CSR_MTIMECMP, arg0);
            // Clear supervisor timer interrupt pending
            csr_clear(LW_RISCV_CSR_MIP, DRF_NUM64(_RISCV_CSR, _MIP, _STIP, 1));
            // Re-enable machine timer interrupt enable
            csr_set(LW_RISCV_CSR_MIE, DRF_NUM64(_RISCV_CSR, _MIE, _MTIE, 1));

            ret.error = SBI_SUCCESS;
            break;
        case SBI_EXTENSION_SHUTDOWN:
            shutdown();

            // If this returned, it failed
            ret.error = SBI_ERR_FAILURE;
            break;
        default:
            break;
    }

#if HALT_ON_SBI_FAILURE
    if (ret.error != SBI_SUCCESS)
    {
        halt();
    }
#endif
    return ret;
}
