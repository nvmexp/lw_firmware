/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @note    This file is branched directly from the VBIOS tree for data
 *          structures that need to be shared between VBIOS microcodes and
 *          driver microcodes. Check the P4 file history to see from where it
 *          was branched.
 *
 *          This means that the file should almost never be directly modified.
 *          If updates to this file are needed for new functionality, changes
 *          to the original source file from the VBIOS tree should be merged
 *          into this file.
 *
 *          Additional definitions that are common but that are not pulled
 *          directly from the VBIOS headers should be placed in addendum files.
 */

#if !defined(__CRYPTO_H__INCLUDED_)
#define __CRYPTO_H__INCLUDED_

#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

/*----------------------------------------------------*/
/*                                                    */
/* defines and structures related to cryptography     */
/*                                                    */
/*----------------------------------------------------*/

#define CRYPTO_SHA256_SIZE             32
#define CRYPTO_DMHASH_SIZE             16
#define CRYPTO_FLW128HASH_SIZE         16
#define CRYPTO_AES128_SIZE             CRYPTO_DMHASH_SIZE
#define CRYPTO_3AES128_SIG_SIZE        32
#define CRYPTO_RSA_MODULUS             128
#define CRYPTO_RSA1K_SIG_SIZE          128
#define CRYPTO_RSA1K_SIG_BITS_SIZE     8 * CRYPTO_RSA1K_SIG_SIZE
#define CRYPTO_RSA2K_SIG_SIZE          256
#define CRYPTO_RSA3K_MODULUS           384
#define CRYPTO_RSA3K_SIG_SIZE          384
#define CRYPTO_RSA3K_SIG_BITS_SIZE     8 * CRYPTO_RSA3K_SIG_SIZE
#define CRYPTO_RSA4K_SIG_SIZE          512
#define CRYPTO_RSA4K_SIG_BITS_SIZE     8 * CRYPTO_RSA4K_SIG_SIZE
#define CRYPTO_ECC256_SIG_SIZE         32
#define CRYPTO_RSAPADDING_TYPE_PKCS1    1
#define CRYPTO_RSAPADDING_TYPE_PSS      2

#if defined(LW_FALCON) || defined(GCC_FALCON)
#pragma pack()
#else
#pragma pack(pop)
#endif

#endif    // __CRYPTO_H__INCLUDED_
