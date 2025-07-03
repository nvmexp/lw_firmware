/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <liblwriscv/libc.h>

#include "tegra_se.h"
#include "tegra_lwpka_gen.h"
#include "tegra_lwpka_ec.h"
#include "tegra_lwpka_mod.h"
#include "crypto_ec.h"

#include "utils.h"
#include "ecc_key_gen.h"
#include "debug.h"

/*!
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * This code is for testing and libCCC usage purposes only!
 * Do not replicate blindly for production usecases. Consult with your local crypto expert!
 * 
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 * */

/** @brief Verify generated key pair matches expectation from test vector
 *
 * @param algorithm: defines which elliptic lwrve to use
 * @param publicKey: public key is a struct ptr containing a point on the lwrve
 * @param privateKey: private key (only known by private key owner) used to generate public key
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t verifyKeyPair(
    uint8_t* xExpected,
    uint8_t* yExpected,
    uint8_t* keyPriv,
    struct te_ec_point_s* keyPub,
    uint32_t keySizeBytes
);

/** @brief Generate 256 & 384 bit ECC DH (256 & 384) Key Pairs
 *
 * @param None
 *
 * @return None
 */
void generateKeyPairECCTest(void)
{
    /*
        Test vector source: https://csrc.nist.gov/CSRC/media/Projects/Cryptographic-Algorithm-Validation-Program/dolwments/components/ecccdhtestvectors.zip
    
        These can be used to retrieve the shared secret, but for key pair generation we only need the private key (dIUT) and the expected public key (QIUTx, QIUTy) that should be generated from this private key.

        [P-256]
        COUNT = 0
        QCAVSx = 700c48f77f56584c5cc632ca65640db91b6bacce3a4df6b42ce7cc838833d287
        QCAVSy = db71e509e3fd9b060ddb20ba5c51dcc5948d46fbf640dfe0441782cab85fa4ac
        dIUT = 7d7dc5f71eb29ddaf80d6214632eeae03d9058af1fb6d22ed80badb62bc1a534
        QIUTx = ead218590119e8876b29146ff89ca61770c4edbbf97d38ce385ed281d8a6b230
        QIUTy = 28af61281fd35e2fa7002523acc85a429cb06ee6648325389f59edfce1405141
        ZIUT = 46fc62106420ff012e54a434fbdd2d25ccc5852060561e68040dd7778997bd7b

        [P-384]
        COUNT = 0
        QCAVSx = a7c76b970c3b5fe8b05d2838ae04ab47697b9eaf52e764592efda27fe7513272734466b400091adbf2d68c58e0c50066
        QCAVSy = ac68f19f2e1cb879aed43a9969b91a0839c4c38a49749b661efedf243451915ed0905a32b060992b468c64766fc8437a
        dIUT = 3cc3122a68f0d95027ad38c067916ba0eb8c38894d22e1b15618b6818a661774ad463b205da88cf699ab4d43c9cf98a1
        QIUTx = 9803807f2f6d2fd966cdd0290bd410c0190352fbec7ff6247de1302df86f25d34fe4a97bef60cff548355c015dbb3e5f
        QIUTy = ba26ca69ec2f5b5d9dad20cc9da711383a9dbe34ea3fa5a2af75b46502629ad54dd8b7d73a8abb06a3a3be47d650cc99
        ZIUT = 5f9d29dc5e31a163060356213669c8ce132e22f57c9a04f40ba7fcead493b457e5621e766c40a2e3d4d6a04b25e533f1
    */
    struct te_ec_point_s keyPub256 = {0};
    struct te_ec_point_s keyPub384 = {0};

    uint8_t dIUT256[]   = {0x7d, 0x7d, 0xc5, 0xf7, 0x1e, 0xb2, 0x9d, 0xda, 0xf8, 0x0d, 0x62, 0x14, 0x63, 0x2e, 0xea, 0xe0, 0x3d, 0x90, 0x58, 0xaf, 0x1f, 0xb6, 0xd2, 0x2e, 0xd8, 0x0b, 0xad, 0xb6, 0x2b, 0xc1, 0xa5, 0x34};
    uint8_t QIUTx256[]  = {0xea, 0xd2, 0x18, 0x59, 0x01, 0x19, 0xe8, 0x87, 0x6b, 0x29, 0x14, 0x6f, 0xf8, 0x9c, 0xa6, 0x17, 0x70, 0xc4, 0xed, 0xbb, 0xf9, 0x7d, 0x38, 0xce, 0x38, 0x5e, 0xd2, 0x81, 0xd8, 0xa6, 0xb2, 0x30};
    uint8_t QIUTy256[]  = {0x28, 0xaf, 0x61, 0x28, 0x1f, 0xd3, 0x5e, 0x2f, 0xa7, 0x00, 0x25, 0x23, 0xac, 0xc8, 0x5a, 0x42, 0x9c, 0xb0, 0x6e, 0xe6, 0x64, 0x83, 0x25, 0x38, 0x9f, 0x59, 0xed, 0xfc, 0xe1, 0x40, 0x51, 0x41};

    uint8_t dIUT384[]   = {0x3c, 0xc3, 0x12, 0x2a, 0x68, 0xf0, 0xd9, 0x50, 0x27, 0xad, 0x38, 0xc0, 0x67, 0x91, 0x6b, 0xa0, 0xeb, 0x8c, 0x38, 0x89, 0x4d, 0x22, 0xe1, 0xb1, 0x56, 0x18, 0xb6, 0x81, 0x8a, 0x66, 0x17, 0x74, 0xad, 0x46, 0x3b, 0x20, 0x5d, 0xa8, 0x8c, 0xf6, 0x99, 0xab, 0x4d, 0x43, 0xc9, 0xcf, 0x98, 0xa1};
    uint8_t QIUTx384[]  = {0x98, 0x03, 0x80, 0x7f, 0x2f, 0x6d, 0x2f, 0xd9, 0x66, 0xcd, 0xd0, 0x29, 0x0b, 0xd4, 0x10, 0xc0, 0x19, 0x03, 0x52, 0xfb, 0xec, 0x7f, 0xf6, 0x24, 0x7d, 0xe1, 0x30, 0x2d, 0xf8, 0x6f, 0x25, 0xd3, 0x4f, 0xe4, 0xa9, 0x7b, 0xef, 0x60, 0xcf, 0xf5, 0x48, 0x35, 0x5c, 0x01, 0x5d, 0xbb, 0x3e, 0x5f};
    uint8_t QIUTy384[]  = {0xba, 0x26, 0xca, 0x69, 0xec, 0x2f, 0x5b, 0x5d, 0x9d, 0xad, 0x20, 0xcc, 0x9d, 0xa7, 0x11, 0x38, 0x3a, 0x9d, 0xbe, 0x34, 0xea, 0x3f, 0xa5, 0xa2, 0xaf, 0x75, 0xb4, 0x65, 0x02, 0x62, 0x9a, 0xd5, 0x4d, 0xd8, 0xb7, 0xd7, 0x3a, 0x8a, 0xbb, 0x06, 0xa3, 0xa3, 0xbe, 0x47, 0xd6, 0x50, 0xcc, 0x99};

    verifyKeyPair(QIUTx256, QIUTy256, dIUT256, &keyPub256, sizeof(dIUT256));
    verifyKeyPair(QIUTx384, QIUTy384, dIUT384, &keyPub384, sizeof(dIUT384));
    return;
}

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
)
{
    status_t                            ret             = NO_ERROR;
    engine_t                            *engine         = NULL;
    struct se_data_params               input_params    = {0};
    struct se_engine_ec_context         ec_context      = {0};
    struct te_ec_point_s                generatorPoint  = {0};

    // Select Engine
    ret = ccc_select_engine((const struct engine_s **)&engine, ENGINE_CLASS_EC, ENGINE_PKA);
    CCC_ERROR_CHECK(ret);

    ret = ACQUIRE_MUTEX();
    CCC_ERROR_CHECK(ret);

    // Initialize ECDSA Engine Context
    ec_context.engine       = engine;
    ec_context.cmem         = NULL;
    ec_context.ec_flags     = CCC_EC_FLAGS_DYNAMIC_KEYSLOT;
    ec_context.ec_keytype   = KEY_TYPE_EC_PRIVATE;
    ec_context.algorithm    = algorithm;
    ec_context.domain       = TE_CRYPTO_DOMAIN_UNKNOWN;
    ec_context.ec_pkey      = privateKey;
    ec_context.ec_pubkey    = publicKey;

    // Get Generator Point (nist_lwrves.h)
    te_ec_lwrve_id_t lwrve  = TE_LWRVE_NONE;
    switch((keySizeBytes << 3)) 
    {
        case 256:
            lwrve = TE_LWRVE_NIST_P_256;
            break;
        case 384:
            lwrve = TE_LWRVE_NIST_P_384;
            break;
        default:
            break;
    }
    
    ec_context.ec_lwrve = ccc_ec_get_lwrve(lwrve);
    memcpy(generatorPoint.x, ec_context.ec_lwrve->g.gen_x, keySizeBytes);
    memcpy(generatorPoint.y, ec_context.ec_lwrve->g.gen_y, keySizeBytes);
    generatorPoint.point_flags = CCC_EC_POINT_FLAG_NONE;

    // Generate Public Key
    ec_context.ec_k = privateKey;
    ec_context.ec_k_length  = keySizeBytes;
    input_params.point_P = &generatorPoint;
    input_params.point_Q = publicKey;
    input_params.input_size = sizeof(te_ec_point_t);
    input_params.output_size = sizeof(te_ec_point_t);
    ret = engine_lwpka_ec_point_mult_locked(&input_params, &ec_context);
    CCC_ERROR_CHECK(ret);
    
fail:
    RELEASE_MUTEX();
    TRACE_NN("ECC %d KEY GENERATION = ", (keySizeBytes << 3));
    if(ret == NO_ERROR)
    {
        TRACE("success");
    }
    else
    {
        TRACE("failed; err = %d\n", ret);
    }
    DUMP_MEM("PrivateKey", privateKey, keySizeBytes);
    DUMP_MEM("PublicKey.x ", publicKey->x, keySizeBytes);
    DUMP_MEM("PublicKey.y ", publicKey->y, keySizeBytes);
    return ret;
}

