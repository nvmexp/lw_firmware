/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   crypto_api.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * @brief Public values and object definitions for the CCC crypto API.
 *
 * Defines the objects and fields in the CCC API for using the Common Crypto
 * library in the subsystems.
 *
 * The call mechanism depends on the subsystem being used, it is not
 * defined in this file.
 *
 * If CCC is used in library mode the client usually needs to include
 * the following files in this order:
 *
 * <B>crypto_system_config.h</B> :
 *
 * For the CCC configuration file in the subsystem.  This file is not
 * part of CCC core, it belongs to the subsystem configuring CCC for
 * each particular use.
 *
 * This file also defines the interface to the adaptation functions
 * that CCC requires from the subsystem.
 *
 * <B>crypto_ops.h</B> :
 *
 * For the library mode call prototypes (includes crypto_api.h).
 * This file defines the CCC crypto api functions to use in CCC library mode.
 */
#ifndef INCLUDED_CRYPTO_API_H
#define INCLUDED_CRYPTO_API_H

/**
 * @brief Generic definitions
 *
 * @defgroup generic_definitions CCC generic definitions
 */
/*@{*/
#define BYTES_IN_WORD 4U	/**< Number of bytes in a 32 bit integer */
#define BITS_IN_WORD 32U	/**< Number of bits in an unsigned integer
				 * Device registers are accessed as 32 bit words
				 * in out devices.
				 */

/**
 * @brief Max RSA bit size supported by the HW engines
 *
 * Select one of these: 4096U or 2048U
 *
 * If you have PKA1 PKA (rsa/ec) engine => use 4096 bit max size
 * SE RSA0 only supports 2048 bit keys.
 *
 * HAVE_PKA1_RSA enables the PKA1 RSA support in the SE driver code,
 * but since this include file is also included to clients
 * you might want to enable the long RSA keys explicitly (as below)..
 */
#define MAX_RSA_BIT_SIZE 4096U	/**< Max bit length of supported for RSA operations */
#define MAX_RSA_BYTE_SIZE \
    (MAX_RSA_BIT_SIZE / 8U)	/**< Max byte length of supported for RSA operations */

#define RSA_MIN_MODULUS_SIZE_BITS  512U	/**< Min length of RSA keys supported.
					 * Configuration may restrict min approved size
					 * elsewhere, e.g. for security reasons.
					 */

#define SE_AES_MAX_KEYSLOTS 16U	/**< Number of SE engine HW keyslots */
#define SE_RSA_MAX_KEYSLOTS 4U	/**< Number of PKA0 engine HW keyslots */
#define PKA1_MAX_KEYSLOTS   4U	/**< Number of PKA1 engine HW keyslots */
/*@}*/

/************************/

/* This is a call from unfixed old OTE crypto client passing zero as
 * output buffer length. This is wrong, but since I want to be binary
 * compatible with this, I'll pass a magic value to the system telling
 * that "output buffer is long enough", i.e. the value below.
 */
#define MAGIC_OUT_LEN 0xcafefed0U /**< Not used by CCC core, size indicator magic value */

/**
 * @brief Crypto algorithms supported by CCC core
 *
 * @enum te_crypto_algo_e Defines CCC supported crypto algorithm ids
 *
 * @defgroup algorithms CCC Supported algorithms
 */
/*@{*/
typedef enum te_crypto_algo_e {
	TE_ALG_NONE		= 0x0000000, /**< Uninitialized algorithm value */

	/** Special key operations for T23X */

	TE_ALG_KEYOP_KW			= 0x00000020,	/**< [T23X] Key wrap */
	TE_ALG_KEYOP_KUW,	/**< [T23X] Key unwrap */
	TE_ALG_KEYOP_KWUW,	/**< [T23X] Key wrap / unwrap (MODE: ENCRYPT => WRAP, DECRYPT => UNWRAP) */
	TE_ALG_KEYOP_GENKEY,	/**< [T23X] Key generation (random key) */
	TE_ALG_KEYOP_INS,	/**< [T23X] Key insert */
	TE_ALG_KEYOP_CLONE,	/**< [T23X] Key clone */
	TE_ALG_KEYOP_LOCK,	/**< [T23X] Key lock */

	TE_ALG_KEYOP_KDF_1KEY, /**< [T23X] HMAC-SHA256 key drivation (using one key) */
	TE_ALG_KEYOP_KDF_2KEY, /**< [T23X] HMAC-SHA256 key drivation (using two keys) */

	TE_ALG_KEYOP_DISPOSE,	/**< [lwpka11] Key dispose */
	TE_ALG_KEYOP_GENPUB,	/**< [lwpka11] Public key generate */

	/* All cipher modes supported by the HW */

	TE_ALG_AES_ECB_NOPAD	= 0x00001000,	/**< AES w/ Electronic CodeBook mode, no padding */
	TE_ALG_AES_CBC_NOPAD,	/**< AES w/ Cipher Block Chaining mode, no padding */
	TE_ALG_AES_CTR,		/**< AES w/ Counter mode */
	TE_ALG_AES_CTS,		/**< AES w/ CipherText Stealing mode (NIST CBC-CS2 version) */
	TE_ALG_AES_ECB,		/**< AES w/ ECB mode with padding */
	TE_ALG_AES_CBC,		/**< AES w/ CBC mode with padding */

	TE_ALG_AES_OFB,		/**< AES w/ OFB mode (output feedback mode; streaming cipher mode) */
	TE_ALG_AES_XTS,		/**< [T19X/T23X] AES w/ XTS mode (cipher) */

	TE_ALG_AES_CCM,		/**< AES w/ CCM mode (aead) */
	TE_ALG_AES_GCM,		/**< AES w/ GCM mode (aead) */

	/** Message authentication codes.
	 */

	/* CMAC-AES: Hybrid mode.
	 *
	 * Hybrid mode:
	 * CMAC subkeys derived under SW control with AES HW engine.
	 * MAC value callwlation using AES engine.
	 */
	TE_ALG_CMAC_AES		= 0x00002000,	/**< CMAC-AES, AES mac with key lengths 128, 196, 256 */

	TE_ALG_GMAC_AES,			/**< Galois Message Authentication Code (GMAC) w/ AES (mac) */

	TE_ALG_HMAC_MD5		= 0x00004000,	/**< HMAC-MD-5 mac (SW only) */

	/* HMAC-SHA:
	 * T23X      : For SHA-2 digests when key size is one of 128/196/256 bits => HW support,
	 * otherwise hybrid mode.
	 *
	 * T18X/T19X : hybrid mode only (SHA-1 and SHA-2 supported).
	 *
	 * Hybrid mode: SW callwlated IPAD/OPAD, HW SHA engine used for digesting.
	 */
	TE_ALG_HMAC_SHA1,	/**< HMAC-SHA-1 mac */
	TE_ALG_HMAC_SHA224,	/**< HMAC-SHA-224 mac */
	TE_ALG_HMAC_SHA256,	/**< HMAC-SHA-256 mac */
	TE_ALG_HMAC_SHA384,	/**< HMAC-SHA-384 mac */
	TE_ALG_HMAC_SHA512,	/**< HMAC-SHA-512 mac */

	/** Message digests */

	/**
	 * SHA digests callwlated by HW engine.
	 *
	 * NIST truncated digests (SHA512/224, SHA512/256) supported
	 * by HW when init vectors are programmed by SW before
	 * first data chunk processed.
	 */
	TE_ALG_SHA1		= 0x00008000,		/**< SHA-1 */
	TE_ALG_SHA224,		/**< SHA-224 */
	TE_ALG_SHA256,		/**< SHA-256 */
	TE_ALG_SHA384,		/**< SHA-384 */
	TE_ALG_SHA512,		/**< SHA-512 */

	TE_ALG_SHA512_224,	/**< NIST truncated SHA-512/224 digest */
	TE_ALG_SHA512_256,	/**< NIST truncated SHA-512/256 digest */

	/**
	 * SHA-3 algorithms supported by HW in T23X.
	 *
	 * T19X/T18X have R&D SW support only (3rd party SHA-3 algorithm code
	 * needs legal check before merging).
	 */
	TE_ALG_SHA3_224,	/**< SHA-3 with 224 bit digest */
	TE_ALG_SHA3_256,	/**< SHA-3 with 256 bit digest */
	TE_ALG_SHA3_384,	/**< SHA-3 with 384 bit digest */
	TE_ALG_SHA3_512,	/**< SHA-3 with 512 bit digest */

	/** CCC API does not lwrrenlty support SHAKE digests.
	 * Please add a requirement to get support for these.
	 */
	TE_ALG_SHAKE128,	/**< SHAKE-128 sponge functions with variable length output */
	TE_ALG_SHAKE256,	/**< SHAKE-256 sponge functions with variable length output */

	TE_ALG_MD5		= 0x00010000, /**< MD5 digest (R&D only, do not compile in production) */

	/**
	 * WHIRLPOOL supported by SW only.
	 * 3rd party WHIRLPOOL algorithm code needs legal check before merging.
	 */
	TE_ALG_WHIRLPOOL,		       /**< WHIRLPOOL digest (version 3) */

	/** Modular exponentiation */

	TE_ALG_MODEXP	= 0x00020000,	     /**< Plain modular exponentiation */

	/** pka cipher (padded modular exponentation => RSA ciphers) */

#define TE_ALG_RSA_NOPAD TE_ALG_MODEXP       /**< Plain RSA cipher, no padding
					      * (== modular exponentiation)
					      */

	TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA1,   /**< RSA cipher with OAEP padding (mgf SHA1) */
	TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA224, /**< RSA cipher with OAEP padding (mgf SHA224) */
	TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA256, /**< RSA cipher with OAEP padding (mgf SHA256) */
	TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA384, /**< RSA cipher with OAEP padding (mgf SHA384) */
	TE_ALG_RSAES_PKCS1_OAEP_MGF1_SHA512, /**< RSA cipher with OAEP padding (mgf SHA512) */

	TE_ALG_RSAES_PKCS1_V1_5,	     /**< RSA cipher with PKCS1 v1.5 padding */

	/* pka signatures */

	TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1 = 0x00040000,   /**< RSA signature with PSS (mgf SHA1) */
	TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA224, /**< RSA signature with PSS (mgf SHA224) */
	TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256, /**< RSA signature with PSS (mgf SHA256) */
	TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA384, /**< RSA signature with PSS (mgf SHA384) */
	TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA512, /**< RSA signature with PSS (mgf SHA512) */

	TE_ALG_RSASSA_PKCS1_V1_5_SHA1,	     /**< RSA signature with PKCS1 v1.5 padding (SHA1 digest)
					      * (OTE TE_ALG_PKCS1_Block1 in OTE crypto api)
					      */
	TE_ALG_RSASSA_PKCS1_V1_5_SHA224,     /**< RSA signature with PKCS1 v1.5 padding (SHA224 digest) */
	TE_ALG_RSASSA_PKCS1_V1_5_SHA256,     /**< RSA signature with PKCS1 v1.5 padding (SHA256 digest) */
	TE_ALG_RSASSA_PKCS1_V1_5_SHA384,     /**< RSA signature with PKCS1 v1.5 padding (SHA384 digest) */
	TE_ALG_RSASSA_PKCS1_V1_5_SHA512,     /**< RSA signature with PKCS1 v1.5 padding (SHA512 digest) */

	TE_ALG_RSASSA_PKCS1_V1_5_MD5,	     /**< RSA signature with PKCS1 v1.5 padding (MD5 digest) */

	TE_ALG_XMSS_SHA2_20_256,	     /**< XMSS signature verify with h=20 (SHA256 digest) */

	/** derivations */

	TE_ALG_DH			  = 0x00080000, /**< Diffie-Hellman(-Merkle)
							 * Generates either public part or shared secret,
							 * depending on the input data;
							 * modular exponentiation.
							 */

	/** random number generators */

	TE_ALG_RANDOM_DRNG		  = 0x00100000, /**< Random numbers with RNG0 (DRNG).
							 * HW using AES0 engine in CTR mode,
							 * with ring-oscillator chain seeding
							 * entropy to the AES0 engine or Elliptic RNG1
							 * random number generator via APB interface.
							 */

	TE_ALG_RANDOM_TRNG, /**< True random numbers with PKA1 TRNG via APB.
			     * This should not be used by applications, use TE_ALG_RANDOM_DRNG
			     * instead.
			     */

	/** Elliptic lwrve algorithms */

	TE_ALG_ECDH			= 0x00200000,			/**< EC Diffie-Hellman (derivation) */
	TE_ALG_X25519			= 0x00210000,			/**< Lwrve25519 EC diffie hellman (derivation) */
	TE_ALG_ECDSA			= 0x00400000,			/**< EC digital signature algorithm (signature) */
	TE_ALG_ED25519			= 0x00800000,			/**< ED25519 (EDDSA instance) */
	TE_ALG_ED25519PH,						/**< ED25519 with SHA-512 prehash */
	TE_ALG_ED25519CTX,						/**< ED25519 with optional context */

	/* not supported in tot version
	 */
	TE_ALG_AES_EAX			= 0x10000000, /* AES w/EAX mode (ae) */

} te_crypto_algo_t; /**< Crypto algorithm id */
/*@}*/

