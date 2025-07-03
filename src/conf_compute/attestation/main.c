/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
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
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>
#include <lwriscv/gcc_attrs.h>

#include "partitions.h"
#include "misc.h"

// C "entry" to partition, has stack and liblwriscv
int partition_attestation_main(GCC_ATTR_UNUSED LwU64 arg0,
                               GCC_ATTR_UNUSED LwU64 arg1,
                               GCC_ATTR_UNUSED LwU64 arg2,
                               GCC_ATTR_UNUSED LwU64 arg3,
                               GCC_ATTR_UNUSED LwU64 partition_origin)
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

    pPrintf("Attestation shutting down\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return 0;
}


void partition_attestation_trap_handler(void)
{
    pPrintf("In Partition attestation trap handler at 0x%lx, cause 0x%lx val 0x%lx\n",
            csr_read(LW_RISCV_CSR_SEPC),
            csr_read(LW_RISCV_CSR_SCAUSE),
            csr_read(LW_RISCV_CSR_STVAL));

    ccPanic();
}
