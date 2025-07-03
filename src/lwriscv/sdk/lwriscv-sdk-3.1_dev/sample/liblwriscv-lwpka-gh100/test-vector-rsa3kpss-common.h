/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef TESTVECTOR_RSA3KPSS_COMMON_H
#define TESTVECTOR_RSA3KPSS_COMMON_H

#define LW_CEIL(a,b)                (((a)+(b)-1)/(b))
#define MAX_SALT_LENGTH_BYTES       48  // Max today, might need to increase later if needed.

#define CRYPTO_RSA4K_SIG_SIZE       512
#define CRYPTO_RSA3K_SIG_SIZE       384
#define CRYPTO_RSA3K_SIG_BITS_SIZE  8 * CRYPTO_RSA3K_SIG_SIZE

#define SHA256_HASH_BYTE_SIZE       (256U/8U)
#define SHA384_HASH_BYTE_SIZE       (384U/8U)
#define MAX_HASH_SIZE_BYTES         SHA384_HASH_BYTE_SIZE


#define BIG_ENDIAN_SIGNATURE        0b00000001
#define BIG_ENDIAN_MODULUS          0b00000010
#define BIG_ENDIAN_EXPONENT         0b00000100
#define BIG_ENDIAN_SHA_HASH         0b00001000

#define LITTLE_ENDIAN_SIGNATURE     0b00000000
#define LITTLE_ENDIAN_MODULUS       LITTLE_ENDIAN_SIGNATURE
#define LITTLE_ENDIAN_EXPONENT      LITTLE_ENDIAN_SIGNATURE
#define LITTLE_ENDIAN_SHA_HASH      LITTLE_ENDIAN_SIGNATURE

#endif // TESTVECTOR_RSA3KPSS_COMMON_H