/**
 * @brief Operation mode for the selected algorithm.
 *
 * Every supported algorithm can be used in one of these modes of operation.
 *
 * Some algorithms can be used in more than one operation mode (e.g. TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA256
 * can be used for signing and signature verification), some just for one (e.g.
 * TE_ALG_SHA256 can only be used for data digesting)
 *
 * @enum te_crypto_alg_mode_e Defines CCC supported algorithm modes of operation
 *
 * @defgroup algorithm_modes Crypto algorithm modes
 * @ingroup algorithms
 */
/*@{*/
typedef enum te_crypto_alg_mode_e {
	TE_ALG_MODE_ENCRYPT = 0x0, /**< Algorithm used for DATA ENCRYPTION */
	TE_ALG_MODE_DECRYPT,	/**< Algorithm used for DATA DECRYPTION */
	TE_ALG_MODE_VERIFY,	/**< Algorithm used for SIGNATURE VERIFICATION */
	TE_ALG_MODE_SIGN,	/**< Algorithm used for SIGNATURE GENERATION */
	TE_ALG_MODE_DIGEST,	/**< Algorithm used for DATA DIGESTING */
	TE_ALG_MODE_DERIVE,	/**< Algorithm used for DERIVATION */
	TE_ALG_MODE_MAC,	/**< Algorithm used for MESSAGE AUTHENTICATION */
	TE_ALG_MODE_RANDOM,	/**< Algorithm used for RANDOM NUMBER GENERATION */
	TE_ALG_MODE_MAC_VERIFY,	/**< Algorithm used for MESSAGE AUTHENTICATION VERIFICATION */
} te_crypto_alg_mode_t; /**< Crypto algorithm mode */
/*@}*/

/**
 * @ingroup generic_definitions
 */
/*@{*/
#define MAX_IV_SIZE    16U	/**< MAX AES cipher Initialization Vector (IV) size */

#ifndef AES_BLOCK_SIZE
#define AES_BLOCK_SIZE 16U	/**< AES engine block size */
#endif
/*@}*/

/**
 * @brief Supported engines.
 *
 * @enum engine_id_e Known engine ids
 *
 * @defgroup hw_engines Supported engines
 */
/*@{*/
typedef enum engine_id_e {
	CCC_ENGINE_NONE	= 0x0,	/**< Unselected engine id */
	CCC_ENGINE_ANY,		/**< Any engine supporting the selected operation (auto-select) */

	/** SE0 accelerator */
	CCC_ENGINE_SE0_AES0 = 0x10,	/**< SE0 AES0 engine */
	CCC_ENGINE_SE0_AES1,		/**< SE0 AES1 engine */
	CCC_ENGINE_SE0_PKA0,		/**< SE0 PKA0 (RSA) engine (only in T18X/T19X) */
	CCC_ENGINE_SE0_SHA,		/**< SHA engine */

	/** SE1 accelerator (only in selected SoCs, lwrrenlty in T214 only) */
	CCC_ENGINE_SE1_AES0 = 0x20,	/**< [T214] SE1 AES0 engine */
	CCC_ENGINE_SE1_AES1,		/**< [T214] SE1 AES1 engine */
	CCC_ENGINE_SE1_PKA0,		/**< [T214] SE1 PKA0 engine */
	CCC_ENGINE_SE1_SHA,		/**< [T214] SE1 SHA engine */

	CCC_ENGINE_PKA = 0x30,	/**< PKA accelerator for modular operations and
				 * elliptic lwrves and optionally TRNG */
	CCC_ENGINE_RNG1,	/**< RNG1 random number generator (DRNG) */

	/* Software implementation (not using HW acceleration at all) */
	CCC_ENGINE_SOFT     = 0x100, /**< SW engines (when algorithm not supported by HW) */
} engine_id_t; /**< engine type id */

#define CCC_ENGINE_PKA1_PKA CCC_ENGINE_PKA	/**< Backwards compatible name for
						 * generic CCC_ENGINE_PKA
						 */

#if !defined(HAVE_CERT_C_COMPATIBLE_ENGINE_NAMES)
/* Old ENGINE names which violate cert-c.
 *
 * Define HAVE_CERT_C_COMPATIBLE_ENGINE_NAMES in your
 * build to not define these.
 */
#define ENGINE_NONE	CCC_ENGINE_NONE
#define ENGINE_ANY	CCC_ENGINE_ANY
#define ENGINE_SE0_AES0	CCC_ENGINE_SE0_AES0
#define ENGINE_SE0_AES1	CCC_ENGINE_SE0_AES1
#define ENGINE_SE0_PKA0	CCC_ENGINE_SE0_PKA0
#define ENGINE_SE0_SHA	CCC_ENGINE_SE0_SHA
#define ENGINE_SE1_AES0	CCC_ENGINE_SE1_AES0
#define ENGINE_SE1_AES1	CCC_ENGINE_SE1_AES1
#define ENGINE_SE1_PKA0	CCC_ENGINE_SE1_PKA0
#define ENGINE_SE1_SHA	CCC_ENGINE_SE1_SHA
#define ENGINE_PKA	CCC_ENGINE_PKA
#define ENGINE_RNG1	CCC_ENGINE_RNG1
#define ENGINE_SOFT	CCC_ENGINE_SOFT
#define ENGINE_PKA1_PKA	CCC_ENGINE_PKA1_PKA

#endif /* !defined(HAVE_CERT_C_COMPATIBLE_ENGINE_NAMES) */
/*@}*/

