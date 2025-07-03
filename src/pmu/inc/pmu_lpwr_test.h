/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_LPWR_TEST_H
#define PMU_LPWR_TEST_H

/*!
 * @file pmu_lpwr_test.h
 */

/* ------------------------ System includes -------------------------------- */
#include "flcntypes.h"
#include "pmu/pmuif_lpwr_test.h"
/* ------------------------ Application includes --------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief Macro to wait for 350 nano seconds for MSCG engagement.
 */
#define LPWR_TEST_WAIT_TIMEOUT_FOR_MSCG_ENGAGE_NS         (350 * 1000)

/* ------------------------ User defines ----------------------------------- */
/*!
 * @brief LPWR test status codes.
 */
enum {
    LPWR_TEST_SUCCESS                              = 0,
    LPWR_TEST_ERROR_NOT_SUPPORTED                     ,
    LPWR_TEST_ERROR_DMA_SUSP_AFTER_HARD_CRITICAL_ENTER,
    LPWR_TEST_ERROR_DMA_SUSP_AFTER_SOFT_CRITICAL_ENTER,
    LPWR_TEST_ERROR_DMA_WRITE                         ,
    LPWR_TEST_ERROR_DMA_READ_DATA_MISMATCH            ,
    LPWR_TEST_ERROR_MS_ENGAGE_TIMEOUT
};

/*!
 * @brief Structure for LPWR test.
 */
typedef struct
{
    // Mask of all supported tests.
    LwU32 supportMask;

    // Test selected to run.
    LwU32 lwrrentTestId;

    // Test scheduling callback time period.
    LwU32 callbackTimeMs;
} LPWR_TEST;

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS lpwrTestInit(LwU32 testSupportMask, LwU32 testCallbackTimeMs)
            GCC_ATTRIB_SECTION("imem_lpwrTest", "lpwrTestInit");
LwBool lpwrTestIsSupported(LwU32 testId)
            GCC_ATTRIB_SECTION("imem_lpwrTest", "lpwrTestIsSupported");
FLCN_STATUS lpwrTestStart(LwU32 testId)
            GCC_ATTRIB_SECTION("imem_lpwrTest", "lpwrTestStart");
FLCN_STATUS lpwrTestStop(LwU32 testId)
            GCC_ATTRIB_SECTION("imem_lpwrTest", "lpwrTestStop");

#endif // PMU_LPWR_TEST_H
