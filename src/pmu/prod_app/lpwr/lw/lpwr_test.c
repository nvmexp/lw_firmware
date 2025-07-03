/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file lpwr_test.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"
#include "lwostimer.h"
#include "pmu_oslayer.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objlpwr.h"
#include "pmu_lpwr_test.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Macros ----------------------------------------- */
#ifdef UPROC_RISCV
// Allocate 128K on riscv enabled profile 0x8000 * 4(size of int)
#define LPWR_TEST_PAGE_FAULT_BUFFER_SIZE_MAX    0x8000
#else
// Allocate 20K on falcon enabled profile 0x1400 * 4(size of int)
#define LPWR_TEST_PAGE_FAULT_BUFFER_SIZE_MAX    0x1400
#endif // UPROC_RISCV

/* ------------------------- Global Variables ------------------------------- */
// LPWR tests callback structure.
OS_TMR_CALLBACK lpwrTestCallback;

// Buffer for pagefault test.
LwU32 lpwrTestPageFaultBuffer[LPWR_TEST_PAGE_FAULT_BUFFER_SIZE_MAX]
      GCC_ATTRIB_SECTION("dmem_lpwrTest", "lpwrTestPageFaultBuffer");

// Scratch data buffer of size 4K
LwU8 lpwrTestScratchBuffer[RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX]
     GCC_ATTRIB_SECTION("dmem_lpwrTest", "lpwrTestScratchBuffer");

// Error flag for LPWR Test
LwU32 lpwrTestError;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static OsTmrCallbackFunc (s_lpwrTestCallback)
                         GCC_ATTRIB_USED()
                         GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestCallback");

static FLCN_STATUS s_lpwrTestSoftCriticalSection(void)
                   GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestSoftCriticalSection");
static FLCN_STATUS s_lpwrTestHardCriticalSection(void)
                   GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestHardCriticalSection");
static FLCN_STATUS s_lpwrTestDmaOperation(void)
                   GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestDmaOperation");
static FLCN_STATUS s_lpwrTestPageFault(void)
                   GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestPageFault");
static LwBool      s_lpwrTestWaitForMsEngage(void)
                   GCC_ATTRIB_SECTION("imem_lpwrTest", "s_lpwrTestWaitForMsEngage");

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Initialize LPWR Test
 *
 * @param[in] supportMask    Mask of supported tests.
 * @param[in] callbackTimeMs Callback time period.
 *
 * @return    FLCN_OK         On success.
 * @return    FLCN_ERR_XXX    Otherwise.
 */
FLCN_STATUS
lpwrTestInit
(
    LwU32 supportMask,
    LwU32 callbackTimeMs
)
{
    FLCN_STATUS status    = FLCN_ERROR;
    LPWR_TEST  *pLpwrTest = LPWR_GET_TEST();

    // Allocate LPWR TEST info
    if (pLpwrTest == NULL)
    {
        pLpwrTest = lwosCallocType(OVL_INDEX_DMEM(lpwrTest), 1, LPWR_TEST);
        if (pLpwrTest == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;

            goto lpwrTestInit_exit;
        }

        Lpwr.pLpwrTest = pLpwrTest;
    }

    pLpwrTest->callbackTimeMs = callbackTimeMs;

    // Create Timer callback to execute LPWR tests.
    if (!OS_TMR_CALLBACK_WAS_CREATED(&lpwrTestCallback))
    {
        status = osTmrCallbackCreate(&lpwrTestCallback,                                      // LPWR Test TIMER callback structure
                                     OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                                     OVL_INDEX_ILWALID,                                      // ovlImem
                                     s_lpwrTestCallback,                                     // pTmrCallbackFunc
                                     LWOS_QUEUE(PMU, LPWR_LP),                               // queueHandle
                                     callbackTimeMs * 1000,                                  // periodNormalus
                                     callbackTimeMs * 1000,                                  // periodSleepus
                                     OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                                     RM_PMU_TASK_ID_LPWR_LP);                                // taskId
        if (status != FLCN_OK)
        {
            goto lpwrTestInit_exit;
        }
    }

    pLpwrTest->lwrrentTestId = RM_PMU_LPWR_TEST_ID_ILWALID;
    // Store support mask for test specific initialize, if required
    pLpwrTest->supportMask = supportMask;

lpwrTestInit_exit:

    return status;
}

/*!
 * @brief Check LPWR Test is supported
 *
 * @param[in] testId    Test to validate support.
 *
 * @return    LW_TRUE   If Test is supported.
 * @return    LW_FALSE  Otherwise.
 */
