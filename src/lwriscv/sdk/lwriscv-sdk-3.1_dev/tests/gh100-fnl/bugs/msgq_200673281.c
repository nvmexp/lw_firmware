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
#include <stddef.h>
#include <lwmisc.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/fence.h>

#include "tests.h"
#include "util.h"

// MSGQ enhancements -> https://lwbugs/200673281
// Test apply to: gsp, fsp, sec2

#define EMEM_SIZE 0x2000

static void configure_ememr(int no, uint32_t lsize, uint32_t usize)
{
    usize >>= 2;
    lsize >>= 2;

    priWrite(ENGINE_REG(_EMEMR)(no),
             (usize & 0x3ff) << 18 | (lsize & 0x3ff) << 2);
}

static void emem_read(int bank, uint32_t offset, uint32_t *pVal)
{
    uint32_t val;

    if (bank >= ENGINE_REG(_EMEMC__SIZE_1))
    {
        printf("Invalid region: %d\n", bank);
        riscvPanic();
    }

    priWrite(ENGINE_REG(_EMEMC)(bank), offset & ((1 << 24) - 1));

    val = priRead(ENGINE_REG(_EMEMD)(bank));
    if (pVal)
        *pVal = val;
}

static void emem_write(int bank, uint32_t offset, uint32_t val)
{

    if (bank >= ENGINE_REG(_EMEMC__SIZE_1))
    {
        printf("Invalid region: %d\n", bank);
        riscvPanic();
    }

    priWrite(ENGINE_REG(_EMEMC)(bank), offset & ((1 << 24) - 1));
    priWrite(ENGINE_REG(_EMEMD)(bank), val);
    riscvLwfenceIO(); // We realy need it here
}

// direct read/write; offset in bytes but should be aligned;
static uint32_t emem_dread(uint32_t offset)
{
    volatile uint32_t *emem_mmio = (volatile uint32_t *)LW_RISCV_AMAP_EMEM_START;

    return emem_mmio[offset / 4];
}

static void emem_dwrite(uint32_t offset, uint32_t val)
{
    volatile uint32_t *emem_mmio = (volatile uint32_t *)LW_RISCV_AMAP_EMEM_START;

    emem_mmio[offset / 4] = val;
}

// Test if we can chop EMEM into ranges and if EMEMR is updated
uint64_t test_ememr(void)
{
    const uint32_t BS = 0x100; // mapping block size

    int region, others, i;

    // setup regions
    for (region=0; region < ENGINE_REG(_EMEMR__SIZE_1); ++region)
    {
        configure_ememr(region, region * BS, region * BS + BS);
    }

    for (region=0; region < ENGINE_REG(_EMEMR__SIZE_1); ++region)
    {
        printf("Testing region %d... ", region);

        //fill emem with pattern
        for (i=0; i<EMEM_SIZE; i+=4)
        {
            emem_dwrite(i, i | i << 16);
        }

        // clear emem errors
        priWrite(ENGINE_REG(_EMEMR_ISTAT), 0xFF);

        for (others=0; others < ENGINE_REG(_EMEMR__SIZE_1); ++others)
        {
            uint32_t val = 0;
            uint32_t testNum=42 * others + 42;
            uint32_t testAddr = BS * others;

            printf("RW%d", others);
            emem_write(region, testAddr, testNum);
            if ( ((others == region) && (emem_dread(testAddr) == testNum)) ||
                 ((others != region) && (emem_dread(testAddr) != testNum))
                 )
            {
                printf("+");
            } else {
                printf("!w %x %x ", testNum, emem_dread(testAddr));
            }

            emem_read(region, testAddr, &val);
            if ( ((others == region) == (val == testNum)) ||
                 ((others != region) == (val != testNum))
                 )
            {
                printf("+");
            } else {
                printf("!r %x %x ", testNum, val);
            }

            // Check if interrupt was set for invalid accesses (and stay 0 for valid)


            printf(" ");
        }
        printf("\n");
    }
}

// test EMEMR_ISTAT, EMEMR_IEN


// test permissions
// LW_PGSP_CMDQ_HEAD_PRIV_LEVEL_MASK
// LW_PGSP_CMDQ_TAIL_PRIV_LEVEL_MASK

// Test if we can access control to given  EMEMC and MSGQ
uint64_t test_emem_plm(void)
{

}

// test access from other engine

uint64_t test_msgq(void)
{
    return test_ememr();

    return PASSING_DEBUG_VALUE;
}
