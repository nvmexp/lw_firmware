/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <stdint.h>
#include <lwmisc.h>

#include <dev_pri_ringstation_sys.h>
#include <dev_gsp.h>
#include <dev_sec_pri.h>
#include <dev_fsp_pri.h>

#include <liblwriscv/io.h>
#include <liblwriscv/print.h>
#include <liblwriscv/shutdown.h>
#include <lwriscv/csr.h>
#include <lwriscv/fence.h>
#include <lwriscv/peregrine.h>

#include "tests.h"
#include "util.h"

#define EMEM_SIZE 0x2000

static void configure_ememr(int no, uint32_t lsize, uint32_t usize)
{
    usize >>= 2;
    lsize >>= 2;

    priWrite(ENGINE_REG(_EMEMR)(no),
             (usize & 0x3ff) << 18 | (lsize & 0x3ff) << 2);
}

extern void trap_entry(void);
static void init(void)
{
    uint32_t dmemSize = 0;

    // Turn on tracebuffer
    localWrite(LW_PRGNLCL_RISCV_TRACECTL,
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MODE, _STACK) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _MMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _SMODE_ENABLE, _TRUE) |
               DRF_DEF(_PRGNLCL, _RISCV_TRACECTL, _UMODE_ENABLE, _TRUE));

    dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;
    // configure print to the start of DMEM
    printInit((uint8_t*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x1000, 0x1000, 0, 0);

#if ! defined LWRISCV_IS_ENGINE_minion
    // Configure FBIF mode
    localWrite(LW_PRGNLCL_FBIF_CTL,
                (DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE) |
                 DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW)));
#if ! defined LWRISCV_IS_ENGINE_lwdec
    // LWDEC doesn't have this register
    localWrite(LW_PRGNLCL_FBIF_CTL2,
                LW_PRGNLCL_FBIF_CTL2_NACK_MODE_NACK_AS_ACK);
#endif
#endif

    csr_set(LW_RISCV_CSR_MSPM,
              DRF_NUM(_RISCV_CSR, _MSPM, _SPLM, DRF_VAL(_RISCV_CSR, _MSPM, _MPLM, csr_read(LW_RISCV_CSR_MSPM))) |
              DRF_NUM(_RISCV_CSR, _MSPM, _UPLM, DRF_VAL(_RISCV_CSR, _MSPM, _MPLM, csr_read(LW_RISCV_CSR_MSPM)))
              );

    csr_set(LW_RISCV_CSR_MDBGCTL, 1); // current PL

    // Unlock IMEM and DMEM access
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, 0x0);

    // Release priv lockdown
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
               DRF_DEF(_PRGNLCL, _RISCV_BR_PRIV_LOCKDOWN, _LOCK, _UNLOCKED));

    csr_write(LW_RISCV_CSR_MTVEC, (uint64_t) trap_entry);
}

enum engines
{
    ENG_GSP=0,
    ENG_SEC=1,
    ENG_FSP=2,
    ENG_COUNT
};

#if LWRISCV_IS_ENGINE_sec
enum engines self = ENG_SEC;
#elif LWRISCV_IS_ENGINE_gsp
enum engines self = ENG_GSP;
#elif LWRISCV_IS_ENGINE_fsp
enum engines self = ENG_FSP;
#endif

const uint32_t source_ids[ENG_COUNT] = {
    [ENG_GSP]=LW_PPRIV_SYS_PRI_SOURCE_ID_GSP,
    [ENG_SEC]=LW_PPRIV_SYS_PRI_SOURCE_ID_SEC2,
    [ENG_FSP]=LW_PPRIV_SYS_PRI_SOURCE_ID_FSP,
};

const uint32_t ememc[ENG_COUNT] = {
    [ENG_GSP]=LW_PGSP_EMEMC(0),
    [ENG_SEC]=LW_PSEC_EMEMC(0),
    [ENG_FSP]=LW_PFSP_EMEMC(0)
};

const uint32_t mailbox0[ENG_COUNT] = {
    [ENG_GSP]=LW_PGSP_FALCON_MAILBOX0,
    [ENG_SEC]=LW_PSEC_FALCON_MAILBOX0,
    [ENG_FSP]=LW_PFSP_FALCON_MAILBOX0
};

const uint32_t messages[ENG_COUNT] = {
    [ENG_GSP]=0x505347,
    [ENG_SEC]=0x434553,
    [ENG_FSP]=0x505346
};

const char *names[ENG_COUNT] = {
    [ENG_GSP]="GSP",
    [ENG_SEC]="SEC",
    [ENG_FSP]="FSP"
};


static volatile uint32_t *emem= (volatile char *)LW_RISCV_AMAP_EMEM_START;
#define EMEM_WINDOW_SIZE 0x100


static void panic(void)
{
    printf("Unexpected exception %d, at %p mtval %p\n",
           trap_mcause, trap_mepc, trap_mtval);
    while (1);
}

