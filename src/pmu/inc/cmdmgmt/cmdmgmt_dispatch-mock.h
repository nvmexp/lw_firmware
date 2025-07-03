/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_DISPATCH_MOCK_H
#define CMDMGMT_DISPATCH_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "unit_api.h"

/* ------------------------- Defines ---------------------------------------- */
#define CMDMGMT_DISPATCH_MOCK_MAX_CALLS                          ((LwLength)32U)
#define CMDMGMT_DISPATCH_MOCK_MAX_CMDS                           ((LwLength)32U)
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Configuration data for mocking code in @ref cmdmgmt_dispatch.c
 */
typedef struct
{
    /*!
     * @brief   Mocking configuration for @ref cmdmgmtExtCmdDispatch
     */
    struct
    {
        /*!
         * @brief   Return status for each call to @ref cmdmgmtExtCmdDispatch
         */
        FLCN_STATUS errorCodes[CMDMGMT_DISPATCH_MOCK_MAX_CALLS];
    } cmdmgmtExtCmdDispatchConfig;
} CMDMGMT_DISPATCH_MOCK_CONFIG;
/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the CmdMgmt Dispatch mocking configuration
 */
extern CMDMGMT_DISPATCH_MOCK_CONFIG CmdMgmtDispatchMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @copydoc cmdmgmtExtCmdDispatch
 *
 * @note    Mock implementation dependent on @ref CmdMgmtDispatchMockConfig
 */
FLCN_STATUS cmdmgmtExtCmdDispatch_MOCK(DISP2UNIT_RM_RPC *pRequest);

/*!
 * @brief   Initializes the state of the dispatch mocking
 */
void cmdmgmtDispatchMockInit(void);

/*!
 * @brief   Copies out the next available @ref DISP2UNIT_RM_RPC
 *
 * @param[out]  pDispatch   Destination for the copy-out
 * 
 * @return  FLCN_OK                 Successfully copied out
 * @return  FLCN_ERR_ILWALID_STATE  No @ref DISP2UNIT_RM_RPC available to copy out
 */
FLCN_STATUS cmdmgmtDispatchMockGetDispatched(DISP2UNIT_RM_RPC *pDispatch);

#endif // CMDMGMT_DISPATCH_MOCK_H
