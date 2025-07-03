/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vm.c
 * @brief   Virtual Memory area allocator
 *
 * This is allocator for VA spaces (that is requesting of virtual address with
 * no underlying storage).
 */

/* ------------------------ LW Includes ------------------------------------ */
#include <lwmisc.h>
#include <lwtypes.h>

// Register headers
#include <lwriscv-mpu.h>

/* ------------------------ Module Includes -------------------------------- */
#include <lwriscv/print.h>
#include "drivers/drivers.h"

// vm management
sysKERNEL_DATA static LwU64 vaBase = 0;
sysKERNEL_DATA static LwU64 vaPagesUsed = 0;
sysKERNEL_DATA static LwU64 vaPagesMax = 0;

sysKERNEL_CODE FLCN_STATUS vmInit(LwUPtr base, LwU64 pageCount)
{
    vaBase = base;
    vaPagesUsed = 0;
    vaPagesMax = pageCount;

    if ((vaBase > 0) && (vaPagesMax > 0))
    {
        return FLCN_OK;
    }
    else
    {
        return FLCN_ERR_NO_FREE_MEM;
    }
}

// Returns (allocated) piece of VA space or 0
sysKERNEL_CODE LwUPtr vmAllocateVaSpace(LwU64 pageCount)
{
    LwU64 space;

    if ((vaPagesUsed + pageCount) >= vaPagesMax)
    {
        dbgPrintf(LEVEL_CRIT,
                  "Run out of dynamic VA space. Increase engineDYNAMIC_VA_SIZE.\n");
        return 0;
    }

    space = vaBase + (vaPagesUsed * LW_RISCV_CSR_MPU_PAGE_SIZE);

    vaPagesUsed += pageCount;

    return space;
}

void vmFreeSpace(LwUPtr virt, LwU64 pageCount)
{
    // MK TODO: Implement me
}
