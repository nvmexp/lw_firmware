/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "libriscv.h"
#include "libos-config.h"
#include "peregrine-headers.h"
#include "lwmisc.h"
#include "lwtypes.h"
#include <assert.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <memory>

extern "C" {
#include "libelf.h"
#include "libdwarf.h"
}
#include "riscv-core.h"

#include "kernel/lwriscv-2.0/sbi.h"

static void napotDecode(LwU64 encoded, LwU64 & begin, LwU64 & end)
{
    // NAPOT addresses end in 0111...
    // 0   -> 8 byte
    // 01  -> 16 byte
    // 011 -> 32 byte
    
    // Add one to set the 0 and clear the ones
    LwU64 mask = encoded + 1;

    // Find the lowest bit set
    mask = mask & (-mask);

    // Mask out the NAPOT bits at the bottom
    begin = encoded &~ (mask - 1);

    // Shift in the size
    mask *= 8;

    // Compute the end
    end = begin + mask;
}

void riscv_core::sbi()
{
    LwU64 sbi_extension_id = regs[RISCV_A7];
    LwU64 sbi_function_id  = regs[RISCV_A6];

    // @bug: Need sbi_function_id assigned
    if (sbi_extension_id == 0x90001EB && sbi_function_id == 1)
    {
        LwU64 targetPartitionId = regs[RISCV_A0];
        LwU64 entry = regs[RISCV_A1];
        printf("SK-Model: SeparationKernelEntrypointSet(%d, %llx)\n", targetPartitionId, entry);
        assert(targetPartitionId < 64);
        partition[targetPartitionId].entry = entry;

        regs[RISCV_A0] = 0 /* SBI_SUCCESS */;
        regs[RISCV_A1] = rand_sbi();
        regs[RISCV_A2] = rand_sbi();
        regs[RISCV_A3] = rand_sbi();
        regs[RISCV_A4] = rand_sbi();
        regs[RISCV_A5] = rand_sbi();
        regs[RISCV_A6] = rand_sbi();
        regs[RISCV_A7] = rand_sbi();
    }
    else if (sbi_extension_id == 0x90001EB && sbi_function_id == 3)
    {
        LwU64 targetPartitionId = regs[RISCV_A0];
        assert(targetPartitionId < 64);

        LwU64 localPmpIndex = regs[RISCV_A1];
        assert(localPmpIndex < 16);

        LwU64 targetPmpEntryIndex = regs[RISCV_A2];
        assert(targetPmpEntryIndex < 16);

        LwU64 pmpAccessMode = regs[RISCV_A3];
        assert((pmpAccessMode & (PMP_ACCESS_MODE_READ | PMP_ACCESS_MODE_WRITE | PMP_ACCESS_MODE_EXELWTE)) == pmpAccessMode);

        auto & sourcePmpEntry = partition[partitionIndex].pmp[localPmpIndex];
        auto & targetPmpEntry = partition[targetPartitionId].pmp[targetPmpEntryIndex];

        LwU64 begin = regs[RISCV_A4];
        LwU64 size = regs[RISCV_A5];
        LwU64 end = begin + size;

        // Size must be power of 2, and greater than 4
        assert((size > 4) && ((size & (size - 1)) == 0));

        // Address must be size aligned
        assert((begin % size) == 0);

        printf("SK-Model: SeparationKernelPmpMap(local-id:%d, target-partition:%d, target-id:%d, target-access:%d, %llx-%llx)\n", 
            localPmpIndex, targetPartitionId, targetPmpEntryIndex, pmpAccessMode, begin, end);
        printf("SK-Model: %d %llx-%llx\n",sourcePmpEntry.pmpAccess, sourcePmpEntry.begin, sourcePmpEntry.end);

        // Validate subset property
        // @bug: Validate we're partition 0 or trusted.
        assert(begin >= sourcePmpEntry.begin && end <= sourcePmpEntry.end);
        assert((sourcePmpEntry.pmpAccess & pmpAccessMode) == pmpAccessMode);

        // Overwrite target PMP entry
        targetPmpEntry.begin = begin;
        targetPmpEntry.end = end;
        targetPmpEntry.pmpAccess = pmpAccessMode;

        regs[RISCV_A0] = 0 /* SBI_SUCCESS */;
        regs[RISCV_A1] = rand_sbi();
        regs[RISCV_A2] = rand_sbi();
        regs[RISCV_A3] = rand_sbi();
        regs[RISCV_A4] = rand_sbi();
        regs[RISCV_A5] = rand_sbi();
        regs[RISCV_A6] = rand_sbi();
        regs[RISCV_A7] = rand_sbi();        
    }    
    // sbi_set_timer
    else if (sbi_extension_id == 0 && sbi_function_id == 0)
    {
        mtimecmp = regs[RISCV_A0];

#if LIBOS_CONFIG_RISCV_S_MODE
        if (has_supervisor_mode)
            xip &= ~REF_NUM64(LW_RISCV_CSR_SIP_STIP, 1);
        else
#endif
            xip &= ~REF_NUM64(LW_RISCV_CSR_MIP_MTIP, 1);

        regs[RISCV_A0] = 0 /* SBI_SUCCESS */;
        regs[RISCV_A1] = rand_sbi();
        regs[RISCV_A2] = rand_sbi();
        regs[RISCV_A3] = rand_sbi();
        regs[RISCV_A4] = rand_sbi();
        regs[RISCV_A5] = rand_sbi();
        regs[RISCV_A6] = rand_sbi();
        regs[RISCV_A7] = rand_sbi();
    }
    // SeparationKernelShutdown
    else if (sbi_extension_id == 8 && sbi_function_id == 0)
    {
        stopped = true;
        shutdown = true;
        return;
    }
    // SeparationKernelPartitionSwitch
    else if (sbi_extension_id == 0x090001EB && sbi_function_id == 0)
    {
        // Save away partition switch arguments
        LwU64 targetPartition = regs[RISCV_A5];
        LwU64 sourcePartition = partitionIndex;
        LwU64 a0 = regs[RISCV_A0];
        LwU64 a1 = regs[RISCV_A1];
        LwU64 a2 = regs[RISCV_A2];
        LwU64 a3 = regs[RISCV_A3];
        LwU64 a4 = regs[RISCV_A4];

        printf("SK-Model: SeparationKernelPartitionSwitch target:%d entry:%llx args:(%llx, %llx, %llx, %llx, %llx)\n", targetPartition,  partition[targetPartition].entry, a0, a1, a2, a3, a4);

        // Set the current partition index
        partitionIndex = targetPartition;

        // Reset the processor core
        soft_reset();

        // Restore the registers 
        regs[RISCV_A0] = a0;
        regs[RISCV_A1] = a1;
        regs[RISCV_A2] = a2;
        regs[RISCV_A3] = a3;
        regs[RISCV_A4] = a4;
        regs[RISCV_A5] = sourcePartition;  //@bug: Verify A5 is SK's calling partition
        regs[RISCV_A6] = rand_sbi();
        regs[RISCV_A7] = rand_sbi();
    }
    else
    {
        printf("SK-Model: Unknown SBI %llx/%llx\n", sbi_extension_id, sbi_function_id);
        assert(0 && "Unsupported SBI");
        regs[RISCV_A0] = -2 /* SBI_NOT_SUPPORTED */;
        regs[RISCV_A1] = rand_sbi();
        regs[RISCV_A2] = rand_sbi();
        regs[RISCV_A3] = rand_sbi();
        regs[RISCV_A4] = rand_sbi();
        regs[RISCV_A5] = rand_sbi();
        regs[RISCV_A6] = rand_sbi();
        regs[RISCV_A7] = rand_sbi();
    }

    // Scrub temporary registers that SBI is allowed to overwrite
    regs[RISCV_T0] = rand_sbi();
    regs[RISCV_T1] = rand_sbi();
    regs[RISCV_T2] = rand_sbi();
    regs[RISCV_T3] = rand_sbi();
    regs[RISCV_T4] = rand_sbi();
    regs[RISCV_T5] = rand_sbi();
    regs[RISCV_T6] = rand_sbi();
    regs[RISCV_RA] = rand_sbi();
}
