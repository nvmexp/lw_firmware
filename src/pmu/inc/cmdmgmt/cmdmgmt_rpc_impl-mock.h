/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_RPC_IMPL_MOCK_H
#define CMDMGMT_RPC_IMPL_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "fff.h"
#include "flcnifcmn.h"
#include "unit_api.h"

/* ------------------------- Application Includes --------------------------- */
#include "g_pmurmifrpc.h"

/* ------------------------- Defines ---------------------------------------- */
#define CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPCS                          ((LwLength)8U)
#define CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPC_EXELWTE_CALLS             ((LwLength)8U)

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Configuration data for CmdMgmt RPC mocking
 */
typedef struct
{ 
    /*!
     * @brief   Mocking configuration for @ref pmuRmRpcExelwte
     */
    struct
    {
        /*!
         * @brief   Error code to return instead of mocking functionality
         */
        FLCN_STATUS errorCodes[CMDMGMT_RPC_MOCK_MAX_PMU_RM_RPC_EXELWTE_CALLS];
    } pmuRmRpcExelwteConfig;
} CMDMGMT_RPC_MOCK_CONFIG;

/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the CmdMgmt RPC mocking configuration
 */
extern CMDMGMT_RPC_MOCK_CONFIG CmdMgmtRpcMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @copydoc pmuRmRpcExelwte
 *
 * @note    Mock implementation dependent on @ref CmdMgmtRpcMockConfig
 */
FLCN_STATUS pmuRmRpcExelwte_MOCK(RM_PMU_RPC_HEADER *pRpc, LwU32 rpcSize, LwBool bBlock);

/*!
 * @copydoc pmuRpcProcessUnitCmdmgmt

 * @note    Mock implementation of @ref pmuRpcProcessUnitCmdmgmt
 */
DECLARE_FAKE_VALUE_FUNC(FLCN_STATUS, pmuRpcProcessUnitCmdmgmt_MOCK, DISP2UNIT_RM_RPC *);

/*!
 * @brief   Initializes the state of the RPC mocking
 */
void cmdmgmtRpcMockInit(void);

/*!
 * @brief   Copies out the next available RPC
 *
 * @param[out]  pRpc    Destination for the copy-out
 * @param[in]   rpcSize Size of the RPC to copy out
 * 
 * @return  FLCN_OK                 Successfully copied out
 * @return  FLCN_ERR_ILWALID_STATE  No RPC available to copy out
 */
FLCN_STATUS cmdmgmtRpcMockGetSentRpc(RM_PMU_RPC_HEADER *pRpc, LwU32 rpcSize);

#endif // CMDMGMT_RPC_IMPL_MOCK_H
