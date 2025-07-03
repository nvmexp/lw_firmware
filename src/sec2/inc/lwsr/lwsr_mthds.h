/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWSR_MTHDS_H
#define LWSR_MTHDS_H

/*!
 * @file    lwsr_mthds.h
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief The SEC2 LWSR short-hand notation for the LW95A1 LWSR class method.
 *
 * @param[in]   mthd    SEC2 LWSR short-hand notation.
 *
 * @return  LW95A1 LWSR class method.
 */
#define LWSR_METHOD_ID(mthd) LW95A1_LWSR_##mthd

/*!
 * @brief Colwerts an index in appMthdArray into the LW95A1 LWSR method value.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  LW95A1 LWSR class method.
 */
#define LWSR_GET_METHOD_ID(idx) \
    (LWSR_METHOD_ID(MUTEX_ACQUIRE) + ((idx) * 4))

/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define LWSR_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define LWSR_GET_SEC2_METHOD_ID(mthdId) \
    (((mthdId) - LWSR_METHOD_ID(MUTEX_ACQUIRE)) / 4)

/*!
 * Defines of the offset and size of each key in the FB.
 * The layout of each key in the FB will like below:
 * "MUTEX(20Bytes) --> MSG(12Bytes) --> ENC PRIV(16Bytes) --> Signature of ENC PRIV(32Bytes) --> SEMA(4Bytes)"
 */
#define LWSR_MUTEX_KEY_OFFS             0
#define LWSR_MUTEX_KEY_SIZE             20
#define LWSR_MSG_KEY_OFFS               (LWSR_MUTEX_KEY_OFFS + LWSR_MUTEX_KEY_SIZE)
#define LWSR_MSG_KEY_SIZE               12
#define LWSR_ENC_PRIV_KEY_OFFS          (LWSR_MSG_KEY_OFFS + LWSR_MSG_KEY_SIZE)
#define LWSR_ENC_PRIV_KEY_SIZE          48  // Full size of the encrypted vendor private key blob (which includes 16 bytes of encrypted key + 32 bytes of hash signature)
#define LWSR_PLAIN_PRIV_KEY_SIZE        16  // Size of the encrypted vendor private key
#define LWSR_SEMA_OFFS                  (LWSR_ENC_PRIV_KEY_OFFS + LWSR_ENC_PRIV_KEY_SIZE)
#define LWSR_SEMA_SIZE                  4

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS lwsrMutexKeyComputation(void)
    GCC_ATTRIB_SECTION("imem_lwsr", "lwsrMutexKeyComputation");

FLCN_STATUS lwsrCryptEntry(LwU8 *pKeyBuf)
    GCC_ATTRIB_SECTION("imem_lwsrCrypt", "start");

#endif // LWSR_MTHDS_H
