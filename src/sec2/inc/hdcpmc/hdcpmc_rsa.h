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
#ifndef HDCPMC_RSA_H
#define HDCPMC_RSA_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_types.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
LwBool hdcpRsaOaep(LwU8 *pEkpub, const LwU8 *pKm, const LwU8 *pModulus, const LwU8 *pExponent)
    GCC_ATTRIB_SECTION("imem_hdcpRsa", "hdcpRsaOaep");
LwBool hdcpRsaOaepDecrypt(const LwU8 *p, const LwU8 *q, const LwU8 *dP, const LwU8 *dQ, const LwU8 *qIlw, const LwU8 *pEkm, LwU8 *pKm, const LwU8 *n)
    GCC_ATTRIB_SECTION("imem_hdcpRsa", "hdcpRsaOaepDecrypt");
LwBool hdcpVerifyCertificate(const LwU8 *pModulusRoot, const LwU32 exponentRoot, const HDCP_CERTIFICATE *pCert, LwU8 *pHash)
    GCC_ATTRIB_SECTION("imem_hdcpRsa", "hdcpVerifyCertificate");

#endif // HDCPMC_RSA_H
#endif
