/*
 * Copyright (c) 2015-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   tegra_se.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2015
 *
 * @brief Defines and function prototypes for SE engines and contexts.
 *
 * Contains the object and other definitions for code using engine
 * operations.
 */
#ifndef INCLUDED_TEGRA_SE_H
#define INCLUDED_TEGRA_SE_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <tegra_cdev.h>
#include <context_mem.h>

#ifndef MEASURE_PERF_ENGINE_SE_OPERATION
#define MEASURE_PERF_ENGINE_SE_OPERATION 0
#endif

/* Include for struct se_key_slot type */
#include <tegra_se_ks.h>

#if CCC_SOC_WITH_LWPKA
#include <lwpka_engine_data.h>
#else
struct pka1_engine_data; /**< EC and RSA contexts refer to this object with PKA1 engine */
#endif

/** @def CCC_OBJECT_ASSIGN(adst,src)
 * @brief Object assignment macro
 *
 * CCC_OBJECT_ASSIGN macro is used e.g. for struct assignments which
 * the compiler is colwerting to a block move. It may e.g. internally
 * call memcpy() or generate a compatible instruction sequence inline.
 *
 * If the architecture is normally supporting unaligned accesses these
 * block moves may not be strict aligned which in turn may cause
 * issues if the objects exists in some restricted memory section that
 * does not support unaligned accesses (e.g. device memory).
 *
 * In such case this might result in an error condition.
 *
 * This macro is used when CCC core is doing e.g. struct assignment
 * kind of block moves. By default this does an object
 * assignments. This is fine when subsystem using CCC is not using
 * such restricted memory sections and also makes sure that the types
 * are compatible in the block assignments.
 *
 * If a subsystem is using memory which does not support misaligned
 * accesses then this macro can be defined e.g. to be:
 *
 *   se_util_mem_move((uint8_t *)&(dst), (const uint8_t *)&(src), sizeof_u32(dst))
 *
 * This works also in device memory since it uses byte moves for byte
 * aligned accesses.
 */
#ifndef CCC_OBJECT_ASSIGN
#define CCC_OBJECT_ASSIGN(obj_A, obj_B) (obj_A) = (obj_B)
#endif

/** Max/min count for SE status polls for engine free.
 * This value is completely arbitrary => TODO (improve).
 *
 * Clients can use se_engine_set_max_timeout() to change this
 * to a value between the [min .. MAXINT] (excluding bounds).
 */
#ifndef SE_MAX_TIMEOUT
#define SE_MAX_TIMEOUT 0x08000000U	/**< default MAX count for status polling */
#endif
#ifndef SE_MIN_TIMEOUT
#define SE_MIN_TIMEOUT 0x000FFFFFU	/**< default MIN count for status polling */
#endif

#if HAVE_SE_ENGINE_TIMEOUT
/** Set engine status poll limit
 *
 * Default is SE_MAX_TIMEOUT.
 */
status_t se_engine_set_max_timeout(uint32_t max_timeout);
uint32_t get_se_engine_max_timeout(void);
#endif /* HAVE_SE_ENGINE_TIMEOUT */

struct engine_s;

#define SE_AES_BLOCK_LENGTH	16U /**< AES algorithm fixed block size */
#define AES_IV_SIZE		16U /**< AES algorithm IV size */

#define AES_GCM_NONCE_LENGTH	12U /**< AES-GCM/GMAC only support 12 byte nonce value (96 bits) */

#define SE_PKA0_RSA_MAX_MODULUS_SIZE_BITS  2048U /**< PKA0 engine RSA modulus size limit */
#define SE_PKA0_RSA_MAX_EXPONENT_SIZE_BITS 2048U /**< PKA0 engine RSA exponent size limit */
#define RSA_MAX_PUB_EXPONENT_SIZE_BITS 32U  /**< PKA0 engine RSA public key size limit */

/** @brief Operation origin domain */
typedef enum te_crypto_domain_e {
	TE_CRYPTO_DOMAIN_UNKNOWN = 0,
	TE_CRYPTO_DOMAIN_KERNEL = 100,
#if HAVE_USER_MODE
	TE_CRYPTO_DOMAIN_TA
#endif
} te_crypto_domain_t;

#if HAVE_USER_MODE
/** @brief System specific virtual to physical address mapping function,
 * defined in the config file. Used when for systems which support
 * calls from different domains (e.g. user space / kernel space) and
 * the virtual to physical address mapping needs to deal with this.
 */
PHYS_ADDR_T SYSTEM_DOMAIN_V2P(te_crypto_domain_t domain, const void *vaddr);
#endif

#define HAVE_CLOSE_FUNCTIONS 0 /**< Functions deprecated (REMOVE) */

/** True if eid is AES engine id */
static inline bool m_is_aes_engine_id(engine_id_t eid)
{
	return ((CCC_ENGINE_SE0_AES0 == eid) || (CCC_ENGINE_SE0_AES1 == eid));
}

#define IS_AES_ENGINE_ID(eid) m_is_aes_engine_id(eid)

/** Colwert the address to ordinal type to check the low bits for
 * word alignment of the address.
 */
#define IS_WORD_ALIGNED_ADDR(addr)				\
	((((uintptr_t)(addr)) & (BYTES_IN_WORD - 1U)) == 0U)

/** These AES algos require padding */
static inline bool m_aes_mode_padding(te_crypto_algo_t algo)
{
	return ((TE_ALG_AES_ECB == algo) || (TE_ALG_AES_CBC == algo));
}

#define TE_AES_MODE_PADDING(algo)	m_aes_mode_padding(algo)

/** These AES modes use IV (in the sense of programming it to SE HW IV registers)
 */
static inline bool m_aes_mode_use_iv(te_crypto_algo_t algo)
{
	return ((TE_ALG_AES_CBC == algo) || (TE_ALG_AES_CBC_NOPAD == algo) ||
		(TE_ALG_AES_CTS == algo) || (TE_ALG_AES_OFB == algo));
}

#define TE_AES_MODE_USES_IV(algo)	m_aes_mode_use_iv(algo)

/** These AES modes use counter
 */
static inline bool m_aes_mode_counter(te_crypto_algo_t algo)
{
	return ((TE_ALG_AES_CCM == algo) ||
		(TE_ALG_AES_CTR == algo) ||
		(TE_ALG_AES_GCM == algo));
}

#define TE_AES_MODE_COUNTER(algo) m_aes_mode_counter(algo)

static inline bool m_aes_gcm_mode(te_crypto_algo_t algo)
{
	return ((TE_ALG_AES_GCM == algo) || (TE_ALG_GMAC_AES == algo));
}

/**
 * @def TE_AES_GCM_MODE(algo)
 * @brief Return true is this algorithm is either TE_ALG_AES_GCM or TE_ALG_GMAC_AES
 */
#define TE_AES_GCM_MODE(algo) m_aes_gcm_mode(algo)

#if HAVE_NIST_TRUNCATED_SHA2
static inline bool m_is_nist_truncated_digest(te_crypto_algo_t algo)
{
	return ((TE_ALG_SHA512_224 == algo) || (TE_ALG_SHA512_256 == algo));
}
#define IS_NIST_TRUNCATED_DIGEST(algo) m_is_nist_truncated_digest(algo)

#endif

/** True if a is a SHA-2 algorithm */
static inline bool m_is_sha2(te_crypto_algo_t algo)
{
	return ((TE_ALG_SHA224 == algo) || (TE_ALG_SHA256 == algo) ||
		(TE_ALG_SHA384 == algo) || (TE_ALG_SHA512 == algo));
}
#define IS_SHA2(a) m_is_sha2(a)

/** True if SHAKE128 or SHAKE256 algorithm
 */
static inline bool m_is_shake(te_crypto_algo_t algo)
{
	return ((TE_ALG_SHAKE128 == algo) || (TE_ALG_SHAKE256 == algo));
}
#define IS_SHAKE(a) m_is_shake(a)

#if CCC_WITH_SHA3
/** True if a SHA3 algorithm
 */
