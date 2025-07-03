/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main.c
 * @brief  Tons of technical debt in pursuit of a hard deadline
 */
#include <stdint.h>
#include <lwtypes.h>
#include <lwmisc.h>

#include <dev_gsp.h>

#include <rmgspcmdif.h>
#include <riscvifriscv.h>

#include <lwriscv/csr.h>
#include <lwriscv/fence.h>
#include <lwriscv/peregrine.h>

#include <liblwriscv/io.h>
#include <liblwriscv/libc.h>
#include <liblwriscv/mpu.h>
#include <liblwriscv/print.h>

#include "cmdmgmt.h"
#include "stress.h"
#include "testharness.h"
#include "utils.h"

// Target privilege mode for RPC code. 0 is invalid.
MpuContext g_mpuContext;
LwU64 g_fbOffset;
static void enableWatchdogInterrupt(int on);

/*
#define CHECK_WDTMR do {\
    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _WDTMR, _TRUE, localRead(LW_PRGNLCL_FALCON_IRQSTAT))) { \
        goto fail_with_timeout; \
    }\
} while (0);
*/
#define CHECK_WDTMR /* Meow! */

#define RUN_TEST(x, y) do {\
    if ((x)(y)) \
        goto fail_with_error; \
    CHECK_WDTMR \
} while (0);

#define RUN_TEST_EXPECT(x, y, z) do {\
    if ((x)(y) != (z)) \
        goto fail_with_error; \
    CHECK_WDTMR \
} while (0);

// Lily wrote crappy define name here.
// "margin" is how long we want it to count down initially
// "reset margin" is how long before it would fire would we like to maybe pet the watchdog? before it eats us?

#define JUST_IN_TIME_INITIAL_TIMEOUT 5000
#define JUST_IN_TIME_RESET_MARGIN 10
static LwU8 wdtTestHistory = 0;
static LwU64 jitWatchdogTest(LwU64 flags)
{
    // Only fail if WDT has fired before the test on the FIRST run of this test.
    // Rationale: First time we need to fully test WDT.
    // After the first iteration we are only using this test to stress PRI.
    // (we're just using WDT as changing PRI register)
    if (wdtTestHistory != 7)
    {
        if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _WDTMR, _TRUE, localRead(LW_PRGNLCL_FALCON_IRQSTAT))) // WDTMR fired already, this test is invalid.
            return 1;
    }
    if (flags == 0)
    {
        PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
        while (localRead(LW_PRGNLCL_FALCON_WDTMRVAL) > JUST_IN_TIME_RESET_MARGIN); // wait until there's just one tick left
        PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);
    }
    else if (flags == 1)
    {
        LwU64 ptimer_ns, ptimer_ns_initial;
        PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
        ptimer_ns = localRead(LW_PRGNLCL_FALCON_PTIMER0) | (LwU64)(localRead(LW_PRGNLCL_FALCON_PTIMER1))<<32ull;
        ptimer_ns_initial = ptimer_ns;
        do {
            ptimer_ns = localRead(LW_PRGNLCL_FALCON_PTIMER0) | (LwU64)(localRead(LW_PRGNLCL_FALCON_PTIMER1))<<32ull; //380ns ? for this read
        } while ((ptimer_ns - ptimer_ns_initial) < (32*32*JUST_IN_TIME_INITIAL_TIMEOUT - JUST_IN_TIME_RESET_MARGIN * 32*32));
        // each WDTMR tick = 1024 PTIMER units. A PTIMER unit is 1ns (increments by 32 every 32ns.)

        // Becase PRI is slow in P8, GA107 will timeout with 1us margin most of the time, 2us much of the time, and not at 4us.
        PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);
    }
    else if (flags == 2)
    {
        LwU64 ptimer, ptimer_initial;
        PET_WATCHDOG(JUST_IN_TIME_INITIAL_TIMEOUT); // 1.024 ms
        ptimer = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
        ptimer_initial = ptimer;
        do {
            ptimer = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
        } while ((ptimer - ptimer_initial) < (32*JUST_IN_TIME_INITIAL_TIMEOUT - JUST_IN_TIME_RESET_MARGIN * 32));
        // each WDTMR tick = 32 RDCYCLE units. A RDCYCLE unit is 32ns (increments by 1 every 32 ns.)
        PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);
    }
    else // You specified invalid test. True story (GA100 ucode has this bug)
        return 1;

    if (wdtTestHistory != 7)
    {
        if (FLD_TEST_DRF(_PRGNLCL, _FALCON_IRQSTAT, _WDTMR, _TRUE, localRead(LW_PRGNLCL_FALCON_IRQSTAT))) // WDTMR fired, you are dog food.
            return 1;
    }

    wdtTestHistory |= (LwU8)(1<<flags); // set bit corresponding to test

    return 0;
}