/**
 * @brief The ordinal number of these values is passed to the system dependent routine
 * which needs to set the base address or return a failure (any nonzero value is
 * a failure)
 *
 * Each device has it's own base address.
 *
 * NOTE: These values are used as table index values in engine table initializers.
 * All entries must exist in tegra_cdev[] table up to the last value used.
 * [ This will be redesigned in a later commit. ]
 *
 * @defgroup eng_switch_cdev_id Known crypto accelerater device values from 0..N
 */
/*@{*/
typedef enum se_cdev_id_e {
	SE_CDEV_SE0  = 0, /**< SE0 device [0] */
	SE_CDEV_PKA1 = 1, /**< PKA device [1] */
	SE_CDEV_RNG1 = 2, /**< RNG1 device [2] */
	SE_CDEV_SE1  = 3, /**< SE1 device [3] (T214 only) */
} se_cdev_id_t;
/*@}*/

/**
 * @brief Supported EC lwrves (Only prime field lwrves supported by
 * the PKA1 accelerator).
 *
 * Since the PKA1 accelerator does not support binary field lwrves,
 * they are not supported by CCC either.
 *
 * The supported prime field lwrves can be enabled by a group define
 * (e.g. all NIST lwrves) or by enabling individual lwrves one by one
 * (e.g. only support NIST-P256 lwrve).
 *
 * @enum te_ec_lwrve_id_e Known EC lwrve ids
 * @defgroup ec_lwrves Known EC prime lwrves
 */
/*@{*/
typedef enum te_ec_lwrve_id_e {
	TE_LWRVE_NONE = 0x0,	/**< Placeholder for unselected lwrve type  */

	/** Supported NIST prime lwrves */

	TE_LWRVE_NIST_P_192,	/**< NIST-P192 prime lwrve */
	TE_LWRVE_NIST_P_224,	/**< NIST-P224 prime lwrve */
	TE_LWRVE_NIST_P_256,	/**< NIST-P256 prime lwrve */
	TE_LWRVE_NIST_P_384,	/**< NIST-P384 prime lwrve */

	/**
	 * P521 is lwrrently only supported with T23X CCC version.
	 *
	 * Please add a requirement to get this support for T19X.
	 */
	TE_LWRVE_NIST_P_521,	/**< NIST-P521 prime lwrve */

	/** Brainpool lwrves */
	TE_LWRVE_BRAINPOOL_P160r1, /**< Brainpool-P160R1 lwrve */
	TE_LWRVE_BRAINPOOL_P192r1, /**< Brainpool-P192R1 lwrve */
	TE_LWRVE_BRAINPOOL_P224r1, /**< Brainpool-P224R1 lwrve */
	TE_LWRVE_BRAINPOOL_P256r1, /**< Brainpool-P256R1 lwrve */
	TE_LWRVE_BRAINPOOL_P320r1, /**< Brainpool-P320R1 lwrve */
	TE_LWRVE_BRAINPOOL_P384r1, /**< Brainpool-P384R1 lwrve */
	TE_LWRVE_BRAINPOOL_P512r1, /**< Brainpool-P512R1 lwrve */

	/** Brainpool twisted lwrves */
	TE_LWRVE_BRAINPOOL_P160t1, /**< Brainpool-P160T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P192t1, /**< Brainpool-P192T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P224t1, /**< Brainpool-P224T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P256t1, /**< Brainpool-P256T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P320t1, /**< Brainpool-P320T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P384t1, /**< Brainpool-P384T1 twisted lwrve */
	TE_LWRVE_BRAINPOOL_P512t1, /**< Brainpool-P512T1 twisted lwrve */

	/** Montgomery lwrve Lwrve25519 */
	TE_LWRVE_C25519,	/**< Lwrve only used for ECDH with lwrve25519 (X25519) */

	/** Twisted edwards lwrve ED25519 */
	TE_LWRVE_ED25519,	/**< Lwrve only used for signatures with ed25519 lwrve
				 * (ED25519/ED25519PH)
				 */

	/** Koblitz lwrves over prime field Fp */
	TE_LWRVE_KOBLITZ_P_160k1, /**< Koblitz P160K1 lwrve */
	TE_LWRVE_KOBLITZ_P_192k1, /**< Koblitz P192K1 lwrve */
	TE_LWRVE_KOBLITZ_P_224k1, /**< Koblitz P224K1 lwrve */
	TE_LWRVE_KOBLITZ_P_256k1, /**< Koblitz P256K1 lwrve */
} te_ec_lwrve_id_t; /**< Type for lwrve ids */
/*@}*/

/* NOTE: When defining the syscall related objects, remember
 *  that TA is 32 bit entity and Trusty is 64 => object field alignment
 *  and sizes must be correct!!!
 *
 * Do not use size_t for these (size may be differ).
 *
 * NOTE WELL => All field offsets must be kept identical in 32 and 64
 * bit systems (because e.g. TA's are run in aarch-32)
 *
 * If adding pointer fields make sure the field is 64 bit size aligned or
 * it will break the 32/64 bit calling compatibility. This is why the
 * definitions below contain unions with uint64_t and pointer fields,
 * the uint64_t field is for size alignment only. Pointers are not
 * colwerted to scalar values in CCC code. Any raw pointers added
 * must also be verified to belong to the caller address space before
 * being accessed, so please try to avoid adding pointers.
 *
 * Lwrrently there are three pointers in the objects below:
 * - SRC
 * - DST
 * - KEY object
 */

// XXX Note: removed support for passing AES-CMAC a non-standard IV value;
// XXX  code now uses a zero vector as required by RFC. If you need
// XXX  this => add a requirement and it could be added back.
//
/**
 * @brief Block cipher specific init operation setup.
 *
 * This is used by the block cipher init calls. When do_final is called
 * some operations also write back the final value of the IV to this object.
 *
 * Since the only block cipher supported is the AES this is lwrrently
 * fully AES specific object.
 *
 * TODO: Specify which AES modes write back the value here.
 *
 * @defgroup cipher_init_args API cipher init settings
 * @ingroup args_init
 */
/*@{*/
struct cipher_aes_gcm_init_s {
	/* CCC supports 12 byte (96 bit) GCM nonces and 32 bit counters
	 * starting from zero (12 first bytes of IV considered nonce,
	 * counter start value zero (4 bytes). Value in last 4 bytes ignored.
	 */
	uint8_t  nonce[ AES_BLOCK_SIZE ];
	uint8_t  tag[ AES_BLOCK_SIZE ];

	union {
		const uint8_t *aad; /**< AAD input data for AES-GCM */
		uint64_t alignment_aad64; /**< guarantee field size 64 bits
					   * This field is never used.
					   */
	};

	uint32_t nonce_len; /* Lwrrently 12 byte nonces supported (by HW) */
	uint32_t tag_len;
	uint32_t aad_len;
};

struct cipher_aes_ccm_init_s {
	uint8_t nonce[ AES_BLOCK_SIZE ];
	uint8_t tag[ AES_BLOCK_SIZE ];
	union {
		const uint8_t *aad; /**< AAD input data for AES-CCM */
		uint64_t alignment_aad64; /**< guarantee field size 64 bits
					   * This field is never used.
					   */
	};
	uint32_t nonce_len;
	uint32_t tag_len;
	uint32_t aad_len;
};

struct cipher_aes_ctr_init_s {
	uint8_t  ci_counter[MAX_IV_SIZE]; /**< AES-CTR mode IV (nonce+counter) */
	uint8_t  ci_increment;	/**< CTR counter increment if ci_increment > 0,
				 * default is 1.
				 */

	/**
	 * non-zero if ci_counter should be treated as
	 * little endian counter which is the default
	 * in SE HW. But because NIST uses it as a big
	 * endian counter, I use the SPARE_0 bit 0
	 * kludge to swap the counter to big endian by
	 * DEFAULT!
	 *
	 * So, if you want to use the non-standard SE
	 * HW little endian counter increment => set
	 * this value to 1.
	 *
	 * XXXX THIS FIELD IS DEPRECATED and will get removed.
	 * XXXX Little endian counters are not supported, error
	 * XXXX  is reported when used.
	 */
	uint8_t  ci_counter_little_endian;
};

struct cipher_aes_xts_init_s {
	/**
	 * Tweak value is always in little endian
	 * init args is now also passed to UPDATE function calls,
	 * so ci_tweak value can be modified for init/update/dofinal
	 * calls doing XTS encryption.
	 *
	 * XTS algorithm does not use IV => tweak overlaps with iv field.
	 */
	uint8_t  ci_xts_tweak[MAX_IV_SIZE];

	/**
	 * In case the last byte of the XTS data is
	 * not fully included this specifies
	 * how many bits at the end are unused.
	 *
	 * Set this ZERO to include all bits in the
	 * last byte.
	 */
	uint8_t  ci_xts_last_byte_unused_bits;
};

#define INIT_FLAG_CIPHER_NONE		0x0000U
#define INIT_FLAG_CIPHER_USE_PRESET_IV 0x0001U /**< Initial IV value pre-set by caller
						 * (Do not write ci_iv value on operation start)
						 */
/**
 * INIT_FLAG_CIPHER_USE_PRESET_IV is a more future proof name in case
 * IV values are removed from keyslots in the future.
 * Provide backwards compatibility name for transition.
 */
#define INIT_FLAG_CIPHER_USE_KEYSLOT_IV INIT_FLAG_CIPHER_USE_PRESET_IV

