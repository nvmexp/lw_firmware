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
 * @file   tegra_cdev.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2015
 *
 * @brief CheetAh SE/PKA1/RNG1 engine switch.
 *
 * This file defines the engine switch objects and functions.
 */

/* All SE/PKA1 HW engine static configuration data should be set up in tegra_cdev.[hc]
 * files. If not => FIXME.
 */
#ifndef INCLUDED_TEGRA_CDEV_H
#define INCLUDED_TEGRA_CDEV_H

#ifndef CCC_SOC_FAMILY_DEFAULT
#include <crypto_system_config.h>
#endif

#include <include/crypto_api.h>
#include <tegra_cdev_ecall.h>

/** @brief Some subsystems refer to SE_CDEV_SE1 even
 * though they are not used in T21X systems.
 *
 * Avoid compilation issues with SE_CDEV_SE1 symbol in them by
 * defining the symbol but adding no other support for SE1.
 */
#ifndef HAVE_SE_CDEV_SE1_NEEDED_IN_COMPILE
#define HAVE_SE_CDEV_SE1_NEEDED_IN_COMPILE 1
#endif

#ifndef CCC_REG_ACCESS_DEFAULT
#define CCC_REG_ACCESS_DEFAULT 1
#endif

/**
 * @brief Number of phys devices known by the system.
 *
 * The numeric values of enum se_cdev_id_t are passed to device dependent code
 * for initializing the HW engines, looping 0..(MAX_CRYPTO_DEVICES-1)
 *
 * If some board does not have an engine, it needs to set base address to NULL and
 * just return OK. If there is an error, the code shoud just panic if disabling the
 * engine as suggested is not an option.
 *
 * The Trusty code as an example maps the engine base addresses and returns the
 * mapped device base address. For systems that do not use MMU, just return the phys
 * base address of the device as specified in the address map.
 */
#define MAX_CRYPTO_DEVICES 4U

#if defined(ADD_THIS_PROTOTYPE_TO_YOUR_INIT_CODE_AND_CALL_THIS_ONCE) || defined(NEED_TEGRA_INIT_CRYPTO_PROTO)
/**
 * @brief Subsystem must call the CCC setup function tegra_init_crypto_devices()
 * first once to initialize CCC.
 *
 * NOTE:
 * This function call must be the first call to CCC the subsystem
 * makes.  Otherwise CCC will not work properly.
 *
 * When this gets called by the subsystem it triggers calls to the
 * SUBSYSTEM provided function named with the macro SYSTEM_MAP_DEVICE
 * in subsystem CONFIG_FILE.
 *
 * CCC calls this device mapper macro once per known device from
 * tegra_init_crypto_devices()
 *
 * The function prototype matching the function name SYSTEM_MAP_DEVICE
 * is one of these:
 *
 * In older systems the prototype is defined in config file as:
 *   status_t SYSTEM_MAP_DEVICE(uint32_t dev_id, void **base);
 *
 * or when CCC_SYSTEM_MAP_DEVICE_ARGUMENT_NO_CASTING is defined
 * nonzero clearing the Misra concern caused by type casting
 * (the prototype is defined internally for CCC use in this case):
 *
 *   status_t SYSTEM_MAP_DEVICE(se_cdev_id_t dev_id, uint8_t **base);
 *
 * @defgroup ccc_initialization CCC initialization
 */
/*@{*/
status_t tegra_init_crypto_devices(void);

/* Prevent Misra issue with duplicate prototypes on some systems */
#define TEGRA_INIT_CRYPTO_PROTO_DECLARED
/*@}*/
#endif /* defined(ADD_THIS_PROTOTYPE_TO_YOUR_INIT_CODE_AND_CALL_THIS_ONCE) ||
	* defined(NEED_TEGRA_INIT_CRYPTO_PROTO)
	*/

/** @brief Engine class is a bitfield, one engine can support multiple operation classes
 *
 * @ingroup eng_switch_table
 * @defgroup eng_class Engine class bit vector
 */
/*@{*/
#define	CCC_ENGINE_CLASS_AES	0x0001U	/**< Supports normal AES functions */
#define	CCC_ENGINE_CLASS_RSA	0x0002U /**< Supports RSA */
#define	CCC_ENGINE_CLASS_EC	0x0004U /**< Supports EC operations */
#define	CCC_ENGINE_CLASS_SHA	0x0008U	/**< Supports SHA1, SHA2 (in T23X also SHA3 and HMAC) */
#define	CCC_ENGINE_CLASS_MD5	0x0010U /**< SW algo for R&D testing only */
#define	CCC_ENGINE_CLASS_DRNG	0x0020U /**< Supports DRNG (HW entropy seeded DRNG) */
#define	CCC_ENGINE_CLASS_CMAC_AES 0x0040U /**< AES engine supporting CMAC */
#define	CCC_ENGINE_CLASS_TRNG	  0x0080U /**< Supports TRNG (entropy generator) Should not use for SW */
#define	CCC_ENGINE_CLASS_RSA_4096 0x0100U /**< Supports long RSA keys (PKA for 3072 and 4096) */

#define	CCC_ENGINE_CLASS_X25519	 0x0200U /**< Supports X25519 (lwrve25519 ecdh) [PKA] */
#define	CCC_ENGINE_CLASS_ED25519 0x0400U /**< Supports ED25519 signatures [PKA] */
#define	CCC_ENGINE_CLASS_P521	 0x0800U /**< Supports P521 with dedicated functions [PKA] */

#define	CCC_ENGINE_CLASS_AES_XTS 0x1000U /**< [T19x and later] Supports AES-XTS */

#define CCC_ENGINE_CLASS_HMAC_SHA2 CCC_ENGINE_CLASS_SHA /**< [T23X] Same engine supports SHA and HMAC_SHA2 */
#define CCC_ENGINE_CLASS_KDF      0x2000U	/**< [T23X] Key derivation (one or two key KDF) */
#define CCC_ENGINE_CLASS_KW       0x4000U	/**< [T23X] Key wrap and unwrap AES1 engine */
#define CCC_ENGINE_CLASS_GENKEY   0x8000U	/**< [T23X] Generate keys to keyslots */
#define CCC_ENGINE_CLASS_GENRND   CCC_ENGINE_CLASS_DRNG /**< [T23X] SE AES0 GENRND, much like old SE DRNG */

