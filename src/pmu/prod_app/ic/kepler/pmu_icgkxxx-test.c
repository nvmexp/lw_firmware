/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_icgkxxx-test.c
 * @brief   Unit tests for logic in pmu_icgkxxx.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "lwtypes.h"
#include "pmu_objic.h"
#include "cmdmgmt/cmdmgmt.h"
#include "test-macros.h"
#include "regmock.h"
#include "config/pmu-config.h"
#include "pmu_bar0.h"
#include "lwrtos.h"
#include "lwrtos-mock.h"
#include "lwostask.h"
#include "lwoswatchdog-mock.h"
#include "therm/objtherm-mock.h"

#include "dev_graphics_nobundle.h"
#include "dev_lw_xve.h"
#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"
#include "dev_pwr_falcon_csb_addendum.h"

/* ------------------------ Mocked Functions -------------------------------- */
/*!
 * @brief   Override regmock writing behavior to emulate register field
 *          setting/clearing in interrupt registers.
 *
 * @details Emulates behavior of IRQMSET, IRQMCLR, IRQSSET, IRQSCLR, and
 *          LW_CPWR_PMU_INTR_1 registers. In the case of the SET and CLR
 *          registers, the writes are redirected to the corresponding STAT
 *          or MASK register with the bits set appropriately. Any '1' bits
 *          written to LW_CPWR_PMU_INTR_1 instead clear those bits.
 *
 */
static void mockWriteIrq(LwU32 addr, LwU8 size, LwU32 value)
{
    LwU32 prevValue;
    LwU32 newValue;
    switch (addr)
    {
        case LW_CMSDEC_FALCON_IRQMSET:
            prevValue = UTF_IO_CACHE_READ(LW_CMSDEC_FALCON_IRQMASK, size);
            newValue = prevValue | value;
            UTF_IO_CACHE_WRITE(LW_CMSDEC_FALCON_IRQMASK, size, newValue);
            break;
        case LW_CMSDEC_FALCON_IRQMCLR:
            prevValue = UTF_IO_CACHE_READ(LW_CMSDEC_FALCON_IRQMASK, size);
            newValue = prevValue & ~value;
            UTF_IO_CACHE_WRITE(LW_CMSDEC_FALCON_IRQMASK, size, newValue);
            break;
        case LW_CMSDEC_FALCON_IRQSSET:
            prevValue = UTF_IO_CACHE_READ(LW_CMSDEC_FALCON_IRQSTAT, size);
            newValue = prevValue | value;
            UTF_IO_CACHE_WRITE(LW_CMSDEC_FALCON_IRQSTAT, size, newValue);
            break;
        case LW_CMSDEC_FALCON_IRQSCLR:
            prevValue = UTF_IO_CACHE_READ(LW_CMSDEC_FALCON_IRQSTAT, size);
            newValue = prevValue & ~value;
            UTF_IO_CACHE_WRITE(LW_CMSDEC_FALCON_IRQSTAT, size, newValue);
            break;
        case LW_CPWR_PMU_INTR_1:
            prevValue = UTF_IO_CACHE_READ(LW_CPWR_PMU_INTR_1, size);
            newValue = prevValue & ~value;
            UTF_IO_CACHE_WRITE(LW_CPWR_PMU_INTR_1, size, newValue);
            break;
    }
}
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_IC,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests PMU Interrupt Handling")
                UT_SUITE_SET_OWNER("jorgeo"))

UT_CASE_DEFINE(PMU_IC, IcServiceRmCommand,
                UT_CASE_SET_DESCRIPTION("Ensure icService_GMXXX services RM Commands")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_IC, IcServiceWatchdogTimer,
                UT_CASE_SET_DESCRIPTION("Ensure icService_GMXXX services watchdog timer interrupts")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_IC, IcServiceTherm,
                UT_CASE_SET_DESCRIPTION("Ensure icService_GMXXX services therm interrupts")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_IC, IcIgnoreIrrelevantCommands,
                UT_CASE_SET_DESCRIPTION("Ensure interrupts not routed to PMU on IRQ0 are ignored")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_IC, IcIntr1Unhandled,
                UT_CASE_SET_DESCRIPTION("Ensure INTR_1 interrupts not expected to be fired halt the PMU")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Pre-test initialization for icService_GMXXX testing.
 *
 * @details Touches necessary registers so that they exist in register mocking
 *          cache and overrides writing behavior to certain interrupt registers
 *          before calling icPreInit_HAL function.
 */
static void s_icPreInit(void)
{
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMASK, 0U);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSTAT, 0U);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQMODE, 0U);
    REG_WR32(CSB, LW_CPWR_FALCON_IRQDEST, 0U);

    //
    // Following 2 registers must be touched because they are read by preinit function before being written to
    // TODO: these registers are not applicable on GKXXX. Look into how to
    // modify these tests so that we're not writing them from here.
    //
    REG_WR32(CSB, LW_XVE_XUSB_STATUS, 0U);
    REG_WR32(FECS,
             LW_PGRAPH_PRI_FECS_FEATURE_READOUT,
             DRF_DEF(_PGRAPH_PRI, _FECS_FEATURE_READOUT, _ECC_PMU_FALCON, _ENABLED));

    UTF_IO_MOCK(LW_CMSDEC_FALCON_IRQMSET, LW_CMSDEC_FALCON_IRQMSET, NULL, mockWriteIrq);
    UTF_IO_MOCK(LW_CMSDEC_FALCON_IRQMCLR, LW_CMSDEC_FALCON_IRQMCLR, NULL, mockWriteIrq);
    UTF_IO_MOCK(LW_CMSDEC_FALCON_IRQSSET, LW_CMSDEC_FALCON_IRQSSET, NULL, mockWriteIrq);
    UTF_IO_MOCK(LW_CMSDEC_FALCON_IRQSCLR, LW_CMSDEC_FALCON_IRQSCLR, NULL, mockWriteIrq);
    UTF_IO_MOCK(LW_CPWR_PMU_INTR_1, LW_CPWR_PMU_INTR_1, NULL, mockWriteIrq);
    icPreInit_HAL();
}

