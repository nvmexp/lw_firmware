/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_SKE_H
#define HDCP22WIRED_SKE_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

/* ------------------------ External Definitions ---------------------------- */
FLCN_STATUS hdcp22HandleGenerateSkeInit(HDCP22_SESSION *pSession)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredSke", "hdcp22HandleGenerateSkeInit");

#endif //HDCP22WIRED_SKE_H