typedef struct cipher_init_args_s {
	/**
	 * AES mode specific fields
	 */
	union {
		uint8_t	 ci_iv[MAX_IV_SIZE]; /**< IV for "normal" AES modes */

		struct cipher_aes_xts_init_s xts_mode;

		struct cipher_aes_ctr_init_s ctr_mode;

		struct cipher_aes_gcm_init_s gcm_mode;

		struct cipher_aes_ccm_init_s ccm_mode;
	};
	uint32_t flags;		/**< Bitmask of INIT_FLAG_CIPHER_* values */
} cipher_init_args_t; /**< Type for block cipher init settings */
/*@}*/

/**
 * @defgroup rsa_cipher_init_flags API RSA cipher init flags
 * @ingroup rsa_cipher_init_args
 */
/*@{*/
#define INIT_FLAG_RSA_NONE			0x0000U	/**< No RSA flags needed */
#define INIT_FLAG_RSA_DATA_LITTLE_ENDIAN	0x0001U	/**< RSA data is little endian (BE by default) */
#define INIT_FLAG_RSA_DIGEST_INPUT		0x0002U	/**< Digest input before verifying/signing it
							 * with RSA
							 */
#define INIT_FLAG_RSA_SALT_DYNAMIC_LEN		0x0004U	/**< Extract salt len from signature to be verified
							 */
/*@}*/

/**
 * @brief RSA cipher specific init operation setup.
 *
 * This is used by the RSA cipher (OAEP or PKCS#1v1.5) init calls.
 *
 * TODO: Specify which RSA modes write back the value here (if any).
 *
 * @defgroup rsa_cipher_init_args API rsa cipher init settings
 * @ingroup args_init
 */
/*@{*/
#define MAX_OAEP_LABEL_SIZE 32U	/**< Max supported RSA OAEP asymmetric cipher label size */

typedef struct rsa_init_args_s {
	uint32_t flags;		/**< Bitmask of INIT_FLAG_RSA_* values */

	/* Becomes an union if inits added for other modes */
	struct {
		uint32_t oaep_label_length; /**< Length of OAEP label (0..MAX_OAEP_LABEL_SIZE) */
		uint8_t oaep_label[ MAX_OAEP_LABEL_SIZE ]; /**< OAEP label value */
	};
} rsa_init_args_t; /**< Type for RSA cipher init settings */
/*@}*/

/**
 * @defgroup ec_init_flags API EC init flags
 * @ingroup ec_init_args
 */
/*@{*/
#define INIT_FLAG_EC_NONE		0x0000U	/**< No EC mode flag needed  */
#define INIT_FLAG_EC_DATA_LITTLE_ENDIAN	0x0001U	/**< EC op input data is little endian (BE default)
						 * NOTE: LE data is not supported, no requirement.
						 * This flag is a NO-OP.
						 */
#define INIT_FLAG_EC_DIGEST_INPUT	0x0002U /**< Digest input before verifying/signing it with EC
						 * (e.g. ECDSA or EDDSA)
						 */
#define INIT_FLAG_EC_ASN1_SIGNATURE	0x0004U	/**< Specific support for parsing OpenSSL generated
						 * ASN.1 DER formatted binary EC signature  blobs.
						 *
						 * CCC supported a limited ASN.1 number parser
						 * for this specific case only.
						 */
#define INIT_FLAG_EC_KEYSTORE		0x0008U /* EC Keystore related operation */
/*@}*/

/**
 * @brief Elliptic Lwrve (EC) specific init setup.
 *
 * @defgroup ec_init_args API EC init flags
 * @ingroup args_init
 *
 * @struct ec_init_args_s
 */
/*@{*/
#define MAX_ECC_DOM_CONTEXT_SIZE 16U /**< Restricted context size limit; RFC-8032 limit is 255 */

struct dom_context_s {
	uint32_t length; /**< ED25519 optional context len */
	uint8_t value[ MAX_ECC_DOM_CONTEXT_SIZE ]; /**< DOM2 CONTEXT value */
};

typedef struct ec_init_args_s {
	uint32_t	  flags;    /**< Bitmask of INIT_FLAG_EC_* values */
	te_ec_lwrve_id_t  lwrve_id; /**< Selected EC lwrve identity */
	te_crypto_algo_t  sig_digest_algo; /**< Digest algo for EC signature when flag
					    * INIT_FLAG_EC_DIGEST_INPUT used. If spec
					    * requires a specific digest, this is ignored.
					    * (e.g. ED25519 requires SHA-512)
					    */
	struct dom_context_s dom_context;
} ec_init_args_t; /**< Type for EC operation init settings */
/*@}*/

/**
 * @brief T23X Key Wrap / Key Unwrap operation init operation setup.
 *
 * @defgroup kwuw_init_args API T23X Key Wrap / Key Unwrap init settings
 * @ingroup args_init
 */
/*@{*/
struct kwuw_init_args {
	uint8_t  iv[MAX_IV_SIZE]; /**< 12 first bytes of this field is used as a
				   * NONCE for both KW and KUW operations.
				   *
				   * Provided NONCE value MUST BE unique for
				   * each KW operation with the same key.
				   *
				   * TODO: Would be safer if SW generated a strong
				   * random value for the nonce for KW operations
				   * instead of using a client specified value.
				   */
	uint32_t wrap_index;	/**< KW wraps this key out and KUW unwraps a key into this key slot */
};
/*@}*/

/**
 * @brief T23X Key Derivation init operation setup (1-KEY or 2-KEY operations)
 *
 * @defgroup kdf_init_args API T23X 1-KEY / 2-KEY key derivation init settings.
 * @ingroup args_init
 */
/*@{*/

/**< T23x keyslot manifest key purpose values: */
#define CCC_MANIFEST_PURPOSE_ENC	0U /**< AES ENC/DEC */
#define CCC_MANIFEST_PURPOSE_CMAC	1U /**< AES-CMAC */
#define CCC_MANIFEST_PURPOSE_HMAC	2U /**< HMAC-SHA2 */
#define CCC_MANIFEST_PURPOSE_KW		3U /**< key wrap */
#define CCC_MANIFEST_PURPOSE_KUW	4U /**< key unwrap */
#define CCC_MANIFEST_PURPOSE_KWUW	5U /**< key wrap/unwrap */
#define CCC_MANIFEST_PURPOSE_KDK	6U /**< key derivation key */
#define CCC_MANIFEST_PURPOSE_KDD	7U /**< key derivation second key */
#define CCC_MANIFEST_PURPOSE_KDD_KUW	8U /**< key derive for unwrap */
#define CCC_MANIFEST_PURPOSE_XTS	9U /**< AES-XTS */
#define CCC_MANIFEST_PURPOSE_GCM	10U /**< AES-GCM */

/**< T23x keyslot manifest key user values: */
#define CCC_MANIFEST_USER_PSC		1U /**< PSC */
#define CCC_MANIFEST_USER_TZ		2U /**< TZ */
#define CCC_MANIFEST_USER_NS		3U /**< NS */
#define CCC_MANIFEST_USER_FSI		4U /**< FSI */

#define CCC_MANIFEST_FLAGS_NONE		0U /**< t23x manifest default */
#define CCC_MANIFEST_FLAGS_EXPORTABLE	1U /**< t23x manifest exportable */

#define INIT_FLAG_KDF_NONE         0x0000U /**< No kdf flags */
#define INIT_FLAG_KDF_NIST_ENCODE_INPUT 0x0010U /**< [T23x] Input contains "Label" and "Context" octet streams,
						 * CCC encodes them according to NIST 800-108 5.1 KDF
						 * in Counter mode.
						 */
struct kdf_init_args {
	uint32_t kdf_index;	/**< KDF target keyslot */
	uint32_t kdf_flags;	/**< KDF flags */

	/** KDF target keyslot manifest setup values for the derived key (in kdf_index)
	 */
	uint32_t kdf_purpose;	/**< explicit manifest purpose needed (see above) */
	uint32_t kdf_key_bits;	/**< Number of bits for the derived key
				 * (one of 128, 196, 256)
				 */
	uint32_t kdf_user;	/**< User field value for the derived key manifest */
	uint32_t kdf_sw;	/**< 16 bit SW value for the derived key manifest */
};
/*@}*/

/**
 * @brief T19X AES key derivation with AES modes: ECB(nodpad), CBC(nopad), CTR
 *
 * This AES key derivation is only supported in T19X units.
 * HW does not support AES encryption to keyslot in earlier units.
 *
 * For the newer units with key slot access contgrol use the KDF based
 * on HMAC-SHA2, using this code with those is not supported.
 *
 * @defgroup aes_kdf_init_args_s API T19X AES key derivation
 * @ingroup args_init
 */
/*@{*/
struct aes_kdf_init_args_s {
	uint32_t kdf_index; /**< AES KDF target keyslot */
	uint32_t kdf_key_bit_size; /**< AES KDF derive key len (128, 192 or 256)  */

	/* Init values for the supported AES algo modes:
	 * TE_ALG_AES_ECB_NOPAD
	 * TE_ALG_AES_CBC_NOPAD
	 * TE_ALG_AES_CTR
	 */
	union {
		uint8_t	 ci_iv[MAX_IV_SIZE]; /**< IV for "normal" AES modes */
		struct cipher_aes_ctr_init_s ctr_mode;
	};
};
/*@}*/

/**
 * @brief T19X CMAC-AES key derivation with NIST 800-108 Counter mode encoder.
 *
 * The CMAC-AES key derivation is only supported in T19X units.
 * HW does not support AES encryption to keyslot in earlier units.
 *
 * For the newer units with key slot access contgrol use the KDF based
 * on HMAC-SHA2, using this code with those is not supported.
 *
 * @defgroup cmac_kdf_init_args_s API T19X AES key derivation
 * @ingroup args_init
 */
