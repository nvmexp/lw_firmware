/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef HDCP22WIRED_AES_H
#define HDCP22WIRED_AES_H

/* ------------------------ Application includes ---------------------------- */
#include "hdcp22wired_cmn.h"

/* ------------------------ Type Definitions -------------------------------- */
#if !defined(HDCP22_USE_SCP_GEN_DKEY)
// This is only for prevolta code where HW SCP is not available.
#define AES_SC          ((AES_BC - 4) >> 1)
#define AES_MAXBC       (256/32)
#define AES_MAXKC       (256/32)
#define AES_MAXROUNDS   14

/* ------------------------ External Definitions ---------------------------- */
FLCN_STATUS hdcp22AesGenerateDkey128(LwU8 *pMessage, LwU8 *pSkey, LwU8 *pEncMessage, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libHdcp22wiredAes", "hdcp22AesGenerateDkey128");
#endif // HDCP22_USE_SCP_GEN_DKEY

#endif //HDCP22WIRED_AES_H
