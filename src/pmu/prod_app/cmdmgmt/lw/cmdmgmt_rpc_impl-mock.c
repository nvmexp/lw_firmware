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
 * @file   cmdmgmt_rpc_impl-mock.c
 * @brief  Mock implementations of the Falcon DMA routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_rpc_impl-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global RPC mocking configuration private to the mocking
 */
typedef struct
{
    /*!
     * @brief   Private configuration data for @ref pmuRmRpcExelwte
     */
    struct
    {
        /*!
         * @brief   Number of calls to @ref pmuRmRpcExelwte during test.
         */
        LwLength            numCalls;

        /*!
         * @brief   RPCs sent via @ref pmuRmRpcExelwte during test
         */
        PMU_RM_RPC_STRUCT   sentRpcs[CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPCS];

        /*!
         * @brief   Number of RPCs sent via @ref pmuRmRpcExelwte during test
         */
        LwLength            numRpcsSent;

        /*!
         * @brief   Number of RPCs retrieved via @ref cmdmgmtRpcMockGetSentRpc
         */
        LwLength            numRpcsRetrieved;
    } pmuRmRpcExelwteConfig;
} CMDMGMT_RPC_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
CMDMGMT_RPC_MOCK_CONFIG CmdMgmtRpcMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of the private CmdMgmt RPC mocking configuration
 */
static CMDMGMT_RPC_MOCK_CONFIG_PRIVATE CmdMgmtRpcMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
pmuRmRpcExelwte_MOCK
(
    RM_PMU_RPC_HEADER  *pRpc,
    LwU32               rpcSize,
    LwBool              bBlock
)
{
    const LwLength lwrrentCall = CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numCalls;
    const LwLength lwrrentRpc = CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsSent;

    if (lwrrentCall == CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPC_EXELWTE_CALLS)
    {
        UT_FAIL("CmdMgmt RPC mocking cannot support enough calls for current test.\n");
    }

    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numCalls++;
    if (CmdMgmtRpcMockConfig.pmuRmRpcExelwteConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return CmdMgmtRpcMockConfig.pmuRmRpcExelwteConfig.errorCodes[lwrrentCall];
    }

    if (lwrrentRpc == CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPCS)
    {
        UT_FAIL("CmdMgmt RPC mocking cannot support enough RPCs for current test.\n");
    }

    (void)memcpy(&CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.sentRpcs[lwrrentRpc].eventRpc.hdr,
                 pRpc,
                 rpcSize);
    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsSent++;

    return FLCN_OK;
}

DEFINE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuRpcProcessUnitCmdmgmt_MOCK, DISP2UNIT_RM_RPC *);

void
cmdmgmtRpcMockInit(void)
{
    LwLength i;
    for (i = 0U; i < LW_ARRAY_ELEMENTS(CmdMgmtRpcMockConfig.pmuRmRpcExelwteConfig.errorCodes); i++)
    {
        CmdMgmtRpcMockConfig.pmuRmRpcExelwteConfig.errorCodes[i] = FLCN_OK;
    }

    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numCalls = 0U;
    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsSent = 0U;
    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsRetrieved = 0U;

    RESET_FAKE(pmuRpcProcessUnitCmdmgmt_MOCK);
}

FLCN_STATUS
cmdmgmtRpcMockGetSentRpc
(
    RM_PMU_RPC_HEADER  *pRpc,
    LwU32               rpcSize
)
{
    const LwLength lwrrentRetrieved = CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsRetrieved;
    if (lwrrentRetrieved == CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsSent)
    {
        return FLCN_ERR_ILWALID_STATE;
    }

    (void)memcpy(pRpc,
                 &CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.sentRpcs[lwrrentRetrieved].eventRpc.hdr,
                 rpcSize);
    CmdMgmtRpcMockConfigPrivate.pmuRmRpcExelwteConfig.numRpcsRetrieved++;

    return FLCN_OK;
}

/* ------------------------- Static Functions  ------------------------------ */