static inline bool m_is_sha3(te_crypto_algo_t algo)
{
	return ((TE_ALG_SHA3_224 == algo) || (TE_ALG_SHA3_256 == algo) ||
		(TE_ALG_SHA3_384 == algo) || (TE_ALG_SHA3_512 == algo));
}
#define IS_SHA3(a) m_is_sha3(a)
#endif /* CCC_WITH_SHA3 */

/** True if bitsize is a valid AES key size in bits */
static inline bool m_is_valid_aes_keysize_bits(uint32_t keysize)
{
	return ((128U == keysize) ||
		(192U == keysize) ||
		(256U == keysize));
}

#define IS_VALID_AES_KEYSIZE_BITS(bitsize)	m_is_valid_aes_keysize_bits(bitsize)

/** True if bytesize is a valid AES key size in bytes */
static inline bool m_is_valid_aes_keysize_bytes(uint32_t keysize)
{
	return (((128U / 8U) == keysize) ||
		((192U / 8U) == keysize) ||
		((256U / 8U) == keysize));
}

#define IS_VALID_AES_KEYSIZE_BYTES(bytesize)	m_is_valid_aes_keysize_bytes(bytesize)

#if HAVE_SE_RSA
/** SE PKA0 engine supported RSA bitsize. Other engines may support longer
 * keys (IS_VALID_RSA_KEYSIZE_BITS below).
 *
 * Enforce a minumum allowed keysize by configuration.
 *
 * Removed RSA768 from list of accepted key sizes.
 */
static inline bool m_is_valid_rsa_keysize_bits_se_engine(uint32_t bitsize)
{
	return (((512U == bitsize) || (1024U == bitsize)  ||
		(1536U == bitsize) || (2048U == bitsize)) &&
		(bitsize >= RSA_MIN_KEYSIZE_BITS));
}
#define IS_VALID_RSA_KEYSIZE_BITS_SE_ENGINE(bitsize) \
		m_is_valid_rsa_keysize_bits_se_engine(bitsize)

/** SE PKA0 engine supported RSA exponent sizes. Private exponent size
 * matches the modulus size and public exponent size is required to be
 * one 32 bit word (caller needs to zero pad if shorter).
 */
static inline bool m_is_valid_rsa_expsize_bits_se_engine(uint32_t bitsize)
{
	return (IS_VALID_RSA_KEYSIZE_BITS_SE_ENGINE(bitsize) ||
		((BYTES_IN_WORD * 8U) == bitsize));
}
#define IS_VALID_RSA_EXPSIZE_BITS_SE_ENGINE(bitsize) \
		m_is_valid_rsa_expsize_bits_se_engine(bitsize)
#endif /* HAVE_SE_RSA */

/** SE PKA0 engine does not support all these, newer PKA engines
 * add support for 3072 and 4096 bit RSA keys.
 */
static inline bool m_is_valid_rsa_keysize_bits(uint32_t bit_size)
{
	return ((512U  == bit_size) || (1024U == bit_size) ||
		(1536U == bit_size) || (2048U == bit_size) ||
		(3072U == bit_size) || (4096U == bit_size)) &&
		(bit_size >= RSA_MIN_KEYSIZE_BITS);
}

#define IS_VALID_RSA_KEYSIZE_BITS(bit_size)	\
		m_is_valid_rsa_keysize_bits(bit_size)

#if CCC_WITH_ECC
struct te_ec_point_s;
struct pka1_ec_lwrve_s;
#endif

/**
 * @brief algorithm in/out data parameter for CCC core.
 *
 * Contains 4 fields: two references and two sizes.
 * First pointer is an immutable source reference, type defined by algorithm.
 *
 * Second parametrer is either an immutable input or a mutable output reference,
 * as defined by algorithm.
 *
 * Nameless unions used for calling code readability, e.g. src used for input buffers
 * but src_digest used when the buffer expected to contain a digest. These
 * buffer references share the same pointer location to keep the object small.
 *
 * @defgroup data_params Generic input/output parameters for CCC core internal use
 */
/*@{*/
struct se_data_params {
	union {
		const uint8_t *src;		     /**< Input buffer */
		const uint8_t *src_digest;	     /**< Input buffer contains a digest */
#if CCC_WITH_ECC
		const struct te_ec_point_s *point_P; /**< Point P input */
#endif
	};
	union {
		uint8_t *dst;		      /**< output buffer (depends on algorithm) */
		struct te_ec_sig_s *dst_ec_signature; /**< output address for ECDSA signature (sign) */
		const uint8_t *src_signature; /**< Signature buffer (verify) */
#if CCC_WITH_ECC
		struct te_ec_point_s *dst_ec_pubkey; /**< Point output for EC public key generation */
		struct te_ec_point_s *point_Q;	     /**< Point Q is input/output for some operations */
		const te_ec_sig_t *src_ec_signature; /**< CCC ec signature object */
		const uint8_t *src_asn1;	     /**< ASN.1 encoded ECDSA signature */
#endif
	};
	union {
		uint32_t input_size;	     /**< Byte length of src */
		uint32_t src_digest_size;    /**< Byte length os src_digest */
	};
	union {
		uint32_t output_size;	     /**< Byte length of buffer dst above (when writing dst) */
		uint32_t src_signature_size; /**< Byte length of src_asn1 and sec_signature objects */
	};
};
/*@}*/

#define SHA_ASYNC_MAGIC 0xfeed0e01U /**< Magic value for async_sha_ctx_s.magic (SHA) */
#define AES_ASYNC_MAGIC 0xfeed0302U /**< Magic value for async_aes_ctx_s.magic (AES) */

/**
 * @brief State objects for SHA and AES asynchronous calls
 * @defgroup engine_async_state Async state objects
 */
/*@{*/

/**
 * @brief Context for async SHA state
 */
typedef struct  async_sha_ctx_s {
	uint32_t magic;		/**< Magic value, valid if SHA_ASYNC_MAGIC */

	struct se_engine_sha_context *econtext; /**< SHA engine context for async op */
	struct se_data_params input; /**< Async SHA data in/out */

	bool     swap_result_words;  /**< For large block-size SHA digests the results need to be swapped
				      * since they are internally little endian 64 bit values. Short smaller block
				      * SHA engines operate on 32 bit values
				      */
	bool     started;	     /**< SHA async was started */
#if SHA_DST_MEMORY
	uint8_t *aligned_result;     /**< Normally CCC writes SHA output to HASH_REGs but it
				      * can also use DMA to write the result (SHA_DST_MEMORY != 0)
				      * In that case the result buffer must be CACHE_LINE aligned
				      * (address, size) to avoid cache issues
				      *
				      * This is athe reference to such DMA output buffer.
				      */
#endif
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	uint32_t gen_op_perf_usec;   /**< Perf measurements */
#endif
} async_sha_ctx_t;

/**
 * @brief Context for async AES state
 */
typedef struct async_aes_ctx_s {
	uint32_t magic;		/**< Magic value, valid if AES_ASYNC_MAGIC */
	uint32_t spare_reg_0;	/**< Save reg for spare reg value during async AES */

	struct se_engine_aes_context *econtext; /**< AES engine context for async op */
	struct se_data_params input; /**< Async SHA data in/out */

	uint32_t num_blocks;
	uint32_t flush_dst_size;
	bool     leave_key_to_keyslot;
	bool     key_written;
	bool     use_preset_key;
	bool     update_ctr;

	bool     started;	     /**< AES async was started */
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	uint32_t gen_op_perf_usec;   /**< Perf measurements */
#endif
} async_aes_ctx_t;
/*@}*/

/* Max digest result size (for all supported digests)
 * TODO: SHAKE is not included; result size is arbitrary.
 */
#define MAX_DIGEST_LEN 64U

/** @brief SHA engine size limit per chunk: 16777215U = 2^24-1 but size needs
 * to be a multiple of the SHA internal digest block size if
 * the operation is continued with subsequent chunks of data.
 *
 * SHA variants have different internal block sizes (64 and 128 bytes)
 * so we need two max sizes to handle the largest possible data size
 * per chunk accordingly.
 *
 * Async SHA calls must not pass more data than this by
 * each call (update/dofinal), larger synchronous data is
 * auto-split by CCC.
 */