#define	CCC_ENGINE_CLASS_SHA3	   0x10000U /**< SW only or HW SHA-3 engine (only one can be enabled for now) */
#define	CCC_ENGINE_CLASS_WHIRLPOOL 0x20000U /**< SW support for WHIRLPOOL (v3) */

#define	CCC_ENGINE_CLASS_ED448	   0x40000U /**< ED448 signature class [PKA] */

#define	CCC_ENGINE_CLASS_XMSS	   0x80000U /**< SW XMSS signature class */
#define	CCC_ENGINE_CLASS_EC_KS	   0x100000U /**< Supports EC KS operations */
/*@}*/

#if !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES)
/**
 * @brief Provide backwards compatible names; these may be used
 * in current subsystems for engine selection calls.
 *
 * Only provide backwards compatibility for names used in older
 * systems, subsystems should start using the new names.
 *
 * @ingroup certc_compat_names
 */
/*@{*/
#define	ENGINE_CLASS_AES	CCC_ENGINE_CLASS_AES		/**< backwards compatibility */
#define	ENGINE_CLASS_RSA	CCC_ENGINE_CLASS_RSA		/**< backwards compatibility */
#define	ENGINE_CLASS_EC		CCC_ENGINE_CLASS_EC		/**< backwards compatibility */
#define	ENGINE_CLASS_SHA	CCC_ENGINE_CLASS_SHA		/**< backwards compatibility */
#define	ENGINE_CLASS_DRNG	CCC_ENGINE_CLASS_DRNG		/**< backwards compatibility */
#define	ENGINE_CLASS_CMAC_AES	CCC_ENGINE_CLASS_CMAC_AES	/**< backwards compatibility */
#define	ENGINE_CLASS_TRNG	CCC_ENGINE_CLASS_TRNG		/**< backwards compatibility */
#define	ENGINE_CLASS_RSA_4096	CCC_ENGINE_CLASS_RSA_4096	/**< backwards compatibility */
#define	ENGINE_CLASS_X25519	CCC_ENGINE_CLASS_X25519		/**< backwards compatibility */
#define	ENGINE_CLASS_ED25519	CCC_ENGINE_CLASS_ED25519	/**< backwards compatibility */
#define	ENGINE_CLASS_AES_XTS	CCC_ENGINE_CLASS_AES_XTS	/**< backwards compatibility */
/*@}*/
#endif /* !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES) */

// XXX TODO => consider supporting
// CLONE and LOCK are operations for all SE keyslots. Keymover normally deals with these...
// Not sure yet if CCC needs to support these. Should be simple to add when/if required.
// INS is supported by CCC for key injection

/** @def CCC_ENGINE_CLASS_IS_EC(cl)
 *  @brief Returns true if CL bitmask contains EC class bits
 */
#define CCC_ENGINE_CLASS_IS_EC(cl)						\
	(((cl) & (CCC_ENGINE_CLASS_EC | CCC_ENGINE_CLASS_X25519 |		\
		  CCC_ENGINE_CLASS_ED25519 | CCC_ENGINE_CLASS_P521)) != 0U)

/** @def CCC_ENGINE_CLASS_IS_RSA(cl)
 *  @brief Returns true if CL bitmask contains RSA class bits
 */
#define CCC_ENGINE_CLASS_IS_RSA(cl)						\
	(((cl) & (CCC_ENGINE_CLASS_RSA | CCC_ENGINE_CLASS_RSA_4096)) != 0U)

typedef uint32_t engine_class_t; /**< engine class bit field type */

/**
 * @brief engine state set based on subsystem response to SYSTEM_MAP_DEVICE macro.
 *
 * @defgroup eng_switch_cdev_state State of device after SYSTEM_MAP_DEVICE
 */
/*@{*/
typedef enum se_cdev_state_e {
	SE_CDEV_STATE_NONEXISTENT = 0, /**< Device does not exist */
	SE_CDEV_STATE_OFF	  = 1, /**< Device is not on */
	SE_CDEV_STATE_ACTIVE	  = 2, /**< Device active (OK) */
	SE_CDEV_STATE_BLOCKED	  = 3, /**< Device blocked */
} se_cdev_state_t;
/*@}*/

/**
 * @brief One for each device, like LW SE engine 1, LW SE engine 2 and PKA
 */
typedef struct {
	const char  *cd_name; /**< device printable name */
	se_cdev_id_t cd_id; /**< device id */
	uint8_t	    *cd_base; /**< mapped base address */
	se_cdev_state_t cd_state; /**< device state */
} se_cdev_t;

/* Forward declared structures
 */
struct se_data_params;

/* Use type correct version for now, using uint_8 reduces the table
 * size to half.
 *
 * Note that the enum values are defined so that the colwersion
 * to uint8_t will not cause value overlaps.
 */
#define CENGINE_TABLE_ENUMS 1

#if CENGINE_TABLE_ENUMS
typedef enum ft_m_lock_e tegra_mutex_lock_proc_t;
typedef enum ft_m_unlock_e tegra_mutex_release_proc_t;

typedef enum ft_sha_e engine_sha_t;

typedef enum ft_aes_e engine_aes_t;

typedef enum ft_rsa_e engine_rsa_t;

typedef enum ft_sha_poll_e engine_sha_async_done_t;
typedef enum ft_sha_async_e engine_sha_async_start_t;
typedef enum ft_sha_async_e engine_sha_async_finish_t;

typedef enum ft_aes_poll_e engine_aes_async_done_t;
typedef enum ft_aes_async_e engine_aes_async_start_t;
typedef enum ft_aes_async_e engine_aes_async_finish_t;
typedef enum ft_ec_e engine_ec_t;
typedef enum ft_mod_e engine_mod_t;
typedef enum ft_rng_e engine_rng_t;

#else

/* WIP feature for now. This compiles and works, but enums are now
 * assigned to uint8_t and vice versa. Compiler does not care, but
 * some misra checkers probably will => this is WIP for checking
 * size reductions!!!
 */
typedef uint8_t tegra_mutex_lock_proc_t;
typedef uint8_t tegra_mutex_release_proc_t;

typedef uint8_t engine_sha_t;

typedef uint8_t engine_aes_t;

typedef uint8_t engine_rsa_t;

typedef uint8_t engine_sha_async_done_t;
typedef uint8_t engine_sha_async_start_t;
typedef uint8_t engine_sha_async_finish_t;

