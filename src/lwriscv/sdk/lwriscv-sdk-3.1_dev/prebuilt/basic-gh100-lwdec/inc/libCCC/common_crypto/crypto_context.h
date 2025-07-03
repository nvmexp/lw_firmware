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
 * @file   ccc_context_api.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * Definitions for common crypto code.
 *
 * These are used by CCC internally but also required
 * when CCC is linked to the subsystem as a library.
 */
#ifndef INCLUDED_CCC_CONTEXT_API_H
#define INCLUDED_CCC_CONTEXT_API_H

#ifndef CCC_SOC_FAMILY_DEFAULT
/* Some subsystems do not include the CCC config file before including
 * other CCC headers; normally crypto_ops.h. So include the config
 * here unless the soc family is defined.
 */
#include <crypto_system_config.h>
#endif

#include <context_mem.h>

/** @brief ORed bit field bit values for context state.
 *
 * Most algorithms require at least the INITIALIZED bit and if
 * algorithm requires a KEY also requires KEY_SET bit to be on before
 * update/dofinal can be called.
 *
 * After ALLOCATE or FREE => object state forced to RESET; after this it can be
 *  reused for any supported operation of same or different class.
 *
 * @ingroup crypto_context_api_args
 * @defgroup context_state context state bits
 */
/*@{*/
#define TE_HANDLE_FLAG_RESET		       0x00000U /**< reset state => forced value after ALLOCATE or FREE */
#define TE_HANDLE_FLAG_PERSISTENT	       0x10000U /**< Unused now; for future use */
#define TE_HANDLE_FLAG_INITIALIZED	       0x20000U /**< when state == RESET => or_bit(INITIALIZED) */
#define TE_HANDLE_FLAG_KEY_SET		       0x40000U /**< when state == INITIALIZED => or_bit(KEY_SET) */
#define TE_HANDLE_FLAG_TWO_KEYS		       0x80000U /**< for e.g. XTS mode using two keys (add SW XTS support...) */
#define TE_HANDLE_FLAG_ERROR		       0x00001U /**< Context is in error state */
#define TE_HANDLE_FLAG_FINALIZED	       0x00002U /**< multi-step op completed (do_final done) */

#if 0

// UNUSED => delete if not needed

/* FIXME...
 * NOTE: May need to add restrictions for TA use of pre-existing SE keys.
 * "Non-privileged secure TAs" should only have access to "dynamic keys", i.e.
 * those keys it loads for its own ops. My "dynamic task loaded" code commits
 * support some such authorization mechanisms by manifest, but they are not yet merged!
 */
#define TE_KEY_FLAG_NONE	0x0000U /* no key flags set */
#define TE_KEY_FLAG_NO_KEY	0x0001U	/* algorithm does not use a key */
#define TE_KEY_FLAG_PRESET	0x0002U	/* key preset to slot, key used as is */
#define TE_KEY_FLAG_CIPHERED	0x0004U	/* key encrypted XXX how to open it
					 * (e.g. decipher via a "predefined key slot key", etc...)
					 * This may work with AES keys, but (AFAIK) AES key decipher
					 * target can not be an RSA key slot???
					 * ==> even if RSA priv key decipher would allow targetting
					 *  RSA keyslots => we have only one keyslot for our use =>
					 *  no point in deciphering RSA keys to RSA slots
					 *  if the deciphering RSA key is in memory...
					 */
#endif // 0

typedef uint32_t te_crypto_handle_t; /**< Type of unique handle for the context */

/* context key type is identical with this (at least for now) */
typedef te_args_key_data_t te_crypto_key_t;
/*@}*/

/** @brief One per each crypto context requested by a TA (access via ta->als[n] als_head list)
 *  or for each active crypto operation from the kernel (OS).
 *
 * When doing kernel crypto ops via the top level API, one of these
 * is provided by the caller per operation (in this case not allocated by the SE driver).
 *
 * If your code uses the engine API directly then this context is not required.
 *
 * @ingroup context_layer_api
 * @defgroup context_api_async_macros Macros for context async API
 */
/*@{*/
#define CRYPTO_CONTEXT_MAGIX 0xfeedcafeU	/**< Magic number in context object */

#if HAVE_SE_ASYNC
/** @brief Context object async operation states */
typedef enum async_active_state_e {
	CONTEXT_ASYNC_STATE_ACTIVE_NONE = 0,
	CONTEXT_ASYNC_STATE_ACTIVE_UPDATE,
	CONTEXT_ASYNC_STATE_ACTIVE_DOFINAL
} async_active_state_t;