LwBool
lpwrTestIsSupported
(
    LwU32 testId
)
{
    LPWR_TEST *pLpwrTest = LPWR_GET_TEST();

    if ((testId < RM_PMU_LPWR_TEST_ID_MAX) &&
        (pLpwrTest->supportMask & BIT(testId)))
    {
        return LW_TRUE;
    }
    else
    {
        return LW_FALSE;
    }
}

/*!
 * @brief Start the LPWR test exelwtion.
 *
 * @param[in] testId         LPWR test ID based on RM_PMU_LPWR_TEST_ID_xyz.
 *
 * @return    FLCN_OK        On success.
 * @return    FLCN_ERR_XXX   Otherwise.
 */
FLCN_STATUS
lpwrTestStart
(
    LwU32 testId
)
{
    FLCN_STATUS status    = FLCN_OK;
    LPWR_TEST  *pLpwrTest = LPWR_GET_TEST();

    if (pLpwrTest->lwrrentTestId != RM_PMU_LPWR_TEST_ID_ILWALID)
    {
        // The previously running test should have been stopped
        // before scheduling a new test.
        status = FLCN_ERR_ILLEGAL_OPERATION;
        goto lpwrTestStart_exit;
    }

    // Cache Test to run.
    pLpwrTest->lwrrentTestId = testId;
    status = osTmrCallbackSchedule(&lpwrTestCallback);

lpwrTestStart_exit:

    return status;
}

/*!
 * @brief Stop the LPWR test exelwtion.
 *
 * @param[in] testId LPWR test ID based on RM_PMU_LPWR_TEST_ID_xyz.
 *
 * @return    FLCN_OK       On success.
 * @return    FLCN_ERR_XXX  Otherwise.
 */
FLCN_STATUS
lpwrTestStop
(
    LwU32 testId
)
{
    FLCN_STATUS status    = FLCN_OK;
    LPWR_TEST  *pLpwrTest = LPWR_GET_TEST();

    if (testId != pLpwrTest->lwrrentTestId)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto lpwrTestStop_exit;
    }

    pLpwrTest->lwrrentTestId = RM_PMU_LPWR_TEST_ID_ILWALID;
    osTmrCallbackCancel(&lpwrTestCallback);

lpwrTestStop_exit:

    return status;
}

/*!
 * @ref OsTmrCallback
 *
 * @brief Handles exelwtion of requested LPWR test.
 *
 * @param[in] pCallback Pointer to OS_TMR_CALLBACK
 */