/*!
 * @brief   Pre-test setup for icService_GMXXX testing (ServiceRmCommand).
 *
 * @details Calls PreInit function to initialize interrupts, then sets interrupt
 *          registers to indicate a pending second-level interrupt that is an
 *          Rm Command. Finally, reset the last queue id seen.
 */
static void s_icPreTestServiceRmCommand(void)
{
    LwU32 pendingIrqStat = 0U;
    LwU32 pendingPmuIntr = 0U;

    lwrtosMockInit();

    s_icPreInit();

    // Set up interrupt to indicate second-level interrupt to PMU
    pendingIrqStat = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _SECOND, 1U, pendingIrqStat);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSSET, pendingIrqStat);

    // Set up PMU interrupt to indicate RM command; using UTF_IO since REG_WR behavior has been overridden
    pendingPmuIntr = FLD_SET_DRF_NUM(_CPWR_PMU, _INTR_1, _CMD, 1U, pendingPmuIntr);
    UTF_IO_CACHE_WRITE(LW_CPWR_PMU_INTR_1, sizeof(LwU32), pendingPmuIntr);
}

/*!
 * @brief   Pre-test setup for icService_GMXXX testing (IgnoreIrrelevantCommands).
 *
 * @details Calls PreInit function to initialize interrupts, then sets interrupt
 *          registers to indicate multiple first-level interrupts that are either
 *          not meant for the PMU or are arriving via IRQ0 and thus shouldn't be handled
 *          by icService. Returns the IrqStat register value that should be left
 *          untouched by icService_GMXXX.
 */
static LwU32 s_icPreTestIgnoreIrrelevantCommands(void)
{
    LwU32 expectedIrqStat = 0U;

    s_icPreInit();

    // Set interrupts that should be ignored to pending
    expectedIrqStat = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT);
    expectedIrqStat |= DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _GPTMR,  1U) |
                       DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _HALT,   1U) |
                       DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _SWGEN0, 1U) |
                       DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _CTXE,   1U) |
                       DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _LIMITV, 1U);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSTAT, expectedIrqStat);

    return expectedIrqStat;
}

/*!
 * @brief   Ensure icService_GMXXX services RM commands
 *
 * @details Ensures that icService_GMXXX processes an interrupt indicating an RM Command by
 *          forwarding a command dispatch to the CmdMgmt task.
 */
UT_CASE_RUN(PMU_IC, IcServiceRmCommand)
{
    LwU32 pendingIrqStat = 0U;
    DISPATCH_CMDMGMT dispatch;

    s_icPreTestServiceRmCommand();
    icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));

    // Make sure command dispatch was sent
    UT_ASSERT(lwrtosQueueIdSendFromISR_MOCK_fake.call_count == 1U);
    (void)memcpy(&dispatch,
                 &LwrtosMockQueueConfig.lwrtosQueueIdSendFromISRConfig.queuedItems[0U],
                 sizeof(dispatch));
    UT_ASSERT(lwrtosQueueIdSendFromISR_MOCK_fake.arg0_val == LWOS_QUEUE_ID(PMU, CMDMGMT));
    UT_ASSERT(dispatch.disp2unitEvt == DISP2UNIT_EVT_RM_RPC);

    // Make sure that serviced interrupt is cleared
    pendingIrqStat = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT);
    UT_ASSERT(!FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _SECOND, 1U, pendingIrqStat));
    UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_PMU_INTR_1), 0U);
}

/*!
 * @brief   Pre-test setup for icService_GMXXX testing (IcServiceWatchdogTimer).
 *
 * @details Calls PreInit function to initialize interrupts, then sets interrupt
 *          registers to indicate a pending interrupt for the watchdog timer
 *          (WDTMR)
 */
static void
IcServiceWatchdogTimerPreTest(void)
{
    LwU32 pendingIrqStat = 0U;
    LwU32 pendingPmuIntr = 0U;

    lwosWatchdogMockInit();

    s_icPreInit();

    // Set up interrupt to indicate WDTMR interrupt to PMU
    pendingIrqStat = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _WDTMR, 1U, pendingIrqStat);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSSET, pendingIrqStat);
}

