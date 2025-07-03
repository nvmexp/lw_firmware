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
 * @file   cmdmgmt_supersurface-mock.c
 * @brief  Mock implementations of the Falcon DMA routines
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_supersurface-mock.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Global SuperSurface mocking configuration private to the mocking
 */
typedef struct 
{
    /*!
     * @brief   Private configuration data for @ref cmdmgmtSuperSurfaceMemberDescriptorsInit
     */
    struct
    {
        /*!
         * @brief   Number of calls to @ref cmdmgmtSuperSurfaceMemberDescriptorsInit during test.
         */
        LwLength            numCalls;
    } cmdmgmtSuperSurfaceMemberDescriptorsInitConfig;
} CMDMGMT_SUPERSURFACE_MOCK_CONFIG_PRIVATE;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Global Variables ------------------------------- */
CMDMGMT_SUPERSURFACE_MOCK_CONFIG CmdMgmtSuperSurfaceMockConfig;

/* ------------------------- Static Variables ------------------------------- */
/*!
 * @brief   Instance of the private CmdMgmt SuperSurface mocking configuration
 */
static CMDMGMT_SUPERSURFACE_MOCK_CONFIG_PRIVATE CmdMgmtSuperSurfaceMockConfigPrivate;

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
FLCN_STATUS
cmdmgmtSuperSurfaceMemberDescriptorsInit_MOCK(void)
{
    const LwLength lwrrentCall = CmdMgmtSuperSurfaceMockConfigPrivate.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.numCalls;
    if (lwrrentCall == CMDMGMT_SUPERSURFACE_MOCK_MAX_CALLS)
    {
        UT_FAIL("CmdMgmt SuperSurface mocking cannot support enough calls for current test.\n");
    }
    CmdMgmtSuperSurfaceMockConfigPrivate.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.numCalls++;

    if (CmdMgmtSuperSurfaceMockConfig.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.errorCodes[lwrrentCall] != FLCN_OK)
    {
        return CmdMgmtSuperSurfaceMockConfig.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.errorCodes[lwrrentCall];
    }

    // No functionality to mock
    return FLCN_OK;
}

void
cmdmgmtSuperSurfaceMockInit(void)
{
    LwLength i;
    for (i = 0U; i < LW_ARRAY_ELEMENTS(CmdMgmtSuperSurfaceMockConfig.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.errorCodes); i++)
    {
        CmdMgmtSuperSurfaceMockConfig.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.errorCodes[i] = FLCN_OK;
    }
    CmdMgmtSuperSurfaceMockConfigPrivate.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.numCalls = 0U;
}

LwLength
cmdmgmtSuperSurfaceMockMemberDescriptorInitGetNumCalls(void)
{
    return CmdMgmtSuperSurfaceMockConfigPrivate.cmdmgmtSuperSurfaceMemberDescriptorsInitConfig.numCalls;
}

/* ------------------------- Static Functions  ------------------------------ */
