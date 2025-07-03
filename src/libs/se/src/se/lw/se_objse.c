/*
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   se_objse.c
 * @brief  Non halified library functions
 *
 */

#include "se_objse.h"
#include "se_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Implementation --------------------------------- */

/*!
 * @brief Tries to acquire the SE mutex.  Read operation signifies that we want to aquire 
 *         the mutex. 
 *  
 * @return SE_OK if successful. SE_ERR_MUTEX_ACQUIRE if unsuccessful.
 */
SE_STATUS seMutexAcquireSEMutex(void)
{
    return seMutexAcquire_HAL(&Se);
}

/*!
 * @brief Tries to acquire the SE mutex in HS Code.  Read operation signifies that we want to aquire 
 *         the mutex. 
 *  
 * @return SE_OK if successful. SE_ERR_MUTEX_ACQUIRE if unsuccessful.
 */
SE_STATUS seMutexAcquireSEMutexHs(void)
{
    return seMutexAcquireHs_HAL(&Se);
}

/*!
 * @brief Releases the SE mutex. 
 *  
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seMutexReleaseSEMutex(void)
{
    return seMutexRelease_HAL(&Se);
}

/*!
 * @brief Releases the SE mutex in HS Mode. 
 *  
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seMutexReleaseSEMutexHs(void)
{
    return seMutexReleaseHs_HAL(&Se);
}

/*!
 * @brief Function to initialize SE for PKA operations
 *
 * @param[in] bitCount       Radix or the size of the operation
 * @param[in/out] pPkaReg    Struct containing the number of words based
 *                           on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seInit
(
    LwU32 bitCount,
    PSE_PKA_REG pPkaReg
)
{
    return seInit_HAL(&Se, bitCount, pPkaReg);
}

/*!
 * @brief Function to initialize SE for PKA operations in HS Mode
 *
 * @param[in] bitCount       Radix or the size of the operation
 * @param[in/out] pPkaReg    Struct containing the number of words based
 *                           on the radix
 *
 * @return SE_OK if successful. Error code if unsuccessful.
 */
SE_STATUS seInitHs
(
    SE_KEY_SIZE bitCount,
    PSE_PKA_REG pPkaReg
)
{
    return seInitHs_HAL(&Se, bitCount, pPkaReg);
}

