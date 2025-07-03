/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
#ifndef HDCP22_SRM_H
#define HDCP22_SRM_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS hdcpRevocationCheck(LwU8 *pKsvList, LwU32 numKsvs, LwU32 srmFbAddr, LwU8 *pTempBuf, LwBool bUseTestKeys, LwU8 *pFirstRevokedDevice)
    GCC_ATTRIB_SECTION("imem_hdcpSrm", "hdcpRevocationCheck");

#endif // HDCP22_SRM_H
#endif