typedef uint8_t engine_aes_async_done_t;
typedef uint8_t engine_aes_async_start_t;
typedef uint8_t engine_aes_async_finish_t;
typedef uint8_t engine_ec_t;
typedef uint8_t engine_mod_t;
typedef uint8_t engine_rng_t;
#endif /* CENGINE_TABLE_ENUMS */

/** @brief Engine layer locked functions to call for the engine operations,
 * NULL if not defined for this engine.
 *
 * The function handlers are in @ref eng_switch_process_object object.
 *
 * Pre-requisites have to be set up before these get called, e.g. mutexes need
 * to be locked (if required) and the parameter objects initialized.
 *
 * By coding convention, these fields should only be assigned
 * the *_locked() function handlers.
 *
 * @ingroup eng_switch_api
 * @defgroup eng_switch_process_handlers Engine operation handler entry into engine layer
 */
/*@{*/
/*@}*/


/**
 * @brief struct engine_s field e_proc type defining engine layer handlers
 *
 * @ingroup eng_switch_process_handlers
 * @defgroup eng_switch_process_object Engine object e_proc object type
 *
 * @struct engine_switch_proc_s
 */
/*@{*/
/*@}*/

struct engine_switch_proc_s {
#if HAVE_SE_AES
	/**
	 * @brief Engine switch entries to AES engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_aes_ops Engine switch AES operations
	 */
	/*@{*/
	engine_aes_t p_aes; /**< AES handler */
	engine_aes_t p_cmac; /**< CMAC-AES handler */
	/*@}*/
#if HAVE_SE_ASYNC_AES
	/**
	 * @brief Engine switch entries to async AES engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_aes_async_ops Engine switch AES async operations
	 */
	/*@{*/
	engine_aes_async_done_t p_aes_async_done; /**< AES async check handler */
	engine_aes_async_start_t p_aes_async_start; /**< AES async start handler */
	engine_aes_async_finish_t p_aes_async_finish; /**< AES blocking finalize handler */
	/*@}*/
#endif
#if HAVE_SE_KAC_KEYOP
	/**
	 * @brief Engine switch entries to [T23X] key wrap/unwrap engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_aes_kwuw_ops Engine switch key wrap/unwrap operations
	 */
	/*@{*/
	engine_aes_t p_aes_kwuw; /**< [T23X] key wrap/unwrap handler */
	/*@}*/
#endif
#endif /* HAVE_SE_AES */

	/**
	 * @brief Engine switch entries to SHA engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_sha_ops Engine switch SHA operations
	 */
	/*@{*/
#if HAVE_SE_SHA
	engine_sha_t p_sha; /**< SHA handler */
#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA
	engine_sha_t p_sha_keyop; /**< Keyed SHA engine operation (KDF or HW HMAC_SHA) */
#endif
	/*@}*/

#if HAVE_SE_ASYNC_SHA
	/**
	 * @brief Engine switch entries to async SHA engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_sha_async_ops Engine switch SHA async operations
	 */
	/*@{*/
	engine_sha_async_done_t p_sha_async_done; /**< SHA async check handler */
	engine_sha_async_start_t p_sha_async_start; /**< SHA async start handler */
	engine_sha_async_finish_t p_sha_async_finish; /**< SHA blocking finalize handler */
	/*@}*/
#endif /* HAVE_SE_ASYNC_SHA */
#endif /* HAVE_SE_SHA */

#if CCC_WITH_RSA
	/**
	 * @brief Engine switch entries to PKA modular exponentiation engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_mod_exp_ops Engine switch modular exponentiation operations
	 */
	/*@{*/
	engine_rsa_t p_rsa_exp; /**< modular exponentiation handler */
	/*@}*/
#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECC
	/**
	 * @brief Engine switch entries to PKA elliptic lwrve point operations engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_point_ops Engine switch EC point operations
	 */
	/*@{*/
	engine_ec_t p_ec_p_mult; /**< EC point multiply */
	engine_ec_t p_ec_p_add; /**< EC point add */
	engine_ec_t p_ec_p_double; /**< EC point double */
	engine_ec_t p_ec_p_verify; /**< EC verify point on lwrve */
	engine_ec_t p_ec_p_shamir_trick; /**< EC shamir's trick */
#if HAVE_LWPKA11_SUPPORT
	engine_ec_t p_ec_ks_ecdsa_sign; /**< ECDSA sign with keystore key */
	engine_ec_t p_ec_ks_keyop; /**< EC keystore key operation */
#endif
	/*@}*/
#endif

#if CCC_WITH_MODULAR
#if CCC_WITH_ED25519 && CCC_WITH_ECC
	/**
	 * @brief Special purpose Lwrve25519 operarations; the modulus value must be p=2^255-19
	 * in D0 bank register; using Euler's algorithms.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_c25519_mod_ops Engine switch lwrve25519 modular operations
	 */
	/*@{*/
	engine_mod_t p_mod_c25519_exp; /**< lwrve25519 mod exp */
	engine_mod_t p_mod_c25519_sqr; /**< lwrve25519 square */
	engine_mod_t p_mod_c25519_mult; /**< lwrve25519 multiply */
	/*@}*/
#endif

	/**
	 * @brief Engine switch entries to PKA modular operation engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_mod_ops Engine switch modular operations
	 */
	/*@{*/
	engine_mod_t p_mod_multiplication; /**< modular multiply */
	engine_mod_t p_mod_addition; /**< modular add */
	engine_mod_t p_mod_subtraction;  /**< modular subtract */
	engine_mod_t p_mod_reduction; /**< modular reduce */
	engine_mod_t p_mod_division;  /**< modular division */
	engine_mod_t p_mod_ilwersion; /**< modular ilwersion */
	engine_mod_t p_mod_bit_reduction; /**< modular bit reduce */
	engine_mod_t p_mod_bit_reduction_dp; /**< modular bit reduce double precision */
	/*@}*/

#if HAVE_GEN_MULTIPLY
	/**
	 * @brief Engine switch entries to PKA generic double-precision multiply engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_gen_mul_ops Engine switch generic multiply
	 */
	/*@{*/
	engine_mod_t p_gen_multiplication; /**< Non-modular(!) multiplication */
	engine_mod_t p_mod_gen_multiplication; /**< Non-modular(!) multiplication immediatelly followed
						* by DP bit serial reduction
						*/
	/*@}*/
#endif /* HAVE_GEN_MULTIPLY */

#if HAVE_P521 && CCC_WITH_ECC
	/**
	 * @brief Engine switch entries to PKA modular multiply NIST-P521 lwrve engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_p521_mul_ops Engine switch multiply operations for NIST-P521 lwrve
	 */
	/*@{*/
	engine_mod_t p_mod_mult_m521; /**< P521 multiply */
	engine_mod_t p_mod_mult_mont521; /**< P521 montgomery multiply */
	/*@}*/
#endif
#endif /* CCC_WITH_MODULAR */

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG
	/**
	 * @brief Engine switch entries to SE/PKA random number generator engine layer code.
	 *
	 * @ingroup eng_switch_process_object
	 * @defgroup eng_switch_ec_rng_ops Engine switch rng operations
	 */
	/*@{*/
	engine_rng_t p_rng; /**< DRNG/TRNG operations */
	/*@}*/
#endif
};

