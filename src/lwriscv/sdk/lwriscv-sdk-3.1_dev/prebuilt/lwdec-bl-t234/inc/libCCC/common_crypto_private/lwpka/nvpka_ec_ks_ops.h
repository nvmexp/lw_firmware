/*
 * Copyright (c) 2021, NVIDIA Corporation. All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/* NVPKA 1.1 keystore operations
 */
#ifndef INCLUDED_LWPKA_EC_KS_OPS_H
#define INCLUDED_LWPKA_EC_KS_OPS_H

/** @brief ECC keystore.
 *
 * @defgroup ecc_keystore ECC keystore operations and values.
 */
/*@{*/

/* These should likely get moved to crypto_api.h
 * At some later time....
 */
#define CCC_MANIFEST_EC_KS_USER_NONE	0U /**< NONE */
#define CCC_MANIFEST_EC_KS_USER_PWR	1U /**< PWR */
#define CCC_MANIFEST_EC_KS_USER_LWDEC	2U /**< NVDEC */
#define CCC_MANIFEST_EC_KS_USER_SEC	3U /**< SEC */
#define CCC_MANIFEST_EC_KS_USER_GSP	4U /**< GSP */
#define CCC_MANIFEST_EC_KS_USER_PRIV	5U /**< PRIV */
#define CCC_MANIFEST_EC_KS_USER_FSP	6U /**< FSP */

#if HAVE_LWPKA11_SUPPORT == 0
/* No EC keystores enabed: operations will fail
 */
#define NVPKA_MAX_EC_KEYSTORES 0U

#else

/* HW DISPOSE is restricted operation which will always fail
 * when not done by ROM code. So don't enable it unless required.
 */
#define CCC_EC_KS_DISPOSE_SUPPORTED 0

/* We have only one keystore location in NVPKA-1.1 prepare for more
 * "keystores".
 */
#define NVPKA_MAX_EC_KEYSTORES 1U

static inline bool m_ec_ks_keyop_algo(te_crypto_algo_t alg)
{
	return ((TE_ALG_KEYOP_GENKEY == alg) || (TE_ALG_KEYOP_INS == alg) ||
		(TE_ALG_KEYOP_GENPUB == alg) || (TE_ALG_KEYOP_DISPOSE == alg) ||
		(TE_ALG_KEYOP_LOCK == alg));
}

#define CCC_EC_KS_KEYOP_ALGO(alg) m_ec_ks_keyop_algo(alg)

static inline bool m_ec_ks_curve_supported(te_ec_curve_id_t curve_id)
{
	return ((TE_CURVE_NIST_P_256 == curve_id) ||
		(TE_CURVE_NIST_P_384 == curve_id) ||
		(TE_CURVE_NIST_P_521 == curve_id));
}

#define CCC_EC_KS_CURVE_SUPPORTED(curve_id) m_ec_ks_curve_supported(curve_id)

/*@}*/

#if HAVE_LWPKA11_ECKS_KEYOPS
/* Support function to map algorithm to nvpka_engine_ops
 */
status_t nvpka_ec_ks_map_algo_to_operation(te_crypto_algo_t algo,
					   enum nvpka_engine_ops *eop_p);

/** @brief Insert EC private key to keystore
 *
 * @defgroup ec_keystore_primitives EC keystore primitive operations.
 * @ingroup ec_keystore
 */
/*@{*/
status_t nvpka_ec_ks_insert_key_locked(const engine_t *engine,
				       enum pka1_op_mode op_mode,
				       uint32_t keystore,
				       const uint8_t *keydata,
				       bool key_big_endian,
				       uint32_t ec_ks_user,
				       uint32_t ec_ks_flags,
				       uint32_t ec_ks_sw);

/** @brief Generate a random EC private key to keystore.
 */
status_t nvpka_ec_ks_gen_key_locked(const engine_t *engine,
				    enum pka1_op_mode op_mode,
				    uint32_t keystore,
				    uint32_t ec_ks_user,
				    uint32_t ec_ks_flags,
				    uint32_t ec_ks_sw);

/** @brief Generate a keystore public key with the existing private keystore key.
 */
status_t nvpka_ec_ks_gen_pub_locked(const engine_t *engine,
				    uint32_t keystore,
				    te_ec_point_t *pubkey);

#if CCC_EC_KS_DISPOSE_SUPPORTED
/** @brief Dispose keystore key pair.
 */
status_t nvpka_ec_ks_dispose_locked(const engine_t *engine,
				    uint32_t keystore);
#endif

/** @brief Lock keystore key pair (sticky-to-reset).
 */
status_t nvpka_ec_ks_lock_locked(const engine_t *engine,
				 uint32_t keystore);
/*@}*/

/** @brief Dispatch function to call the EC keystore primitives
 *  e.g. via CCC crypto api.
 *
 * @defgroup ec_keystore_dispatch EC keystore dispatcher
 * @ingroup ec_keystore
 */
/*@{*/
status_t engine_nvpka_ec_ks_keyop_locked(struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext);
#endif /* HAVE_LWPKA11_ECKS_KEYOPS */

/** @brief EC keystore supported algorithms
 *
 * @defgroup ec_keystore_algorithms EC keystore algorithms
 * @ingroup ec_keystore
 */
/*@{*/

/** @brief ECDSA with selected curves.
 *
 * EC keystore supports the NIST-P256, NIST-P384 and NIST-P521 curves.
 */
status_t engine_nvpka_ec_ks_ecdsa_sign_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext);
/*@}*/
#endif /* HAVE_LWPKA11_SUPPORT */
#endif /* INCLUDED_LWPKA_EC_KS_OPS_H */