static void runStressTestLoop(void)
{
    struct lz4CompressionTestConfig lz4TestConfig;
    register LwU32 iterations = 0; //we need to keep this value in register if we move it to FB.

    LZ4_malloc_init(DMEM_START, 20480);
    localWrite(MEMRW_GADGET_CTRL, 0); // clear GADGET_CTRL if we changed privilege
    localWrite(MEMRW_GADGET_STATUS, iterations); // iterations completed

    printf("Init time: %d us\n", WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS - localRead(LW_PRGNLCL_FALCON_WDTMRVAL));
    enableWatchdogInterrupt(0);   // Disable watchdog interrupt

    appRunFromFb();

    volatile LwU32 ctrl = 0;
    while (ctrl == 0)
    {
        {
            LwU64 ptimer0, ptimer1;
#ifdef ENABLE_PROFILING
            LwU64 ptimerX;
#endif
            ptimer0 = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
            LwU32 testIdx = 1;
            //Stress test loop.

            // PRI polling test:
#ifdef ENABLE_PROFILING
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
            RUN_TEST(jitWatchdogTest,0); // Test 1
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
            RUN_TEST(jitWatchdogTest,1); // Test 2
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
            RUN_TEST(jitWatchdogTest,2); // Test 3
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            // Test 4 - cached LFSR test (CRC table in cache)
            lfsrTest(0x7800, LW_RISCV_AMAP_DMEM_START, 0); // 60KiB DTCM truncated LFSR
            verify(LW_RISCV_AMAP_DMEM_START, 61440, 0xaca75c7a); //total coincidence. A cat's CTA?
            fastAlignedMemSet(LW_RISCV_AMAP_DMEM_START, 0xa5, 61440);
            verify(LW_RISCV_AMAP_DMEM_START, 61440, 0x64490a48);
            fastAlignedMemSet(LW_RISCV_AMAP_DMEM_START, 0, 61440);
            verify(LW_RISCV_AMAP_DMEM_START, 61440, 0xec1c6272);

            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            // ****** WARNING ******
            // * This test is SLOW *
            // * 680ms on GA107 P8 *
            // *********************
            changeFbCachable(LW_FALSE);
            __asm__ __volatile__("fence.i");
            // Test 5 - uncached LFSR test in FB
            lfsrTest(0x7800, g_fbOffset + 0x10000, 0); // 60KiB DTCM truncated LFSR
            verify(g_fbOffset + 0x10000, 61440, 0xaca75c7a); //total coincidence. A cat's CTA?
            fastAlignedMemSet(g_fbOffset + 0x10000, 0xa5, 61440);
            verify(g_fbOffset + 0x10000, 61440, 0x64490a48);
            fastAlignedMemSet(g_fbOffset + 0x10000, 0, 61440);
            verify(g_fbOffset + 0x10000, 61440, 0xec1c6272);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            changeFbCachable(LW_TRUE);
            __asm__ __volatile__("fence.i");
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, ptimerX);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            // Test 6 - cached LFSR test in IMEM (CRC table in cache)
            fastAlignedMemSet(g_beginOfImemFreeSpace, 0xa5, 40960);
            verify(g_beginOfImemFreeSpace, 40960, 0x407fa956);
            fastAlignedMemSet(g_beginOfImemFreeSpace, 0, 40960);
            verify(g_beginOfImemFreeSpace, 40960, 0x2c2bb90a);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            RUN_TEST(pipelinedMulTest, 0); // Test 7 - MUL test
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
#endif

            // This entire junk is a house of cards.
            // Biggest problem I have here is that we need to maximize the amount of ITCM
            // tested by Test 6 above, while saving enough ITCM to survive moving code from TCM to / from FB.

            // TLDR When porting just don't change anything and stay below 16KiB in .code
            // You can survive with compressed ISA easily.
#if 0 // MK TODO
            fastAlignedMemCopy((void*)(g_fbOffset + 0x8000), IMEM_START, 0x4000); // save code from ITCM
            changeExelwtionLocation(1);
            app_run_from_dmem();
#endif
            // The LZ4 test copies the code into a temp buffer, runs LZ4 compress to FB, wipes the source buffer, and decompresses from FB to DMEM.
            // The CRC is verified before compression and after decompression.
            // Note that DMEM_START is ALSO the compression scratch buffer, but decompression does not need it.
            // So we can get away with using DMEM_START as the decompression target as well as the compression scratch buffer.
            fastAlignedMemCopy((void*)0x112000, (void*)0xA00000, 0x4000); // copy 16K of code to temp IMEM buffer. Can't find define for 0xA00000

#ifdef ENABLE_PROFILING
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
            lz4TestConfig.src_size = 0x4000;
            lz4TestConfig.src = (void*)0x112000;    // MK HACKED - Temp copy of code in ITCM. Lily originally wrote misleading comment here.
            lz4TestConfig.src2 = DMEM_START;        // Must use DMEM as ultimate destination because ITCM is broken (on GA100)
            lz4TestConfig.dest = (void*)(g_fbOffset + 0x2000); // FB compress destination
            lz4TestConfig.dest_size = 0x4000;
            RUN_TEST(lz4CompressionTest, &lz4TestConfig);  // Test 8 - LZ4 test
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

#if 0 // MK TODO: unsure if we can do it :(
            // Test 9 - IMEM LFSR test
            lfsrTest(0x8000, 0x100000, 0); // 64KiB IMEM LFSR
            verify(0x100000, 65536, 0x7dd0af99);
            fastAlignedMemSet(0x100000, 0xa5, 65536);
            verify(0x100000, 65536, 0xfdb5742b);
            fastAlignedMemSet(0x100000, 0, 65536);
            verify(0x100000, 65536, 0xd7978eeb);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#endif

            // MK HACKED: that's all we can do :( Test 9 - IMEM LFSR test
            lfsrTest(0x4000, (LwU64)&__imem_test_buffer_start, 0); // 32KiB IMEM LFSR
            verify((LwU64)&__imem_test_buffer_start, 0x8000, 0x622f3ee2); // hope number is good
            fastAlignedMemSet((LwU64)&__imem_test_buffer_start, 0xa5, 0x8000);
            verify((LwU64)&__imem_test_buffer_start, 0x8000, 0xa5e6c620);
            fastAlignedMemSet((LwU64)&__imem_test_buffer_start, 0, 0x8000);
            verify((LwU64)&__imem_test_buffer_start, 0x8000, 0x011ffca6);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif


            // Test 10 - BP test
            RUN_TEST(branchPredictorStressTest, 131072);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif

            // Test 11 - BP test with MUL
            RUN_TEST(branchPredictorMultiplyStressTest, 131072);
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
#if 0
            fastAlignedMemCopy((void*)0x100000, (void*)(g_fbOffset + 0x8000), 0x4000); // restore code to ITCM
            changeExelwtionLocation(0);
            appRunFromImem();
#endif

            // Test 12 - Pi callwlation test
            // This test is really slow. Takes around 370ms at GA107 boot clocks.
            // For reference, a Raspberry Pi can do the same (at 1GHz) in 285ms. So clock for clock we're actually faster...
            RUN_TEST_EXPECT(callwlatePiAndVerify, 0x40490fec, 0x555555); // AKA pi value we want to halt on
            localWrite(MEMRW_GADGET_CTRL, testIdx++);
#ifdef ENABLE_PROFILING
            printf("Test %d time: %u us\n", testIdx - 1, (csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME) - ptimerX)/32);
            ptimerX = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
#endif
            // We don't test the divider much, but code rarely uses it either.
            if (! (iterations % 100))
                printf("Iteration %d, IRQSTAT=%x WDTMR=%d\n", iterations, localRead(LW_PRGNLCL_FALCON_IRQSTAT), localRead(LW_PRGNLCL_FALCON_WDTMRVAL));

            iterations++;
            localWrite(MEMRW_GADGET_STATUS, iterations); // iterations completed
            localWrite(MEMRW_GADGET_CTRL, 0); // reset to zero

            // Pet the watchdog.
            //
            PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);
            //
            ptimer1 = csr_read(LW_RISCV_CSR_HPMCOUNTER_TIME);
            if ((iterations % 100) == 3)
            {
                printf("> Iteration takes %u ticks (%u WDTMR us)\n", ptimer1 - ptimer0, (ptimer1 - ptimer0) / 32);
            }
