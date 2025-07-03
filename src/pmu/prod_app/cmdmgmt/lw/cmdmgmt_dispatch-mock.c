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
 * @file   cmdmgmt_dispatch-mock.c
 * @brief  Mock implementations of the FbQueue routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_dispatch-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global dispatch mocking configuration private to the mocking
 */
typedef struct 
{
    /*!
     * @brief   Private configuration data for @ref cmdmgmtExtCmdDispatch
     */
    struct
    {
        /*!
         * @brief   Number of calls to @ref cmdmgmtExtCmdDispatch during test.
         */
        LwLength        numCalls;

        /*!
         * @brief   Events dispatched via @ref cmdmgmtExtCmdDispatch during test
         */
        DISP2UNIT_RM_RPC dispatchedCmds[CMDMGMT_DISPATCH_MOCK_MAX_CMDS];

        /*!
         * @brief   Number of commands sent via @ref cmdmgmtExtCmdDispatch during test
         */
        LwLength        numDispatched;

        /*!
         * @brief   Number of commands retrieved via
         *          @ref cmdmgmtDispatchMockGetDispatched
         */
        LwLength        numRetrieved;
    } cmdmgmtExtCmdDispatchConfig;
} CMDMGMT_DISPATCH_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
CMDMGMT_DISPATCH_MOCK_CONFIG CmdMgmtDispatchMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of the private CmdMgmt Dispatch mocking configuration
 */
static CMDMGMT_DISPATCH_MOCK_CONFIG_PRIVATE CmdMgmtDispatchMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
cmdmgmtExtCmdDispatch_MOCK
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    const LwLength lwrrentCall = CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numCalls;
    const LwLength lwrrentDispatched = CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numDispatched;

    if (lwrrentCall == CMDMGMT_DISPATCH_MOCK_MAX_CALLS)
    {
        UT_FAIL("CmdMgmt Dispatch mocking cannot support enough calls for current test.\n");
    }
    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numCalls++;

    if (CmdMgmtDispatchMockConfig.cmdmgmtExtCmdDispatchConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return CmdMgmtDispatchMockConfig.cmdmgmtExtCmdDispatchConfig.errorCodes[lwrrentCall];
    }

    if (lwrrentDispatched == CMDMGMT_DISPATCH_MOCK_MAX_CMDS)
    {
        UT_FAIL("CmdMgmt Dispatch mocking cannot support enough commands for current test.\n");
    }
    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numDispatched++;

    (void)memcpy(&CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.dispatchedCmds[lwrrentDispatched],
                 pRequest,
                 sizeof(*pRequest));

    return FLCN_OK;
}

void
cmdmgmtDispatchMockInit(void)
{
    LwLength i;
    for (i = 0U; i < LW_ARRAY_ELEMENTS(CmdMgmtDispatchMockConfig.cmdmgmtExtCmdDispatchConfig.errorCodes); i++)
    {
        CmdMgmtDispatchMockConfig.cmdmgmtExtCmdDispatchConfig.errorCodes[i] = FLCN_OK;
    }

    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numCalls = 0U;
    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numDispatched = 0U;
    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numRetrieved = 0U;
}

FLCN_STATUS
cmdmgmtDispatchMockGetDispatched
(
    DISP2UNIT_RM_RPC *pDispatch
)
{
    const LwLength lwrrentRetrieved = CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numRetrieved;
    if (lwrrentRetrieved == CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numDispatched)
    {
        return FLCN_ERR_ILWALID_STATE;
    }
    CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.numRetrieved++;

    (void)memcpy(pDispatch,
                 &CmdMgmtDispatchMockConfigPrivate.cmdmgmtExtCmdDispatchConfig.dispatchedCmds[lwrrentRetrieved],
                 sizeof(*pDispatch));

    return FLCN_OK;
}

/* ------------------------- Static Functions  ------------------------------ */
