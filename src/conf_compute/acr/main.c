/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include <lwmisc.h>
#include <liblwriscv/print.h>
#include <liblwriscv/io.h>
#include <liblwriscv/ssp.h>
#include <lwriscv/status.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include "partitions.h"
#include "rpc.h"
#include "misc.h"

extern void  acrPartitionEntry(LwU64 partition_origin);
extern void  acrExceptionHandler(void);

// C "entry" to partition, has stack and liblwriscv
int partition_acr_main(GCC_ATTR_UNUSED LwU64 arg0,
                       GCC_ATTR_UNUSED LwU64 arg1,
                       GCC_ATTR_UNUSED LwU64 arg2,
                       GCC_ATTR_UNUSED LwU64 arg3,
                       LwU64 partition_origin)
{
    LwU64 data64;

    data64 = csr_read(LW_RISCV_CSR_SRSP);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRPL, _LEVEL3, data64);
    data64 = FLD_SET_DRF64(_RISCV_CSR, _SRSP, _SRSEC, _SEC, data64);
    csr_write(LW_RISCV_CSR_SRSP, data64);

    if (sspGenerateAndSetCanaryWithInit() != LWRV_OK)
    {
        ccPanic();
    }

    if (partition_origin == PARTITION_ID_INIT)
    {
#if CC_IS_RESIDENT
        pPrintf("Switching to RM partition.\n");
        partitionSwitchNoreturn(0, 0, 0, 0, PARTITION_ID_ACR, PARTITION_ID_RM);
#else // CC_IS_RESIDENT
        pPrintf("Processing GSP-RM boot.\n");
        acrPartitionEntry(partition_origin);
#endif // CC_IS_RESIDENT
    } else {
        pPrintf("ACR called from partition %llu.\n", partition_origin);

        acrPartitionEntry(partition_origin);
    }

    // We should not reach this code ever
    pPrintf("ACR shutting down\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
    return 0;
}
