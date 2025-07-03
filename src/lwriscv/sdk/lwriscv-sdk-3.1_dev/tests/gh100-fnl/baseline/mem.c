/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#include "tests.h"
#include "util.h"

static const uint32_t imem_expected_size=LWRISCV_IMEM_SIZE;
static const uint32_t dmem_expected_size=LWRISCV_DMEM_SIZE;

#if LWRISCV_EMEM_SIZE > 0 && (!LWRISCV_HAS_PRI && ENGINE_REG(_HWCFG) == 0)
#error Engine configuration states it has EMEM, but no HWCFG2 available.
#endif

uint64_t testMemConfig(void)
{
    uint32_t hwcfg3;
    uint32_t v;

    printf("Testing memory configuration.\n");
    hwcfg3 = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    // Check memory and cache size
    printf("IMEM=0x%x ? ", imem_expected_size);
    v = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _IMEM_TOTAL_SIZE, hwcfg3) << 8;
    if (imem_expected_size == v)
        printf("OK\n");
    else
    {
        printf("FAIL => 0x%x\n", v);
        return MAKE_DEBUG_NOSUB(TestId_Hwcfg, 0, imem_expected_size, v);
    }

    printf("DMEM=0x%x ? ", dmem_expected_size);
    v = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, hwcfg3) << 8;
    if (dmem_expected_size == v)
        printf("OK\n");
    else
    {
        printf("FAIL => 0x%x\n", v);
        return MAKE_DEBUG_NOSUB(TestId_Hwcfg, 1, dmem_expected_size, v);
    }

#if LWRISCV_EMEM_SIZE > 0
    {
        uint32_t reg = priRead(ENGINE_REG(_HWCFG));

        printf("EMEM=0x%x ? ", LWRISCV_EMEM_SIZE);

        v = (reg & 0xFF) << 8;
        if (v == LWRISCV_EMEM_SIZE)
            printf("OK\n");
        else
        {
            printf("FAIL => 0x%x\n", v);
            return MAKE_DEBUG_NOSUB(TestId_Hwcfg, 2, LWRISCV_EMEM_SIZE, v);
        }
    }
#endif

    return PASSING_DEBUG_VALUE;
}