/** @brief Verify generated key pair matches expectation from test vector
 *
 * @param algorithm: defines which elliptic lwrve to use
 * @param publicKey: public key is a struct ptr containing a point on the lwrve
 * @param privateKey: private key (only known by private key owner) used to generate public key
 * @param keySizeBytes: key size in bytes (only 256b and 384b are lwrrently supported)
 *
 * @return status_t: NO_ERROR on success, error (from error_code.h) on failure
 */
static status_t verifyKeyPair(
    uint8_t* xExpected,
    uint8_t* yExpected,
    uint8_t* keyPriv,
    struct te_ec_point_s* keyPub,
    uint32_t keySizeBytes
)
{
    status_t ret = eccGetKeyPair(TE_ALG_ECDH, keyPub, keyPriv, keySizeBytes);
    printf("ECC DH %d Key Pair Generation: ", (keySizeBytes << 3));
    if ((ret != NO_ERROR) ||
        (memCompare(xExpected, keyPub->x, keySizeBytes)) ||
        (memCompare(yExpected, keyPub->y, keySizeBytes)))
    {
        printf("failed %d\n", ret);
        DUMP_MEM("PrivateKey  ", keyPriv, keySizeBytes);
        DUMP_MEM("PublicKey.x ", keyPub->x, keySizeBytes);
        DUMP_MEM("xExpected   ", xExpected, keySizeBytes);
        DUMP_MEM("PublicKey.y ", keyPub->y, keySizeBytes);
        DUMP_MEM("yExpected   ", yExpected, keySizeBytes);
    }
    else
    {
        printf("success\n");
    }
    return ret;
}