/**
 * @brief Main engine object type for CCC core.
 *
 * Each engine belongs to a device if it has HW support; otherwise
 * e_dev refers to @ref cdev_software implying there is no HW support
 * for the engine.
 *
 * @defgroup eng_engine_type Engine object type
 */
/*@{*/
typedef struct engine_s {
	const char	*e_name; /**< engine printable name */
	const se_cdev_t *e_dev; /**< link to immutable device which contains this engine */
	engine_id_t	 e_id; /**< engine id */
	engine_class_t   e_class; /**< bit vector of engine capabilities */

	// engine register range for this engine
	uint32_t	 e_regs_begin; /**< engine register offset region start */
	uint32_t	 e_regs_end;   /**< engine register offset region end */

	bool		  e_do_remap; /**< Now supports a single range of register range to be
				       * mapped by a fixed offset. This is only done if e_remap is true.
				       *
				       * If more mappings required, make this a table and loop it.
				       *
				       * Used for mapping a range of AES0 registers (0x200..0x1ff) to
				       * AES1 (0x400..0x4ff)
				       *
				       * Remapping is used to be able to use the Lwpu DRF macros
				       * with AES0 engine register field names. If the
				       * actual access is using AES1 engine but the AES0
				       * names in the DRF macros => the e_do_remap
				       * will cause the numeric AES0 offsets to get mapped
				       * to AES1 range.
				       *
				       * This requires that the HW folks design the units
				       * so that offsets to any AES0 and AES1 from the beginning
				       * of the region is fixed. This is true for current
				       * SE engines, including T23X.
				       */
	struct {
		uint32_t rm_start; /**< engine offset remapper start offset (only if e_do_remap) */
		uint32_t rm_end;   /**< engine offset remapper end offset (only if e_do_remap) */
	} e_remap_range;

	struct engine_switch_proc_s e_proc; /**< Function handlers: entries to engine layer */

	struct {
		/**
		 * @brief Engine switch entries for HW APB mutex lock functions
		 *
		 * @defgroup eng_switch_ec_mutex_ops Engine switch HW mutex operations
		 * @ingroup eng_engine_type
		 */
		/*@{*/
		tegra_mutex_lock_proc_t m_lock;    /**< device HW mutex lock function for the engine */
		tegra_mutex_release_proc_t m_unlock;  /**< device HW mutex unlock function for the engine */
		/*@}*/
	} e_mutex;
} engine_t;
/*@}*/

/**
 * @brief Engine switch layer: interface from process layer to engine
 * layer.
 *
 * The synchronous operation handlers configured to the engine switch
 * are engine layer "locked functions". See @ref eng_switch_process_handlers
 *
 * As a naming convention CCC functions that should be called with the
 * APB mutex held are named as *FUNCTION_locked* and the mutex
 * APB should be held on call.
 *
 * The process layer manages the APB mutex before each engine switch
 * synchronous operation call with the macros (defined in <tegra_se.h>
 * unless redefined by subsystem). These macros find the correct device
 * mutex lock/unlock functions from the engine object:
 *
 * - HW_MUTEX_TAKE_ENGINE(eng) => eng->m_lock
 * - HW_MUTEX_RELEASE_ENGINE(eng) => eng->m_unlock
 *
 * Note that the APB mutex locks the <b>device</b> not the engine in
 * the device; there can be more than one engine in a single device.
 *
 * After taking the mutex the caller can use one of the calls in
 * @ref eng_switch_access to access engine layer via engine switch.
 *
 * @defgroup eng_switch_api Engine switch
 */
/*@{*/
/*@}*/

/**
 * @brief Engine switch access methods
 *
 * @ingroup eng_switch_api
 * @defgroup eng_switch_access Engine switch access
 */
/*@{*/
#if HAVE_SE_SHA

struct se_engine_sha_context;

/** @brief SHA engine call ops */
enum ecall_sha_variant_e {
	CDEV_ECALL_NONE,
	CDEV_ECALL_SHA = 0x10,
	CDEV_ECALL_SHA_KEYOP,
};

/**
 * @brief call the engine layer SHA functions via the engine switch.
 *
 * Process layer calls this function with the macros related to the SHA engines
 * to access the engine layer via the engine switch:
 * - CCC_ENGINE_SWITCH_SHA
 * - CCC_ENGINE_SWITCH_KDF [T23X]
 * - CCC_ENGINE_SWITCH_HMAC [T23X]
 *
 * T23X support keyed-sha operations in the HW engine. Such keyed operations
 * are e.g. HMAC-SHA2 and KDF (key derivations implemented with HMAC-SHA).
 *
 * In T18X/T19X the SHA engine does not support keyed operations, but
 * such keyed digest operations can be dealt with a combination of SW
 * and HW.
 *
 * CCC supports such "HYBRID operation mode" for some algorithms, like
 * HMAC-SHA by SW deriving the HMAC ipad/opad values and key wrapping
 * using the HW SHA engine for deriving these and also for all
 * bulk data processing.
 *
 * @param engine Selected SHA engine
 * @param input data parameters
 * @param econtext sha engine context
 * @param evariant SHA engine call variant
 * @return NO_ERROR on success error code otherwise.
 */
status_t engine_method_sha_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   struct se_engine_sha_context *econtext,
					   enum ecall_sha_variant_e evariant);

/** @brief SHA engine call => SHA digest
 *
 * @def CCC_ENGINE_SWITCH_SHA(eng, in_params, ectx)
 */
#define CCC_ENGINE_SWITCH_SHA(eng, in_params, ectx)			\
	engine_method_sha_econtext_locked(eng, in_params, ectx, CDEV_ECALL_SHA)

