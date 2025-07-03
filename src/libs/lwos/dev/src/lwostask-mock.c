/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwostask-mock.c
 * @brief  Mock implementations of the LWOS task routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwrtos.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwostask-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global LWOS task mocking configuration private to the mocking
 */
typedef struct 
{
    /*!
     * @brief   Mocking configuration for @ref lwosTaskCreate
     */
    struct
    {
        /*!
         * @brief   Number of calls during test
         */
        LwLength numCalls;

        /*!
         * @brief   Next task handle to use for allocation
         */
        LwrtosTaskHandle nextTaskHandle;
    } lwosTaskCreateConfig;
} LWOS_TASK_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
LWOS_TASK_MOCK_CONFIG LwosTaskMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of the private LWOS Task mocking configuration
 */
static LWOS_TASK_MOCK_CONFIG_PRIVATE LwosTaskMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
lwosTaskCreate_MOCK
(
    LwrtosTaskHandle *pxTaskHandle,
    PRM_RTOS_TCB_PVT pTcbPvt,
    LwrtosTaskFunction pvTaskCode,
    LwU8 taskID,
    LwU16 usStackDepth,
    LwBool bUsingFPU,
    LwU32 uxPriority,
    LwU8 privLevelExt,
    LwU8 privLevelCsb,
    LwU8 ovlCntImemLS,
    LwU8 ovlCntImemHS,
    LwU8 ovlIdxImem,
    LwU8 ovlCntDmem,
    LwU8 ovlIdxStack,
    LwU8 ovlIdxDmem,
    LwBool bEnableImemOnDemandPaging
)
{
    const LwLength lwrrentCall = LwosTaskMockConfigPrivate.lwosTaskCreateConfig.numCalls;
    if (lwrrentCall == LWOS_TASK_MOCK_TASK_CREATE_MAX_CALLS)
    {
        UT_FAIL("LWOS Task mocking cannot support enough calls to lwosTaskCreate for current test.");
    }
    LwosTaskMockConfigPrivate.lwosTaskCreateConfig.numCalls++;

    if (LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes[lwrrentCall];
    }

    *pxTaskHandle = LwosTaskMockConfigPrivate.lwosTaskCreateConfig.nextTaskHandle;
    LwosTaskMockConfigPrivate.lwosTaskCreateConfig.nextTaskHandle = (LwrtosTaskHandle)
        (((LwUPtr)LwosTaskMockConfigPrivate.lwosTaskCreateConfig.nextTaskHandle) + 1U);

    return FLCN_OK;
}

void
lwosTaskEventErrorHandler_MOCK
(
    FLCN_STATUS status
)
{
    //
    // TODO: existing tests did not need any functionality in here. Add more
    // here if necessary.
    //
}

DEFINE_FAKE_VOID_FUNC(osFlcnPrivLevelLwrrTaskSet_MOCK, LwU8, LwU8);

DEFINE_FAKE_VOID_FUNC(osFlcnPrivLevelLwrrTaskGet_MOCK, LwU8 *, LwU8 *);

void
lwosTaskMockInit(void)
{
    LwLength i;

    for (i = 0U;
         i < LW_ARRAY_ELEMENTS(LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes);
         i++)
    {
        LwosTaskMockConfig.lwosTaskCreateConfig.errorCodes[i] = FLCN_OK;
        RESET_FAKE(lwosInit_MOCK)
}
    LwosTaskMockConfigPrivate.lwosTaskCreateConfig.numCalls = 0U;
    LwosTaskMockConfigPrivate.lwosTaskCreateConfig.nextTaskHandle = (LwrtosTaskHandle)0U;

    RESET_FAKE(osTaskIdGet_MOCK)
    osTaskIdGet_MOCK_fake.return_val = 0;
}

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, lwosInit_MOCK)

DEFINE_FAKE_VALUE_FUNC(LwU8, osTaskIdGet_MOCK)

/* ------------------------- Static Functions  ------------------------------ */
