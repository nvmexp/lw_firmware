/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
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

FLCN_STATUS scpCtrCrypt(LwU8 *pSalt, LwBool bShortlwt, LwU8 keyIndex, LwU8 *pSrc,
                        LwU32 size, LwU8 *pDest, const LwU8 *pIvBuf)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "scpCtrCrypt");
#else // ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "scpCtrCrypt");
#endif // ON_DEMAND_PAGING_BLK

FLCN_STATUS scpCtrCryptWithKey(LwU8 *pKey, LwBool bShortlwt, LwU8 *pSrc,
                               LwU32 size, LwU8 *pDest, const LwU8 *pIvBuf)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "scpCtrCryptWithKey");
#else // ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "scpCtrCryptWithKey");
#endif // ON_DEMAND_PAGING_BLK

FLCN_STATUS scpCbcCrypt(LwU8 *pSalt, LwBool bIsEncrypt, LwBool bShortlwt,
                        LwU8 keyIndex, LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                        const LwU8 *pIvBuf)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "scpCbcCrypt");
#else // ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "scpCbcCrypt");
#endif // ON_DEMAND_PAGING_BLK

FLCN_STATUS scpCbcCryptWithKey(LwU8 *pKey, LwBool bIsEncrypt, LwBool bShortlwt,
                               LwU8 *pSrc, LwU32 size, LwU8 *pDest,
                               const LwU8 *pIvBuf)
#ifdef ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_resident", "scpCbcCryptWithKey");
#else // ON_DEMAND_PAGING_BLK
    GCC_ATTRIB_SECTION("imem_libScpCryptHs", "scpCbcCryptWithKey");
#endif // ON_DEMAND_PAGING_BLK

#endif // SCP_CRYPT_H