/** @brief SHA engine call => [T23X] HW KDF 1-KEY or 2-KEY derivations
 *
 * @def CCC_ENGINE_SWITCH_KDF(eng, in_params, ectx)
 */
#define CCC_ENGINE_SWITCH_KDF(eng, in_params, ectx)			\
	engine_method_sha_econtext_locked(eng, in_params, ectx, CDEV_ECALL_SHA_KEYOP)

/** @brief SHA engine call => [T23X] HW HMAC-SHA2
 *
 * @def CCC_ENGINE_SWITCH_HMAC(eng, in_params, ectx)
 */
#define CCC_ENGINE_SWITCH_HMAC(eng, in_params, ectx)		\
	engine_method_sha_econtext_locked(eng, in_params, ectx, CDEV_ECALL_SHA_KEYOP)
#endif /* HAVE_SE_SHA */

#if HAVE_SE_AES

struct se_engine_aes_context;

/** @brief AES engine call ops */
enum ecall_aes_variant_e {
	CDEV_ECALL_AES_NONE,
	CDEV_ECALL_AES = 0x20,
	CDEV_ECALL_AES_MAC,
	CDEV_ECALL_AES_KWUW,
};

status_t engine_method_aes_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   struct se_engine_aes_context *econtext,
					   enum ecall_aes_variant_e evariant);

/* These calls use the SE AES engines, passing the aes engine context
 */
#define CCC_ENGINE_SWITCH_AES(eng, in_params, ectx)			\
	engine_method_aes_econtext_locked(eng, in_params, ectx, CDEV_ECALL_AES)
#define CCC_ENGINE_SWITCH_CMAC(eng, in_params, ectx)		\
	engine_method_aes_econtext_locked(eng, in_params, ectx, CDEV_ECALL_AES_MAC)
#define CCC_ENGINE_SWITCH_KWUW(eng, in_params, ectx)		\
	engine_method_aes_econtext_locked(eng, in_params, ectx, CDEV_ECALL_AES_KWUW)
#endif /* HAVE_SE_AES */

#if CCC_WITH_RSA

struct se_engine_rsa_context;

/** @brief RSA engine call ops */
enum ecall_rsa_variant_e {
	CDEV_ECALL_RSA_NONE,
	CDEV_ECALL_RSA_EXP = 0x30,
};

/* CDEV method calls, called by CCC_ENGINE_SWITCH_*() macros.
 */
status_t engine_method_rsa_exp_econtext_locked(const engine_t *engine,
					       struct se_data_params *input,
					       struct se_engine_rsa_context *econtext,
					       enum ecall_rsa_variant_e evariant);

/* Rsa context for rsa modular exponentiation operation
 */
#define CCC_ENGINE_SWITCH_RSA(eng, in_params, ectx)			\
	engine_method_rsa_exp_econtext_locked(eng, in_params, ectx, CDEV_ECALL_RSA_EXP)
#endif /* CCC_WITH_RSA */

#if CCC_WITH_DRNG || HAVE_PKA1_TRNG

struct se_engine_rng_context;

/** @brief RNG engine call ops */
enum ecall_rng_variant_e {
	CDEV_ECALL_RNG_NONE,
	CDEV_ECALL_RNG = 0x40,
};

status_t engine_method_rng_econtext_locked(const engine_t *engine,
					   struct se_data_params *input,
					   const struct se_engine_rng_context *econtext,
					   enum ecall_rng_variant_e evariant);

#define CCC_ENGINE_SWITCH_RNG(eng, in_params, ectx)			\
	engine_method_rng_econtext_locked(eng, in_params, ectx, CDEV_ECALL_RNG)
#endif /* CCC_WITH_DRNG || HAVE_PKA1_TRNG */

#if HAVE_SE_ASYNC

/** @brief AES and SHA async operations
 */
enum ecall_async_variant_e {
	CDEV_ECALL_ASYNC_NONE,
	CDEV_ECALL_ASYNC_CHECK = 0x50,
	CDEV_ECALL_ASYNC_START,
	CDEV_ECALL_ASYNC_FINISH,
};

#if HAVE_SE_ASYNC_SHA

struct async_sha_ctx_s;

status_t engine_method_async_sha_check_actx_locked(const engine_t *engine,
						   const struct async_sha_ctx_s *actx,
						   bool *is_done,
						   enum ecall_async_variant_e evariant);

status_t engine_method_async_sha_actx_locked(const engine_t *engine,
					     struct async_sha_ctx_s *actx,
					     enum ecall_async_variant_e evariant);

#define CCC_ENGINE_SWITCH_SHA_ASYNC_CHECK(eng, a_ctx, is_done)	\
	engine_method_async_sha_check_actx_locked(eng, a_ctx, is_done, CDEV_ECALL_ASYNC_CHECK)

#define CCC_ENGINE_SWITCH_SHA_ASYNC_START(eng, a_ctx)		\
	engine_method_async_sha_actx_locked(eng, a_ctx, CDEV_ECALL_ASYNC_START)

#define CCC_ENGINE_SWITCH_SHA_ASYNC_FINISH(eng, a_ctx)		\
	engine_method_async_sha_actx_locked(eng, a_ctx, CDEV_ECALL_ASYNC_FINISH)
#endif /* HAVE_SE_ASYNC_SHA */

#if HAVE_SE_ASYNC_AES

struct async_aes_ctx_s;

status_t engine_method_async_aes_check_actx_locked(const engine_t *engine,
						   const struct async_aes_ctx_s *actx,
						   bool *is_done,
						   enum ecall_async_variant_e evariant);

status_t engine_method_async_aes_actx_locked(const engine_t *engine,
					     struct async_aes_ctx_s *actx,
					     enum ecall_async_variant_e evariant);

#define CCC_ENGINE_SWITCH_AES_ASYNC_CHECK(eng, a_ctx, is_done)	\
	engine_method_async_aes_check_actx_locked(eng, a_ctx, is_done, CDEV_ECALL_ASYNC_CHECK)

#define CCC_ENGINE_SWITCH_AES_ASYNC_START(eng, a_ctx)		\
	engine_method_async_aes_actx_locked(eng, a_ctx, CDEV_ECALL_ASYNC_START)

#define CCC_ENGINE_SWITCH_AES_ASYNC_FINISH(eng, a_ctx)		\
	engine_method_async_aes_actx_locked(eng, a_ctx, CDEV_ECALL_ASYNC_FINISH)
