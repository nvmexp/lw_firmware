/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_LC_H
#define HDCP22WIRED_LC_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

/* ------------------------ External Definitions --------------------------- */
FLCN_STATUS hdcp22HandlePairing(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "hdcp22HandlePairing");
FLCN_STATUS hdcp22HandleLcInit(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "hdcp22HandleLcInit");
FLCN_STATUS hdcp22CallwlateL(HDCP22_SESSION *pSession, LwU8 *pL)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "hdcp22CallwlateL");
FLCN_STATUS hdcp22HandleVerifyL(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredLc", "hdcp22HandleVerifyL");

#endif //HDCP22WIRED_LC_H
