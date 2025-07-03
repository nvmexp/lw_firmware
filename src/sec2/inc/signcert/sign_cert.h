/* _LWRM_COPYRIGHT_BEGIN_
*
* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* _LWRM_COPYRIGHT_END_
*/

#ifndef SEC2_SIGNCERT_H
#define SEC2_SIGNCERT_H

/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
/* ------------------------ Types definitions ------------------------------ */


/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
FLCN_STATUS signCertEkSignHash(LwU32 *pHash, LwU32 *pSig)
    GCC_ATTRIB_SECTION("imem_spdm", "signCertEkSignHash");

FLCN_STATUS signCertGetEkPrivate(LwU32 *pPrivKey, LwU32 privKeySize)
    GCC_ATTRIB_SECTION("imem_spdm", "signCertGetEkPrivate");

FLCN_STATUS signCertGetEkPublic(LwU32 *pPubKey)
    GCC_ATTRIB_SECTION("imem_spdm", "signCertGetEkPublic");

/* ------------------------ Global Variables ------------------------------- */

#endif // SEC2_SIGNCERT_H