/** @def CONTEXT_ASYNC_GET_ACTIVE(cx)
 * @brief fetch the context async operation active state
 */
#define CONTEXT_ASYNC_GET_ACTIVE(cx) ((cx)->ctx_async_state)

/** @def CONTEXT_ASYNC_SET_ACTIVE(cx)
 * @brief Set the context async operation active state
 *
 * Only used by CCC core.
 */
#define CONTEXT_ASYNC_SET_ACTIVE(cx, as) ((cx)->ctx_async_state = (as))

#endif /* HAVE_SE_ASYNC */
/*@}*/

/** @brief Class layer definitions
 *
 * @ingroup class_layer_api
 * @defgroup class_layer Algorithm class types and operations
 */
/*@{*/
/*@}*/

/** @brief Algorithm classifications
 *
 * All algorithms are classified in exactly one of
 * these enum values by the @ref context_layer_api
 *
 * @ingroup class_layer_api
 * @defgroup class_layer_grouping Algorithm grouping to classes
 */
/*@{*/
typedef enum te_crypto_class_e {
	TE_CLASS_CIPHER = 0,	/**< Cipher algorithms (e.g. AES modes) */
	TE_CLASS_MAC,		/**< Message authentication codes (e.g. HMAC and CMAC) */
	TE_CLASS_AE,		/**< Authenticating encryption modes (e.g. AES-GCM) */
	TE_CLASS_DIGEST,	/**< Message digests (e.g. SHA-2 family digests) */
	TE_CLASS_ASYMMETRIC_CIPHER, /**< Asymmetric ciphers (e.g. RSA-OAEP) */
	TE_CLASS_ASYMMETRIC_SIGNATURE, /**< Asymmetric signatures (e.g. ED25519) */
	TE_CLASS_KEY_DERIVATION,       /**< Key derivations (e.g. ECDH) */
	TE_CLASS_RANDOM,	       /**< Random number generators (e.g. DRNG) */

	// T23X...
	TE_CLASS_KW,		       /**< T23X: Key wrapping (Key Wrap / Key Unwrap) */
} te_crypto_class_t;
/*@}*/

/**
 * @brief Values for the run_state_s object rs_select selector field.
 *
 * @ingroup class_layer_api
 * @defgroup class_object_selector Internal class state object union field selector
 */
/*@{*/
enum rs_selector_e {
	RUN_STATE_SELECT_NONE = 0x00000,
	RUN_STATE_SELECT_INIT = 0x0F0F0,
	RUN_STATE_SELECT_MD   = 0x0FE0F,
	RUN_STATE_SELECT_ACIP = 0x0EE1E,
	RUN_STATE_SELECT_CIP  = 0x0DE2D,
	RUN_STATE_SELECT_AE   = 0x0CE3C,
	RUN_STATE_SELECT_ASIG = 0x0BE4B,
	RUN_STATE_SELECT_DRV  = 0x0AE5A,
	RUN_STATE_SELECT_MAC  = 0x09E69,
	RUN_STATE_SELECT_RNG  = 0x08E78,
	RUN_STATE_SELECT_KWUW = 0x07E87
};

struct se_md_state_s;
struct se_acipher_state_s;
struct se_cipher_state_s;
struct se_ae_state_s;
struct se_asig_state_s;
struct se_derive_state_s;
struct se_mac_state_s;
struct se_rng_state_s;
struct se_kwuw_state_s;

struct run_state_s {
	union rs_object {
		/**< Only for object lifetime management */
		uint8_t		 *s_object;

		/**< For runtime types, guarded by selector */
		struct se_md_state_s	 *s_md;
		struct se_acipher_state_s *s_acipher;
		struct se_cipher_state_s *s_cipher;
		struct se_ae_state_s	 *s_ae;
		struct se_asig_state_s	 *s_asig;
		struct se_derive_state_s *s_drv;
		struct se_mac_state_s	 *s_mac;
		struct se_rng_state_s	 *s_rng;
		struct se_kwuw_state_s	 *s_kwuw;
	} rs;

	enum rs_selector_e rs_select;
};
/*@}*/

#if HAVE_CONTEXT_MEMORY
// XXX May need to use union types to avoid cert-c cast violations in the memory manager
// implementation in context_mem.c
// => split out struct cmem_desc_s types into small include file and inject here as well.
// Each of the pointers may need to be replaced with union { pointer cmem_xxx; struct xxx; }
//

