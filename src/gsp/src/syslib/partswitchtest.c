/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#if PARTITION_SWITCH_TEST
#include <riscv_csr.h>
#include <lwrtos.h>
#include <lwriscv/print.h>
#include <shlib/partswitch.h>
#include "partswitch.h"               // Interface for SHM

#define PARTITION_1_ID 1

// TODO: cleanup partition switch test and time measurement.
sysSHARED_CODE LwBool
partitionSwitchTest(void)
{
    struct switch_time_measurement *sharedMeasurement;
    struct switch_time_measurement startMeasurement, endMeasurement;
    int i;

    dbgPrintf(LEVEL_ALWAYS, "Testing partition switch in %s...\n",
              lwrtosIS_KERNEL() ? "kernel" :
              lwrtosIS_PRIV() ? "privileged task" : "task");

    for (i=0; i<5; ++i)
    {
        LwU64 offset; // we return offset to shared memory

        startMeasurement.time    = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
        startMeasurement.instret = csr_read(LW_RISCV_CSR_HPMCOUNTER_INSTRET);
        startMeasurement.cycle   = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);

        offset = partitionSwitch(
                    PARTITION_1_ID,
                    PARTSWITCH_FUNC_MEASURE_SWITCHING_TIME, // func ID that partition need to handle.
                    0,
                    0,
                    0,
                    0
                    );
        endMeasurement.time    = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
        endMeasurement.instret = csr_read(LW_RISCV_CSR_HPMCOUNTER_INSTRET);
        endMeasurement.cycle   = csr_read(LW_RISCV_CSR_HPMCOUNTER_CYCLE);

        if ((offset + sizeof(struct switch_time_measurement)) > PARTSWITCH_SHARED_CARVEOUT_SIZE)
        {
            dbgPrintf(LEVEL_ALWAYS, "Received invalid response: 0x%llx!\n",
                      offset);
            return LW_FALSE;
        }
        sharedMeasurement = (struct switch_time_measurement *)(PARTSWITCH_SHARED_CARVEOUT_VA + offset);

        dbgPrintf(LEVEL_ALWAYS, "%i: ->HS %lld ns, %lld ins, %lld cy ", i,
                  (sharedMeasurement->time   - startMeasurement.time) * 32,
                  sharedMeasurement->instret - startMeasurement.instret,
                  sharedMeasurement->cycle   - startMeasurement.cycle);
        dbgPrintf(LEVEL_ALWAYS, "->OS %lld ns, %lld ins, %lld cy ",
                  (endMeasurement.time    - sharedMeasurement->time) * 32,
                  endMeasurement.instret - sharedMeasurement->instret,
                  endMeasurement.cycle   - sharedMeasurement->cycle);
        dbgPrintf(LEVEL_ALWAYS, "TOT %lld ns, %lld ins, %lld cy\n",
                  (endMeasurement.time    - startMeasurement.time) * 32,
                  endMeasurement.instret - startMeasurement.instret,
                  endMeasurement.cycle   - startMeasurement.cycle);
    }
    dbgPrintf(LEVEL_ALWAYS, "Partition switch test succeeded.\n");
    return LW_TRUE;
}
#endif // PARTITION_SWITCH_TEST