#define MAX_SHA_ENGINE_CHUNK_SIZE_BSIZE64  0xFFFFC0U /**< Max chunk size for short block SHA digests */
#define MAX_SHA_ENGINE_CHUNK_SIZE_BSIZE128 0xFFFF80U /**< Max chunk size for long block SHA digests */

/** @brief Keep this for backwards compatibility, this max size is ok
 * for both 64 and 128 byte block size.
 */
#define MAX_SHA_ENGINE_CHUNK_SIZE MAX_SHA_ENGINE_CHUNK_SIZE_BSIZE128

/** @brief Largest AES block size multiple supported by AES engine. */
#define MAX_AES_ENGINE_CHUNK_SIZE 0xFFFFF0U

#define SHA_BLOCK_SIZE_64 (512U / 8U) /**< Shorter sha digest have smaller block size */
#define SHA_BLOCK_SIZE_128 (1024U / 8U) /**< SHA-512 and SHA-384 use 128 byte blocks */

#define SHA_CONTEXT_STATE_BITS 512U	/**< Max bits in SHA1/SHA2 context state */
#define KECCAK_CONTEXT_STATE_BITS 1600U /**< Bits in KECCAK context state */

#define SHAKE128_BLOCK_SIZE ((KECCAK_CONTEXT_STATE_BITS - 256U) / 8U) /**< SHAKE-128 block size */
#define SHAKE256_BLOCK_SIZE ((KECCAK_CONTEXT_STATE_BITS - 512U) / 8U) /**< SHAKE-256 block size */

/** @brief The above SHAKE block sizes are not pow2 aligned;
 * this is the nearest power of 2 value larger than those.
 */
#define SHAKE_BLOCK_SIZE_ALIGNED 256U

/** @brief SHA-3 intermediate result size is 1600 bits (200 bytes),
 * it is larger than any supported digest result length, except
 * SHAKE XOF sizes.
 */
#define SHA3_INTERMEDIATE_RESULT_SIZE (KECCAK_CONTEXT_STATE_BITS / 8U)

/** @brief For the sha_context gather buffer size: multiple (N)
 * of the digest internal block size. Using N == 1.
 */
#if CCC_WITH_SHA3
/** @brief SHA3 intermediate sizes are 1600 bits long.
 *
 * Subsystems may not allow aligned mempool requests
 * unless the alignment is pow2 => use nearest pow2.
 */
#define MAX_MD_ALIGNED_SIZE DMA_ALIGN_SIZE(SHAKE_BLOCK_SIZE_ALIGNED)
#define MAX_MD_BLOCK_SIZE SHA3_INTERMEDIATE_RESULT_SIZE

#define MAX_SHAKE_XOF_BITLEN_IN_REGISTER	KECCAK_CONTEXT_STATE_BITS /* 50 word size regs */
#define MAX_SHAKE_XOF_BITLEN			134217728U	 /* HW limit (16 MB) */

#else
/** @brief SHA-512 uses longest supported block size of 128 bytes
 *
 * Subsystems may not allow aligned mempool object requests
 * unless the alignment is pow2.
 */
#define MAX_MD_ALIGNED_SIZE SHA_BLOCK_SIZE_128
#define MAX_MD_BLOCK_SIZE SHA_BLOCK_SIZE_128
#endif /* CCC_WITH_SHA3 */

/** @brief Max bytes processed by single SHA engine task (multiple of
 * digest internal block size).
 *
 * @param econtext sha engine context
 * @param max_chunk_size_p referenced object set to max input chunk size OK for HW
 *
 * @return NO_ERROR if success, specific error if fails
 *
 * @ingroup engine_sha
 * @defgroup engine_sha_support SHA engine support functions
 */
/*@{*/
status_t se_engine_get_sha_max_chunk_size(const struct se_engine_sha_context *econtext,
					  uint32_t *max_chunk_size_p);
/*@}*/

/*
 * @defgroup process_layer_api Process layer entry handlers
 *
 * Function pointer types have been removed => s
 * TODO: add docs for process layer entry function (i.e. API => PROCESS functions)
 */
/*@{*/
/*@}*/

/* Context memory object reference from upper layers or from caller.
 *
 * This type is for fields in the engine contexts and can be NULL
 * (uses subsystem memory) or when HAVE_CONTEXT_MEMORY is set and this
 * is not null => use CCC context memory management for the algorithm
 * context.
 *
 * At some later time could could remove subsystem memory support as
 * an option and return error when context memory is not set when
 * enabled.
 */
struct context_mem_s;

/**
 * @brief Engine layer
 *
 * @defgroup engine_layer Engine layer
 */
/*@{*/
/*@}*/

/**
 * @brief SHA engine layer items
 *
 * @ingroup engine_layer
 * @defgroup engine_sha SHA engine layer
 */
/*@{*/
/**
 * @brief SHA does not use keys, but the HW SHA engine does for KDF, HMAC, etc.
 */
#define SHA_FLAGS_NONE			0x0000U /**< SHA context flags (no flags) */
#define SHA_FLAGS_USE_PRESET_KEY	0x0001U /**< [T23X] Use preset key from keyslot in SHA HW engine ops */
#define SHA_FLAGS_LEAVE_KEY_TO_KEYSLOT	0x0002U /**< [T23X] Leave key in SHA HW keyslot */
#define SHA_FLAGS_DYNAMIC_KEYSLOT	0x0004U	/**< driver selects how to set up the key */
#define SHA_FLAGS_HW_CONTEXT		0x0008U	/**< [T23X] operation uses HW keyslot context */
#define SHA_FLAGS_SW_IMPLEMENTATION	0x0010U	/**< SW or HYBRID version of the algorithm */

/**
 * @brief SHA engine context object type.
 *
 * Engine layer functions operate on this object type and this must not
 * refer to any upper layer data structures.
 *
 * In context layer calls this object is part of se_sha_context for colwenience
 * but it does not have to be.
 *
 * Object initalized with: sha_engine_context_static_init()
 */
typedef struct se_engine_sha_context {
	const struct engine_s *engine; /**< Selected SHA engine object ref */
	struct context_mem_s  *cmem; /* context memory for this call chain or NULL */
	union {
		uint8_t	 tmphash[ MAX_MD_BLOCK_SIZE ]; /**< SHA/HMAC intermediate/final value saved here.
							* But SHA DMA does not write to this buffer so
							* it does not need to be CL aligned. Max used
							* length is the longest block size in any
							* related algorithm (SHA-x, HMAC-SHA-x)
							*/
		uint32_t	 tmphash32[ MAX_MD_BLOCK_SIZE / BYTES_IN_WORD ]; /* Same, accessed
										  * as words
										  */
	};
	bool		 is_first;	/**< First call for this context */
	bool		 is_last;	/**< TE_OP_DOFINAL call */
	bool		 is_hw_hmac;	 /**< True when using HW HMAC support for HMAC-SHA2 (in T23X) */
	te_crypto_algo_t algorithm;	 /**< original algorithm, if this is e.g. a HMAC-SHA, KDF, etc... */
	te_crypto_algo_t hash_algorithm; /**< Related digest algorithm (gets mapped to hw algorithm) */
	uint32_t         sha_flags;      /**< Bit vector SHA_FLAGS_* */
	uint32_t	 byte_count;	 /**< Byte count passed for this context */
	uint32_t	 block_size;	/**< fixed value per hash_algorithm (algo internal block size).
					 */
	uint32_t	 hash_size;	/**< hash algorithm (internal) size.
					 * For truncated digests this is NOT the same as result length,
					 * e.g. for SHA-512/256 this is 64 but result size is 32 bytes
					 * (SW truncated).
					 */
	te_crypto_domain_t domain;	/**< user space or kernel space addresses */
	uint32_t	 perf_usec;	/**< Perf measurements */
#if CCC_WITH_SHA3
	uint32_t	 shake_xof_bitlen; /**< SHAKE XOF output bitlen */
#endif

#if HAVE_HW_HMAC_SHA || HAVE_SE_KAC_KDF
	struct {
		struct se_key_slot se_ks;	/**< SE keyslot setup for keyed SHA operations */

#if HAVE_SE_KAC_KDF
		/* Target keyslot client provided manifest fields => VERIFY
		 */
		kac_manifest_purpose_t kdf_mf_purpose; /**< KDF function target keyslot
							* manifest purpose
							*/
		kac_manifest_user_t kdf_mf_user; /**< KDF function target keyslot
						  * manifest user
						  */
		kac_manifest_size_t kdf_mf_keybits; /**< KDF function target keyslot
						     * manifest key size
						     */
		uint32_t kdf_mf_sw; /**< KDF function target keyslot manifest 16 bit
				     * SW field value
				     */
#endif /* HAVE_SE_KAC_KDF */
	};
#endif /* HAVE_HW_HMAC_SHA || HAVE_SE_KAC_KDF */
} se_engine_sha_context_t;
/*@}*/

