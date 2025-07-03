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
 * @file    pmu_objpmu-test.c
 * @brief   Unit tests for logic in pmu_objpmu.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "lwtypes.h"
#include "flcnretval.h"
#include "pmusw.h"
#include "pmu_objpmu.h"
#include "test-macros.h"
#include "regmock.h"
#include "config/pmu-config.h"
#include "osptimer.h"
#include "osptimer-mock.h"
#include "lwrtos.h"
#include "lwostask.h"
#include "pmu/pmu_pmugp10x-mock.h"
#include "pmu/pmu_pmutu10x-mock.h"

/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_OBJPMU,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests Core PMU module")
                UT_SUITE_SET_OWNER("jorgeo"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegValPositive,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 recognizes reg values meeting conditions")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegValNegative,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 rejects reg values not meeting conditions")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegModePositive,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 recognizes reg value meeting conditions for different modes")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegModeNegative,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 rejects reg value not meeting conditions for different modes")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegMaskPositive,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 recognizes reg value meeting conditions for different masks")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegMaskNegative,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 rejects reg value not meeting conditions for different masks")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCorePollRegTimeoutCorrect,
                UT_CASE_SET_DESCRIPTION("Ensure PollReg32 passes correct timeout along")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCoreConstructSuccess,
                UT_CASE_SET_DESCRIPTION("Tests constructPmu on success")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_OBJPMU, PmuCoreConstructDmemVaInitFail,
                UT_CASE_SET_DESCRIPTION("Tests constructPmu if DMEMVA initialization fails.")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Ensures PollReg32 recognizes register value meeting conditions
 *
 * @details Ensures that pmuPollReg32 will return LW_TRUE when the specified
 *          conditions are met for various values.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegValPositive)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0xFFFFFFFF;
    LwU32 val = 0xDEADBEEF;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;
    LwBool result;

    osPTimerMockInit();
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    val = 0;
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    val = 0xFFFFFFFF;
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    val = 0x01010101;
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);
}

/*!
 * @brief   Ensures PollReg32 rejects register value not meeting conditions
 *
 * @details Ensures that pmuPollReg32 will return LW_FALSE when the specified
 *          conditions are not met by various values.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegValNegative)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0xFFFFFFFF;
    LwU32 val = 0;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;
    LwBool result;

    REG_WR32(CSB, addr, val);

    osPTimerMockInit();
    val = 0xDEADBEEF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    val = 0xFFFFFFFF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    val = 0x10101010;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    REG_WR32(CSB, addr, val);

    osPTimerMockInit();
    val = 0xDEADBEEF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    val = 0xFFFFFFFF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    val = 0;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);
}

/*!
 * @brief   Ensures PollReg32 recognizes register value meeting conditions in
 *          different modes
 *
 * @details Ensures that pmuPollReg32 will return LW_TRUE when the specified
 *          conditions are met in various modes.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegModePositive)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0xFFFFFFFF;
    LwU32 val = 0xDEADBEEF;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;
    LwBool result;

    osPTimerMockInit();
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_CSB_NEQ;
    val = 0xDEADABCD;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_BAR0_EQ;
    REG_WR32(BAR0, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_BAR0_NEQ;
    val = 0xDEADBEEF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);
}

/*!
 * @brief   Ensures PollReg32 rejects register value not meeting conditions in
 *          different modes.
 *
 * @details Ensures that pmuPollReg32 will return LW_FALSE when the specified
 *          conditions are not met in various modes.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegModeNegative)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0xFFFFFFFF;
    LwU32 val = 0xDEADBEEF;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_NEQ;
    LwBool result;

    osPTimerMockInit();
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_CSB_EQ;
    val = 0xDEADABCD;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_BAR0_NEQ;
    REG_WR32(BAR0, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    mode = PMU_REG_POLL_MODE_BAR0_EQ;
    val = 0xDEADBEEF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);
}

/*!
 * @brief   Ensures PollReg32 recognizes register value meeting conditions with
 *          different masks
 *
 * @details Ensures that pmuPollReg32 will return LW_TRUE when the specified
 *          conditions are met for various masks.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegMaskPositive)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0xFFFFFFFF;
    LwU32 val = 0xFFFFFFFF;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;
    LwBool result;

    osPTimerMockInit();
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    mask = 0xFFFFFFFF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    val =  0x10101010;
    mask = 0x10101010;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);

    osPTimerMockInit();
    val = 0;
    mask = 0;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT(result);
}

/*!
 * @brief   Ensures PollReg32 rejects register value not meeting conditions with
 *          different masks
 *
 * @details Ensures that pmuPollReg32 will return LW_FALSE when the specified
 *          conditions are not met for various masks.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegMaskNegative)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0;
    LwU32 val = 0xDEADBEEF;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;
    LwBool result;

    osPTimerMockInit();
    REG_WR32(CSB, addr, val);
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    mask = 0xF0F0F0F0;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    mask = 0xEFFFFFFF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);

    osPTimerMockInit();
    val = 0;
    REG_WR32(CSB, addr, val);
    val= 0xFFFFFFFF;
    mask = 0xFFFFFFFF;
    result = pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_FALSE(result);
}

/*!
 * @brief   Ensures PollReg32 passes along the correct timeout
 *
 * @details Ensures that pmuPollReg32 actually passes the timeout we provide
 *          to the OS timer function.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCorePollRegTimeoutCorrect)
{
    LwU32 addr = 0x10;
    LwU32 mask = 0;
    LwU32 val = 0;
    LwU32 timeoutNs = 1;
    LwU32 mode = PMU_REG_POLL_MODE_CSB_EQ;

    osPTimerMockInit();

    REG_WR32(CSB, addr, val);
    pmuPollReg32Ns_IMPL(addr, mask, val, timeoutNs, mode);
    UT_ASSERT_EQUAL_UINT(osPTimerCondSpinWaitNs_MOCK_fake.arg2_val, timeoutNs);
}

/*
 * @brief Initializes state for @ref PmuCoreConstructSuccess
 */
static void
PmuCoreConstructSuccessPreTest(void)
{
    pmugp10xMockInit();
    pmutu10xMockInit();
}

/*!
 * @brief   Tests constructPmu on success
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCoreConstructSuccess)
{
    FLCN_STATUS status;

    PmuCoreConstructSuccessPreTest();

    status = constructPmu_IMPL();

    UT_ASSERT_EQUAL_UINT(status, FLCN_OK);
}

/*
 * @brief Initializes state for @ref PmuCoreConstructDmemVaInitFail
 */
static void
PmuCoreConstructDmemVaInitFailPreTest(void)
{
    pmugp10xMockInit();
    pmuDmemVaInit_GP10X_fake.return_val = FLCN_ERROR;

    pmutu10xMockInit();
}

/*!
 * @brief   Tests constructPmu if DMEMVA initialization fails.
 */
UT_CASE_RUN(PMU_OBJPMU, PmuCoreConstructDmemVaInitFail)
{
    FLCN_STATUS status;

    PmuCoreConstructDmemVaInitFailPreTest();

    status = constructPmu_IMPL();

    UT_ASSERT_NOT_EQUAL_UINT(status, FLCN_OK);
}

/* ------------------------ Local Mock Definitions -------------------------- */
// TODO: move these all to common files
void pmuPreInitDmemAperture_GMXXX(void)
{
}
void pmuPreInitFlcnPrivLevelCacheAndConfigure_GM20X(void)
{
}
void pmuPreInitDebugFeatures_GV10X(void)
{
}
void pmuPreInitGPTimer_GMXXX(void)
{
}
