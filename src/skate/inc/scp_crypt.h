/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SCP_CRYPT_H
#define SCP_CRYPT_H

/* ------------------------ System includes --------------------------------- */
/* ------------------------ Application includes ---------------------------- */
#include "scp_common.h"
/* ------------------------ Defines ----------------------------------------- */
/* ------------------------ Types definitions ------------------------------- */
/* ------------------------ External definitions ---------------------------- */
/* ------------------------ Static variables -------------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

int scpCbcCrypt(LwU8 *pSalt, LwBool bIsEncrypt, LwBool bShortlwt,
                        LwU8 keyIndex, LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                        const LwU8  *pIvBuf);

int scpCbcCryptWithKey(LwU8 *pKey, LwBool bIsEncrypt, LwBool bShortlwt,
                        LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                        const LwU8  *pIvBuf);
void scpCbcCryptWithKmem
(
    LwU8        *pSrc,
    LwU32        size,
    LwU8        *pDest,
    const LwU8  *pIvBuf,
    LwBool       bEncrypt,
    LwU32        keySlotId
);
#endif // SCP_CRYPT_H