#ifdef ENABLE_PROFILING
            if (iterations == 3)
            {
                ICD_HALT
            }
#endif
        }
        if (pollUnloadCommand(LW_FALSE))
        {
            printf("Final iteration count: %d, IRQSTAT=%x WDTMR=%d\n", iterations, localRead(LW_PRGNLCL_FALCON_IRQSTAT), localRead(LW_PRGNLCL_FALCON_WDTMRVAL));

            return;
        }

    }
// fail_with_timeout:
//     printf("Watchdog timer hit on test %d.\n", localRead(LW_PRGNLCL_FALCON_MAILBOX0) + 1);
//     goto fail_meow;

fail_with_error:
    enableWatchdogInterrupt(1);   // Enable watchdog interrupt for early failure detection.
    printf("Failed test %d with error.\n", localRead(LW_PRGNLCL_FALCON_MAILBOX0) + 1);

// fail_meow:
    FAIL_MEOW("Halting core on failure.\n");
    while (1);              // should not reach here
}

static void runLfsrSanity(void)
{
    // MK: We can use whole DMEM because we run from FB at this point (stack et al)
    LwU64 lsfrTestBase = (LwU64)DMEM_START;

    printf("Running LFSR sanity @ %p\n", (void*)lsfrTestBase);
    lfsrTest(0x7800, lsfrTestBase, 1); // 60KiB DTCM truncated LFSR

    printf("Verifying LFSR output...\n");
    verify(lsfrTestBase, 61440, 0xaca75c7a); //total coincidence. A cat's CTA?

    printf("Clearing DMEM...\n");
    fastAlignedMemSet(lsfrTestBase, 0xa5, 61440);
    verify(lsfrTestBase, 61440, 0x64490a48);

    printf("LFSR sanity passed.\n");
}

