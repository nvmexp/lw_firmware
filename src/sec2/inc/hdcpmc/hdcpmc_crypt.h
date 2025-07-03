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
#ifndef HDCPMC_CRYPT_H
#define HDCPMC_CRYPT_H

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
#include "hdcpmc/hdcpmc_types.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define hdcpEncryptEpair(pEpair, pRandNum)                                    \
    hdcpCryptEpair(pEpair, pRandNum, LW_TRUE)
#define hdcpDecryptEpair(pEpair, pRandNum)                                    \
    hdcpCryptEpair(pEpair, pRandNum, LW_FALSE)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
void hdcpSwapEndianness(void *pOutData, const void *pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_hdcpmc", "hdcpSwapEndianness");
FLCN_STATUS hdcpGetDkey(HDCP_SESSION *pSession, LwU8 *pDkey, LwU8 *pTmpBuf)
    GCC_ATTRIB_SECTION("imem_hdcpDkey", "hdcpGetDkey");
FLCN_STATUS hdcpGetDcpKey(void)
    GCC_ATTRIB_SECTION("imem_hdcpDcpKey", "start");
FLCN_STATUS hdcpEncrypt(HDCP_ENCRYPTION_STATE *pState, LwU64 inBuf, LwU32 numBlocks, LwU8 streamId, LwU64 outBuf, LwU8 *pBuf, LwU64 *inCtr, LwU32 encOff)
    GCC_ATTRIB_SECTION("imem_hdcpEncrypt", "start");
FLCN_STATUS hdcpCryptEpair(LwU8 *pEpair, LwU8 *pRandNum, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_hdcpCryptEpair", "start");

#endif // HDCPMC_CRYPT_H
#endif