struct context_mem_s {
	uint8_t *cmem_buf;
	uint32_t cmem_size;

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* DMA memory buffer in system with multiple memory types
	 *
	 * DMA buffers (CACHE_LINE aligned allocated from here, if provided)
	 */
	uint8_t *cmem_dma_buf;
	uint32_t cmem_dma_size;
#endif /* CCC_WITH_CONTEXT_DMA_MEMORY */

#if CCC_WITH_CONTEXT_KEY_MEMORY
	/* Secure memory buffer in system with multiple memory types
	 *
	 * Key objects allocated from here if set.
	 */
	uint8_t *cmem_key_buf;
	uint32_t cmem_key_size;
#endif /* CCC_WITH_CONTEXT_KEY_MEMORY */
};

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE
struct implicit_cmem_s {
	struct context_mem_s imp;

	/* 0U if unused, otherwise ctx_handle of the context using cmem_buffer
	 * (internal buffer reserved for that context)
	 */
	te_crypto_handle_t cmem_owner_ctx_handle;
};

/* For setting a caller buffer to a be the cmem used
 * implicitly (when a static page is not ok).
 */
status_t context_set_implicit_cmem_buffer(enum cmem_type_e cmtype,
					  uint8_t *buf_arg,
					  uint32_t blen_arg);
#endif /* HAVE_IMPLICIT_CONTEXT_MEMORY_USE */

#else /* not HAVE_CONTEXT_MEMORY */
struct context_mem_s {
	uint32_t unused_contents;
};
#endif /* HAVE_CONTEXT_MEMORY */

/**
 * @brief Context API object for holding the crypto operation state.
 *
 * This is the object used for the context API layer call context.
 * All CCC context API calls pass a reference to the subsystem
 * provided context object which is valid after it is set up to the
 * point it gets reset.
 *
 * Default configurations will automatically reset the context object
 * in an error condition. No new operations can not be done with the
 * context unless it is initalized again for <algorithm,mode> tupple.
 *
 * @ingroup crypto_context_api_args
 * @defgroup crypto_context_object CCC API context object
 */
/*@{*/

struct crypto_class_funs_s;

typedef struct crypto_context_s {
	uint32_t	     ctx_app_index;	/**< ID of the app owning this crypto context */
	te_crypto_handle_t   ctx_handle;	/**< context ID (unique in a TA context) */
	uint32_t	     ctx_handle_state;	/**< TE_HANDLE_FLAGs for this context */
	te_crypto_domain_t   ctx_domain;	/**< Domain of operations; address space of buffers.
						 * CCC in library mode => always TE_CRYPTO_DOMAIN_KERNEL
						 * Call from user space => TE_CRYPTO_DOMAIN_TA which is
						 * only supported if HAVE_USER_SPACE is nonzero.
						 */

	te_crypto_class_t    ctx_class;		/**< Operation class */
	te_crypto_algo_t     ctx_algo;		/**< Algorithm (for example TE_ALG_AES_CBC_NOPAD) */
	te_crypto_alg_mode_t ctx_alg_mode;	/**< Mode of algorithm (for example TE_ALG_MODE_DECRYPT) */

	uint32_t	     ctx_magic;		/**< For detecting inits of uninitialized contexts */

	/* Key material is a LARGE object so split out of this object. Keeps context object
	 * smaller for operations that do not need keys (e.g. digests).
	 */
	te_crypto_key_t	     *ctx_key;		/**< operation key info, if any */

	const struct crypto_class_funs_s *ctx_funs;	/**< crypto functions for this operation class */
	struct run_state_s	   ctx_run_state; /**< crypto op specific opaque runtime state */

#if HAVE_USER_MODE
#if defined(CCC_CONTEXT_NODE_T)
	/* Only chain these contexts in a list when the client is
	 * in a different address space and can not handle its
	 * own contexts
	 *
	 * This type and field is not used by CCC core, this is the only
	 * reference to both in CCC core files.
	 */
	CCC_CONTEXT_NODE_T     ctx_node;	/**< TA active context chain / unused in
						 * OS originated crypto ops (kernel/lib mode)
						 */
#endif /* defined(CCC_CONTEXT_NODE_T) */
#endif /* HAVE_USER_MODE */

#if HAVE_SE_ASYNC
	async_active_state_t ctx_async_state;	/**< State of the async call sequence */
#endif /* HAVE_SE_ASYNC */

	struct context_mem_s ctx_mem;		/** Contents used only with
						 * HAVE_CONTEXT_MEMORY
						 */
} crypto_context_t;
/*@}*/