static const char *mcauses[] = {
    "Instruction address misaligned",
    "Instruction access fault",
    "Illegal instruction",
    "Breakpoint",
    "Load address misaligned",
    "Load access fault",
    "Store/AMO address misaligned",
    "Store/AMO access fault",
    "Environment call from U-mode",
    "Environment call from S-mode",
    "Reserved",
    "Environment call from M-mode",
    "Instruction page fault",
    "Load page fault",
    "Reserved",
    "Store/AMO page fault",
};

void appException(void)
{
    LwU64 cause = csr_read(LW_RISCV_CSR_SCAUSE);
    enableWatchdogInterrupt(1);

    if (cause < 0x10)
    {
        printf("Exception %s (0x%llx) at %p val %llx!\n",
               mcauses[cause],
               cause,
               csr_read(LW_RISCV_CSR_SEPC),
               csr_read(LW_RISCV_CSR_STVAL));
    } else
    {
        printf("Unknown exception 0x%llx at %p val %llx!\n",
               cause,
               csr_read(LW_RISCV_CSR_SEPC),
               csr_read(LW_RISCV_CSR_STVAL));
    }
    FAIL_MEOW_PRINT;
    riscvFenceRWIO();
}

static void enableWatchdogInterrupt(int on)
{
    LwU32 wdtBitMask = 0;
    wdtBitMask = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _WDTMR, _SET, wdtBitMask);
    if (on)
    {
        localWrite(LW_PRGNLCL_RISCV_IRQMSET, wdtBitMask);
    }
    else
    {
        localWrite(LW_PRGNLCL_RISCV_IRQMCLR, wdtBitMask);
    }
}