static FLCN_STATUS
s_lpwrTestCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS status = FLCN_OK;
    LPWR_TEST  *pLpwrTest = LPWR_GET_TEST();

    lpwrTestError = LPWR_TEST_SUCCESS;
    switch (pLpwrTest->lwrrentTestId)
    {
        case RM_PMU_LPWR_TEST_ID_MS_ODP_DMA:
             status = s_lpwrTestDmaOperation();
            break;

        case RM_PMU_LPWR_TEST_ID_MS_ODP_HARD_CRITICAL:
            status = s_lpwrTestHardCriticalSection();
            break;

        case RM_PMU_LPWR_TEST_ID_MS_ODP_SOFT_CRITICAL:
             status = s_lpwrTestSoftCriticalSection();
            break;

        case RM_PMU_LPWR_TEST_ID_MS_ODP_PAGE_FAULT:
             status = s_lpwrTestPageFault();
            break;

        case RM_PMU_LPWR_TEST_ID_ILWALID:
        default:
             status = FLCN_ERR_NOT_SUPPORTED;
             lpwrTestError = LPWR_TEST_ERROR_NOT_SUPPORTED;
            break;
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief Wait for MSCG to engage.
 *
 * @return FLCN_OK      On Success.
 * @return FLCN_ERR_XXX Otherwise.
 */
static LwBool
s_lpwrTestWaitForMsEngage(void)
{
    FLCN_TIMESTAMP startTimeNs;
    LwBool         bEngaged = LW_TRUE;

    osPTimerTimeNsLwrrentGet(&startTimeNs);
    while (!PG_IS_ENGAGED(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
    {
        if (osPTimerTimeNsElapsedGet(&startTimeNs) > LPWR_TEST_WAIT_TIMEOUT_FOR_MSCG_ENGAGE_NS)
        {
            bEngaged = LW_FALSE;
            break;
        }
    }

    return bEngaged;
}

/*!
 * @brief Validates Soft critical section code path.
 *
 * @return FLCN_OK      On Success.
 * @return FLCN_ERR_XXX Otherwise.
 */
static FLCN_STATUS
s_lpwrTestSoftCriticalSection(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (!s_lpwrTestWaitForMsEngage())
    {
        lpwrTestError = LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT;
        status = FLCN_OK;
        goto s_lpwrTestSoftCriticalSection_exit;
    }

    // Suspend scheduler by waking MSCG if engaged and disallow MSCG until resume scheduler.
    appTaskSchedulerSuspend();

    // Check if DMA is resumed.
    if (!lwosDmaIsSuspended())
    {
        status = FLCN_OK;
    }
    else
    {
        // Set Error, if DMA is supended, after call to appTaskSchedulerSuspend
        lpwrTestError = LPWR_TEST_ERROR_DMA_SUSP_AFTER_SOFT_CRITICAL_ENTER;
    }

    // Resume scheduler and allow MSCG
    appTaskSchedulerResume();

s_lpwrTestSoftCriticalSection_exit:

    return status;
}

/*!
 * @brief Validates Hard critical section code path.
 *
 * @return FLCN_OK      On Success.
 * @return FLCN_ERR_XXX Otherwise.
 */
static FLCN_STATUS
s_lpwrTestHardCriticalSection(void)
{
    FLCN_STATUS status = FLCN_ERROR;

    if (!s_lpwrTestWaitForMsEngage())
    {
        lpwrTestError = LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT;
        status = FLCN_OK;
        goto s_lpwrTestHardCriticalSection_exit;
    }

    // Enter critical and exit MSCG if engaged and disallow MSCG until critical section exit.
    appTaskCriticalEnter();

    if (!lwosDmaIsSuspended())
    {
        status = FLCN_OK;
    }
    else
    {
        // Set Error, if DMA is supended, after call to appTaskCriticalEnter
        lpwrTestError = LPWR_TEST_ERROR_DMA_SUSP_AFTER_HARD_CRITICAL_ENTER;
    }

    // Exit critical section and allow MSCG
    appTaskCriticalExit();

s_lpwrTestHardCriticalSection_exit:

    return status;
}

/*!
 * @brief Validates DMA code path.
 *
 * @return FLCN_OK      On Success.
 * @return FLCN_ERR_XXX Otherwise.
 */
static FLCN_STATUS
s_lpwrTestDmaOperation(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       bufIdx;

    if (!s_lpwrTestWaitForMsEngage())
    {
        lpwrTestError = LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT;
        goto s_lpwrTestDmaOperation_exit;
    }

    // Populate data to write into lpwrTestSurface
    for (bufIdx = 0; bufIdx < RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX; bufIdx++)
    {
        lpwrTestScratchBuffer[bufIdx] = bufIdx % 255;
    }

    if (!s_lpwrTestWaitForMsEngage())
    {
        lpwrTestError = LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT;
        goto s_lpwrTestDmaOperation_exit;
    }

    // Write to FB over DMA, This should wake MSCG due to FB access
    status = ssurfaceWr(&lpwrTestScratchBuffer,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.lpwr.lpwrTestSurface.fbBuffer),
        RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX);
    if (status != FLCN_OK)
    {
        // Set Error, if DMA write failed.
        lpwrTestError = LPWR_TEST_ERROR_DMA_WRITE;
        goto s_lpwrTestDmaOperation_exit;
    }

    memset(lpwrTestScratchBuffer, 0xff, sizeof(LwU8)*RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX);

    // Read from FB over DMA, This should wake MSCG due to FB access
    (void)ssurfaceRd(&lpwrTestScratchBuffer,
                  RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(all.lpwr.lpwrTestSurface.fbBuffer),
                  RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX);

    // Compare the data read from FB.
    for (bufIdx = 0; bufIdx < RM_PMU_LPWR_TEST_FB_BUFFER_SIZE_MAX; bufIdx++)
    {
        if (lpwrTestScratchBuffer[bufIdx] != (bufIdx % 255))
        {
            // Set Error, if DMA read has data mismatch.
            lpwrTestError = LPWR_TEST_ERROR_DMA_READ_DATA_MISMATCH;
            status = FLCN_ERR_DMA_GENERIC;
            break;
        }
    }

s_lpwrTestDmaOperation_exit:

    return status;
}

/*!
 * @brief Validates page fault service code path.
 *
 * @return FLCN_OK      On Success.
 * @return FLCN_ERR_XXX Otherwise.
 */
static FLCN_STATUS
s_lpwrTestPageFault(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 bufIdx;

    if (!s_lpwrTestWaitForMsEngage())
    {
        lpwrTestError = LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT;
        return status;
    }

    for (bufIdx = 0; bufIdx < LPWR_TEST_PAGE_FAULT_BUFFER_SIZE_MAX; bufIdx++)
    {
        // Write some data in non-resident large memory to inject page fault.
        lpwrTestPageFaultBuffer[bufIdx] = 10 * (bufIdx + 1);
    }

    return status;
}
