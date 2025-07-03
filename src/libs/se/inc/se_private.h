/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SE_PRIVATE_H
#define SE_PRIVATE_H

#include "setypes.h"

/* ------------------------ Function Prototypes ---------------------------- */
// Private Mutex Management
SE_STATUS seMutexAcquireSEMutex(void)
    GCC_ATTRIB_SECTION("imem_libSE", "seMutexAcquireSEMutex");
SE_STATUS seMutexAcquireSEMutexHs(void)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seMutexAcquireSEMutexHs");
SE_STATUS seMutexReleaseSEMutex(void)
    GCC_ATTRIB_SECTION("imem_libSE", "seMutexReleaseSEMutex");
SE_STATUS seMutexReleaseSEMutexHs(void)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seMutexReleaseSEMutexHs");

// Private SE PKA functions
SE_STATUS sePkaWriteRegister(LwU32 regAddr, LwU32 *pDataAddr, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSE", "sePkaWriteRegister");
SE_STATUS sePkaReadRegister(LwU32 output[], LwU32 regAddr, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSE", "sePkaReadRegister");
SE_STATUS sePkaWritePkaOperandToBankAddress(LwU32 bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSE", "sePkaWritePkaOperandToBankAddress");
SE_STATUS sePkaReadPkaOperandFromBankAddress(SE_PKA_BANK bankId, LwU32 index, LwU32 *pOutput, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSE", "sePkaReadPkaOperandFromBankAddress");
SE_STATUS sePkaWriteRegisterHs(LwU32 regAddr, LwU32 *pDataAddr, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSEHs", "sePkaWriteRegisterHs");
SE_STATUS sePkaReadRegisterHs(LwU32 output[], LwU32 regAddr, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSEHs", "sePkaReadRegisterHs");
SE_STATUS sePkaWritePkaOperandToBankAddressHs(LwU32 bankId, LwU32 index, LwU32 *pData, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSEHs", "sePkaWritePkaOperandToBankAddressHs");
SE_STATUS sePkaReadPkaOperandFromBankAddressHs(SE_PKA_BANK bankId, LwU32 index, LwU32 *pOutput, LwU32 pkaKeySizeDW)
    GCC_ATTRIB_SECTION("imem_libSEHs", "sePkaReadPkaOperandFromBankAddressHs");

// Private Helper Functions
LwBool seBigNumberEqual(LwU32 *, LwU32 *, LwU32)
    GCC_ATTRIB_SECTION("imem_libSE", "seBigNumberEqual");
void *seMemSet(void *pDst, LwU8 value, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSE", "seMemSet");
void *seMemCpy(void *pDst, const void *pSrc, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSE", "seMemCpy");
void seSwapEndianness(void *pOutData, const void *pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSE", "seSwapEndianness");
void *seMemSetHs(void *pDst, LwU8 value, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seMemSetHs");
void *seMemCpyHs(void *pDst, const void *pSrc, LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seMemCpyHs");
void seSwapEndiannessHs(void *pOutData, const void *pInData, const LwU32 size)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seSwapEndiannessHs");
LwU64 seMulu32Hs(LwU32 a, LwU32 b)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seMulu32Hs");

// Private Initialization Functions
SE_STATUS seInit(LwU32 bitCount, PSE_PKA_REG pPkaReg)
    GCC_ATTRIB_SECTION("imem_libSE", "seInit");
SE_STATUS seInitHs(SE_KEY_SIZE bitCount, PSE_PKA_REG pPkaReg)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seInitHs");
/* ------------------------- Macros and Defines ---------------------------- */
#define CHECK_SE_STATUS(expr) do {       \
    status = (expr);                     \
    if (status != SE_OK)                 \
    {                                    \
        goto ErrorExit;                  \
    }                                    \
} while (LW_FALSE)


// 
// This macro releases the mutex if the mutex was acquired properly.
// If the current status indicates a failure, that status is preserved
// (report first failure).  If the current status is SE_OK, replace with
// status from the mutex release.
//
#define CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus) do {  \
    if (mutexStatus == SE_OK)                             \
    {                                                     \
        mutexStatus = seMutexReleaseSEMutex();            \
        if (status == SE_OK)                              \
        {                                                 \
            status = mutexStatus;                         \
        }                                                 \
    }                                                     \
} while (LW_FALSE)

// 
// This macro releases the mutex if the mutex was acquired properly in HS Mode.
// If the current status indicates a failure, that status is preserved
// (report first failure).  If the current status is SE_OK, replace with
// status from the mutex release.
//
#define CHECK_MUTEX_STATUS_AND_RELEASE_HS(mutexStatus) do {  \
    if (mutexStatus == SE_OK)                                \
    {                                                        \
        mutexStatus = seMutexReleaseSEMutexHs();             \
        if (status == SE_OK)                                 \
        {                                                    \
            status = mutexStatus;                            \
        }                                                    \
    }                                                        \
} while (LW_FALSE)
#endif // SE_PRIVATE_H
