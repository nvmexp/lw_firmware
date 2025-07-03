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
#include <stdbool.h>
#include <stddef.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <lwriscv/fence.h>
#include <liblwriscv/shutdown.h>
#include "tests.h"
#include "util.h"

static const uint32_t imem_size=LWRISCV_IMEM_SIZE;
static const uint32_t dmem_size=LWRISCV_DMEM_SIZE;
static const uint32_t emem_size=LWRISCV_EMEM_SIZE;

#if LWRISCV_EMEM_SIZE > 0 && (!LWRISCV_HAS_PRI && ENGINE_REG(_HWCFG) == 0)
#error Engine configuration states it has EMEM, but no HWCFG2 available.
#endif


uint64_t testMemAccess(void)
{
    uint64_t validOffsets[] = {
        LW_RISCV_AMAP_IMEM_START,
        LW_RISCV_AMAP_IMEM_START + imem_size - 8,
        LW_RISCV_AMAP_DMEM_START,
        LW_RISCV_AMAP_DMEM_START + dmem_size - 8,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START : 0,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START + emem_size - 8 : 0,
        0
    };
    uint64_t ilwalidOffsets[] = {
        LW_RISCV_AMAP_IMEM_START - 8,
        LW_RISCV_AMAP_IMEM_START + imem_size,
        LW_RISCV_AMAP_DMEM_START - 8,
        LW_RISCV_AMAP_DMEM_START + dmem_size,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START - 8: 0,
        emem_size > 0 ? LW_RISCV_AMAP_EMEM_START + emem_size : 0,
        0
    };
    int i = 0;
    int results = 0;

    printf("Testing memory ranges..");

    printf("+ ");
    while (validOffsets[i])
    {
        // Try reading memory
        volatile uint64_t *ptr = (volatile uint64_t *)validOffsets[i];
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        loadAddress((uint64_t)&ptr[0]);
        riscvLwfenceRWIO();
        if (trap_mepc == 0)
            results |= (1 << i);
        else
            TRAP_CLEAR();
        i++;
    }
    if (((1 << i) - 1) != results)
    {
        printf("FAIL valid.\n");
        return MAKE_DEBUG_NOSUB(TestId_TcmRanges, 0, ((1 << i) - 1), results);
    }

    i = 0;
    results = 0;

    printf("- ");
    while (ilwalidOffsets[i])
    {
        // Try reading memory
        volatile uint64_t *ptr = (volatile uint64_t *)ilwalidOffsets[i];
        TRAP_CLEAR();
        riscvLwfenceRWIO();
        loadAddress((uint64_t)&ptr[0]);
        riscvLwfenceRWIO();
        if ((trap_mepc != 0) && (trap_mcause == 5))
        {
            results |= (1 << i);
            TRAP_CLEAR();
        }
        i++;
    }
    if (((1 << i) - 1) != results)
    {
        printf("FAIL Invalid.\n");
        return MAKE_DEBUG_NOSUB(TestId_TcmRanges, 1, ((1 << i) - 1), results);
    }

    printf("OK.\n");
    return PASSING_DEBUG_VALUE;
}

uint64_t testReadMemErr(void)
{
    printf("Testing read memerr..");
#if LWRISCV_HAS_PRI
    // Test pri load fault (error code)
    {
        const uint32_t MEMERR_TEST_ADDRESS = 0x1212340;
        uint32_t r, addr_lo;

        TRAP_CLEAR();
        riscvLwfenceRWIO();
        loadAddress32((uint64_t)LW_RISCV_AMAP_PRIV_START + MEMERR_TEST_ADDRESS);
        riscvLwfenceRWIO();

        if ((trap_mepc == 0) || (trap_mcause != 5) || (trap_mtval != (LW_RISCV_AMAP_PRIV_START + MEMERR_TEST_ADDRESS)))
        {
            printf("FAIL reason.\n");
            return MAKE_DEBUG_NOVAL(TestId_MemErrRead, 0, 0);
        }

        r = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_STAT);
        if (!FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE, r))
        {
            printf("FAIL ERR_STAT_VALID.\n");
            return MAKE_DEBUG_NOSUB(TestId_MemErrRead, 1, DRF_DEF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _VALID, _TRUE), r);
        }

        if (!FLD_TEST_DRF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _WRITE, _FALSE, r))
        {
            printf("FAIL ERR_STAT_WRITE.\n");
            return MAKE_DEBUG_NOSUB(TestId_MemErrRead, 2, DRF_DEF(_PRGNLCL, _RISCV_PRIV_ERR_STAT, _WRITE, _FALSE), r);
        }

        addr_lo = localRead(LW_PRGNLCL_RISCV_PRIV_ERR_ADDR);
        if (addr_lo != MEMERR_TEST_ADDRESS)
        {
            printf("FAIL ADDRESS.\n");
            return MAKE_DEBUG_NOSUB(TestId_MemErrRead, 3, addr_lo, MEMERR_TEST_ADDRESS);
        }

        if (localRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO) != 0xbadf1100)
        {
            printf("FAIL ERR_INFO.\n");
            return MAKE_DEBUG_NOSUB(TestId_MemErrRead, 4, localRead(LW_PRGNLCL_RISCV_PRIV_ERR_INFO), 0xbadf1100);
        }

        localWrite(LW_PRGNLCL_RISCV_PRIV_ERR_STAT, REF_DEF(LW_PRGNLCL_RISCV_PRIV_ERR_STAT_VALID, _FALSE));
        localWrite(LW_PRGNLCL_FALCON_IRQSCLR, REF_DEF(LW_PRGNLCL_FALCON_IRQSCLR_MEMERR, _SET));
    }

    printf("OK\n");
#else
    printf("Skipped, no pri\n");
#endif
    return PASSING_DEBUG_VALUE;
}
