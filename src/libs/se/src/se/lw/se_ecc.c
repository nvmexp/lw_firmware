/*
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   se_ecc.c
 * @brief  Non halified library functions for supporting Elliptic Lwrve Cryptography.
 *         This file contains low-level math functions such as modular addition, modular
 *         multiplication, EC point multiplication and others,  as well as support for
 *         some higher level EC algorithms (ECDSA) that makes use of the low-level math
 *         functions.
 *
 *         SE supplies HW support for Elliptic Lwrve Point Multiplication:
 *               P = kG based on lwrve C (y^2 = x^3 + ax + b mod p).
 *
 *         Additionally, SE supplies some more fundamental algorithms like:
 *              Modular Addition, Modular Multiplication, Elliptic Lwrve Point Verification.
 *
 *         For more information see:
 *              //hw/ar/doc/system/Architecture/Security/Reference/Partners/Elliptic/EDN_0243_PKA_User_Guide_v1p15
 *              //hw/ar/doc/t18x/se/SE_PKA_IAS.doc
 *
 *         Sample HW test code at:
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se/
 *              //hw/gpu_ip/unit/falcon/6.0/fmod/se_stimgen/
 *
 *
 */

#include "se_objse.h"
#include "seapi.h"
#include "se_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */


/*!
 * @brief Elliptic lwrve point multiplication over a prime modulus (Q = kP)
 *
 * @param[in]  modulus                     Prime modulus
 * @param[in]  a                           a from EC equation
 * @param[in]  generatorPointX             Generator base point X  (Px in above equation)
 * @param[in]  generatorPointY             Generator base point Y  (Py in above equation)
 * @param[in]  scalarK                     scalar k  (often times this is the private key passed by caller)
 * @param[out] outPointX                   Resulting X point of multiplication
 * @param[out] outPointY                   Resulting Y point of multiplication
 * @param[in]  seEccKeySize                Enum type that sets key size
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seECPointMult
(
    LwU32           modulus[],
    LwU32           a[],
    LwU32           generatorPointX[],
    LwU32           generatorPointY[],
    LwU32           scalarK[],
    LwU32           outPointX[],
    LwU32           outPointY[],
    SE_ECC_KEY_SIZE seEccKeySize
)
{
    SE_STATUS  status      = SE_OK;
    SE_STATUS  mutexStatus = SE_ERR_MUTEX_ACQUIRE;
    SE_PKA_REG sePkaReg    = { 0 };

    //
    // Check input data in public APIs
    // The out params may be NULL if the API user chooses, but both should not be.
    // The out params will be checked for NULL in seECPointMult_HAL before writing.
    // If both are NULL, there is nothing to do.
    //
    if ((modulus == NULL)                            ||
        (a == NULL)                                  ||
        (generatorPointX == NULL)                    ||
        (generatorPointY == NULL)                    ||
        ((outPointX == NULL) && (outPointY == NULL)) ||
        (seEccKeySize < SE_ECC_MIN_KEY_SIZE)         ||
        (seEccKeySize > SE_ECC_MAX_KEY_SIZE)         ||
        (scalarK == NULL))
    {
        CHECK_SE_STATUS(SE_ERR_BAD_INPUT_DATA);
    }

    //
    // Acquire mutex and init SE.
    // Preserve mutex acquire status in order to properly release.
    //
    mutexStatus =  seMutexAcquireSEMutex();
    CHECK_SE_STATUS(mutexStatus);
    CHECK_SE_STATUS(seInit(seEccKeySize, &sePkaReg));

    CHECK_SE_STATUS(seECPointMult_HAL(&Se, modulus, a, generatorPointX, generatorPointY, scalarK, outPointX, outPointY, &sePkaReg));

ErrorExit:
    CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus);
    return status;
}

/*!
 * @brief Elliptic lwrve point verification over a prime modulus (y^2 = x^3 + ax + b mod p)
 *        This function verifies that a specific point is on the lwrve defined by the EC
 *        equation.
 *  
 *
 * @param[in]  modulus                     Prime modulus
 * @param[in]  a                           a from EC equation
 * @param[in]  b                           b from EC equation
 * @param[in]  pointX                      X coord of point which we checking to see if it is on the lwrve
 * @param[in]  pointY                      Y coord of point which we checking to see if it is on the lwrve
 * @param[in]  seEccKeySize                Enum type that sets key size
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seECPointVerification
(
    LwU32           modulus[],
    LwU32           a[],
    LwU32           b[],
    LwU32           pointX[],
    LwU32           pointY[],
    SE_ECC_KEY_SIZE seEccKeySize
)
{
    SE_STATUS  status      = SE_OK;
    SE_STATUS  mutexStatus = SE_ERR_MUTEX_ACQUIRE;
    SE_PKA_REG sePkaReg    = { 0 };

    // Check input data in public APIs  
    if ((modulus == NULL)                      ||
        (a == NULL)                            ||
        (b == NULL)                            ||
        (pointX == NULL)                       ||
        (pointY == NULL)                       ||
        (seEccKeySize < SE_ECC_MIN_KEY_SIZE)   ||
        (seEccKeySize > SE_ECC_MAX_KEY_SIZE))
    {
        CHECK_SE_STATUS(SE_ERR_BAD_INPUT_DATA);
    }

    //
    // Acquire mutex and init SE.
    // Preserve mutex acquire status in order to properly release.
    //
    mutexStatus = seMutexAcquireSEMutex();
    CHECK_SE_STATUS(mutexStatus);
    CHECK_SE_STATUS(seInit(seEccKeySize, &sePkaReg));

    CHECK_SE_STATUS(seECPointVerification_HAL(&Se, modulus, a, b, pointX, pointY, &sePkaReg));

ErrorExit:
    CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus);
    return status;
}

/*!
 * @brief Creates an ECDSA P256 signature for a hash of a message, given a private key
 *
 * @param[in]  modulus                     Prime modulus
 * @param[in]  gorder                      Field order
 * @param[in]  a                           a from EC equation
 * @param[in]  b                           b from EC equation
 * @param[in]  generatorPointX             Generator base point X
 * @param[in]  generatorPointY             Generator base point Y
 * @param[in]  msgHash                     Hash to create signature on
 * @param[in]  privateKey                  Private key
 * @param[in]  retryLoopCount              Number of times to retry for a good signature
 * @param[out] sigR                        First half of signature
 * @param[out] sigS                        Second half of signature
 * @param[in]  seEccKeySize                Enum type that sets key size
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 *
 */
