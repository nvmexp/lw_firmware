/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEAPI_H
#define SEAPI_H

/*
 * This file defines the functions that are available publically.  Calling one
 * of these functions will handle the setup and initialization steps,  including
 * acquiring the SE PKA mutex.  The requested cryptography function will be
 * performed, and the SE PKA mutex released.
 *
 * These are the only functions that should be called from outside of SE.
 *
 */
/* ------------------------ System includes -------------------------------- */
/* ------------------------ Application includes --------------------------- */
#include "setypes.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Types definitions ------------------------------ */

//
// Lwrrently the only key length tested is 256,  so it is the only available size.
// Once we have rmTest in place to cover all key lengths,  the other sizes will be added.
//
typedef enum _SE_ECC_KEY_SIZE
{
    SE_ECC_MIN_KEY_SIZE = 256,
    SE_ECC_KEY_SIZE_256 = 256,
    SE_ECC_MAX_KEY_SIZE = 256,
} SE_ECC_KEY_SIZE;

//
// Lwrrently SE lib doesn't provide any supported RSA operations,  when those are added
// appropriate defines will be added to cover RSA key lengths.
//

/* ------------------------ External definitions --------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// SE True Random Number functions  
SE_STATUS seTrueRandomGetNumber(LwU32 *pNum, LwU32 sizeInDWords)
    GCC_ATTRIB_SECTION("imem_libSE", "seTrueRandomGetNumber");
SE_STATUS seTrueRandomGetNumberHs(LwU32 *pNum, LwU32 sizeInDWords)
    GCC_ATTRIB_SECTION("imem_libSEHs", "seTrueRandomGetNumber");

// SE ECC Functions
SE_STATUS seECPointMult(LwU32 modulus[],         LwU32 pointA[],     LwU32 generatorPointX[],
                        LwU32 generatorPointY[], LwU32 privateKey[], LwU32 outPointX[],
                        LwU32 outPointY[],       SE_ECC_KEY_SIZE seEccKeySize)
    GCC_ATTRIB_SECTION("imem_libSE", "seECPointMult");
SE_STATUS seECPointVerification(LwU32 modulus[], LwU32 a[], LwU32 b[], LwU32 pointX[],
                                LwU32 pointY[],  SE_ECC_KEY_SIZE seEccKeySize)
    GCC_ATTRIB_SECTION("imem_libSE", "seECPointVerification");
SE_STATUS seECDSASignHash(LwU32 modulus[],         LwU32 gorder[],          LwU32 a[],       LwU32 b[],
                          LwU32 generatorPointX[], LwU32 generatorPointY[], LwU32 msgHash[], LwU32 privateKey[], 
                          LwU32 retryLoopCount,    LwU32 sigR[],            LwU32 sigS[],    SE_ECC_KEY_SIZE seEccKeySize)
    GCC_ATTRIB_SECTION("imem_libSE", "seECDSASignHash");

// SE RSA Functions
SE_STATUS seRsaModularExp(SE_KEY_SIZE keysize, LwU32 modulus[], LwU32 exponent[], LwU32 base[], LwU32 output[])
    GCC_ATTRIB_SECTION("imem_libSE", "seRsaModularExp");
SE_STATUS seRsaModularExpHs(SE_KEY_SIZE keysize, LwU32 modulus[], LwU32 exponent[], LwU32 base[], LwU32 output[])
    GCC_ATTRIB_SECTION("imem_libSEHs", "seRsaModularExpHs");

#endif // SEAPI_H