/*@{*/
struct cmac_kdf_init_args_s {
	uint32_t kdf_index; /**< CMAC KDF target keyslot */
	uint32_t kdf_key_bit_size; /**< CMAC KDF derive key len (128, 192 or 256)  */
};
/*@}*/

/**
 * @brief MAC operation init settings
 *
 * Normally not used. Set this to use CMAC-AES for key generation.
 *
 * @defgroup mac_init_args MAC operation init args.
 * @ingroup args_init
 */

/*@{*/
#define INIT_FLAG_MAC_NONE         0x0000 /**< No mac flags */
#define INIT_FLAG_MAC_DST_KEYSLOT  0x0020U /**< CMAC-AES output to keyslot. If flag set
					    * target keyslot [0..15] in mac_dst_keyslot.
					    *
					    * Default: bits 0..127.
					    * To write the upper quad (bits 128..255), set flag
					    * INIT_FLAG_MAC_DST_UPPER_QUAD
					    */
#define INIT_FLAG_MAC_DST_UPPER_QUAD 0x0040U /**< CMAC-AES output to keyslot bits 128..255.
					      */
#define INIT_FLAG_MAC_DST_KEY128   0x0100U /**< CMAC-AES output 128 bit key to keyslot */
#define INIT_FLAG_MAC_DST_KEY192   0x0200U /**< CMAC-AES output 192 bit key to keyslot */
#define INIT_FLAG_MAC_DST_KEY256   0x0400U /**< CMAC-AES output 256 bit key to keyslot */

#define INIT_FLAG_MAC_KEY_SIZE_MASK (INIT_FLAG_MAC_DST_KEY128 | INIT_FLAG_MAC_DST_KEY192 | INIT_FLAG_MAC_DST_KEY256)

struct mac_init_args {
	uint32_t mac_flags;	     /**< MAC flags */
	uint32_t mac_dst_keyslot;    /**< DST keyslot for INIT_FLAG_MAC_DST_KEYSLOT */
};
/*@}*/

/**
 * @brief SHAKE operation init settings
 *
 * Requested number of bits in SHAKE XOF output.
 *
 * @defgroup shake_init_args SHAKE operation init args.
 * @ingroup args_init
 */

/*@{*/
struct shake_init_args {
	uint32_t shake_xof_bitlen;    /**< Requested output length of XOF in bits */
};
/*@}*/

/**
 * @brief XMSS signature verification init settings.
 *
 * Lwrrently no XMSS specific init flags are defined.
 *
 * @defgroup xmss_init_args XMSS operation init args.
 * @ingroup args_init
 */

/*@{*/
#define INIT_FLAG_XMSS_NONE 0x0000U

struct xmss_init_args {
	uint32_t flags;	     /**< XMSS flags */
};
/*@}*/

/**
 * @brief Values for async_state field for engine state.
 *
 * SW state of the async operation engine state (SHA or AES engines
 * only supported).  After async operation is started the engine state
 * can be polled. If the polled engine HW state is IDLE =>
 * async_state field is set to ASYNC_CHECK_ENGINE_IDLE, otherwise it
 * is set to ASYNC_CHECK_ENGINE_BUSY.
 *
 * The CCC ASYNC interface provides asynchronous start, asynchronous
 * poll and blocking finalize operations. The async operations can be
 * used via the CCC API or directly using the CCC engine API layer.
 *
 * This is in the te_args_init_s object ofr technical reasons, it is used
 * internally by CCC core and should be initialized to ASYNC_CHECK_ENGINE_NONE
 * (zero value). It will remain ASYNC_CHECK_ENGINE_NONE unless the
 * operation is used in (and supports) asynchronous operation mode.
 *
 * @defgroup async_state CCC API async operation states
 * @ingroup primitive_operations_async
 */
/*@{*/
#define ASYNC_CHECK_ENGINE_NONE		0x0000U	/**< no async operation in progress */
#define ASYNC_CHECK_ENGINE_BUSY		0x0001U	/**< engine is busy */
#define ASYNC_CHECK_ENGINE_IDLE		0x0002U	/**< engine is now idle */

/** ASYNC_GET_SE_ENGINE_STATE - Extract engine async operation state from the argument
 * object after TE_OP_ASYNC_CHECK_STATE returns. See @ref crypto_args for argument object.
 *
 * @param arg CCC context api argument object after TE_OP_ASYNC_CHECK_STATE
 *
 * @return Returns of the engine async state.
 */
#define ASYNC_GET_SE_ENGINE_STATE(arg) ((arg)->ca_init.async_state)
/*@}*/

/**
 * @brief Collection of above init operation values for all algorithms.
 *
 * Init object also supports asynchronous SHA and asynchronous AES
 * operation mode state checks with the async_state field by the
 * ASYNC_GET_SE_ENGINE_STATE macro.
 *
 * @defgroup args_init CCC API TE_OP_INIT operation settings
 * @ingroup crypto_args
 */
/*@{*/
typedef struct te_args_init_s {
	/** Algorithm type specific initializations in the union fields */
	union {
		rsa_init_args_t     rsa; /**< RSA init operation setup */
		ec_init_args_t      ec;	/**< EC init operation setup */
		cipher_init_args_t  aes; /**< AES init operation setup */
		struct kwuw_init_args kwuw; /**< [T23X] KW/KUW init operation setup */
		struct kdf_init_args kdf; /**< [T23X] Key derivation init operation setup */
		struct mac_init_args mac; /**< MAC init operation setup */
		struct aes_kdf_init_args_s aes_kdf; /**< [T19X] AES Key derivation init operation setup */
		struct cmac_kdf_init_args_s cmac_kdf; /* [T19X] CMAC-AES Key derivation init operation setup */
		struct shake_init_args shake; /**< SHAKE init operation setup */
		struct xmss_init_args xmss; /**< XMSS init operation setup */
	};

	engine_id_t engine_hint; /**< Caller can pass a hint to use a specific engine for this
				  * operation. SE driver may override this selection; on return
				  * the SE driver selected engine is returned in this field.
				  *
				  * At any time the driver may also switch to another unit
				  * during the operation (this is a hint only).
				  */

	uint32_t    async_state; /**< For asynchronous operation engine status check */
} te_args_init_t;
/*@}*/

struct te_ec_point_s;
struct te_ec_sig_s;

/**
 * @brief Input/output arguments for crypto primitive operations processing
 * caller data (TE_OP_UPDATE and TE_OP_DOFINAL).
 *
 * This object contains four fields: two 32 bit unsigned integer fields
 * for object sizes and two 64 bit pointer fields for data arguments.
 *
 * The 2 pointer fields are forced to be 64 bit long because the API
 * needs to be binary compatible with 32 bit user space and 64 bit
 * kernel mode operations.
 *
 * The 64 bit union fields alignment_src64 and alignment_dst64 are not
 * referred to in CCC code, they are only used for pointer field
 * alignment purposes.
 *
 * NOTE: The second pointer field is used as writable output buffer
 * pointer for algorithms that need to output data buffers (dst
 * field). For all other use cases the pointer is to an immutable
 * object (read-only) for additional input data.
 *
 * There exist no cases when a read-only input buffer would be
 * colwerted into a writable data buffer by use of such union mapping:
 * If the object is passed in a read-only field it is never referenced
 * via the mutable dst field.
 *
 * Input/output object size is 2*32 bits + 2*64 bits == 192 bits (24 bytes).
 *
 * @defgroup args_data CCC API data input/output parameter object
 * @ingroup crypto_args
 */
/*@{*/
typedef struct te_args_data_s {
	/** <b>Byte size of the first buffer</b> */
	union {
		uint32_t   src_size;		/**< input data size */
		uint32_t   src_digest_size;	/**< or input src_digest size */
		uint32_t   src_kdf_label_len;	/**< or KAC KDF label len */
	};

	/** <b>Byte size of the second buffer</b> */
	union {
		uint32_t   dst_size;		/**< destination size */
		uint32_t   src_signature_size;  /**< or input src_signature size */
		uint32_t   src_kwrap_size;	/**< or input src_kwrap size */
		uint32_t   src_mac_size;	/**< or input src_mac size */
		uint32_t   src_kdf_context_len;	/**< or KAC KDF context len */
	};

	/** <b>First buffer/object address (immutable buffer)</b> */
	union {
		/** First buffer address: 64 bit aligned
		 * source object pointer (field size 8 bytes
		 * even when the caller uses 4 byte pointers)
		 */
		const uint8_t *src;		/**< src buffer addr (when not hash to sig val) */
		const uint8_t *src_digest;	/**< or message digest for signature validation */
		const struct te_ec_point_s *src_point; /**< input for EC ops requiring point P (ECDH) */

		const uint8_t *src_kdf_label;	/**< T23X KDF/KDD "Label" from NIST 800-108 */
		uint64_t alignment_src64;	/**< guarantee field align/size 64 bits
						 * (this field never used).
						 */
	};

	/** <b>Second buffer address (mutable) or object (immutable) address, depending on algorithm)</b>
	 *
	 * Second buffer address: 64 bit aligned destination
	 * OR source pointer, operation specific
	 * selection (field size 8 bytes even when the
	 * caller uses 4 byte pointers)
	 *
	 * Note: This is used either as a destination pointer (mutable) or
	 * as a source pointer (immutable) but never both. These are in the
	 * same field only for saving 8 bytes in the size.
	 */
	union {
		uint8_t    *dst;		/**< output buffer address (when operation needs
						 * to write data out. Use one of the const object
						 * pointers when read only input is provided).
						 */
		struct te_ec_sig_s *dst_ec_signature; /**< or output address of ECDSA signature (sign) */

		const uint8_t *src_signature;	/**< or address of RSA signature */
		const uint8_t *src_asn1;	/**< or an ASN.1 source blob (LIMITED SUPPORT!) */
		const struct te_ec_sig_s *src_ec_signature; /**< or address of ECDSA signature (verify) */
		const uint8_t *src_kwrap;	/**< or address of Key Wrapped data blob (64 bytes) */
		const uint8_t *src_mac;		/**< or address of MAC value to verify */
		const uint8_t *src_kdf_context;	/**< T23X KDF/KDD "Context" from NIST 800-108 */

		uint64_t    alignment_dst64; /**< guarantee field align/size 64 bits
					      * (field never used in code).
					      */
	};
} te_args_data_t;
/*@}*/

