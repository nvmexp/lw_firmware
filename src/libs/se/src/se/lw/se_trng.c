/*
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/*!
 * @file   se_trng.c
 * @brief  Non halified library functions for supporting true random number generator.
 *
 */
#include "se_objse.h"
#include "se_private.h"
#include "seapi.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Gets a random number from true random number generator
 *
 * @param[out] pRandNum      - the random number generated is returned in this
 * @param[in]  sizeInDWords  - the size of the random number requested in dwords
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seTrueRandomGetNumber(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status      = SE_OK;
    SE_STATUS mutexStatus = SE_OK;

    //
    // Check input data in public APIs
    //
    if ((pRandNum == NULL) || (sizeInDWords <= 0))
    {
        status = SE_ERR_BAD_INPUT_DATA;
        goto ErrorExit;
    }

    //
    // Acquire mutex and init SE.
    // Preserve mutex status, only free if mutex acquire passed.
    //
    mutexStatus = seMutexAcquireSEMutex();
    status = mutexStatus;
    CHECK_SE_STATUS(mutexStatus);

    // Pass NULL for SE_PKA_REG as we don't need to initialize radix for TRNG
    CHECK_SE_STATUS(seInit(SE_ECC_MIN_KEY_SIZE, NULL));

    // Get the random number
    CHECK_SE_STATUS(seTrueRandomGetNumber_HAL(&Se, pRandNum, sizeInDWords));

ErrorExit:
    CHECK_MUTEX_STATUS_AND_RELEASE(mutexStatus);
    return status;
}

/*!
 * @brief Gets a random number from true random number generator for HS Mode
 *
 * @param[out] pRandNum      - the random number generated is returned in this
 * @param[in]  sizeInDWords  - the size of the random number requested in dwords
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seTrueRandomGetNumberHs(LwU32 *pRandNum, LwU32 sizeInDWords)
{
    SE_STATUS status      = SE_OK;
    SE_STATUS mutexStatus = SE_OK;

    //
    // Check input data in public APIs
    //
    if ((pRandNum == NULL) || (sizeInDWords <= 0))
    {
        status = SE_ERR_BAD_INPUT_DATA;
        goto ErrorExit;
    }

    //
    // Acquire mutex and init SE.
    // Preserve mutex status, only free if mutex acquire passed.
    //
    mutexStatus = seMutexAcquireSEMutexHs();
    status = mutexStatus;
    CHECK_SE_STATUS(mutexStatus);

    // Pass NULL for SE_PKA_REG as we don't need to initialize radix for TRNG
    CHECK_SE_STATUS(seInitHs(SE_KEY_SIZE_256, NULL));

    // Get the random number
    CHECK_SE_STATUS(seTrueRandomGetNumberHs_HAL(&Se, pRandNum, sizeInDWords));

ErrorExit:
    CHECK_MUTEX_STATUS_AND_RELEASE_HS(mutexStatus);
    return status;
}

