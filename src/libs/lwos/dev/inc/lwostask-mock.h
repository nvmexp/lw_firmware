/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_TASK_MOCK_H
#define LWOS_TASK_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "fff.h"
#include "flcnifcmn.h"
#include "lwrtos.h"
#include "lw_rtos_extension.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
#define LWOS_TASK_MOCK_TASK_CREATE_MAX_CALLS                     ((LwLength) 4U)

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Configuration data for mocking code in @ref lwostask.c
 */
typedef struct
{
    /*!
     * @brief   Mocking configuration for @ref lwosTaskCreateConfig
     */
    struct
    {
        /*!
         * @brief   Error codes to return instead of mocking functionality
         */
        FLCN_STATUS errorCodes[LWOS_TASK_MOCK_TASK_CREATE_MAX_CALLS];
    } lwosTaskCreateConfig;
} LWOS_TASK_MOCK_CONFIG;
/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the LWOS Task mocking configuration
 */
extern LWOS_TASK_MOCK_CONFIG LwosTaskMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @copydoc lwosTaskCreate
 *
 * @note    Mock implementation dependent on @ref LwosTaskMockConfig
 */
FLCN_STATUS lwosTaskCreate_MOCK(
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
   LwBool bEnableImemOnDemandPaging);

/*!
 * @copydoc osFlcnPrivLevelLwrrTaskSet
 *
 * @note    Mock implementation of @ref osFlcnPrivLevelLwrrTaskSet
 */
DECLARE_FAKE_VOID_FUNC(osFlcnPrivLevelLwrrTaskSet_MOCK, LwU8, LwU8);

/*!
 * @copydoc osFlcnPrivLevelLwrrTaskGet
 *
 * @note    Mock implementation of @ref osFlcnPrivLevelLwrrTaskGet
 */
DECLARE_FAKE_VOID_FUNC(osFlcnPrivLevelLwrrTaskGet_MOCK, LwU8 *, LwU8 *);

/*!
 * @copydoc lwosInit
 *
 * @note    Mock implementation of @ref lwosInit
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, lwosInit_MOCK)

/*!
 * @copydoc osTaskIdGet
 *
 * @note    Mock implementation of @ref osTaskIdGet
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, osTaskIdGet_MOCK)

/*!
 * @brief   Initializes the state of LWOS Task mocking
 */
void lwosTaskMockInit(void);

#endif // LWOS_TASK_MOCK_H