/** ECDSA signature value
 *
 * @defgroup ec_signature_flags EC signature flags
 * @ingroup ec_signature
 */
/*@{*/
#define CCC_EC_SIG_FLAG_NONE		0x0000U	/**< No EC signature flags used */
#define CCC_EC_SIG_FLAG_LITTLE_ENDIAN	0x0001U	/**< EC signature is little endian (default BE) */
/*@}*/

/**
 * @brief EC signature CCC object.
 *
 * <R,S> in separate fields and associated signature flags in sig_flags.
 *
 * Max size used by NIST P-521 (TODO: support P-521)
 *
 * @defgroup ec_signature EC signature CCC object
 */
/*@{*/
#define MAX_ECC_WORDS 20U	/**< Max size of supported ECC prime length in words */
#define TE_MAX_ECC_BYTES (MAX_ECC_WORDS * BYTES_IN_WORD) /**< Max byte size of ECC prime length */

typedef struct te_ec_sig_s {
	uint8_t r[TE_MAX_ECC_BYTES]; /**< EC signature R */
	uint8_t s[TE_MAX_ECC_BYTES]; /**< EC signature S */
	uint32_t sig_flags;	/**< CCC_EC_SIG_FLAG_* flag bits */
} te_ec_sig_t;
/*@}*/

/**
 * @brief Defines and objects for key arguments.
 *
 * @defgroup key_type Key type indicator for the key object
 * @ingroup set_key
 */
/*@{*/
typedef enum te_key_type_e {
	KEY_TYPE_NONE = 0x0,	/**< No key type selected */
	KEY_TYPE_AES,		/**< AES key (up to 2*256 bit keys for XTS) */
	KEY_TYPE_RSA_PUBLIC,	/**< RSA public key type (max 4096 bit modulus and 32 bit public key) */
	KEY_TYPE_EC_PUBLIC,	/**< Elliptic lwrve keys (EC lwrve point) */
	KEY_TYPE_HMAC,		/**< Arbitrary size limit for HMAC keys is about one kilobyte.
				 * T23X: HW HMAC-SHA2 limits size to 128, 196 or 256 bit keys.
				 */
	KEY_TYPE_RSA_PRIVATE,	/**< RSA private key (max 4096 bit modulus, private key and 32
				 * bit public key). Also used for DH.
				 */
	KEY_TYPE_EC_PRIVATE,	/**< EC private key (prime field size scalar value) */
	KEY_TYPE_KDF,		/**< T23X: HW limits size to 128, 192 or 256 bit key derivation key */
	KEY_TYPE_XMSS_PUBLIC,	/**< XMSS public key (root_seed; 64 bytes) */
	KEY_TYPE_XMSS_PRIVATE,	/**< XMSS private key */
} te_key_type_t;

#define KEY_TYPE_DH KEY_TYPE_RSA_PRIVATE /**< Plain DH uses modular exponentiation with a private key */
/*@}*/

/** Generic key flags (KEY_FLAG_*)
 *
 * @defgroup key_flags Generic key object flags
 * @ingroup set_key
 */

/*
 * allowed key slots: 0..15 or 0..4 and "dynamic"
 * which (will hopefully some day would) allow the driver to select a key slot for this operation
 * in a dynamic fashion. For now the "dynamic slot" is actually "pre-defined fixed slot number for CCC"
 * since the HW does not support swapping keys out.
 *
 * We also may need to device a new "authority semantic" => only TAs with this
 * authority are allowed to use "fixed keyslots". All other TA's would automatically
 * use the "dynamic" keyslots only...
 * (authorities (as TA manifest properties) will be added as part of dynamic
 *   loading commits, easy to add after that).
 */

/*@{*/
#define KEY_FLAG_NONE		 0x0000U /**< No key flags selected */
#define KEY_FLAG_PLAIN		 0x0001U /**< Import plaintext key (XXX should be default?) */
#define KEY_FLAG_IMPORT_BOUND	 0x0002U /**< xx Importing a device bound encrypted key (in k_opaque_key field) */
#define KEY_FLAG_DYNAMIC_SLOT	 0x0004U /**< Driver selects the keyslot (k_keyslot value ignored)
					  * for PKA1 this is default, for SE PKA0 it is not (XXX RE-THINK!)
					  */
#define KEY_FLAG_USE_KEYSLOT_KEY 0x0008U /**< Do not overwrite the key in keyslot, just use it */
#define KEY_FLAG_EKS_KEY	 0x0010U /**< xx in case we support EKS keys in the kernel. */
#define KEY_FLAG_EXPORT_BOUND	 0x0020U /**< xx export this key (ciphered, device bound) */
#define KEY_FLAG_LITTLE_ENDIAN	 0x0040U /**< key in little endian */
#define KEY_FLAG_LEAVE_KEY_IN_KEYSLOT 0x0080U /**< Do not erase key from keyslot after use
					       * (exposed to other subsystems having access to SE)
					       */
#define KEY_FLAG_FORCE_KEYSLOT	 0x0100U /**< Force use of keyslots (PKA1 defaults to dynamic) */
#define KEY_FLAG_KMAT_VALID	 0x0200U /**< Only used when KEY_FLAG_USE_KEYSLOT_KEY is set.
					  * Flag indicates that the KEY MATERIAL is present/valid in
					  * key object when it is not really needed for key operations.
					  *
					  * Used (e.g.) when pre-loaded key is used but
					  * RSA-PSS signature representative value should be verified to be
					  * in range [0..N-1] inclusive (N == modulus)
					  */
#define KEY_FLAG_RFC_FORMAT	 0x0400U /**< In case several key formats for a key, this specifies
					  * the relevant RFC key format is used.
					  * e.g. RFC-8391 XMSS public key in RFC key format.
					  */


/*@}*/

/**
 * @ingroup generic_definitions
 */
/*@{*/
#define MAX_RSA_MODULUS_BYTE_SIZE (MAX_RSA_BIT_SIZE / 8U) /**< Max RSA modulus byte size */
#define MAX_RSA_PRIV_EXPONENT_BYTE_SIZE MAX_RSA_MODULUS_BYTE_SIZE /**< RSA private key size matches
								   * the max modulus size in bytes
								   */

#define MAX_AES_KEY_BYTE_SIZE 32U /**< 256 bit AES key size in bytes */
/*@}*/

/** RSA public key
 *
 * Order and size of first two public key object fields match the
 * rsa_private_t object contents.
 *
 * @defgroup rsa_public_set_key RSA public key CCC object
 * @ingroup set_key
 */
/*@{*/
typedef struct {
	uint8_t pub_exponent[4]; /**< RSA public key (4 bytes).
				  *
				  * Often a small well-known prime
				  * number, like 65537 (0x10001) is
				  * used.
				  */
	uint8_t modulus[MAX_RSA_MODULUS_BYTE_SIZE]; /**< RSA modulus */
} rsa_pubkey_t;
/*@}*/

/** RSA private key (includes public key exponent)
 *
 * Order and size of first two private key object fields match the
 * rsa_pubkey_t object contents.
 *
 * @defgroup rsa_private_set_key RSA private key CCC object
 * @ingroup set_key
 */
/*@{*/
typedef struct {
	uint8_t pub_exponent[4]; /**< Public exponent in the private key material */
	uint8_t modulus[MAX_RSA_MODULUS_BYTE_SIZE]; /**< RSA modulus */
	uint8_t priv_exponent[MAX_RSA_PRIV_EXPONENT_BYTE_SIZE]; /**< Private exponent */
} rsa_private_t;
/*@}*/

/**
 * @ingroup generic_definitions
 */
/*@{*/
#define MAX_KEY_SIZE ((uint32_t)sizeof(rsa_private_t)) /**< This is the longest structured
							* key material object size.
							*/

#define CCC_ED25519_COMPRESSED_POINT_SIZE	32U /**< ED2559 lwrve size */
#define CCC_ED25519_SIGNATURE_SIZE		64U /**< ED25519 signature size */
/*@}*/

/** EC point flags
 *
 * @defgroup ec_point_flags EC point flags
 * @ingroup ec_point_type
 */
/*@{*/
#define CCC_EC_POINT_FLAG_NONE		0x0000U	/**< No EC point flags set */
#define CCC_EC_POINT_FLAG_LITTLE_ENDIAN	0x0001U	/**< EC point X and Y are in little endian byte order */
#define CCC_EC_POINT_FLAG_UNDEFINED	0x0002U	/**< EC point X and Y are undefined */
#define CCC_EC_POINT_FLAG_COMPRESSED_ED25519 0x0004U /**< Compressed ed25519 point (little-endian) */
#define CCC_EC_POINT_FLAG_IN_KEYSLOT	0x0008U	/**< EC point in keyslot */
/*@}*/

