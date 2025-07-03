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
#ifndef HDCPMC_SCP_H
#define HDCPMC_SCP_H

/*!
 * @file    hdcpmc_scp.h
 * @brief   Interface to the SCP.
 *
 * Due to schedule constraints, we are maintaining this file as an interface
 * to the SCP encryption functions. When a more comprehensive SCP encryption
 * library has been developed, this file shall be replaced with library.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * hdcpEcbCryptMode encryption modes.
 *
 * MODE_HDCP_ENCRYPT -> HDCP style encryption
 * MODE_CRYPT        -> General encryption/decryption (session)
 * IMEM_TRANSFER     -> Plain copy
 */
#define HDCP_ECB_CRYPT_MODE_HDCP_ENCRYPT                                      0
#define HDCP_CBC_CRYPT_MODE_CRYPT                                             1
#define HDCP_ECB_CRYPT_MODE_IMEM_TRANSFER                                     2
#define HDCP_DRE_CRYPT_MODE_HDCP_ENCRYPT                                      3
#define HDCP_DRE_CRYPT_MODE_CTS_ENCRYPT                                       4
#define HDCP_ECB_CRYPT_MODE_CRYPT                                             5

/*!
 * Key table indexes
 *
 * These defines are used while selecting SCP key table indexes
 *
 * _SESSION   : Key index used for encryption session details.
 *              Need not be same across chips
 * _EPAIR     : Key index used for encrypting/decrypting pairing
 *              info. Should be same across all chips
 */
#define HDCP_SCP_KEY_INDEX_SESSION                                           60
#define HDCP_SCP_KEY_INDEX_EPAIR                                             60
#define HDCP_SCP_KEY_INDEX_LC128                                              5
#define HDCP_SCP_KEY_INDEX_DRE                                                9

/*!
 * Key table indexes
 *
 * These defines are used while selecting SCP key table indexes
 *
 * _SESSION   : Key index used for encryption session details.
 *              Need not be same across chips
 * _EPAIR     : Key index used for encrypting/decrypting pairing
 *              info. Should be same across all chips
 */
#define HDCP_SCP_KEY_INDEX_SESSION                                           60
#define HDCP_SCP_KEY_INDEX_EPAIR                                             60
#define HDCP_SCP_KEY_INDEX_LC128                                              5
#define HDCP_SCP_KEY_INDEX_DRE                                                9

 /* ------------------------- Prototypes ------------------------------------- */
void hdcpSetAesKey(LwU8 *pKey)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "hdcpSetAesKey");
void hdcpSetEncryptionKeys(LwU8 *pAesData, LwU8 *pKs, LwU8 *pLc)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "hdcpSetEncryptionKeys");
void hdcpSetEpairEncryptionKey(LwU8 *pEpairKey, LwBool bIsEncrypt)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "hdcpSetEpairEncryptionKey");
FLCN_STATUS hdcpEcbCrypt(LwU32 dmemOffset, LwU32 size, LwU32 destOffset, LwBool bIsEncrypt, LwBool bShortlwt, LwU32 mode)
    GCC_ATTRIB_SECTION("imem_libHdcpCryptHs", "hdcpEcbCrypt");

FLCN_STATUS hdcpRandomNumber(LwU8 *pDest, LwS32 size)
    GCC_ATTRIB_SECTION("imem_hdcpRand", "start");
FLCN_STATUS hdcpGenerateDkey(LwU8 *pRtxCtr, LwU8 *pKmRnXor, LwU8 *pDkey)
    GCC_ATTRIB_SECTION("imem_hdcpGenerateDkey", "start");
FLCN_STATUS hdcpXor128Bits(LwU8 *pIn1, LwU8 *pIn2, LwU8 *pOut)
    GCC_ATTRIB_SECTION("imem_hdcpXor", "start");

#endif // HDCPMC_SCP_H
#endif