/**
 * @brief SHA context object type for the process layer.
 *
 * Process layer functions operate on this object type.
 *
 * This object contains the se_engine_sha_context for the context layer calls.
 *
 * Object initalized with: sha_context_static_init()
 * @ingroup process_layer_api
 */
/*@{*/
#define CCC_DMA_MEM_SHA_BUFFER_SIZE MAX_MD_ALIGNED_SIZE

#if CCC_USE_DMA_MEM_BUFFER_ALLOC && SHA_DST_MEMORY
#define CCC_DMA_MEM_SHA_RESERVATION_SIZE (2U * MAX_MD_ALIGNED_SIZE)
#else
#define CCC_DMA_MEM_SHA_RESERVATION_SIZE MAX_MD_ALIGNED_SIZE
#endif

struct se_sha_context {
	/**< Buffer could be made longer for optimization reasons later.
	 * Must be multiple of MAX_MD_ALIGNED_SIZE.
	 *
	 * se_sha_context.buf must be in contigous memory when used by sha hw as input
	 *
	 * Buf size CCC_DMA_MEM_SHA_BUFFER_SIZE in both cases.
	 */
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
	uint8_t  *buf;
#else
	uint8_t  buf[ CCC_DMA_MEM_SHA_BUFFER_SIZE ]; /**< intermediate buffer for update/dofinal */
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */
	uint32_t used;			  /**< 0..sizeof(buf)-1, index to buf */
	uint32_t byte_count;		  /**< Bytes passed through this context */

	/* engine code uses these fields
	 * no special alignment requirements for this.
	 */
	se_engine_sha_context_t ec; /**< Engine context object */

#if HAVE_SE_ASYNC_SHA
	struct {
		bool		 async_start; /**< True when async SHA was started in HW */
		async_sha_ctx_t  async_context;  /**< Async SHA operation context */
	};
#endif
};
/*@}*/

/**
 * @brief AES engine layer items
 *
 * @ingroup engine_layer
 * @defgroup engine_aes AES engine layer
 */
/*@{*/
#define AES_FLAGS_NONE			0x00000U
#define AES_FLAGS_USE_PRESET_KEY	0x00001U
#define AES_FLAGS_LEAVE_KEY_TO_KEYSLOT	0x00002U
#define AES_FLAGS_DYNAMIC_KEYSLOT	0x00004U /* driver selects how to set up the key */
#define AES_FLAGS_USE_KEYSLOT_IV	0x00008U /* AES CBC (and related) and OFB modes
						  * use IV field.
						  */
#define AES_FLAGS_HW_CONTEXT		0x00100U /* Operation uses HW keyslot context (T23X) */
#define AES_FLAGS_SW_IMPLEMENTATION	0x00200U /* SW or HYBRID version of the algorithm */

#define AES_FLAGS_CTR_MODE_BIG_ENDIAN	0x01000U /* ctr mode value is big endian, need to swap bytes
						  * for host ordinal value
						  */

#define AES_FLAGS_CTR_MODE_SWAP_ENDIAN	AES_FLAGS_CTR_MODE_BIG_ENDIAN /* BW compatibility */

#define AES_FLAGS_DST_KEYSLOT		0x02000U /**< AES operation (or CMAC) derive key to keyslot */
#define AES_FLAGS_CMAC_DERIVE		0x04000U /**< CMAC used for deriving key */
#define AES_FLAGS_DERIVE_KEY_128	0x10000U /**< AES key derivation: 128 bit key result */
#define AES_FLAGS_DERIVE_KEY_192	0x20000U /**< AES key derivation: 192 bit key result */
#define AES_FLAGS_DERIVE_KEY_256	0x40000U /**< AES key derivation: 256 bit key result */
#define AES_FLAGS_DERIVE_KEY_HIGH	0x80000U /**< AES key derivation: derive upper word quad.
						  * "Action" flag set by caller; if not set
						  * AES key derivation sets the low word quad
						  * of the DST keyslot.
						  */
#define AES_FLAGS_AE_AAD_DATA	        0x100000U /**< AES AE: processing AAD data is allowed.
						   * Cleared after AAD processed and
						   * ENC/DEC data handling starts.
						   *
						   * This is only set in INIT and once cleared
						   * it will not be set again for the context.
						   */
#define AES_FLAGS_AE_FINALIZE_TAG       0x200000U /**< AES AE: finalize TAG (used for GCM) */

#ifdef AES_CTR_MODE_NAME_COMPATIBILITY
/* Finally named the unnamed struct for AES-CTR mode fields. Since
 * those fields may be directly used by T19X key unwrap calls provide
 * the fllowing macros for name compatibility.
 *
 * AES-XTS also renamed but those fields are not used by external
 * code.
 */
#define ctr_mode_linear_counter ctr.counter
#define ctr_mode_increment ctr.increment
#endif /* AES_CTR_MODE_NAME_COMPATIBILITY */