/**
 * @brief EC point type contains space for inline X and Y co-ordinate
 * values and associated flags.
 *
 * XXX P521 uses at most 66 bytes per co-ordinate; but
 * XXX  rounded to words this is 17 words => max 68 bytes (not 80?)
 * XXX  => TODO (check if this size can be reduced...)
 *
 * @defgroup ec_point_type EC point CCC object
 */
/*@{*/
typedef struct te_ec_point_s {
	union {
		uint8_t x[TE_MAX_ECC_BYTES]; /**< X co-ordinate (byte array) */
		uint8_t ed25519_compressed_point[CCC_ED25519_COMPRESSED_POINT_SIZE];
	};
	uint8_t y[TE_MAX_ECC_BYTES]; /**< Y co-ordinate (byte array) */
	uint32_t point_flags;	/**< CCC_EC_POINT_FLAG_* flag bits. See @ref ec_point_flags */
} te_ec_point_t;
/*@}*/

// XXX Could .k_ec_public be directly te_ec_point_t type field???? TODO

/** EC public key (EC point)
 *
 * @defgroup ec_public_set_key EC public key CCC object
 * @ingroup set_key
 */
/*@{*/
typedef struct {
	te_ec_point_t pubkey;
} ec_pubkey_t;
/*@}*/

/** EC private key
 *
 * @defgroup ec_private_set_key EC private key CCC object
 * @ingroup set_key
 */
/*@{*/
typedef struct {
	te_ec_point_t	pubkey;	       /**< PUBKEY (point on lwrve P = dQ) */
	uint8_t  key[TE_MAX_ECC_BYTES]; /**< Private key (must be 2 < d < order)
					 *
					 * For ED25519 bytes 32..63 additionally contain
					 * the upper 32 bytes of the SHA512 digest
					 * (the "prefix" for creating a signature)
					 */
	uint32_t	key_length;		/**< byte length */
} ec_private_t;
/*@}*/

/**
 * @brief T23X KDF key material (1KEY and 2KEY)
 *
 * @defgroup kdf_set_key Define key derivation key material
 * @ingroup set_key
 */
/*@{*/
typedef struct {
	uint8_t kdk[MAX_AES_KEY_BYTE_SIZE]; /**< KDK key for KDF */
	uint8_t kdd[MAX_AES_KEY_BYTE_SIZE]; /**< KDD key for KDF */
} kdf_key_t; /**< T23X key derivation keys */
/*@}*/

/**
 * @brief Defines and objects for defining keys for the algorithms.
 *
 * @defgroup set_key CCC API TE_OP_SET_KEY key parameters for algorithms
 * @ingroup args_set_key_ref
 */
/*@{*/

/**
 * @brief Max length of HMAC key
 *
 * Arbitrary SW limit (length over 512 bytes).
 */
#define MAX_HMAC_KEY_SIZE MAX_KEY_SIZE

/**
 * @brief Largest opaque key field size (max length key ciphered with a padding AES mode)
 * (round up to full aes block and add one block)
 */
#define MAX_OPAQUE_KEY_SIZE \
	((((MAX_KEY_SIZE + AES_BLOCK_SIZE) / AES_BLOCK_SIZE) * AES_BLOCK_SIZE) + AES_BLOCK_SIZE)

/**
 * @brief Diffie-Hellman(-Merkle) (DH) field naming only, uses RSA priv key
 * internally, but this name improves code readability.
 *
 * <b>.pub_exponent</b> field is never used with DH, which uses only
 * <b>.modulus</b> and <b>.priv_exponent</b>.
 *
 * DH field name <b>k_dh</b> overloads <b>k_rsa_priv</b>, this operation is
 * like RSA private key modular exponentiation.
 */
#define k_dh k_rsa_priv

/**
 * @brief XMSS signature verification public key value.
 *
 * The 64 byte root_seed is used as the public key to verify the signature.
 * for the TE_ALG_XMSS_SHA2_20_256 algorithm.
 *
 * The signature and message are passed as input arguments.
 *
 * @defgroup xmss_public_set_key XMSS operation public key value
 * @ingroup set_key
 */

/*@{*/
#define XMSS_SHA256_HASH_SIZE 32U
#define XMSS_ROOT_SEED_SIZE_SHA2_20_256 (2U * XMSS_SHA256_HASH_SIZE)

#define XMSS_HASH_SIZE_MAX XMSS_SHA256_HASH_SIZE
#define XMSS_PUBKEY_OID_LEN 4U
#define XMSS_ROOT_SEED_MAX_SIZE (2U * XMSS_HASH_SIZE_MAX)

struct xmss_pubkey_s {
	/* length implicitly defined by algorithm;
	 * also needs to be set to k_byte_size in key object fields
	 */
	union {
		/* RFC-8391 pubkey (octet sequence):
		 *
		 * +--------------+
		 * | OID  (4)     |
		 * +--------------+
		 * | ROOT (n)     |
		 * +--------------+
		 * | SEED (n)     |
		 * +--------------+
		 */
		struct {
			uint8_t pubkey[XMSS_PUBKEY_OID_LEN + (2U * XMSS_HASH_SIZE_MAX)];
		} rfc8391;

		uint8_t	 root_seed[XMSS_ROOT_SEED_MAX_SIZE]; /**< XMSS root seed */
	};
};
/*@}*/

typedef struct te_args_key_data_s {
	/**
	 * @brief Let this union be the first field in this object.
	 * The key material may be accessed with a DMA engine.
	 *
	 * (Only for DMA reading now: e.g. long SHA hmac keys are read with SHA
	 * DMA engine. If ever need to DMA write here => this would then need
	 * be cache line aligned).
	 */

	/* TODO =>
	 * This could also support an encrypted key (not supported).
	 * Could use a "per device" KEK to cipher exported
	 * keys locked for this device and then imported back in
	 * later. Then keys could also be loaded back in to the device
	 * and stored as encrypted objects outside (all such keys
	 * would be device locked by aes /w KEK key)
	 */
	union {
		uint32_t k_word_align[1];	/**< unused field, forces word aligned field addresses */
		uint8_t k_aes_key[MAX_AES_KEY_BYTE_SIZE * 2U]; /**< Double size for XTS <key,subkey> */
		uint8_t k_hmac_key[MAX_HMAC_KEY_SIZE]; /**< HMAC key (max key size) */
		rsa_pubkey_t k_rsa_pub;	/**< RSA public key */
		rsa_private_t k_rsa_priv; /**< RSA private key */
		ec_pubkey_t  k_ec_public; /**< EC public key */
		ec_private_t k_ec_private; /**< EC private key */
		kdf_key_t k_kdf; /**< Key derivation KDK and KDD values */
		struct xmss_pubkey_s k_xmss_public; /**< XMSS root_seed for signature verification */
		uint8_t k_opaque_key[MAX_OPAQUE_KEY_SIZE]; /**< Opaque key (not used,
							    * only for space allocation)
							    */
	};

	te_key_type_t k_key_type; /**< Type of key definition in this object */
	uint32_t    k_flags; /**< KEY_FLAG_* bit field values, e.g. KEY_FLAG_PLAIN.
			      *
			      * See @ref key_flags
			      */

	uint32_t    k_byte_size;      /**< Key length in bytes (e.g. HMAC key length varies) */
	uint32_t    k_keyslot;	      /**< 0..15 (symmetric) or 0..3 (pka) */

	/**
	 * 0..15 (symmetric keyslot for subkeys, e.g. for XTS mode tweak)
	 *
	 * Older SOCs use the k_keyslot to hold both the XTS key and Tweak key.
	 */
	union {
		uint32_t    k_tweak_keyslot; /**< XTS tweak (T23X uses
					      * a separate key slot
					      * for XTS tweak key)
					      */
		uint32_t    k_kdd_keyslot;	/**< T23X: another name for the field holding
						 * KDF 2KEY derivation second keyslot index (KDD key)
						 */
	};
} te_args_key_data_t;
/*@}*/

/**
 * @brief Address of the immutable key object.
 *
 * Union contains an address to te_args_key_data_t object for passing
 * key data to CCC and makes sure that there is always 8 bytes space
 * for the pointer even in 32 bit systems where pointer size is 4
 * bytes:
 *
 * This can now contain both 32 bit user space virtual address (kdata)
 * or a kernel 64 bit virtual address (kdata_kv).
 *
 * API object field.
 *
 * @defgroup args_set_key_ref CCC API key parameters object reference
 * @ingroup crypto_args
 */
/*@{*/
typedef struct te_args_set_key_s {
	union {
		uint64_t	    alignment_kdata64;	/**< guarantee kdata field size is 8 bytes
							 * (field never used in code)
							 */
		te_args_key_data_t *kdata;	/**< may be either a 32 or 64 bit pointer
						 *
						 * This should really be made const.
						 * CCC does not modify the *kdata object,
						 * but some clients have been written so
						 * that they use CCC types internally and
						 * do construct the *kdata object locally
						 * via this pointer before passing
						 * it to CCC => too late to change.
						 */
	};
} te_args_set_key_t;
/*@}*/

/**
 * @brief The synchronous primitive operations are used for setting up runtime
 * state and performing all CCC supported operations.
 *
 * @defgroup primitive_operations_sync CCC API synchronous primitive operations
 * @ingroup primitive_operations
 */
