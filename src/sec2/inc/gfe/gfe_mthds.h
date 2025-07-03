/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GFE_MTHDS_H
#define GFE_MTHDS_H

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief The SEC2 GFE short-hand notation for the LW95A1 GFE class method.
 *
 * @param[in]   mthd    SEC2 GFE short-hand notation.
 *
 * @return  LW95A1 GFE class method.
 */
#define GFE_METHOD_ID(mthd) LW95A1_GFE_##mthd

/*!
 * @brief Colwerts an index in appMthdArray into the LW95A1 GFE method value.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  LW95A1 GFE class method.
 */
#define GFE_GET_METHOD_ID(idx) \
    (GFE_METHOD_ID(READ_ECID) + ((idx) * 4))

/*!
 * @brief Obtains a pointer to the method's parameter which is stored within the
 *        frame buffer.
 *
 * @param[in]   idx     Index in the appMthdArray.
 *
 * @return  The offset within the frame buffer where the method's parameter is
 *          stored.
 */
#define GFE_GET_METHOD_PARAM_OFFSET(idx) appMthdArray[(idx)]

/*!
 * @brief Colwerts method ID to index used for appMthdArray
 *
 * @param[in]   mthdId   Method ID from class header
 *
 */
#define GFE_GET_SEC2_METHOD_ID(mthdId) \
    (((mthdId) - GFE_METHOD_ID(READ_ECID)) / 4)

#define GFE_RESPONSE_BUF_SIZE    (256)

// AES block size constants
#define AES_B                               16
#define AES_DW                              4
#define AES_RK                              44

// define this to use RSA1K, undefine it for RSA2K support
#define USE_RSA_1K

#ifdef USE_RSA_1K
#define RSA_KEY_SIZE                 (1024)
#else // RSA 2K
#define BIGINT_MAX_DW                192
#define RSA_KEY_SIZE                 (2048)
#endif

#define GFE_RSA_AES_IV_SIZE_BYTES    (128/8)

#define GFE_RSA_SIGN_SIZE            (RSA_KEY_SIZE/8)
#define GFE_RSA_PRIV_KEY_N           (RSA_KEY_SIZE/8)  // modulus
#define GFE_RSA_PRIV_KEY_P           (RSA_KEY_SIZE/16) // prime1
#define GFE_RSA_PRIV_KEY_Q           (RSA_KEY_SIZE/16) // prime2
#define GFE_RSA_PRIV_KEY_DP          (RSA_KEY_SIZE/16) // exponent1
#define GFE_RSA_PRIV_KEY_DQ          (RSA_KEY_SIZE/16) // exponent2
#define GFE_RSA_PRIV_KEY_IQ          (RSA_KEY_SIZE/16) // coefficient

#define GFE_RSA_PRIV_KEY_OFF_N       (0)
#define GFE_RSA_PRIV_KEY_OFF_P       (GFE_RSA_PRIV_KEY_OFF_N  + GFE_RSA_PRIV_KEY_N)
#define GFE_RSA_PRIV_KEY_OFF_Q       (GFE_RSA_PRIV_KEY_OFF_P  + GFE_RSA_PRIV_KEY_P)
#define GFE_RSA_PRIV_KEY_OFF_DP      (GFE_RSA_PRIV_KEY_OFF_Q  + GFE_RSA_PRIV_KEY_Q)
#define GFE_RSA_PRIV_KEY_OFF_DQ      (GFE_RSA_PRIV_KEY_OFF_DP + GFE_RSA_PRIV_KEY_DP)
#define GFE_RSA_PRIV_KEY_OFF_IQ      (GFE_RSA_PRIV_KEY_OFF_DQ + GFE_RSA_PRIV_KEY_DQ)

#define GFE_RSA_PRIV_KEY_SIZE_ORI    (GFE_RSA_PRIV_KEY_N + GFE_RSA_PRIV_KEY_P + GFE_RSA_PRIV_KEY_Q + GFE_RSA_PRIV_KEY_DP \
                                     + GFE_RSA_PRIV_KEY_DQ + GFE_RSA_PRIV_KEY_IQ)
#define GFE_RSA_PRIV_KEY_SIZE        (GFE_RSA_AES_IV_SIZE_BYTES + GFE_RSA_PRIV_KEY_SIZE_ORI + SHA256_HASH_BLOCK)

#define RSAOAEP_BUFFERSIZE           (RSA_KEY_SIZE/8)  // RSAOAEP_BUFFERSIZE: 1024/8=128, 2048/8=256;         
#define GFE_SIZE_PRIME_8             (RSA_KEY_SIZE/16) // GFE_SIZE_P_8, GFE_SIZE_Q_8:  1024/16=64, 2048/16=128              
#define GFE_SIZE_RX_PRIV_EXPONENT_8  (RSA_KEY_SIZE/16) // GFE_SIZE_RX_PRIV_EXPONENT_8: 1024/16=64, 2048/16=128
#define GFE_SIZE_RX_MODULUS_32       (RSA_KEY_SIZE/32) // GFE_SIZE_RX_MODULUS_32: // 1024/32=32, 2048/32=64   
#define GFE_SIZE_RX_PRIV_EXPONENT_32 (RSA_KEY_SIZE/64) // GFE_SIZE_RX_PRIV_EXPONENT_32: 1024=16, 2048=32      
#define GFE_SIZE_RX_MODULUS_8        (RSA_KEY_SIZE/8)  // GFE_SIZE_RX_MODULUS_8: 1024=128, 1024=256           

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS gfeHandleReadEcid(void)
    GCC_ATTRIB_SECTION("imem_gfe", "gfeHandleReadEcid");

void gfeRsaSign(const LwU8 *p, const LwU8 *q, const LwU8 *dP, const LwU8 *dQ,
                const LwU8 *qIlw, const LwU8 *Ekm, LwU8 *Km, const LwU8 *n)
    GCC_ATTRIB_SECTION("imem_gfe", "gfeRsaSign");
FLCN_STATUS gfeEmsaPssEncode(LwU8 *pMsg, LwU32 msgLen, LwU8 *pEm, LwU32 emLen)
    GCC_ATTRIB_SECTION("imem_gfe", "gfeEmsaPssEncode");
FLCN_STATUS gfeGetRandNo(LwU8 *pDest, LwS32 size)
    GCC_ATTRIB_SECTION("imem_gfe", "_gfeGetRandNo");

FLCN_STATUS gfeCryptEntry(LwBool bEncrypt, LwU8 *pKeyBuf,
                          LwU32 sz, LwU8 *pDest, LwU8 *pIvBuf)
    GCC_ATTRIB_SECTION("imem_gfeCryptHS", "start");

#endif // GFE_MTHDS_H