typedef struct se_engine_aes_context {
	const struct engine_s *engine;
	struct context_mem_s  *cmem; /* context memory for this call chain or NULL */
	te_crypto_algo_t algorithm;
	uint32_t aes_flags;

	bool     is_encrypt;
	bool     is_hash;
	bool	 is_first;
	bool	 is_last;

	te_crypto_domain_t domain;
	uint32_t perf_usec;

	struct se_key_slot se_ks;	/* SE keyslot setup */

	// XXXXXXXXXXXX FIXME: Use named struct fields in the union, remove
	// XXXXXXXXXXXX    prefixes from the struct fields

	/* Fields get 64 bit aligned
	 */
	union {
		// For AES modes with IV (strict aliasing support)
		uint32_t iv_stash[ AES_IV_SIZE / BYTES_IN_WORD ];

		// For AES-CTR
		struct {
			// Big endian 128 bit AES CTR mode counter
			uint32_t counter[ SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
			uint8_t  increment; /* value to increment linear counter per AES block */
		} ctr;
#if HAVE_AES_XTS
		struct {
			// For AES-XTS
			//
			// 128 bit AES-XTS tweak value
			uint32_t tweak[ SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
			uint8_t  last_byte_unused_bits;
		} xts;
#endif

#if CCC_WITH_AES_GCM
		// For AES-GCM
		struct {
			uint32_t *counter; /**< Big endian: NONCE(3 words) || COUNTER(1 word) */
			uint8_t *tag;	    /**< GCM tag callwlated in context */
			uint32_t aad_len;   /**< Processed AAD length (for TAG callwlation) */
		} gcm;
#endif

#if HAVE_AES_CCM
		// For AES-CCM
		struct {
			uint32_t *counter;
			uint8_t *wbuf;
			uint32_t wbuf_size; // XXX now the wbuf buffer size (constant length)
			uint32_t tag_len;
			uint32_t nonce_len;
			uint32_t aad_padded_len;
		} ccm;
#endif

#if HAVE_CMAC_AES
		/* For AES MAC algos */
		struct {
			uint32_t phash[SE_AES_BLOCK_LENGTH / BYTES_IN_WORD ];
		} aes_mac;
#endif /* HAVE_CMAC_AES */

#if HAVE_SE_KAC_KEYOP
		/* T23X KW/KUW (key wrapping) */
		struct {
			uint32_t *counter;
			uint8_t *tag;
			uint8_t *wkey;		/* Max 256 bit key; parse manifest for length */
			uint8_t *wbuf;		/* Using aes_context->buf for KUW to write the STATUS */
			uint32_t manifest;	/* Fixed length (4 bytes) ==> Make 32 bit value for now */
		} kwuw;
#endif
	};
} se_engine_aes_context_t;
/*@}*/

/**
 * @brief context returned by aes init operation in process layer
 * @ingroup process_layer_api
 */
/*@{*/
#define CCC_DMA_MEM_AES_BUFFER_SIZE DMA_ALIGN_SIZE(SE_AES_BLOCK_LENGTH)

struct se_aes_context {
	/**
	 * se_aes_context.buf must be in contigous memory when used by aes hw as input
	 * Let this be the first field in the object; (alignment requirements)
	 *
	 * Intermediate buffer for update/dofinal, needs to be cache line aligned
	 * to avoid alignment issues on cache flushes/ilwalidations.
	 * (Only required if writing to buf, but this is only a minor size expansion)
	 *
	 * Buf size CCC_DMA_MEM_AES_BUFFER_SIZE in both cases.
	 */
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
	uint8_t  *buf;
#else
	uint8_t  buf[ CCC_DMA_MEM_AES_BUFFER_SIZE ];
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */
	uint32_t used;			     /**< 0..sizeof(buf)-1, index to buf */

	union {
		struct {
			uint8_t *pk1;	/**< pk1 and pk2 refer to space for the callwlated CMAC-AES
					 *  subkeys K1 and K2. The space is defined by the caller.
					 *  E.g. in crypto_mac.c: the se_mac_state_t object fields
					 *  pk1 and pk2 are used for this purpose.
					 */
			uint8_t *pk2;	/**< Because only CMAC-AES uses these fields space is reserved in MAC
					 * state object not in this object.
					 * Saves 2*32 - 2*sizeof(uint8_t *) bytes.
					 */
		} cmac;

#if HAVE_AES_CCM
		struct {
			const uint8_t *aad;
			const uint8_t *nonce;
			uint32_t aad_len;
		} ccm;
#endif

#if CCC_WITH_AES_GCM
		struct {
			const uint8_t *aad;
		} gcm;
#endif
	};

	uint32_t byte_count;		     /**< number of source bytes passed through aes */
	uint32_t dst_total_written;	     /**< number of bytes written through AES engine
					      * to dst since init()
					      */

	// engine code uses these fields
	// no special alignment requirements
	se_engine_aes_context_t ec;

#if HAVE_SE_ASYNC_AES
	struct {
		bool		async_start;
		async_aes_ctx_t async_context;
	};
#endif
};
/*@}*/

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
/**
 * @brief DRNG/TRNG engine layer items
 *
 * @ingroup engine_layer
 * @defgroup engine_random DRNG/TRNG engine layer
 */
/*@{*/

/** SE AES RANDOM, RNG1/LWRNG and PKA1 TRNG entropy generator use
 * the same context types.
 */
typedef struct se_engine_rng_context {
	const struct engine_s *engine;
	/* No cmem context required in nrg engine context yet */
	te_crypto_algo_t algorithm;
	te_crypto_domain_t domain;
	uint32_t perf_usec;
} se_engine_rng_context_t;
/*@}*/

/**
 * @ingroup process_layer_api
 */
/*@{*/
struct se_rng_context {
	se_engine_rng_context_t ec;
};
/*@}*/
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */

#if CCC_WITH_ECC
/**
 * @brief PKA1 engine layer items
 *
 * @ingroup engine_layer
 * @defgroup engine_ecc PKA1 engine layer
 */
/*@{*/

// XXX TODO: Consider adding EC flag implying that the private key has been "expanded" already
// XXX (e.g. for ED25519 the scalar key value must be SHA512(value) and then processed, the
// XXX  key value is actually 2*32 bytes, including the upper half of the SHA-512 digest
// XXX  as well. This upper half (h) is used only when signing)
//
#define CCC_EC_FLAGS_NONE			0x000000U
#define CCC_EC_FLAGS_USE_PRESET_KEY		0x000001U
#define CCC_EC_FLAGS_LEAVE_KEY_TO_KEYSLOT	0x000002U
#define CCC_EC_FLAGS_DYNAMIC_KEYSLOT	0x000004U /* driver selects how to set up the key */
#define CCC_EC_FLAGS_USE_KEYSLOT		0x000008U /* Use the SE keyslot for key material */
//
#define CCC_EC_FLAGS_BIG_ENDIAN_KEY		0x000100U
#define CCC_EC_FLAGS_BIG_ENDIAN_DATA	0x000400U
#define CCC_EC_FLAGS_USE_PRIVATE_KEY	0x000800U
#define CCC_EC_FLAGS_DIGEST_INPUT_FIRST	0x001000U
#define CCC_EC_FLAGS_ASN1_SIGNATURE		0x002000U
//
#define CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN	0x010000U
#define CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN	0x020000U
#define CCC_EC_FLAGS_PKEY_MULTIPLY		0x040000U
#if HAVE_LWPKA11_SUPPORT
#define CCC_EC_FLAGS_KEYSTORE			0x080000U /* EC keystore related operation */
#endif

#if !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES)
/* These are defined only for backwards compatibility,
 * no longer used by CCC. The CCC_E* names are used by
 * CCC code for Cert-C compatibility.
 */
#define EC_FLAGS_NONE			CCC_EC_FLAGS_NONE
#define EC_FLAGS_USE_PRESET_KEY	CCC_EC_FLAGS_USE_PRESET_KEY
#define EC_FLAGS_LEAVE_KEY_TO_KEYSLOT	CCC_EC_FLAGS_LEAVE_KEY_TO_KEYSLOT
#define EC_FLAGS_DYNAMIC_KEYSLOT	CCC_EC_FLAGS_DYNAMIC_KEYSLOT
#define EC_FLAGS_USE_KEYSLOT		CCC_EC_FLAGS_USE_KEYSLOT

#define EC_FLAGS_BIG_ENDIAN_KEY	CCC_EC_FLAGS_BIG_ENDIAN_KEY
#define EC_FLAGS_BIG_ENDIAN_DATA	CCC_EC_FLAGS_BIG_ENDIAN_DATA
#define EC_FLAGS_USE_PRIVATE_KEY	CCC_EC_FLAGS_USE_PRIVATE_KEY
#define EC_FLAGS_DIGEST_INPUT_FIRST	CCC_EC_FLAGS_DIGEST_INPUT_FIRST
#define EC_FLAGS_ASN1_SIGNATURE	CCC_EC_FLAGS_ASN1_SIGNATURE

#define EC_FLAGS_SCALAR_K_LITTLE_ENDIAN CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN
#define EC_FLAGS_SCALAR_L_LITTLE_ENDIAN CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN
#define EC_FLAGS_PKEY_MULTIPLY		CCC_EC_FLAGS_PKEY_MULTIPLY
#endif /* !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES) */

/** @brief PKA1 engine contexts have no special alignment requirements
 * (it does not support DMA memory accesses)
 */
typedef struct se_engine_ec_context {
	const struct engine_s *engine;
	struct context_mem_s  *cmem; /* context memory for this call chain or NULL */
	uint32_t         ec_flags;
	te_key_type_t    ec_keytype;
	uint32_t         ec_keyslot;
	te_crypto_algo_t algorithm;
	te_crypto_domain_t domain;

	uint8_t	*ec_pkey;		/**< lwrve specific private key value (if set, len is lwrve nbytes) */
	const uint8_t	*ec_k;			/**< k value (for point multiply and Shamir's trick) */
	const uint8_t	*ec_l;			/**< l value (for Shamir's trick) */
	uint32_t	 ec_k_length;		/**< k byte length */
	uint32_t	 ec_l_length;		/**< l byte length */
	uint32_t	 perf_usec;
#if 0
	bool		 ec_pubkey_verified;	// XXX Maybe a point property rather than separate here => FIXME
#endif
	struct te_ec_point_s   *ec_pubkey;

	const struct pka1_ec_lwrve_s *ec_lwrve;

#if CCC_SOC_WITH_LWPKA
	struct pka_engine_data pka_data; /**< Embed this object to the context in LWPKA. */
#else
	struct pka1_engine_data *pka1_data;
#endif

#if HAVE_LWPKA11_SUPPORT
	struct ec_ks_slot ec_ks;	/* ECC keystore setup */
#endif
} se_engine_ec_context_t;
/*@}*/

/**
 * @ingroup process_layer_api
 * @brief Context for EC operations
 */
/*@{*/
struct se_ec_context {
	te_crypto_alg_mode_t mode;

	te_crypto_algo_t md_algorithm; /**< ECDSA signature digest algo if required/given */

	// Engine level uses these fields
	se_engine_ec_context_t ec;
#if CCC_WITH_ED25519
	struct dom_context_s dom_context;
#endif
};
/*@}*/
#endif /* CCC_WITH_ECC */

#if HAVE_PKA1_TRNG
/** @brief Entropy generator (Elliptic CLP-800) in PKA1 device
 *
 * @ingroup engine_random
 */
/*@{*/
status_t engine_pka1_trng_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_pka1_trng_close_locked(void);
#endif
/*@}*/
#endif /* HAVE_PKA1_TRNG */

#if HAVE_RNG1_DRNG
/** @brief DRNG random number generator (Elliptic CLP-890) in RNG1 device
 * @ingroup engine_random
 */
/*@{*/
status_t engine_rng1_drng_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_rng1_drng_close_locked(void);
#endif
/*@}*/
#endif /* HAVE_RNG1_DRNG */

#if CCC_WITH_RSA
/**
 * @brief RSA engine layer items (PKA0 or PKA1)
 *
 * @ingroup engine_layer
 * @defgroup engine_rsa RSA engine layer
 */
/*@{*/
#define RSA_FLAGS_NONE			0x0000U
#define RSA_FLAGS_USE_PRESET_KEY	0x0001U
#define RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT	0x0002U
#define RSA_FLAGS_DYNAMIC_KEYSLOT	0x0004U	/**< driver selects how to set up the key */
#define RSA_FLAGS_USE_KEYSLOT		0x0008U	/**< Use the SE keyslot for key meterial */
//
#define RSA_FLAGS_BIG_ENDIAN_MODULUS	0x0100U
#define RSA_FLAGS_BIG_ENDIAN_EXPONENT	0x0200U
#define RSA_FLAGS_BIG_ENDIAN_DATA	0x0400U
#define RSA_FLAGS_USE_PRIVATE_KEY	0x0800U
#define RSA_FLAGS_DIGEST_INPUT_FIRST	0x1000U
#define RSA_FLAGS_DYNAMIC_SALT		0x2000U
#define RSA_FLAGS_KMAT_VALID		0x4000U

// Engine level uses these fields
typedef struct se_engine_rsa_context {
	const struct engine_s *engine;
	struct context_mem_s  *cmem; /* context memory for this call chain or NULL */
	uint32_t	 rsa_flags;
	te_crypto_algo_t algorithm;
	te_crypto_domain_t domain;
	uint32_t	 perf_usec;

	// RSA key access
	te_key_type_t	 rsa_keytype;
	uint32_t	 rsa_keyslot;	  /**< RSA keyslot 0..3 */
	uint32_t	 rsa_size;	  /**< RSA modulus in bytes == RSA key size == RSA private exp size */
	uint32_t	 rsa_pub_expsize; /**< RSA public exponent size in bytes */
	const uint8_t	*rsa_pub_exponent;
	const uint8_t	*rsa_modulus;
	const uint8_t	*rsa_priv_exponent; /**< HW expects size == rsa_size => so be it. */

#if CCC_SOC_WITH_LWPKA
	struct pka_engine_data pka_data; /**< Embed this object to the context in LWPKA. */
#else
	struct pka1_engine_data *pka1_data; /**< Some engines require
					     * preprocessing of values
					     * (e.g. PKA1), etc data.
					     * Engine specific values
					     * stored here (montgomery
					     * colwersion values for
					     * PKA1).
					     */
#endif /* CCC_SOC_WITH_LWPKA */

#if HAVE_OAEP_LABEL
	uint8_t  *oaep_label_digest;
#endif
} se_engine_rsa_context_t;
/*@}*/

/**
 * @brief Context for RSA operations
 * @ingroup  process_layer_api
 */
/*@{*/
struct se_rsa_context {
	/* First field in the object (alignment is smaller) */
	uint8_t  buf[ MAX_RSA_MODULUS_BYTE_SIZE ]; /**< Intermediate
						    * buffer for RSA
						    * pad and
						    * signature
						    * handling
						    */

	uint32_t used;			     /**< 0..sizeof(buf)-1, index to buf */

	te_crypto_alg_mode_t mode;

	bool	       is_encrypt;

	/* signature related fields */
	te_crypto_algo_t md_algorithm; /**< hash algorithm for OAEP or sig validation */

	uint32_t	 sig_slen;
	/**< SALT length in the RSA-PSS signature. This is a random
	 * value to make each signature unique even when signing the
	 * same digest value. The salt length in a signature can
	 * be set by the signer; common is to have salt length same as
	 * digest length. By default CCC determines the salt length
	 * from the signature and adapts to any valid salt length
	 * the signature has. See HAVE_DEFAULT_RSA_PSS_VERIFY_DYNAMIC_SALT
	 * in crypto_asig.c.
	 */

	// Engine-context level uses these fields
	se_engine_rsa_context_t ec;
};
/*@}*/

#if HAVE_SE_RSA
/**
 * @brief perform rsa modular exponentiation with PKA0
 *
 * @param input_params structure se_data_params
 * @param econtext rsa engine context
 *
 * @return NO_ERROR on success, error on failure
 *
 * @ingroup engine_rsa
 */
/*@{*/
status_t engine_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext);
/*@}*/

#if HAVE_CLOSE_FUNCTIONS
/*
 * @brief dummy function
 *
 * @ingroup engine_rsa
 */
/*@{*/
void engine_rsa_close_locked(void);
/*@}*/
#endif

/**
 * @brief SE PKA0 keyslot clear function.
 *
 * Internal function, clients should use the generic
 * se_clear_device_rsa_keyslot() function instead.
 *
 * @param engine ref to SE PKA0 engine
 * @param rsa_keyslot index of keyslot to clear [0..4] inclusive (PKA0 keyslot)
 * @param rsa_size key size in bytes
 * @return NO_ERROR on success, error code otherwise.
 */
status_t tegra_se_clear_rsa_keyslot_locked(const struct engine_s *engine,
					   uint32_t rsa_keyslot,
					   uint32_t rsa_size);

/**
 * @brief SE PKA0 keyslot write function.
 *
 * Internal function, clients should use the generic
 * se_set_device_rsa_keyslot() function instead.
 *
 * @param engine ref to SE PKA0 engine
 * @param rsa_keyslot index of keyslot to clear [0..4] inclusive (PKA0 keyslot)
 * @param rsa_expsize_bytes exponent size in bytes
 * @param rsa_size_bytes key size in bytes
 * @param exponent exponent buffer
 * @param modulus modulus buffer
 * @param exponent_big_endian true if exponent byte order is big endian
 * @param modulus_big_endian true if modulus byte order is big endian
 * @return NO_ERROR on success, error code otherwise.
 */
status_t rsa_write_key_locked(
	const struct engine_s *engine,
	uint32_t rsa_keyslot,
	uint32_t rsa_expsize_bytes, uint32_t rsa_size_bytes,
	const uint8_t *exponent, const uint8_t *modulus,
	bool exponent_big_endian, bool modulus_big_endian);
#endif /* HAVE_SE_RSA */
#endif /* CCC_WITH_RSA */

#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA
/** @brief Keyed SHA engine operations for T23X
 */
status_t engine_sha_keyop_process_locked(
	struct se_data_params *input_params,
	struct se_engine_sha_context *econtext);
#endif /* HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA */

/**
 * @ingroup engine_sha
 */
/*@{*/

/**
 * @brief perform SHA hash on given input
 *
 * @param input_params structure se_data_params
 * @param econtext SHA egine context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_sha_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_sha_context *econtext);
/*@}*/

#if !defined(HAVE_LONG_ASYNC_NAMES)
/* Be backwards compatible with long names by default in T19X.
 * Define this 0 in config file if long names are not used.
 *
 * Otherwise GVS issues for T19X compilations.
 */
#define HAVE_LONG_ASYNC_NAMES CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X)
#endif

#if HAVE_SE_ASYNC

#define ASYNC_FUNCTION_EXPOSE 1 /**< Make async api engine functions accessible */

#if HAVE_SE_ASYNC_SHA
/**
 * @ingroup engine_sha
 */
/*@{*/

/** @brief SHA engine process layer async state check
 *
 * @param ac Async SHA context
 * @param is_idle_p referenced object set true if SHA HW engine is done
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_sha_locked_async_done(const async_sha_ctx_t *ac, bool *is_idle_p);

/** @brief SHA engine process layer async (non-blocking) start function
 *
 * @param ac Async SHA context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_sha_locked_async_start(async_sha_ctx_t *ac);

/** @brief SHA engine process layer (blocking) finalize function
 *
 * @param ac Async SHA context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_sha_locked_async_finish(async_sha_ctx_t *ac);

#if HAVE_LONG_ASYNC_NAMES
/**
 * @brief Backwards compat exposed async function names for SHA
 *
 * Issue since not unique in first 31 chars. Only for backwards
 * compatibility. Use the provided shorter names in new code.
 *
 * Same as engine_sha_locked_async_done
 */
status_t engine_sha_process_locked_async_done(const async_sha_ctx_t *ac, bool *is_idle_p);

/** @brief same as engine_sha_locked_async_start
 */
status_t engine_sha_process_block_locked_async_start(async_sha_ctx_t *ac);

/** @brief same as engine_sha_locked_async_finish
 */
status_t engine_sha_process_block_locked_async_finish(async_sha_ctx_t *ac);
#endif /* HAVE_LONG_ASYNC_NAMES */
/*@}*/
#endif /* HAVE_SE_ASYNC_SHA */

/*
 * @ingroup engine_aes
 */
/*@{*/
#if HAVE_SE_ASYNC_AES
/** @brief AES engine process layer async state check
 *
 * @param ac Async AES context
 * @param is_idle_p referenced object set true if AES HW engine is done
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_aes_locked_async_done(const async_aes_ctx_t *ac, bool *is_idle_p);

#if HAVE_LONG_ASYNC_NAMES
/**
 * @brief Backwards compat exposed async function names for AES
 *
 * Issue since not unique in first 31 chars. Only for backwards
 * compatibility. Use the provided shorter names in new code.
 *
 * Same as engine_aes_locked_async_done
 */
status_t engine_aes_process_locked_async_done(const async_aes_ctx_t *ac, bool *is_idle_p);

/** @brief same as engine_aes_locked_async_start
 */
status_t engine_aes_process_block_locked_async_start(async_aes_ctx_t *ac);

/** @brief same as engine_aes_locked_async_finish
 */
status_t engine_aes_process_block_locked_async_finish(async_aes_ctx_t *ac);
#endif /* HAVE_LONG_ASYNC_NAMES */
#endif /* HAVE_SE_ASYNC_AES */

/*@}*/

#else

/**
 * @brief Do not expose engine async functions when not supporting async calls.
 */
#define ASYNC_FUNCTION_EXPOSE 0

#endif /* HAVE_SE_ASYNC */

#if HAVE_CLOSE_FUNCTIONS
/**
 * @ingroup engine_sha_support
 * @brief dummy function
 */
/*@{*/
void engine_sha_close_locked(void);
/*@}*/
#endif

/**
 * @ingroup engine_aes
 */
/*@{*/

#if ASYNC_FUNCTION_EXPOSE
/** @brief AES engine process layer async (non-blocking) start function
 *
 * The synchronous call is implemented by calling async_start/async_finish
 *  in a sequence.
 *
 * @param ac Async AES context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_aes_locked_async_start(async_aes_ctx_t *ac);

/** @brief AES engine process layer (blocking) finalize function
 *
 * @param ac Async AES context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_aes_locked_async_finish(async_aes_ctx_t *ac);
#endif /* ASYNC_FUNCTION_EXPOSE */

/**
 * @brief Perform specified AES operation on input data
 *
 * @param input_params structure se_data_params
 * @param econtext AES engine context
 *
 * @return NO_ERROR on success, error on failure
 */
status_t engine_aes_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_aes_close_locked(void);
#endif

#if HAVE_CMAC_AES
status_t engine_cmac_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_cmac_close_locked(void);
#endif
#endif /* HAVE_CMAC_AES */

#if HAVE_SE_GENRND
status_t engine_genrnd_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_genrnd_close_locked(void);
#endif
#elif HAVE_SE_RANDOM
status_t engine_rng0_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_rng0_close_locked(void);
#endif
#endif /* HAVE_SE_RANDOM */
/*@}*/

#if HAVE_SE_KAC_KEYOP
status_t engine_aes_keyop_process_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext);
#endif /* HAVE_SE_KAC_KEYOP */

