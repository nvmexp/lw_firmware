/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef ECC_KEY_GEN_H
#define ECC_KEY_GEN_H

#include "crypto_api.h"

/** @brief Generate 256 & 384 bit ECC DH (256 & 384) Key Pairs
 *
 * @param None
 *
 * @return None
 */
void generateKeyPairECCTest(void);

/** @brief Generate ECC Key Pair
 *
 * @param algorithm: defines which elliptic lwrve to use
 * @param publicKey: public key is a struct ptr containing a point on the lwrve
 * @param privateKey: private key (only known by private key owner) used to generate public key
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
status_t eccGetKeyPair(
    te_crypto_algo_t algorithm,
    struct te_ec_point_s* publicKey,
    uint8_t* privateKey,
    uint32_t keySizeBytes
);

#endif  // ECC_KEY_GEN_H