/*@{*/
#define	TE_OP_NOP      0x0000U	/**< NOP (no operation) */
#define TE_OP_ALLOCATE 0x0001U	/**< ALLOCATE (only used for calls from another address space) */
#define	TE_OP_INIT     0x0002U	/**< INIT operation (initialize the crypto context) */
#define	TE_OP_UPDATE   0x0004U	/**< UPDATE (provide one more chunk of data to the algorithm).
				 *
				 *
				 * See @ref args_data
				 */
#define	TE_OP_DOFINAL  0x0008U	/**< DOFINAL (completes an operation, optionally provide one more
				 * chunk fo data to the algorithm)
				 *
				 * See @ref args_data
				 */
#define	TE_OP_RESET    0x0010U	/**< RESET a completed operation or error in context object */
#define	TE_OP_RELEASE  0x0020U	/**< RELEASE (only for objects ALLOCATEd before) */
#define	TE_OP_SET_KEY  0x0040U	/**< SET_KEY for the operation. See @ref set_key */

#define TE_OP_COMBINED_OPERATION 0x1000U /**< One shot / single call operation.
					  * Shorthand for: INIT | (SET_KEY) | DOFINAL | RESET
					  *
					  * The SET_KEY bit when part of a COMBINED_OPERATION is
					  * ignored if the operation does not use a key.
					  *
					  * The simplest way to perform any crypto operation is to
					  * use the opcode TE_OP_COMBINED_OPERATION since it will
					  * execute all steps in a single call.
					  *
					  * <b>This opcode can be used when there is no need to
					  * use TE_OP_UPDATE calls, i.e. when all data needed
					  * to be processed is present for the single operation call.
					  *
					  * In that case it is always recommended to use
					  * TE_OP_COMBINED_OPERATION.</b>
					  */
/*@}*/

/**
 * Some SE engines allow asyncronous calls; this is an optional
 * feature and must be enabled in CCC at compile time.
 *
 * Lwrrently SHA engine and AES engines support asynchronous
 * operations.
 *
 * These operations should be used in place of the UPDATE and DOFINAL
 * operations in place of the above ones to use the engines
 * asynchronously.
 *
 * If so used, the client is responsible of "finishing" the operation
 * or the MUTEX timeour will trigger resetting the engine.
 *
 * The "CHECK" operation can optionally be used to poll the engine
 * state in a non-blocking way.
 *
 * The "FINISH" will complete the operation (update or dofinal)
 * previously started in context.
 *
 * Note: The MUTEX WATCHDOG may trigger during an async call unless
 * the operation is finalized before that oclwrs. Watchdog trigger can
 * also be prevented by CHECKing the async operation state before the
 * watchdog triggers: the poll will reset the watchdog down counter
 * back to the preset start value.
 *
 * The mutex watchdog reset oclwrs in 1.6 seconds after grabbing the
 * mutex: the operation must be "finished" before that happens (or SE
 * engine will get reset).
 *
 * @defgroup primitive_operations_async CCC API asynchronous primitive operations
 * @ingroup primitive_operations
 */
/*@{*/
#define	TE_OP_ASYNC_UPDATE_START   0x00010000U /**< Start an ASYNC UPDATE operation.
						* Does not block.
						*/
#define	TE_OP_ASYNC_DOFINAL_START  0x00020000U /**< Start an ASYNC DOFINAL operation.
						* Does not block.
						*/
#define	TE_OP_ASYNC_CHECK_STATE	   0x00040000U /**< Check if started ASYNC operation has completed.
						* Does not block.
						*/
#define	TE_OP_ASYNC_FINISH	   0x00080000U /**< Complete a started ASYNC operation.
						* This blocks until ASYNC operation completes.
						*/
/*@}*/

/**
 * These are the supported bit field operation codes
 *
 * In the KERNEL context the TE_OP_COMBINED_OPERATION is effectively a short hand for:
 * TE_OP_INIT | TE_OP_SET_KEY | TE_OP_DOFINAL | TE_OP_RESET
 *
 * In a TA context it is adding context creation and releasing operations:
 * TE_OP_ALLOCATE | TE_OP_INIT | TE_OP_SET_KEY | TE_OP_DOFINAL | TE_OP_RESET | TE_OP_RELEASE
 *
 * Note that the TE_OP_SET_KEY is silently ignored by the call if the operation does not use a key
 * (e.g. a message digest) AND it is not the only operation performed by the call;
 * in a combined operation this is always true.
 *
 * @defgroup primitive_operations CCC API primitive operations
 * @ingroup context_layer_api
 */
/*@{*/
typedef uint32_t te_crypto_op_t; /**< Bit mask of requested operations for this CCC API call */
/*@}*/

/**
 * @brief Crypto context API call argument objects
 *
 * @defgroup crypto_context_api_args Crypto context API call arguments
 * @ingroup context_layer_api
 */
/*@{*/
/*@}*/

/**
 * @brief CCC context API argument object.
 *
 * This type object is used for passing arguments to the CCC API crypto calls.
 * The same object type is used for both the CCC kernel/library mode calls
 * when the client is in the same address space as CCC and for the Trusty
 * TA case when the arguments are passed from 32 bit TA to the 64 bit Trusty
 * with CCC for processing crypto requests via a syscall.
 *
 * Key material is passed by reference; it is only used with
 * TE_OP_SET_KEY operation and never modified by the syscall or
 * crypto_handle_operation (i.e. it is immutable). Caller should clear
 * the key material immediately after the related call returns, the
 * key material is internally cached to the crypto context in the kernel.
 * This cached data is cleared on reset/release operations.
 *
 * The ca_set_key is required only for the operations TE_OP_SET_KEY and
 * TE_OP_COMBINED_OPERATION (and only if the combined operation requires a
 * key). For other ops the ca_set_key.kdata can be NULL.
 *
 * @defgroup crypto_args CCC API argument object
 * @ingroup crypto_context_api_args
 */
/*@{*/
typedef struct te_crypto_args_s {
	uint32_t		ca_handle; /**< Unique crypto context identifying handle.
					    * Lwrrently only used by the separate address space
					    * adaptation layer code to identify the object.
					    *
					    * This field is not used by CCC core code.
					    */

	te_crypto_alg_mode_t	ca_alg_mode; /**< Mode of the crypto algorithm */

	te_crypto_algo_t	ca_algo; /**< Crypto algorithm used in this context */

	te_crypto_op_t		ca_opcode; /**< Bit vector of primitive operations to execute
					    *
					    * Can be ORed to perform multiple primitive
					    * operations per call. Either one op at a time
					    * or a combined operation is possible.
					    */

	/* Primitive operation arguments */

	struct te_args_init_s	 ca_init; /**< Algorithm specific init values for TE_OP_INIT primitive.
					   *
					   * For some algorithms the initialization values are updated
					   * when TE_OP_DOFINAL completes to pass back final values to caller.
					   *
					   * For example, the final initialization vector value
					   * for AES block cipher modes using IV is passed back here.
					   */

	struct te_args_data_s	 ca_data; /**< Data input/output argument for TE_OP_UPDATE and
					   * TE_OP_DOFINAL primitives.
					   */

	struct te_args_set_key_s ca_set_key; /**< This object contains an immutable reference to key
					      * object for defining a key used for the operation
					      * (for TE_OP_SET_KEY primitive)
					      */
} te_crypto_args_t;
/*@}*/

#if !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES)
/** @brief Only for backwards compatibility; collection of old names
 * that were changed due to cert-c rejecting macro names starting with
 * letter 'E'
 *
 * These are defined only for backwards compatibility,
 * no longer used by CCC. The CCC_E* names are used by
 * CCC code for Cert-C compatibility.
 *
 * Define HAVE_CERT_C_COMPATIBLE_MACRO_NAMES to get rid of these
 * incompatible names provided for subsystems (CCC now uses only
 * the CERT-C compatible names internally).
 *
 * @defgroup certc_compat_names CERT-C incompatible names (backwards compatible)
 */
/*@{*/
#define EC_SIG_FLAG_NONE			CCC_EC_SIG_FLAG_NONE /**< bacwards compatibility */
#define EC_SIG_FLAG_LITTLE_ENDIAN		CCC_EC_SIG_FLAG_LITTLE_ENDIAN /**< bacwards compatibility */

#define ED25519_COMPRESSED_POINT_SIZE		CCC_ED25519_COMPRESSED_POINT_SIZE /**< bacwards compatibility */
#define ED25519_SIGNATURE_SIZE			CCC_ED25519_SIGNATURE_SIZE /**< bacwards compatibility */

#define EC_POINT_FLAG_NONE			CCC_EC_POINT_FLAG_NONE /**< bacwards compatibility */
#define EC_POINT_FLAG_LITTLE_ENDIAN		CCC_EC_POINT_FLAG_LITTLE_ENDIAN /**< bacwards compatibility */
#define EC_POINT_FLAG_UNDEFINED			CCC_EC_POINT_FLAG_UNDEFINED /**< bacwards compatibility */
#define EC_POINT_FLAG_COMPRESSED_ED25519	CCC_EC_POINT_FLAG_COMPRESSED_ED25519 /**< bacwards compatibility */
#define EC_POINT_FLAG_IN_KEYSLOT		CCC_EC_POINT_FLAG_IN_KEYSLOT
/*@}*/
#endif /* !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES) */

#endif /* INCLUDED_CRYPTO_API_H */