/** @brief keyslot field/target selector
 */
typedef enum se_aes_kslot_sel_e {
	SE_AES_KSLOT_SEL_ORIGINAL_IV = 80,
	SE_AES_KSLOT_SEL_UPDATED_IV  = 90,
	SE_AES_KSLOT_SEL_KEY         = 95,
#if HAVE_AES_XTS
	SE_AES_KSLOT_SEL_SUBKEY      = 98,
#endif
#if HAVE_SOC_FEATURE_KAC
	SE_KSLOT_SEL_KEYBUF      = SE_AES_KSLOT_SEL_KEY, /**< Synonym even though HW differs */
	SE_KSLOT_SEL_KEYMANIFEST = 99,
	SE_KSLOT_SEL_KEYCTRL     = 100,
#endif
} se_aes_keyslot_id_sel_t;

/**
 * @ingroup engine_aes
 */
/*@{*/
/**
 * @brief Set the AES se key slot value (KEY or ORIGINAL_IV or UPDATED_IV)
 *
 * @param engine selected engine
 * @param keyslot SE key slot number
 * @param keysize_bits_param AES keysize in bits, ignored if writing IV
 * @param id_sel Specify if this entity is a key, original IV or updated IV.
 * Use id_sel to specify the type: * *_KEY for keys, *_ORIGINAL_IV
 * for original IV, *_UPDATED_IV for updated IV.
 * @param keydata Pointer to key data. Must be valid memory location with valid
 * data or NULL. If NULL set the keyslot entity to all zeroes.
 *
 * @return NO_ERROR or error in case of any error.
 */