SE_STATUS seECDSASignHash
(
    LwU32           modulus[],
    LwU32           gorder[],
    LwU32           a[],
    LwU32           b[],
    LwU32           generatorPointX[],
    LwU32           generatorPointY[],
    LwU32           msgHash[],
    LwU32           privateKey[],
    LwU32           retryLoopCount,
    LwU32           sigR[],
    LwU32           sigS[],
    SE_ECC_KEY_SIZE seEccKeySize
)
{
    LwU32      d[MAX_ECC_SIZE_DWORDS]         = { 0 };
    LwU32      M[MAX_ECC_SIZE_DWORDS]         = { 0 };
    LwU32      k_ilwerse[MAX_ECC_SIZE_DWORDS] = { 0 };
    LwU32      randNum[MAX_ECC_SIZE_DWORDS]   = { 0 };
    LwU32      bigZero[MAX_ECC_SIZE_DWORDS]   = { 0 };
    LwU32      numAttempts                    = 0;
    SE_STATUS  status                         = SE_OK;
    SE_STATUS  mutexStatus                    = SE_ERR_MUTEX_ACQUIRE;
    SE_PKA_REG sePkaReg                       = { 0 };

    //
    // Check input data in public APIs  
    // The out params may be NULL if the API user chooses, but both should not be.
    // The out params will be checked for NULL in seECDSASignHash before writing.
    // If both are NULL, there is nothing to do.
    //
    if ((modulus == NULL)                               ||
        (gorder == NULL)                                ||
        (a == NULL)                                     ||
        (b == NULL)                                     ||
        (generatorPointX == NULL)                       ||
        (generatorPointY == NULL)                       ||
        (msgHash == NULL)                               ||
        (privateKey == NULL)                            ||
        (seEccKeySize < SE_ECC_MIN_KEY_SIZE)            ||
        (seEccKeySize > SE_ECC_MAX_KEY_SIZE)            ||
        ((sigR == NULL) && (sigS == NULL)))
    {
        CHECK_SE_STATUS(SE_ERR_BAD_INPUT_DATA);
    }

    //
    // Acquire mutex and init SE.
    // Preserve mutex acquire status in order to properly release.
    //
    mutexStatus = seMutexAcquireSEMutex();
    CHECK_SE_STATUS(mutexStatus);
    CHECK_SE_STATUS(seInit(seEccKeySize, &sePkaReg));

    //
    // The following steps need to be followed in order to sign a piece of message
    //    1.    get SHA256(message)
    //    2.    use RNG engine to get a random number r(0 < r < n) where rP(X1, Y1) != 0
    //    3.    use PKA engine to get R = X1 mod n, if R == 0, go back to 2 (Modular reduction)
    //    4.    use PKA engine to get dR mod n(Modular multiplication) (d = private key)
    //    5.    use PKA engine to get M + dR mod n(Modular addition)
    //    6.    use PKA engine to get r - 1 mod n(Modular ilwersion)
    //    7.    use PKA engine to get S = r - 1(M + dR) mod n, if S == 0, go back to 2 (Modular multiplication)
    //    8.    signature(R, S) is generated and need to be transmitted along with message
    //
    //    Step 1 is perfomed by the caller. It is passed in as msgHash.
    //
    for (numAttempts = 0; numAttempts < retryLoopCount; numAttempts++)
    {
        // 
        // Set status to failure,  status will be updated when algorithm succeeds
        //
        status = SE_ERR_FAILED_ECDSA_ATTEMPTS;

        //
        // Generate a random number
        //
        CHECK_SE_STATUS(seTrueRandomGetNumber_HAL(&Se, randNum, sePkaReg.pkaKeySizeDW));

        // Check if random number is 0, if it is start all over
        if (seBigNumberEqual(randNum, bigZero, sePkaReg.pkaKeySizeDW))
        {
            continue;
        }

        //
        // sigR = X point of point multiplication operation  
        // This is step 2, part 2:  point multiplication of randNum * P(GenX, GenY), R = X1
        //
        CHECK_SE_STATUS(seECPointMult_HAL(&Se, modulus, a, generatorPointX, generatorPointY, randNum, sigR, NULL, &sePkaReg));

        //
        // sigR = sigR mod gorder 
        // This is step 3:  R = X1 (from step 2.2) mod gOrder
        //
        CHECK_SE_STATUS(seModularReduction_HAL(&Se, sigR, gorder, sigR, &sePkaReg));

        //
        // Check if sigR is zero, if it is start over
        //
        if (seBigNumberEqual(sigR, bigZero, sePkaReg.pkaKeySizeDW))
        {
            continue;
        }

        //
        // This is step 4, part 1:  d = privateKey mod gorder
        //
        CHECK_SE_STATUS(seModularReduction_HAL(&Se, privateKey, gorder, d, &sePkaReg));

        //
        // This is step 5, part 1:  M = msgHash mod gorder
        //
        CHECK_SE_STATUS(seModularReduction_HAL(&Se, msgHash, gorder, M, &sePkaReg));

        //
        // This is step 6:  k_ilwerse = 1/k mod gorder 
        CHECK_SE_STATUS(seModularIlwersion_HAL(&Se, randNum, gorder, k_ilwerse, &sePkaReg));

        //
        // sigS = d*sigR mod gorder 
        // This is step 4, part 2:   sigS = d (step 4.1) * R mod gorder (step 3)
        //
        CHECK_SE_STATUS(seModularMult_HAL(&Se, d, sigR, gorder, sigS, &sePkaReg));

        //
        // sigS = M + sigS mod gorder
        // This is step 5, part 2:  M (step 5.1) + sigS (step 4.2) mod gorder
        //
        CHECK_SE_STATUS(seModularAdd_HAL(&Se, M, sigS, gorder, sigS, &sePkaReg));

        //
        // sigS = sigS*(1/k) mod gorder 
        // This is step 7: sigS (step 5.2) * k_ilwerse (step 6) mod gorder
        //
        CHECK_SE_STATUS(seModularMult_HAL(&Se, sigS, k_ilwerse, gorder, sigS, &sePkaReg));

        //
        // Check if sigS is zero, if it is start over
        //
        if (seBigNumberEqual(sigS, bigZero, sePkaReg.pkaKeySizeDW))
        {
            continue;
        }

        //
        // If we reach this point, SUCCESS!
        // End the loop
        //
        status = SE_OK;
        break;
    }

ErrorExit:
    CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus);
    return status;
}