// Proudly stolen from CC ;)
static void setupHostInterrupts(void)
{
    LwU32 irqDest = 0U;
    LwU32 irqMset = 0U;
    LwU32 irqMode = localRead(LW_PRGNLCL_FALCON_IRQMODE);

    // HALT: CPU transitioned from running into halt
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _HALT, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _HALT, _SET, irqMset);

    // SWGEN0: for communication with RM via Message Queue
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN0, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN0, _SET, irqMset);

    // SWGEN1: for the PMU's print buffer
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _SWGEN1, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _SWGEN1, _SET, irqMset);

    // MEMERR: for external mem access error interrupt (level-triggered)
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _MEMERR, _RISCV, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _MEMERR, _SET, irqMset);
    irqMode = FLD_SET_DRF(_PRGNLCL, _FALCON_IRQMODE, _LVL_MEMERR, _TRUE, irqMode);

    // WDTMR: for the PMU's print buffer
    irqDest = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQDEST, _WDTMR, _HOST, irqDest);
    irqMset = FLD_SET_DRF(_PRGNLCL, _RISCV_IRQMSET, _WDTMR, _SET, irqMset);

    localWrite(LW_PRGNLCL_FALCON_IRQMODE, irqMode);
    localWrite(LW_PRGNLCL_RISCV_IRQDEST, irqDest);
    localWrite(LW_PRGNLCL_RISCV_IRQMSET, irqMset);
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, ~irqMset);
    // TODO: Needs to be cleaned up
    localWrite(LW_PRGNLCL_RISCV_IRQDELEG, irqMset);

    // Set boot watchdog limit to 2 * standard limit. In microseconds.
    localWrite(LW_PRGNLCL_FALCON_WDTMRVAL, 2 * WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);
    localWrite(LW_PRGNLCL_FALCON_WDTMRCTL, (1<<1) | (1<<0)); /* USE_PTIMER | ENABLE */
}

static void init(void)
{
    uint32_t dmemSize = localRead(LW_PRGNLCL_FALCON_HWCFG3);
    dmemSize = DRF_VAL(_PRGNLCL, _FALCON_HWCFG3, _DMEM_TOTAL_SIZE, dmemSize) << 8;

    // reset queues
    priWrite(LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE), 0);
    priWrite(LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 0);
    riscvFenceRWIO();
    // configure print to the start of DMEM
    memset((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x800, 0, 0x800);
    printInitEx((LwU8*)LW_RISCV_AMAP_DMEM_START + dmemSize - 0x800, 0x800,
                LW_PGSP_QUEUE_HEAD(RM_RISCV_DEBUG_BUFFER_QUEUE),
                LW_PGSP_QUEUE_TAIL(RM_RISCV_DEBUG_BUFFER_QUEUE), 1);

    // Release PMB lockdown
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB,
               FLD_SET_DRF(_PRGNLCL, _FALCON_LOCKPMB, _DMEM, _UNLOCK,
                           localRead(LW_PRGNLCL_FALCON_LOCKPMB)));

    setupHostInterrupts();
    initCmdmgmt();
    // Initialize MPU metadata so we can play with it
    mpuInit(&g_mpuContext);

    g_fbOffset = (LwUPtr)&__data_fb_buffer;
}

int main(void)
{
    releasePriLockdown();

    csr_write(LW_RISCV_CSR_STVEC, (LwUPtr)app_exception_entry);

    init();

    printf("Generating CRC tables in FB...\n");
    // Generate CRC tables.
    initCrcTable();

    printf("Reconfiguring to run from imem...\n");
    appRunFromImem();

    printf("Now running from IMEM\n");

    // Run LFSR sanity
    runLfsrSanity();
    memset(DMEM_START, 0, 0x7800); // wipe old results
    printf("Reconfiguring to run from fb...\n");
    appRunFromFb();
    // Run LFSR sanity
    runLfsrSanity();
    memset(DMEM_START, 0, 0x7800); // wipe old results

    printf("Reconfiguring for perf... ");
    appEnableRiscvPerf();
    // enable cache in mappings
    changeFbCachable(LW_TRUE);
    printf("Done.\n");

    localWrite(MEMRW_GADGET_CTRL, 0xcccccccc);// signal booting
    runLfsrSanity(); // now with wonderful cache and branch predictor

    PET_WATCHDOG(WATCHDOG_TIMER_TIMEOUT_SOME_SECONDS);

    printf("Init done, ");
    sendRmInitMessage(FLCN_OK);

    printf("If you can see this, stress testing is happening!\n"
           "  |\\      _,,,--,,_ _,)  \n"
           " / o`.-'`LIVE-CAT;-;;'    \n"
           "< o 3  ) )-,_ ) /\\       \n"
           " `--''(_/--' (_/-'       \n");

    runStressTestLoop(); // run in Mbare mode initially.

    pollUnloadCommand(LW_TRUE);
    // Kill watchdog
    localWrite(LW_PRGNLCL_FALCON_WDTMRCTL, 0);
    appShutdown();

    ICD_HALT
            while (1);
    return 0;
}