status_t aes_write_key_iv_locked(
	const struct engine_s *engine,
	uint32_t keyslot, uint32_t keysize_bits_param,
	se_aes_keyslot_id_sel_t id_sel, const uint8_t *keydata);
/*@}*/

/**
 * @brief Perform se key slot clearing
 *
 * @param engine selected engine
 * @param keyslot keyslot to be cleared
 * @param clear_ivs false only if there is no risk in leaving IV values to keyslots
 * @return NO_ERROR if success, specific error if fails
 */
status_t tegra_se_clear_aes_keyslot_locked(const struct engine_s *engine,
					   uint32_t keyslot,
					   bool clear_ivs);

/** @brief SE HW mutex locks are now done by the upper level code calling the SE functions,
 * using the macros below (e.g. in crypto_process.c).
 *
 * All SE functions than must be called with HW mutex held are named as *_locked()
 * and the all "soft verify" that the mutex is locked when they are called (or assert).
 *
 * All of these functions requiring locking are now in tegra_se.c.
 * XXX Same with the PKA1 not only SE0; but those are not yet implemented ;-)
 *
 * These are now called via the engine code...
 *
 * @ingroup mutex_handling
 * @defgroup se_mutex SE APB HW mutex handlers
 */
/*@{*/
/**
 * @brief Lock SE APB HW mutex (called via engine mutex macros)
 *
 * R&D mode outputs a complaint if a SE HW mutex state variable is not 0
 * on call (i.e. mutex is not free at this point).
 *
 * Manage the global state variable hw_state for R&D purposes.
 *
 * Calls low level mutex locker: se0_get_mutex()
 * Lock the HW mutex guarding SE0 APB bus access. Loop until mutex
 * locked.  Set the watchdog counter for releasing the engine in case
 * the mutex is held too long.
 *
 * Timeout depends on engine clock speed: @600 MHz it is about 1.6 ms.
 * Default timeout value is the largest possible timeout value unless
 * overridden by sybsystem compilation flag:
 * SE0_MUTEX_WATCHDOG_TIMEOUT_VALUE).
 *
 * For systems that do not support mutex locks this call is a NOP when
 * HAVE_SE_MUTEX is defined 0.
 *
 * @param engine selected engine
 * @return NO_ERROR on success, error code otherwise.
 */