#endif /* HAVE_SE_ASYNC_AES */

/* TODO: RSA and CMAC not yet supported */

#endif /* HAVE_SE_ASYNC */

#if CCC_WITH_ECC

struct se_engine_ec_context;

/** @brief PKA ECC operations
 */
enum ecall_ec_variant_e {
	CDEV_ECALL_EC_NONE,
	CDEV_ECALL_EC_MULT = 0x60,
#if CCC_WITH_ECC_POINT_ADD || CCC_WITH_ED25519_POINT_ADD || \
			CCC_WITH_P521_POINT_ADD
	CDEV_ECALL_EC_ADD = 0x61,
#endif
#if CCC_WITH_ECC_POINT_DOUBLE || CCC_WITH_ED25519_POINT_DOUBLE || \
			CCC_WITH_P521_POINT_DOUBLE
	CDEV_ECALL_EC_DOUBLE = 0x62,
#endif
	CDEV_ECALL_EC_VERIFY = 0x63,
	CDEV_ECALL_EC_SHAMIR_TRICK = 0x64,
#if HAVE_LWPKA11_SUPPORT
	CDEV_ECALL_EC_ECDSA_SIGN = 0x65,
#if HAVE_LWPKA11_ECKS_KEYOPS
	CDEV_ECALL_EC_KS_KEYOP = 0x66,
#endif
#endif /* HAVE_LWPKA11_SUPPORT */
};

status_t engine_method_ec_econtext_locked(const engine_t *engine,
					  struct se_data_params *input,
					  struct se_engine_ec_context *econtext,
					  enum ecall_ec_variant_e evariant);

#define CCC_ENGINE_SWITCH_EC_MULT(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_MULT)

#if CCC_WITH_ECC_POINT_ADD || CCC_WITH_ED25519_POINT_ADD || \
			CCC_WITH_P521_POINT_ADD
#define CCC_ENGINE_SWITCH_EC_ADD(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_ADD)
#endif
#if CCC_WITH_ECC_POINT_DOUBLE || CCC_WITH_ED25519_POINT_DOUBLE || \
			CCC_WITH_P521_POINT_DOUBLE
#define CCC_ENGINE_SWITCH_EC_DOUBLE(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_DOUBLE)
#endif
#define CCC_ENGINE_SWITCH_EC_VERIFY(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_VERIFY)

#define CCC_ENGINE_SWITCH_EC_SHAMIR_TRICK(eng, in_params, ectx)	\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_SHAMIR_TRICK)

#if HAVE_LWPKA11_SUPPORT
#define CCC_ENGINE_SWITCH_EC_ECDSA_SIGN(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_ECDSA_SIGN)
#if HAVE_LWPKA11_ECKS_KEYOPS
#define CCC_ENGINE_SWITCH_EC_KS_KEYOP(eng, in_params, ectx)		\
	engine_method_ec_econtext_locked(eng, in_params, ectx, CDEV_ECALL_EC_KS_KEYOP)
#endif
#endif /* HAVE_LWPKA11_SUPPORT */
#endif /* CCC_WITH_ECC */

#if CCC_WITH_MODULAR

struct te_mod_params_s;

/* @brief PKA modular operations
 */
enum ecall_mod_variant_e {
	CDEV_ECALL_MOD_NONE,
	CDEV_ECALL_MOD_C25519_EXP = 0x70,
	CDEV_ECALL_MOD_C25519_SQR = 0x71,
	CDEV_ECALL_MOD_C25519_MULT = 0x72,

	CDEV_ECALL_MOD_MULTIPLICATION = 0x80,
	CDEV_ECALL_MOD_ADDITION = 0x81,
	CDEV_ECALL_MOD_SUBTRACTION = 0x82,
	CDEV_ECALL_MOD_REDUCTION = 0x83,
#if CCC_WITH_MODULAR_DIVISION
	CDEV_ECALL_MOD_DIVISION = 0x84,
#endif /* CCC_WITH_MODULAR_DIVISION */
	CDEV_ECALL_MOD_ILWERSION = 0x85,
#if CCC_WITH_BIT_SERIAL_REDUCTION
	CDEV_ECALL_MOD_BIT_REDUCTION = 0x86,
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */
	CDEV_ECALL_MOD_BIT_REDUCTION_DP = 0x87,

	CDEV_ECALL_MOD_GEN_MULTIPLICATION = 0x88,
	CDEV_ECALL_GEN_MULTIPLICATION = 0x89,

	CDEV_ECALL_MOD_MULT_M521 = 0x90,
	CDEV_ECALL_MOD_MULT_MONT521 = 0x91,
};

/** @brief PKA modular operations
 */
status_t engine_method_mod_params_locked(const engine_t *engine,
					 const struct te_mod_params_s *modparam,
					 enum ecall_mod_variant_e evariant);

#if CCC_WITH_ED25519
#define CCC_ENGINE_SWITCH_MODULAR_C25519_EXP(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_C25519_EXP)

#define CCC_ENGINE_SWITCH_MODULAR_C25519_SQR(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_C25519_SQR)

#define CCC_ENGINE_SWITCH_MODULAR_C25519_MULT(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_C25519_MULT)
#endif /* CCC_WITH_ED25519 */

#define CCC_ENGINE_SWITCH_MODULAR_MULTIPLICATION(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_MULTIPLICATION)

#define CCC_ENGINE_SWITCH_MODULAR_ADDITION(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_ADDITION)

#define CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_SUBTRACTION)

#define CCC_ENGINE_SWITCH_MODULAR_REDUCTION(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_REDUCTION)

#if CCC_WITH_MODULAR_DIVISION
#define CCC_ENGINE_SWITCH_MODULAR_DIVISION(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_DIVISION)
#endif /* CCC_WITH_MODULAR_DIVISION */

#define CCC_ENGINE_SWITCH_MODULAR_ILWERSION(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_ILWERSION)

#if CCC_WITH_BIT_SERIAL_REDUCTION
#define CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_BIT_REDUCTION)
#endif /* CCC_WITH_BIT_SERIAL_REDUCTION */

#define CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION_DP(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_BIT_REDUCTION_DP)

#if HAVE_GEN_MULTIPLY
/* This is not a modular operation.
 *
 * This produces double precision output and requires double length output buffer
 * compared with the argument length.
 */
