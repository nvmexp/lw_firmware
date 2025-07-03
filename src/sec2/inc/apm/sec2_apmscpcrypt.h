/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SEC2_APMSCPCRYPT_H
#define SEC2_APMSCPCRYPT_H

/* ------------------------ System includes -------------------------------- */

FLCN_STATUS apmScpCtrCryptWithKey(LwU8 *pKey, LwBool bShortlwt, LwU8 *pSrc,
    LwU32 size, LwU8 *pDest, LwU8 *pIvBuf, LwBool bOutputIv)
    GCC_ATTRIB_SECTION("imem_apm", "apmScpCtrCryptWithKey");

#endif // SEC2_APMSCPCRYPT_H