/***************************************/

#if HAVE_USER_MODE
/**
 * @brief Setup CTX object for any crypto operation supported by CCC.
 *
 * The MODE is verified to be valid for the ALGO. The context
 * must be reset before it can be used for
 * Use this to setup a crypto context for the kernel or TA
 *
 * @param ctx Pre-existing context object will be initialized for the <algo,mode> tuple.
 * @param client_id Arbitrary client request identifier, not used by CCC.
 * @param mode Operation mode suitable requested for the algorithm.
 * @param algo Select one of the crypto algorithms identified in <crypto_api.h>
 * @param domain TE_CRYPTO_DOMAIN_KERNEL when I/O data from CCC address space.
 *  TE_CRYPTO_DOMAIN_TA only when data in a different address space
 *  (user space of operating system, requires HAVE_USER_SPACE 1)
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t crypto_context_setup(struct crypto_context_s *ctx, uint32_t client_id,
			      te_crypto_alg_mode_t mode, te_crypto_algo_t algo,
			      te_crypto_domain_t domain);

/**
 * @brief Reset the crypto context object state and release resources if any held.
 *
 * @param ctx Context object will be reset to initial setup state.
 */
void crypto_context_reset(struct crypto_context_s *ctx);

/**
 * @brief [USER_MODE] Reset and release (free) the context.
 *
 * This can not be called for KERNEL DOMAIN objects.
 *
 * @param ctx_p referenced object set NULL after release.
 */
void crypto_context_release(struct crypto_context_s **ctx_p);
#endif

/**
 * @brief This is the primary API for using the common crypto library
 * with any supported algorithms when CCC core is linked with the
 * subsystem code to provide crypto services.
 *
 * The context API functions are called by the subsystem
 * to setup an existing context, handle primitive operations
 * with that context and then reset the context.
 *
 * The context API dispatches handler calls to the @ref class_layer_api
 * functions configured for the algorithm class.
 *
 * The following call sequence should be used with the context API,
 * assuming arg object is set up before call to use
 * TE_OP_COMBINED_OPERATION, etc. Please refer to the CCC api document
 * for more information.
 *
 * - ret = crypto_kernel_context_setup(&context, 0U, TE_ALG_SHA256, TE_ALG_MODE_DIGEST);
 * - ret = crypto_handle_operation(&context, &arg);
 * - crypto_kernel_context_reset(&context);
 *
 * <b>ret</b> needs to be checked for errors, etc.
 *
 * @defgroup context_layer_api Context layer inbound API
 */
/*@{*/
/**
 * Setup CTX object for any crypto operation supported by CCC.
 * The MODE is verified to be valid for the ALGO.
 *
 * Shorthand to setup a kernel crypto context (CCC in library mode)
 * kernel_id is an informative 16 bit number for log messages; ORed with 0xFEED0000
 *
 * @param ctx Pre-existing context object will be initialized for the <algo,mode> tuple.
 * @param kernel_id Arbitrary client request identifier, not used by CCC.
 * @param mode Operation mode suitable requested for the algorithm.
 * @param algo Select one of the crypto algorithms identified in <crypto_api.h>
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t crypto_kernel_context_setup(struct crypto_context_s *ctx, uint32_t kernel_id,
				     te_crypto_alg_mode_t mode, te_crypto_algo_t algo);

/**
 * @brief Reset the crypto context object state and release resources if any held.
 *
 * Kernel context reset is identical to context reset.
 *
 * @param ctx Context object will be reset to initial setup state.
 */
void crypto_kernel_context_reset(struct crypto_context_s *ctx);

/**
 * @brief Main entry point for CCC supported algorithms using crypto
 * context.
 *
 * All CCC crypto context operations use this call. After initializing
 * a crypto context it can be used for the specified operation using
 * the provided arguments.
 *
 * This function is an operation dispatcher to the generic init,
 * setkey, update, dofinal, reset sequence handlers. Some of these
 * operations may be skipped, depending on the algorithm. E.g. there
 * is not key for the digests and the update operation is normally
 * optional (e.g. digests) and in some cases not allowed (e.g. RSA
 * operations).
 *
 * @param ctx Previously initialized CCC crypto context.
 * @param arg Crypto call arguments
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t crypto_handle_operation(struct crypto_context_s *ctx,
				 te_crypto_args_t *arg);
/*@}*/
#endif /* INCLUDED_CCC_CONTEXT_API_H */
