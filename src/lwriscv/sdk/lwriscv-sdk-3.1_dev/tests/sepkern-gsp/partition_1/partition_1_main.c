/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdint.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/csr.h>
#include <lwriscv/sbi.h>

#define PARTITION_0_ID 0

struct switch_time_measurement {
    uint64_t time;
    uint64_t instret;
    uint64_t cycle;
};

static struct switch_time_measurement measurements __attribute__((section(".partition_shared_data.measurements")));
extern uint64_t __partition_shared_data_start[];
/*
 * arg0-arg4 are parameters passed by partition 0
 */
void partition_1_main(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    SBI_RETURN_VALUE ret;
    measurements.time    = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
    measurements.instret = csr_read(LW_RISCV_CSR_HPMCOUNTER_INSTRET);
    measurements.cycle   = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);

    /*
     * The following 'switch_to' SBI call allows up to 5 opaque parameters to be passed using a0-a4 argument registers
     * a5 argument register is used for partition ID we're switching to
     */
    ret = sbicall6(SBI_EXTENSION_LWIDIA,
                   SBI_LWFUNC_FIRST,
                   ((uint64_t)&measurements) - (uint64_t)&__partition_shared_data_start,
                   0,
                   0,
                   0,
                   0,
                   PARTITION_0_ID);


    printf("Partition 1 critical error: returned from SBI call. Error: 0x%llx, value: 0x%llx. Shutting down\n",
           ret.error, ret.value);
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);
}

void partition_1_trap_handler(void) 
{
    printf("In Partition 1 trap handler at %p, cause 0x%llx val 0x%llx\n",
           csr_read(LW_RISCV_CSR_SEPC),
           csr_read(LW_RISCV_CSR_SCAUSE),
           csr_read(LW_RISCV_CSR_STVAL));

    printf("Shutting down\n");
    sbicall0(SBI_EXTENSION_SHUTDOWN, SBI_LWFUNC_FIRST);

    return;
}