#define CCC_ENGINE_SWITCH_GEN_MULTIPLICATION(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_GEN_MULTIPLICATION)

/* This does a dp gen multiply immediately followed by a double
 * precision bit serial reduction as a single operation.
 *
 * This produces single precision output and does not requires double
 * length output buffer because the double length value is not read
 * out from the HW, it is reduced directly from the previous operation
 * result register.
 */
#define CCC_ENGINE_SWITCH_MODULAR_GEN_MULTIPLICATION(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_GEN_MULTIPLICATION)
#endif /* HAVE_GEN_MULTIPLY */

#if HAVE_P521
#define CCC_ENGINE_SWITCH_MODULAR_MULT_M521(eng, modparam)		\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_MULT_M521)

#define CCC_ENGINE_SWITCH_MODULAR_MULT_MONT521(eng, modparam)	\
	engine_method_mod_params_locked(eng, modparam, CDEV_ECALL_MOD_MULT_MONT521)
#endif /* HAVE_P521 */
#endif /* CCC_WITH_MODULAR */
/*@}*/

/**
 * @brief Capability based engine lookup functions from engine switch ccc_engine[] table
 *
 * @ingroup eng_switch_table
 * @defgroup eng_engine_select engine select methods
 */
/*@{*/
/**
 * @brief Select the engine to use for the engine_class_t operation.
 *
 * ENGINE_HINT is a suggestion which to use (incorrect suggestions ignored),
 *  and code may anyway select any engine it likes (ignore hint).
 *
 * Selected engine is returned in *ENG_P. Code may change the engine
 * at any later time.
 *
 * @param eng_p referenced object set to the selected engine
 * @param eclass CLASS bit to match from engine table e_class field
 * @param engine_hint suggestion to use some engine. Ignored if eclass is not supported
 * by the suggested engine. It can be overridden for any reason, e.g. if using
 * long rsa keys and PKA0 was hinted => automagically switched to PKA.
 * @return NO_ERROR on success error code otherwise.
 */
status_t ccc_select_engine(const engine_t **eng_p, engine_class_t eclass, engine_id_t engine_hint);

/**
 * @brief Like ccc_select_engine(), but lookup a device implementing the engine class.
 * If found, set the ENGINE matching the class in that device to *eng_p
 *
 * Software engines do not match any device => they are not found by this function.
 *
 * @param device_id identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param eng_p referenced object set to selected engine
 * @param eclass CLASS bit to match from engine table e_class field
 * @return NO_ERROR on success error code otherwise.
 */
status_t ccc_select_device_engine(se_cdev_id_t device_id, const engine_t **eng_p,
				  engine_class_t eclass);
/*@}*/

/**
 * @brief All read/write to engine memory mapped registers use these
 * register access functions.
 *
 * In normal cases the register address is callwlated by adding
 * the parameter offset reg to the device base address (found via engine).
 *
 * If the engine is set up to do register range remapping
 * (eng->e_do_remap == true), all input code reg offsets in range
 * [rm_start .. rm_end] are linearly re-mapped to the HW engine range
 * [e_regs_begin..e_regs_end]. If the register offset is out of
 * range for the engine, no mapping takes place.
 *
 * This happens for example if the register to read is in AES0
 * register range in the code but the runtime selected engine is AES1
 * => the register offset value gets mapped from AES0 register offset
 * range to AES1 register offset range.
 *
 * The automagic remapping allows CCC core code to refer to AES0
 * engine symbolic name definitions with the LWDRF macros in code, but
 * still use both the AES0 and AES1 engines at runtime.
 *
 * <b>The remapping works only when all matching registers in mapped
 * engines are within a fixed offset from each other. This is true for
 * all current and planned LW SE engine designs</b>. Lwrrenlty only AES
 * engines require such mapping to be used.
 *
 * @defgroup eng_switch_register_api Engine register access API
 */
/*@{*/

static inline void ccc_device_set_reg_normal(const se_cdev_t *cdev, uint32_t reg, uint32_t val)
{
	TEGRA_WRITE32(&cdev->cd_base[reg], val);
}

static inline uint32_t ccc_device_get_reg_normal(const se_cdev_t *cdev, uint32_t reg)
{
	return TEGRA_READ32(&cdev->cd_base[reg]);
}

#ifndef ccc_device_set_reg
#define ccc_device_set_reg(cdev, reg, val) ccc_device_set_reg_normal(cdev, reg, val)
#endif

#ifndef ccc_device_get_reg
#define ccc_device_get_reg(cdev, reg) ccc_device_get_reg_normal(cdev, reg)
#endif

#if CCC_INLINE_REGISTER_ACCESS
/**
 * @brief Normal code version use these inline functions for engine register reads.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @return 32 bit register value
 */
static inline uint32_t tegra_engine_get_reg_normal(const engine_t *eng, uint32_t reg)
{
	uint32_t reg1 = reg;

	if (BOOL_IS_TRUE(eng->e_do_remap) &&
	    (eng->e_remap_range.rm_start <= reg1) &&
	    (reg1 <= eng->e_remap_range.rm_end)) {
		uint32_t reg_offset = reg1 - eng->e_remap_range.rm_start;
		/* CERT INT30-C: Avoid integer overflow */
		if (eng->e_regs_begin <= (CCC_UINT_MAX - reg_offset)) {
			reg1 = eng->e_regs_begin + reg_offset;
		}
	}

	return ccc_device_get_reg(eng->e_dev, reg1);
}

/**
 * @brief Normal code version use these inline functions for engine register writes.
 *
 * This is the inline version. Same function in tegra_cdev.c when not inlining.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @param data 32 bit data to write
 */
static inline void tegra_engine_set_reg_normal(const engine_t *eng, uint32_t reg, uint32_t data)
{
	uint32_t reg1 = reg;

	if (BOOL_IS_TRUE(eng->e_do_remap) &&
	    (eng->e_remap_range.rm_start <= reg1) &&
	    (reg1 <= eng->e_remap_range.rm_end)) {
		uint32_t reg_offset = reg1 - eng->e_remap_range.rm_start;
		/* CERT INT30-C: Avoid integer overflow */
		if (eng->e_regs_begin <= (CCC_UINT_MAX - reg_offset)) {
			reg1 = eng->e_regs_begin + reg_offset;
		}
	}

	ccc_device_set_reg(eng->e_dev, reg1, data);
}

#else

/**
 * @brief Normal code version use these inline functions for engine register reads.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @return 32 bit register value
 */
