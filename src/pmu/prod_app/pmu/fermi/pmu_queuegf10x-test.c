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
 * @file    pmu_queuegf10x-test.c
 * @brief   Unit tests for logic in pmu_queuegf10x.c 
 */

/* ------------------------ Mocked Function Prototypes ---------------------- */
/* ------------------------ Unit-Under-Test --------------------------------- */
/* ------------------------ Includes ---------------------------------------- */
#include "pmusw.h"
#include "lwtypes.h"
#include "lwrtos.h"
#include "test-macros.h"
#include "regmock.h"
#include "dev_pwr_csb.h"

/* ------------------------ Mocked Functions -------------------------------- */
/* ------------------------ Type Definitions -------------------------------- */
/* ------------------------ Defines and Macros ------------------------------ */
/* ------------------------ Static Functions -------------------------------- */
/* ------------------------ Tests ------------------------------------------- */
UT_SUITE_DEFINE(PMU_QUEUE,
                UT_SUITE_SET_COMPONENT("PMU Core")
                UT_SUITE_SET_DESCRIPTION("Tests Core PMU module")
                UT_SUITE_SET_OWNER("jorgeo"))

UT_CASE_DEFINE(PMU_QUEUE, PmuQueueInitCorrect,
                UT_CASE_SET_DESCRIPTION("Ensure pmuQueue code inits head and tail registers correctly")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_QUEUE, PmuQueueHeadSetCorrect,
                UT_CASE_SET_DESCRIPTION("Ensure pmuQueue code sets head register correctly and triggers interrupt")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_QUEUE, PmuQueueHeadGetCorrect,
                UT_CASE_SET_DESCRIPTION("Ensure pmuQueue code gets head register correctly")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

UT_CASE_DEFINE(PMU_QUEUE, PmuQueueTailGetCorrect,
                UT_CASE_SET_DESCRIPTION("Ensure pmuQueue code gets tail register correctly")
                UT_CASE_SET_TYPE(REQUIREMENTS)
                UT_CASE_LINK_REQS("TODO")
                UT_CASE_LINK_SPECS("TODO"))

/*!
 * @brief   Ensure pmuQueue code inits head and tail registers correctly
 *
 * @details Ensures that pmuQueuePmuToRmInit will correctly initialize 
 *          the LW_CPWR_PMU_MSGQ_HEAD & LW_CPWR_PMU_MSGQ_TAIL 
 *          registers.
 */
UT_CASE_RUN(PMU_QUEUE, PmuQueueInitCorrect)
{
    LwU32 headTailStart = 0xDEADBEEF;
    LwU32 pendingIrq;

    // Make sure head and tail registers are correctly initialized
    pmuQueuePmuToRmInit_GMXXX(headTailStart);
    UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_PMU_MSGQ_HEAD), headTailStart);
    UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_PMU_MSGQ_TAIL), headTailStart);
}

/*!
 * @brief   Ensure pmuQueue sets head register correctly and triggers interrupt
 *
 * @details Ensures that pmuQueuePmuToRmHeadSet will correctly
 *          update the MSGQ_HEAD register as well as trigger an
 *          interrupt to host.
 */
UT_CASE_RUN(PMU_QUEUE, PmuQueueHeadSetCorrect)
{
    LwU32 headReg = 0xBEEFDEAD;
    LwU32 pendingIrq;

    // Make sure head is updated and interrupt is triggered
    pmuQueuePmuToRmHeadSet_GMXXX(headReg);
    UT_ASSERT_EQUAL_UINT(REG_RD32(CSB, LW_CPWR_PMU_MSGQ_HEAD), headReg);

    pendingIrq = REG_RD32(CSB, LW_CMSDEC_FALCON_IRQSSET);
    UT_ASSERT(FLD_TEST_DRF(_CMSDEC_FALCON, _IRQSSET, _SWGEN0, _SET, pendingIrq));

}

/*!
 * @brief   Ensure pmuQueue code gets head register correctly
 *
 * @details Ensures that pmuQueuePmuToRmHeadGet correctly returns
 *          the value of the MSGQ_HEAD register.
 */
UT_CASE_RUN(PMU_QUEUE, PmuQueueHeadGetCorrect)
{
    LwU32 headReg = 0xDEADBEEF;
    LwU32 headRegTest;

    REG_WR32(CSB, LW_CPWR_PMU_MSGQ_HEAD, headReg);

    headRegTest = pmuQueuePmuToRmHeadGet_GMXXX();
    UT_ASSERT_EQUAL_UINT(headRegTest, headReg);
}

/*!
 * @brief   Ensure pmuQueue code gets tail register correctly
 *
 * @details Ensures that pmuQueuePmuToRmTailGet correctly returns
 *          the value of the MSGQ_TAIL register.
 */
UT_CASE_RUN(PMU_QUEUE, PmuQueueTailGetCorrect)
{
    LwU32 tailReg = 0xDEADBEEF;
    LwU32 tailRegTest;

    REG_WR32(CSB, LW_CPWR_PMU_MSGQ_TAIL, tailReg);

    tailRegTest = pmuQueuePmuToRmTailGet_GMXXX();
    UT_ASSERT_EQUAL_UINT(tailRegTest, tailReg);
}