status_t tegra_se0_mutex_lock(const struct engine_s *engine);

/**
 * @brief Release SE APB HW mutex
 *
 * Calls low level mute release: se0_release_mutex()
 * For systems that do not support mutex locks this call is a NOP when
 * HAVE_SE_MUTEX is defined 0.
 *
 * @param engine selected engine
 */
void tegra_se0_mutex_release(const struct engine_s *engine);
/*@}*/

/**
 * @brief System specific engine setup/lock macros called at different steps on HW access.
 *
 * The following macros are used (mainly) by the macros
 * HW_MUTEX_TAKE_ENGINE/HW_MUTEX_RELEASE_ENGINE.
 *
 * The pair SW_MUTEX_TAKE/SW_MUTEX_RELEASE are also used in other places
 * to protect variable access in an SMP system (lwrrently only in crypto_context.c).
 *
 * @ingroup eng_switch_api
 * @defgroup mutex_handling Mutex handling macros
 */
/*@{*/
/** @def SW_MUTEX_TAKE
 *
 * @brief SW mutex acquire is called to handle e.g. SMP systems
 * (like Trusty) to lock out other OS instances. This is called first
 * to create an SW lock to access the HW.  HW does not need to be
 * running when this is called.
 */
#if !defined(SW_MUTEX_TAKE) || defined(DOXYGEN)
#define SW_MUTEX_TAKE
#endif

/** @def SW_MUTEX_RELEASE
 *
 * @brief SW mutex release
 */
#if !defined(SW_MUTEX_RELEASE) || defined(DOXYGEN)
#define SW_MUTEX_RELEASE
#endif

/** @brief SYSTEM_CRYPTO_INIT macro called after the SW_MUTEX_TAKE is called,
 * but before any HW access. E.g. turn on clocks for the system, so that the HW locks can be grabbed next.
 *
 * After this macro the HW mutex lock for the device is grabbed in the macro HW_MUTEX_TAKE_ENGINE
 * by the mutex acquire/release calls which are engine specific functions in the
 * tegra_cengine table defined in tegra_cdev.c
 */
#ifndef SYSTEM_CRYPTO_INIT
#define SYSTEM_CRYPTO_INIT(engine)
#endif

/** @brief This macro is called before SW_MUTEX_RELEASE is called to undo SYSTEM_CRYPTO_INIT.
 */
#ifndef SYSTEM_CRYPTO_DEINIT
#define SYSTEM_CRYPTO_DEINIT(engine)
#endif

/** @brief After the HW mutex has been grabbed the SYSTEM_ENGINE_RESERVE(engine) macro is called.
 * This can be used to do system specific setup (e.g. setting SMMU bypasses or what ever
 * is required by the system). When the engine is getting released, the SYSTEM_ENGINE_RELEASE(engine)
 * is called first (HW mutex still held).
 */
#ifndef SYSTEM_ENGINE_RESERVE
#define SYSTEM_ENGINE_RESERVE(engine)
#endif

/** @brief This macro is called before HW mutex released to undo SYSTEM_ENGINE_RESERVE.
 */
#ifndef SYSTEM_ENGINE_RELEASE
#define SYSTEM_ENGINE_RELEASE(engine)
#endif

/* Note: Mutex macros defined in tegra_cdev_ecall.h
 */

/** @brief Added SW mutex (if defined) to keep keep out trusty in other CPUs accessing SE
 * while used by this CPU instance (in an SMP system, like Trusty).
 *
 * Does not prevent any TA's (like lwcrypto) or other TZ code from accessing
 * the crypto engines => will need to remove permissions to access from lwcrypto and other parties...
 * See crypto_system_config.h.
 *
 * The functionality can be modified with the system dependent macros used by these
 * macros. These top level macros are defined here to keep the sequence consistent.
 */
#ifndef HW_MUTEX_TAKE_ENGINE
/**
 * @def HW_MUTEX_TAKE_ENGINE(engine)
 * @brief Lock access to crypto device containing engine
 *
 * Change at 20190911: Mutex acquire returns status, this macro
 * now uses error call CCC_ERROR_CHECK to handle it.
 *
 * This change can not be made default before subsystems are updated;
 *
 * Some subsystems using the async algorithms or directly access CCC
 * engine functions do use there macros in their subsystem code for
 * locking the mutexes. In case they do not have compatible error
 * handling compilation would fail.
 *
 * This can be fixed by (please prefer #1):
 *
 * 1) adding compatible error handling (i.e. fail: label handling
 *    errors and status_t ret variable).
 *
 * or
 *
 * 2) define HAVE_MUTEX_LOCK_STATUS 0 which ignores the status code
 *    returned from mutex acquire function.
 *
 * @param engine lock device containing this engine
 */
#if HAVE_MUTEX_LOCK_STATUS
static inline status_t hw_mutex_take_engine_m(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;

	SW_MUTEX_TAKE;
	SYSTEM_CRYPTO_INIT(engine);
	ret = DO_HW_MUTEX_ENGINE_LOCK(engine);
	if (NO_ERROR != ret) {
		SYSTEM_CRYPTO_DEINIT(engine);
		SW_MUTEX_RELEASE;
		goto fail;
	}
	SYSTEM_ENGINE_RESERVE(engine);
fail:
	return ret;
}

#define HW_MUTEX_TAKE_ENGINE(engine)					\
	{ ret = hw_mutex_take_engine_m(engine);				\
	  CCC_ERROR_CHECK(ret, LTRACEF("mutex lock failed: %d\n", ret));\
	}

#else

static inline void hw_mutex_take_engine_m(const struct engine_s *engine)
{
	SW_MUTEX_TAKE;
	SYSTEM_CRYPTO_INIT(engine);
	(void)DO_HW_MUTEX_ENGINE_LOCK(engine);
	SYSTEM_ENGINE_RESERVE(engine);
}

#define HW_MUTEX_TAKE_ENGINE(engine) hw_mutex_take_engine_m(engine)

#endif /* HAVE_MUTEX_LOCK_STATUS */
#endif /* HW_MUTEX_TAKE_ENGINE */

#ifndef HW_MUTEX_RELEASE_ENGINE
static inline void hw_mutex_release_engine_m(const struct engine_s *engine)
{
	SYSTEM_ENGINE_RELEASE(engine);
	DO_HW_MUTEX_ENGINE_RELEASE(engine);
	SYSTEM_CRYPTO_DEINIT(engine);
	SW_MUTEX_RELEASE;
}

/**
 * @def HW_MUTEX_RELEASE_ENGINE(engine)
 * @brief Allow access to crypto device containing engine
 * (locked previously by the caller)
 *
 * @param engine Release held locks for device containing this engine
 */
#define HW_MUTEX_RELEASE_ENGINE(engine) hw_mutex_release_engine_m(engine)

#endif
/*@}*/

#endif /* INCLUDED_TEGRA_SE_H */
