/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CMDMGMT_SUPERSURFACE_MOCK_H
#define CMDMGMT_SUPERSURFACE_MOCK_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"

/* ------------------------- Application Includes --------------------------- */
#include "g_pmurmifrpc.h"

/* ------------------------- Defines ---------------------------------------- */
#define CMDMGMT_SUPERSURFACE_MOCK_MAX_CALLS                       ((LwLength)1U)

/* ------------------------- Type Definitions ------------------------------- */
/*!
 * @brief   Configuration data for CmdMgmt SuperSurface mocking
 */
typedef struct
{ 
    /*!
     * @brief   Mocking configuration for @ref cmdmgmtSuperSurfaceMemberDescriptorsInit
     */
    struct
    {
        /*!
         * @brief   Error code to return instead of mocking functionality
         */
        FLCN_STATUS errorCodes[CMDMGMT_SUPERSURFACE_MOCK_MAX_CALLS];
    } cmdmgmtSuperSurfaceMemberDescriptorsInitConfig;
} CMDMGMT_SUPERSURFACE_MOCK_CONFIG;

/* ------------------------- External Definitions --------------------------- */
/*!
 * @brief   Instance of the CmdMgmt SUPERSURFACE mocking configuration
 */
extern CMDMGMT_SUPERSURFACE_MOCK_CONFIG CmdMgmtSuperSurfaceMockConfig;

/* ------------------------- Function Prototypes ---------------------------- */
/*!
 * @copydoc cmdmgmtSuperSurfaceMemberDescriptorsInit
 *
 * @note    Mock implementation dependent on @ref CmdMgmtSuperSurfaceMockConfig
 */
FLCN_STATUS cmdmgmtSuperSurfaceMemberDescriptorsInit_MOCK(void);

/*!
 * @brief   Initializes the state of the SuperSurface mocking
 */
void cmdmgmtSuperSurfaceMockInit(void);

/*!
 * @brief   Returns the number of times @ref cmdmgmtSuperSurfaceMemberDescriptorsInit
 *          was called during test
 */
LwLength cmdmgmtSuperSurfaceMockMemberDescriptorInitGetNumCalls(void);

#endif // CMDMGMT_SUPERSURFACE_MOCK_H
