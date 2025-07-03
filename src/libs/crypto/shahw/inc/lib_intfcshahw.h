/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIB_INTFCSHAHW_H
#define LIB_INTFCSHAHW_H

/*!
 * @file   lib_intfcshahw.h
 */

/* ------------------------ System includes -------------------------------- */
#include "lwuproc.h"
#include "flcntypes.h"
#include "osptimer.h"
#include "string.h"

/* ------------------------ Application includes --------------------------- */
#include "lib_shahw.h"

/* ------------------------ Macros and Defines ----------------------------- */
/*!
 * SHA_CONTEXT and HMAC_CONTEXT structures are used to keep track of various
 * state information required for SHA operation. User is responsible for
 * initializing the members of the context structs before usage of the library.
 * The library will update state as necessary.
 */
typedef struct _SHA_CONTEXT
{
    /*!
     * algoId: SHA algorithm ID - SHA_ID_SHA_1, SHA_ID_SHA_224, etc
     */
    SHA_ID algoId;

    /*!
     * msgSize: Total message size remaining, in bytes
     */
    LwU32 msgSize;

    /*!
     * mutexId: ID of HW mutex with ownership during SHA context.
     *          User is responsible for maintaining, but kept for colwinience.
     */
    LwU8 mutexId;
} SHA_CONTEXT;

typedef struct _HMAC_CONTEXT
{
    /*!
     * algoId: SHA algorithm ID - should always be SHA_ID_SHA256, as we only support
     *         HMAC SHA 256 for now.
     */
    SHA_ID algoId;

    /*!
     * msgSize: Total size of message remaining, in bytes.
     *          Only tracks the message for which the HMAC should be generated,
     *          key and inner/outer hash sizes are tracked separately.
     */
    LwU32 msgSize;

    /*!
     * mutexId: ID of HW mutex with ownership during SHA context.
     *          User is responsible for maintaining, but kept for colwinience.
     */
    LwU8 mutexId;

    /*
     * keyBuffer: Buffer to hold key used for HMAC operation. Maximum size is
     *            SHA_256_BLOCK_SIZE_BYTE - if key is larger, user should hash first.
     */
    LwU8 keyBuffer[SHA_256_BLOCK_SIZE_BYTE];

    /*!
     * keySize: Size of key in above buffer, in bytes.
     */
    LwU32 keySize;
} HMAC_CONTEXT;

/* ------------------------ Function Prototypes ---------------------------- */

//
// LS Functions
//

// High-level wrappers for basic functionality.
FLCN_STATUS shahwSha1(LwU8 *pMessage, LwU32 messageSize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwSha1");

FLCN_STATUS shahwSha256(LwU8 *pMessage, LwU32 messageSize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwSha256");

FLCN_STATUS shahwHmacSha256(LwU8 *pMessage, LwU32 messageSize, LwU8 *pKey,
                            LwU32 keySize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwHmacSha256");

// Lower-level functions for more control over SHA HW operation.
FLCN_STATUS shahwAcquireMutex(LwU8 mutexId)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwAcquireMutex");

FLCN_STATUS shahwReleaseMutex(LwU8 mutexId)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwReleaseMutex");

FLCN_STATUS shahwOpInit(SHA_CONTEXT *pShaContext)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwOpInit");

FLCN_STATUS shahwOpUpdate(SHA_CONTEXT *pShaContext, LwU32 size,
                          LwBool bDefaultIv, LwU64 addr, SHA_SRC_TYPE srcType,
                          LwU32 dmaIdx, LwU32 *pIntStatus, LwBool bWait)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwOpUpdate");

FLCN_STATUS shahwOpFinal(SHA_CONTEXT *pShaContext, LwU32 *pDigest,
                         LwBool bScrubReg, LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwOpFinal");

FLCN_STATUS shahwHmacSha256Init(HMAC_CONTEXT *pHmacContext, LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwHmacSha256Init");

FLCN_STATUS shahwHmacSha256Update(HMAC_CONTEXT *pHmacContext, LwU32 size,
                                  LwU64 addr, SHA_SRC_TYPE srcType, LwU32 dmaIdx,
                                  LwU32 *pIntStatus, LwBool bWait)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwHmacSha256Update");

FLCN_STATUS shahwHmacSha256Final(HMAC_CONTEXT *pHmacContext, LwU32 *pHmac,
                                 LwBool bScrubReg, LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwHmacSha256Final");

FLCN_STATUS shahwSoftReset(void)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwSoftReset");

FLCN_STATUS shahwWaitTaskComplete(LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahw", "shahwWaitTaskComplete");

//
// HS Functions
//

// High-level wrappers for basic functionality.
FLCN_STATUS shahwSha1_hs(LwU8 *pMessage, LwU32 messageSize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwSha1_hs");

FLCN_STATUS shahwSha256_hs(LwU8 *pMessage, LwU32 messageSize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwSha256_hs");

FLCN_STATUS shahwHmacSha256_hs(LwU8 *pMessage, LwU32 messageSize, LwU8 *pKey,
                               LwU32 keySize, LwU8 *pDigest)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwHmacSha256_hs");

// Lower-level functions for more control over SHA HW operation.
FLCN_STATUS shahwAcquireMutex_hs(LwU8 mutexId)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwAcquireMutex_hs");

FLCN_STATUS shahwReleaseMutex_hs(LwU8 mutexId)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwReleaseMutex_hs");

FLCN_STATUS shahwOpInit_hs(SHA_CONTEXT *pShaContext)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwOpInit_hs");

FLCN_STATUS shahwOpUpdate_hs(SHA_CONTEXT *pShaContext, LwU32 size,
                             LwBool bDefaultIv, LwU64 addr,
                             SHA_SRC_TYPE srcType, LwU32 dmaIdx,
                             LwU32 *pIntStatus, LwBool bWait)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwOpUpdate_hs");

FLCN_STATUS shahwOpFinal_hs(SHA_CONTEXT *pShaContext, LwU32 *pDigest,
                         LwBool bScrubReg, LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwOpFinal_hs");

FLCN_STATUS shahwHmacSha256Init_hs(HMAC_CONTEXT *pHmacContext,
                                   LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwHmacSha256Init_hs");

FLCN_STATUS shahwHmacSha256Update_hs(HMAC_CONTEXT *pHmacContext, LwU32 size,
                                     LwU64 addr, SHA_SRC_TYPE srcType,
                                     LwU32 dmaIdx, LwU32 *pIntStatus,
                                     LwBool bWait)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwHmacSha256Update_hs");

FLCN_STATUS shahwHmacSha256Final_hs(HMAC_CONTEXT *pHmacContext, LwU32 *pHmac,
                                 LwBool bScrubReg, LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwHmacSha256Final_hs");

FLCN_STATUS shahwSoftReset_hs(void)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwSoftReset_hs");

FLCN_STATUS shahwWaitTaskComplete_hs(LwU32 *pIntStatus)
    GCC_ATTRIB_SECTION("imem_libShahwHs", "shahwWaitTaskComplete_hs");

#endif // LIB_INTFCSHAHW_H