#define PLM_SET_SOURCE_WITH_REPORT_ERROR(name, read, write, source) \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_PROTECTION, read)             | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_PROTECTION, write)           | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _READ_VIOLATION, _REPORT_ERROR)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _WRITE_VIOLATION, _REPORT_ERROR)    | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL, _BLOCKED)     | \
    DRF_DEF(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL, _BLOCKED)    | \
    DRF_NUM(_PRGNLCL, _##name##_PRIV_LEVEL_MASK, _SOURCE_ENABLE, source)

static void setupEmem(void)
{
    int i;

    setPlmAll(PLM_L3);
    for (i=0; i<ENG_COUNT; ++i)
    {
        // configure window
        configure_ememr(i, i * EMEM_WINDOW_SIZE, i*EMEM_WINDOW_SIZE + EMEM_WINDOW_SIZE);
        // configure PLM
        uint32_t plm = PLM_SET_SOURCE_WITH_REPORT_ERROR(FALCON_WDTMR, 0xF, 0xF, 0) | ((1U << source_ids[i]) << 12);
        if (i == self) // MK HACK
            plm |= ((1U << LW_PPRIV_SYS_PRI_SOURCE_ID_LOCAL_SOURCE_FALCON)) << 12;
        priWrite(ENGINE_REG(_EMEM_PRIV_LEVEL_MASK)(i), plm);
    }
//    setPlmAll(PLM_L0);
}

static bool emem_read(enum engines engine, int bank, uint32_t offset, uint32_t *pVal)
{
    uint32_t val;

    if (bank >= ENGINE_REG(_EMEMC__SIZE_1))
    {
        printf("Invalid region: %d\n", bank);
        riscvPanic();
    }

    TRAP_CLEAR();
    priWrite(ememc[engine] + bank * 8, offset & ((1 << 24) - 1));
    if (trap_mcause != 0)
        panic();

    TRAP_CLEAR();
    val = priRead(ememc[engine] + bank * 8 + 4);
    if (trap_mcause == 5)
    {
        TRAP_CLEAR();
        return false;
    }
    if (trap_mcause != 0)
        panic();

    if (pVal)
        *pVal = val;

    return true;
}

static bool emem_write(enum engines engine, int bank, uint32_t offset, uint32_t val)
{

    if (bank >= ENGINE_REG(_EMEMC__SIZE_1))
    {
        printf("Invalid region: %d\n", bank);
        riscvPanic();
    }

    TRAP_CLEAR();
    priWrite(ememc[engine] + bank * 8, offset & ((1 << 24) - 1));
    if (trap_mcause != 0)
        panic();

    TRAP_CLEAR();
    priWrite(ememc[engine] + bank * 8 + 4, val);
    riscvLwfenceIO(); // We realy need it here
    if (trap_mcause != 0)
        panic();

    return true;
}


static void waitForOthers(int no)
{
    int i, ready;

    while (1)
    {
        ready = 0;
    for (i=0; i<ENG_COUNT; ++i)
    {
        TRAP_CLEAR();
        if (trap_mcause == 0 && priRead(mailbox0[i]) >= no)
            ready++;
        TRAP_CLEAR();
    }
        if (ready == ENG_COUNT)
            break;
    }
}

static uint64_t testOwn(void)
{
    int engine, bank;
    for (engine=0; engine<ENG_COUNT; ++engine)
    {
        for (bank=0; bank<ENG_COUNT; ++bank)
        {
            uint32_t val = 42;
            bool r;

            printf("Writing to %s bank %s ...", names[engine], names[bank]);
            emem_write(engine, bank, bank * EMEM_WINDOW_SIZE, messages[self]);

            r = emem_read(engine, bank, bank * EMEM_WINDOW_SIZE, &val);
            if (bank == self) // we should be able to write to our bank
            {
                if (r && val == messages[self])
                {
                    printf("OK1\n");
                } else
                    printf("FAIL1 %d 0x%x 0x%x\n", r, val, messages[self]);
            } else
            {
                if (!r || val != messages[self])
                {
                    printf("OK2\n");
                } else
                    printf("FAIL2\n");
            }
        }
    }
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0, ENG_COUNT);
    return PASSING_DEBUG_VALUE;
}

static uint64_t testOther(void)
{
    int i;

    printf("Tesing my emem writes...");
    for (i=0; i<ENG_COUNT; ++i)
    {
        if (emem[(i * EMEM_WINDOW_SIZE) / 4] == messages[i])
            printf("OK ");
        else
            printf("FAIL ");
    }
    printf("\n");

    return PASSING_DEBUG_VALUE;
}

int main(void)
{
    init();
    printf("Running on %s\n", ENGINE_NAME);
    printf("Setting apertures..\n");
    setupEmem();

    // Notify others we're ready
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0, 1);

    printf("Waiting for other engines...\n");
    waitForOthers(1);
    printf("Wait done, testing.\n");
    testOwn();
    printf("Waiting for others to finish testing.\n");
    waitForOthers(ENG_COUNT);
    testOther();

    csr_write(LW_RISCV_CSR_MSCRATCH, PASSING_DEBUG_VALUE);

    if (csr_read(LW_RISCV_CSR_MSCRATCH) == PASSING_DEBUG_VALUE)
    {
        printf("Test PASSED.\n");
    } else
    {
        uint64_t r = csr_read(LW_RISCV_CSR_MSCRATCH);
        printf("Test FAILED. Test: %d Subtest: %d IDX: %d Exp: 0x%x Got: 0x%x\n",
               r >> 56,
               (r >> 48) & 0xFF,
               (r >> 40) & 0xFF,
               (r >> 16) & 0xFFFF,
               r & 0xFFFF);
    }
    localWrite(LW_PRGNLCL_RISCV_ICD_CMD, 0);
    while(1);
    return 0;
}