/*!
 * @brief   Ensure icService_GMXXX services watchdog timer
 *
 * @details Ensures that icService_GMXXX processes an interrupt indicating
 *          the watchdog timer fired by calling into the watchdog.
 */
UT_CASE_RUN(PMU_IC, IcServiceWatchdogTimer)
{
    if (PMUCFG_FEATURE_ENABLED(PMUTASK_WATCHDOG))
    {
        LwU32 pendingIrqStat = 0U;

        IcServiceWatchdogTimerPreTest();

        icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));

        // Make sure notification was forwarded to the watchdog
        UT_ASSERT_EQUAL_UINT(lwosWatchdogCallbackExpired_MOCK_fake.call_count, 1U);

        // Make sure that serviced interrupt is cleared
        pendingIrqStat = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT);
        UT_ASSERT(!FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _WDTMR, 1U, pendingIrqStat));
    }
}

/*!
 * @brief   Pre-test setup for icService_GMXXX testing (IcServiceTherm).
 *
 * @details Calls PreInit function to initialize interrupts, then sets interrupt
 *          registers to indicate a pending interrupt for the therm interrupt
 *          (THERM)
 */
static void
IcServiceThermPreTest(void)
{
    LwU32 pendingIrqStat = 0U;
    LwU32 pendingPmuIntr = 0U;

    thermHalMockInit();

    s_icPreInit();

    // Set up interrupt to indicate THERM interrupt to PMU
    pendingIrqStat = FLD_SET_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _THERM, 1U, pendingIrqStat);
    REG_WR32(CSB, LW_CMSDEC_FALCON_IRQSSET, pendingIrqStat);
}

/*!
 * @brief   Ensure icService_GMXXX services therm interrupts
 *
 * @details Ensures that icService_GMXXX processes an interrupt indicating
 *          the therm interrupt fired by calling into the the therm module.
 */
UT_CASE_RUN(PMU_IC, IcServiceTherm)
{
    LwU32 pendingIrqStat = 0U;

    IcServiceThermPreTest();

    icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));

    if (PMUCFG_FEATURE_ENABLED(PMU_THERM_INTR))
    {
        // Make sure notification was forwarded to therm
        UT_ASSERT_EQUAL_UINT(thermService_MOCK_fake.call_count, 1U);
    }

    // Make sure that serviced interrupt is cleared
    pendingIrqStat = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT);
    UT_ASSERT(!FLD_TEST_DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _THERM, 1U, pendingIrqStat));
}

/*!
 * @brief   Ensure interrupts not routed to PMU through IRQ0 are ignored by icService_GMXXX
 *
 * @details Ensures that icService_GMXXX ignores interrupts whose destination is
 *          not the PMU or that are received through IRQ1 by checking that their
 *          pending bits are not cleared.
 */
UT_CASE_RUN(PMU_IC, IcIgnoreIrrelevantCommands)
{
    LwU32 pendingIrqStat = 0U;
    LwU32 expectedIrqStat = 0U;

    expectedIrqStat = s_icPreTestIgnoreIrrelevantCommands();
    icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));

    // Make sure irrelevant interrupts were not cleared
    pendingIrqStat = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSTAT);
    UT_ASSERT(pendingIrqStat == expectedIrqStat);
}

/*!
 * @brief   Pre-test setup for icService_GMXXX testing (IcIntr1UnhandledPreTest).
 *
 * @details Calls PreInit function to initialize interrupts, then sets interrupt
 *          registers to indicate a pending interrupt for the "SECOND"
 *          interrupt's 0th sub-interrupt. This interrupt is (lwrrently) never
 *          expected to be handled.
 */
void
IcIntr1UnhandledPreTest(void)
{
    s_icPreInit();

    // Set up interrupt to indicate second-level interrupt to PMU
    REG_WR32(CSB,
             LW_CMSDEC_FALCON_IRQSSET,
             DRF_NUM(_CMSDEC_FALCON, _IRQSTAT, _SECOND, 1U));

    //
    // Set up PMU interrupt to indicate an interrupt on bit 0. We lwrrently
    // expect this interrupt to never be handled, on any chip/profile, so use it
    // as a proxy for all unhandled interrupts.
    //
    UTF_IO_CACHE_WRITE(LW_CPWR_PMU_INTR_1,
                       sizeof(LwU32),
                       LWBIT32(0U));

}

/*!
 * @brief Ensure INTR_1 interrupts not expected to be fired halt the PMU
 */
UT_CASE_RUN(PMU_IC, IcIntr1Unhandled)
{
    LwU32 intr1;

    IcIntr1UnhandledPreTest();

    icService_HAL(&Ic, icGetPendingUprocInterrupts_HAL(&Ic));

    //
    // TODO: For now, just ensure that the interrupts are cleared. In the
    // future, we want to HALT the PMU on unhandled interrupts. This test should
    // be updated once that code is implemented.
    //
    // UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1U);
    UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_PMU_INTR_1), 0U);
}