uint32_t tegra_engine_get_reg_normal(const engine_t *eng, uint32_t reg);

/**
 * @brief Normal code version use these inline functions for engine register writes.
 *
 * Prototype for this function in tegra_cdev.c when not inlining.
 *
 * @param eng engine
 * @param reg register offset from base address
 * @param data 32 bit data to write
 */
void tegra_engine_set_reg_normal(const engine_t *eng, uint32_t reg, uint32_t data);

#endif /* CCC_INLINE_REGISTER_ACCESS */

/* If a system has some completely different register access mechanism
 * it is possible to override the CCC default register access functions.
 * This is the case for e.g. future dGPU units.
 *
 * To do that you need to define your own versions of these macros
 * for the files using the specific method.
 *
 * tegra_engine_set_reg(engine, reg, data)
 * tegra_engine_get_reg(engine, reg)
 * tegra_engine_get_reg_NOTRACE(engine, reg)
 *
 * In addition you must set CCC_REG_ACCESS_DEFAULT 0 to gate this feature
 * for the files you need to replace the register access methods.
 * If this is not 0 you will get a compile error if overriding
 * the defaults.
 */
#if TRACE_REG_OPS
/**
 * @brief Device register write with tracing
 *
 * @param engine object ref
 * @param register offset from base address (prefer symbolic name)
 * @data  32 bit word value to write
 * @rname reg name or similar string (Must not be NULL)
 * @fun_s calling function __func__
 * @line_i calling line
 *
 * Write engine register with data; tracing.
 *
 * This function is for R&D only.
 * Never use these on any code that deals with secret values.
 */
void tegra_engine_set_reg_trace_log(const engine_t *engine,
				    uint32_t reg,
				    uint32_t data,
				    const char *rname,
				    const char *fun_s,
				    uint32_t line_i);

/**
 * @brief Device register read with tracing
 *
 * @param engine object ref
 * @param register offset from base address (prefer symbolic name)
 * @rname reg name or similar string (Must not be NULL)
 * @fun_s calling function __func__
 * @line_i calling line
 *
 * @return 32 bit word value from register
 *
 * Read engine register; tracing.
 *
 * This function is for R&D only.
 * Never use these on any code that deals with secret values.
 */
uint32_t tegra_engine_get_reg_trace_log(const engine_t *engine,
					uint32_t reg,
					const char *rname,
					const char *fun_s,
					uint32_t line_i);

#if !defined(tegra_engine_set_reg) || CCC_REG_ACCESS_DEFAULT
/**
 * @def tegra_engine_set_reg(engine, reg, data)
 * @brief These macros are for heavy R&D only.
 *
 * Write engine register with data; tracing.
 *
 * Never use these on any code that deals with secret values.
 */
#define tegra_engine_set_reg(engine, reg, data)		\
	tegra_engine_set_reg_trace_log(engine, reg, data, #reg, __func__, __LINE__)
#endif

#if !defined(tegra_engine_get_reg_NOTRACE) || CCC_REG_ACCESS_DEFAULT
/**
 * @def tegra_engine_get_reg_NOTRACE(engine, reg)
 * @brief same as tegra_engine_get_reg() without tracing
 *
 * Read engine register.
 */
#define tegra_engine_get_reg_NOTRACE(engine, reg) 	\
	tegra_engine_get_reg_normal(engine, reg)
#endif

#if !defined(tegra_engine_get_reg) || CCC_REG_ACCESS_DEFAULT
/**
 * @def tegra_engine_get_reg(engine, reg)
 * @brief These macros are for heavy R&D only.
 *
 * Read engine register; tracing.
 *
 * Never use these on any code that deals with secret values.
 */
#define tegra_engine_get_reg(engine, reg) 		\
	tegra_engine_get_reg_trace_log(engine, reg, #reg, __func__, __LINE__)
#endif

#else /* production version */

#if !defined(tegra_engine_set_reg) || CCC_REG_ACCESS_DEFAULT
/**
 * @brief Production register write
 *
 * @param eng engine
 * @param reg register offset from base address
 * @data  32 bit word value to write
 */
#define tegra_engine_set_reg(engine, reg, data) \
	tegra_engine_set_reg_normal(engine, reg, data)
#endif

#if !defined(tegra_engine_get_reg) || CCC_REG_ACCESS_DEFAULT
/**
 * @brief Production register read
 *
 * @param eng engine
 * @param reg register offset from base address
 * @return 32 bit register value
 */
#define tegra_engine_get_reg(engine, reg)		\
	tegra_engine_get_reg_normal(engine, reg)
#endif

#if !defined(tegra_engine_get_reg_NOTRACE) || CCC_REG_ACCESS_DEFAULT
/**
 * @def tegra_engine_get_reg_NOTRACE(engine, reg)
 * @brief same as tegra_engine_get_reg() in this setup.
 *
 * Read engine register. Never trace.
 */
#define tegra_engine_get_reg_NOTRACE(engine, reg)	\
	tegra_engine_get_reg_normal(engine, reg)
#endif

#endif /* TRACE_REG_OPS */
/*@}*/

#ifdef CCC_WAIT_FOR_INTERRUPT
/**
 * @brief Prototype for the subsystem specific interrupt handler function.
 *
 * Define the name of the function in subsystem adaptation code that
 * handles the interrupts from DEVID. The engine is provided to
 * identify in which engine in the device the operation has been
 * started and should interrupt soon; also the device base addresses
 * can be found from there if required.
 *
 * There are additonal defines to enable interrupts per device, but
 * the intention is to make all enabled devices call this same
 * interrupt handler.
 *
 * Lwrrently PKA interrupts are supported.
 *
 * Note: Interrupt support is optional, do not define this if your
 * subsystem does not handle interrupts. In that case operation
 * completion is polled.
 *
 * Define CCC_WAIT_FO_INTERRUPT to be your subsystem function in CONFIG_FILE.
 *
 * @param devid identify device with SE_CDEV_* (enum se_cdev_id_e value)
 * @param eng_id identify engine
 * @param engine engine object
 * @return NO_ERROR on success error code otherwise.
 */
status_t CCC_WAIT_FOR_INTERRUPT(se_cdev_id_t devid, engine_id_t eng_id, const struct engine_s *engine);
#endif /* CCC_WAIT_FOR_INTERRUPT */

#endif /* INCLUDED_TEGRA_CDEV_H */
