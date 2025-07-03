/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   hdcp22wired_objhdcp22wired.c
 * @brief  Container-object for FLCN HDCP22WIRED routines. Contains generic non-HAL
 *         interrupt-routines plus logic required to hook-up chip-specific
 *         interrupt HAL-routines.
 */

/* ------------------------- System Includes -------------------------------- */
#include "hdcp22wired_cmn.h"
#include "hdcp22wired_objhdcp22wired.h"

/* ------------------------- Application Includes --------------------------- */
#include "config/g_hdcp22wired_private.h"
#include "config/hdcp22wired-config.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Function Definitions ------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 *  @brief Init LW_PDISP_SOR_HDCP22_CTRL reg setting.
 *  @param[in]   void
 *  @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredInitSorHdcp22CtrlReg(void)
{
#ifdef HDCP22_SUPPORT_MST
    return hdcp22wiredInitSorHdcp22CtrlReg_HAL(&Hdcp22wired);
#else
    // Default return OK if MST not supported.
    return FLCN_OK;
#endif
}

/*!
 *  @brief  Reads lock/unlock bit, set by SEC2
 *         and returns the same
 *  @param[out] isType1LockActive Pointer to fill if type1 is locked or not
 *  @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredIsType1LockActive
(
    LwBool *isType1LockActive
)
{
    return  hdcp22wiredIsType1LockActive_HAL(&Hdcp22wired, isType1LockActive);
}

/*!
 * @brief Initialize secure memory storage for hdcp22 secrets.
 *
 *  @returns    FLCN_STATUS  FLCN_OK on successfull exelwtion
 *                           Appropriate error status on failure.
 */
FLCN_STATUS
hdcp22wiredInitSelwreMemoryStorage(void)
{
    return hdcp22wiredKmemInit_HAL(&Hdcp22wired, sizeof(HDCP22_SELWRE_MEMORY));
}

