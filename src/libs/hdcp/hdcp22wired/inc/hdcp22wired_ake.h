/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_AKE_H
#define HDCP22WIRED_AKE_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

/* ------------------------ External Definitions ---------------------------- */
FLCN_STATUS hdcp22RecvAkeSendCert(HDCP22_SESSION *pSession, HDCP22_CERTIFICATE *pCertRx)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAke", "hdcp22RecvAkeSendCert");
FLCN_STATUS hdcp22HandleMasterKeyExchange(HDCP22_SESSION *pSession, LwU8 *pKpubRx)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAke", "hdcp22HandleMasterKeyExchange");

#endif //HDCP22WIRED_AKE_H
