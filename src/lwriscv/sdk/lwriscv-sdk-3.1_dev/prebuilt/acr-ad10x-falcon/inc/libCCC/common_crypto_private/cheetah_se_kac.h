/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* New features for key access control (KAC) support in SE engines
 */
#ifndef INCLUDED_TEGRA_SE_KAC_H
#define INCLUDED_TEGRA_SE_KAC_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#if HAVE_SOC_FEATURE_KAC == 0
#error "KAC include file compiled in old system"
#endif

#if HAVE_SE_UNWRAP
#error "T19X unwrap defined for T23X system"
#endif

/* Define these to make the HW dependent include files misra compatible.
 * The directory is specific in the rules.mk build file.
 */
#include <arse0.h>

/* Key Wrap writes a constant 512U bit formatted blob to memory
 */
#define SE_KEY_WRAP_MEMORY_BYTE_SIZE (512U / 8U)

/* ORIGIN field is set and managed by the HW.
 */
typedef enum kac_manifest_origin_e {
	// XXX Replace with SE_UNIT_KEYMANIFEST_PKT_ORIGIN_HW when defined.
	KAC_MANIFEST_ORIGIN_HW	 = SE_UNIT_KEYMANIFEST_PKT_ORIGIN_FUSE,
	KAC_MANIFEST_ORIGIN_PSC	 = SE_UNIT_KEYMANIFEST_PKT_ORIGIN_PSC,
	KAC_MANIFEST_ORIGIN_TZ	 = SE_UNIT_KEYMANIFEST_PKT_ORIGIN_TZ,
	KAC_MANIFEST_ORIGIN_NS	 = SE_UNIT_KEYMANIFEST_PKT_ORIGIN_NS,
	KAC_MANIFEST_ORIGIN_FSI	 = SE_UNIT_KEYMANIFEST_PKT_ORIGIN_FSI,
} kac_manifest_origin_t;

typedef enum kac_manifest_user_e {
	KAC_MANIFEST_USER_RESERVED = SE_UNIT_KEYMANIFEST_PKT_USER_RESERVED,
	KAC_MANIFEST_USER_PSC	    = SE_UNIT_KEYMANIFEST_PKT_USER_PSC,
	KAC_MANIFEST_USER_TZ	    = SE_UNIT_KEYMANIFEST_PKT_USER_TZ,
	KAC_MANIFEST_USER_NS	    = SE_UNIT_KEYMANIFEST_PKT_USER_NS,
	KAC_MANIFEST_USER_FSI	    = SE_UNIT_KEYMANIFEST_PKT_USER_FSI,
} kac_manifest_user_t;

typedef enum kac_manifest_purpose_e {
	// XXX changed in IAS 0.5 RC (November 21) =>
	KAC_MANIFEST_PURPOSE_ENC   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_ENC, /* CMAC uses ENC purpose in T23x */

	KAC_MANIFEST_PURPOSE_CMAC  = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_CMAC,
	KAC_MANIFEST_PURPOSE_HMAC  = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_HMAC,

	KAC_MANIFEST_PURPOSE_KW    = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KW, /* Key wrap */
	KAC_MANIFEST_PURPOSE_KUW   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KUW,      /* Key unwrap */
	KAC_MANIFEST_PURPOSE_KWUW  = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KWUW,
	KAC_MANIFEST_PURPOSE_KDK   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KDK,      /* Key Derivation Key */
	KAC_MANIFEST_PURPOSE_KDD   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KDD,      /* Key Derivation Data */
	KAC_MANIFEST_PURPOSE_KDD_KUW = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_KDD_KUW,
	KAC_MANIFEST_PURPOSE_XTS   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_XTS,
	KAC_MANIFEST_PURPOSE_GCM   = SE_UNIT_KEYMANIFEST_PKT_PURPOSE_GCM,
	// 0xB..0xF reserved
	KAC_MANIFEST_PURPOSE_UNKNOWN   = 0x00004B1D,
} kac_manifest_purpose_t;

typedef enum kac_manifest_size_e {
	KAC_MANIFEST_SIZE_128 = SE_UNIT_KEYMANIFEST_PKT_SIZE_KEY128,
	KAC_MANIFEST_SIZE_192 = SE_UNIT_KEYMANIFEST_PKT_SIZE_KEY192,
	KAC_MANIFEST_SIZE_256 = SE_UNIT_KEYMANIFEST_PKT_SIZE_KEY256,
	KAC_MANIFEST_SIZE_RESERVED = SE_UNIT_KEYMANIFEST_PKT_SIZE_RESERVED,
} kac_manifest_size_t;

struct kac_manifest {
	kac_manifest_origin_t	 mf_origin;	/*  2 bit field, 1:0 (maintained by HW) */
	/* reserved: 3:2 */
	kac_manifest_user_t	 mf_user;	/*  2 bit field, bits 5:4 */
	/* reserved: 7:6 */
	kac_manifest_purpose_t	 mf_purpose;	/*  4 bit field, bits 10:8 */
	/* reserved: 11 */
	bool			 mf_exportable;	/*  1 bit field, bit  12 */
	/* reserved: 13 */
	kac_manifest_size_t	 mf_size;	/*  2 bit field, bits 15:14 */
	uint32_t		 mf_sw;		/* 16 bit field, bits 31:16 */
};

// XXX There is a engine keyslot LOCK operation => (TODO ==> check IAS how lock works)
struct kac_ctrl {
#if 0	// XXX "valid" seems to be removed from 0.5 RC IAS ===> VERIFY!!!!
	bool			 ctrl_valid;	/*  1 bit field, 31 (maintained by HW, R/O) */
#endif
	/* reserved: 30:1 */
	bool			 ctrl_lock;	/* 1 bit field, 0 (false = not locked, true = locked) */
};

#define KAC_MANIFEST_SW_VALUE_IS_VALID(x) (((x) >> 16U) == 0U)
#define KAC_MANIFEST_SW_FIELD_MASK 0xFFFFU

struct kac {
	/* Register: SE0_*_CRYPTO_KEYTABLE_MANIFEST_0 */
	struct kac_manifest k_manifest;

	/* Register: SE0_*_CRYPTO_KEYTABLE_CTRL_0 */
	struct kac_ctrl k_ctrl;
};

#ifndef SE_KAC_KEYSLOT_USER
/* Default owner of lwrrently used SE keyslots.
 *
 * The CCC_MANIFEST_USER_xxx field values in crypto_api.h
 * are supported by the HW.
 */
#define SE_KAC_KEYSLOT_USER CCC_MANIFEST_USER_PSC
#endif

/* SE key KAC manifest attribute flags */
#define SE_KAC_FLAGS_NONE       0x00000U
#define SE_KAC_FLAGS_EXPORTABLE 0x00001U /* Allows key to be exported (wrapped) if set */
#define SE_KAC_FLAGS_LOCK       0x00002U /* Lock keyslot (NOT IMPLEMENTED YET!!!!! => TODO: implement) */
#define SE_KAC_FLAGS_PURPOSE_SECONDARY_KEY 0x00004U /* Refers to the "other" key used by the algorithm
						     * Defined for the TE_ALG_KEYOP_KDF_2KEY =>
						     *   key refers to the KDK key (primary is KDD key)
						     *
						     * Note:
						     * TE_ALG_AES_XTS does not use this now, since both
						     * AES-XTS key and the tweak key use the same
						     * manifest XTS purpose.
						     */

/* se_kac_keyslot should be passed the CCC_MANIFEST_USER_* values
 * available to clients.
 */
status_t se_set_engine_kac_keyslot(const engine_t *engine,
				   uint32_t keyslot,
				   const uint8_t *keydata,
				   uint32_t keysize_bits,
				   uint32_t se_kac_user,
				   uint32_t se_kac_flags,
				   uint32_t se_kac_sw,
				   te_crypto_algo_t se_kac_algorithm);

status_t se_set_device_kac_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   const uint8_t *keydata, uint32_t keysize_bits,
				   uint32_t se_kac_user,
				   uint32_t se_kac_flags,
				   uint32_t se_kac_sw,
				   te_crypto_algo_t se_kac_algorithm);

status_t se_set_eng_kac_keyslot_purpose(const engine_t *engine,
					uint32_t keyslot,
					const uint8_t *keydata,
					uint32_t keysize_bits,
					uint32_t se_kac_user,
					uint32_t se_kac_flags,
					uint32_t se_kac_sw,
					uint32_t se_kac_purpose);

status_t se_set_device_kac_keyslot_purpose(se_cdev_id_t device_id, uint32_t keyslot,
					   const uint8_t *keydata, uint32_t keysize_bits,
					   uint32_t se_kac_user,
					   uint32_t se_kac_flags,
					   uint32_t se_kac_sw,
					   uint32_t se_kac_purpose);

#if HAVE_SE_KAC_GENKEY
status_t se_engine_kac_keyslot_genkey(const engine_t *engine,
				      uint32_t keyslot,
				      uint32_t keysize_bits,
				      uint32_t se_kac_user,
				      uint32_t se_kac_flags,
				      uint32_t se_kac_sw,
				      te_crypto_algo_t se_kac_algorithm);

status_t se_device_kac_keyslot_genkey(se_cdev_id_t device_id, uint32_t keyslot,
				      uint32_t keysize_bits,
				      uint32_t se_kac_user,
				      uint32_t se_kac_flags,
				      uint32_t se_kac_sw,
				      te_crypto_algo_t se_kac_algorithm);
#endif /* HAVE_SE_KAC_GENKEY */

#if HAVE_SE_KAC_LOCK
status_t se_engine_kac_keyslot_lock(const engine_t *engine,
				    uint32_t keyslot);

status_t se_device_kac_keyslot_lock(se_cdev_id_t device_id,
				    uint32_t keyslot);
#endif /* HAVE_SE_KAC_LOCK */

#if HAVE_SE_KAC_CLONE
/**
 * @brief The keyslot clone functions for engine.
 *
 * Some manifest fields for the target keyslot need to be
 * provided by the caller: USER, SW and EXPORTABLE.
 *
 * @param engine AES or SHA engine object ref.
 * @param dst_keyslot Target keyslot of the operation.
 * @param src_keyslot Source keyslot of the operation.
 * @param clone_user user of the cloned keyslot (CCC_MANIFEST_USER_xxx)
 * @param clone_sw 16 bit SW field for the cloned keyslot
 * @param clone_kac_flags zero or CCC_MANIFEST_FLAGS_EXPORTABLE (CCC_MANIFEST_FLAGS_xxx)
 */
status_t se_engine_kac_keyslot_clone(const engine_t *engine,
				     uint32_t dst_keyslot,
				     uint32_t src_keyslot,
				     uint32_t clone_user,
				     uint32_t clone_sw,
				     uint32_t clone_kac_flags);

/**
 * @brief The keyslot clone functions for a device.
 *
 * Some manifest fields for the target keyslot need to be
 * provided by the caller: USER, SW and EXPORTABLE.
 *
 * For the operation, this call version internally uses the AES engine
 * settings to perform the keyslot clone operation.
 *
 * @param device AES or SHA device object ref.
 * @param dst_keyslot Target keyslot of the operation.
 * @param src_keyslot Source keyslot of the operation.
 * @param clone_user user of the cloned keyslot (CCC_MANIFEST_USER_xxx)
 * @param clone_sw 16 bit SW field for the cloned keyslot
 * @param clone_kac_flags zero or CCC_MANIFEST_FLAGS_EXPORTABLE (CCC_MANIFEST_FLAGS_xxx)
 */
status_t se_device_kac_keyslot_clone(se_cdev_id_t device_id,
				     uint32_t dst_keyslot,
				     uint32_t src_keyslot,
				     uint32_t clone_user,
				     uint32_t clone_sw,
				     uint32_t clone_kac_flags);
#endif /* HAVE_SE_KAC_CLONE */

/* For SE device (AES and MAC) algorithm keys only
 *
 * If other engines added to SE device, may need to support more
 * complex key material.
 */
struct se_key_slot {
	const uint8_t *ks_keydata;	/* AES key material, read only */
	uint32_t ks_bitsize;		/* Bitsize of key material */
	uint32_t ks_slot;		/* Keyslot used by the algorithm */
	union {
		uint32_t ks_subkey_slot;	/* Multi-key operation keyslot.
						 *
						 * For XTS this is same as ks_slot
						 *  unless HAVE_SOC_FEATURE_KAC is set
						 *
						 * This field is also KDF_2KEY
						 * key2_index slot for second
						 * KDF key (i.e. the KDD key).
						 *
						 * The second key material is
						 * in ks_keydata at key size
						 * offset (as for XTS subkey).
						 */
#if HAVE_SE_KAC_KDF
		struct {
			uint32_t ks_kdd_slot;
			const uint8_t *ks_kdd_key;
		};
#endif
	};

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
	/* T1XX operations
	 */
	uint32_t ks_target_keyslot;	/* T1XX dst kslot to unwrap or derive to */
#endif

	/* Features only in T23X with Key Access Control (KAC) support
	 */
	union {
		uint32_t ks_wrap_index;		/* KAC kslot to wrap from or unwrap to */
#if HAVE_SE_KAC_KDF
		uint32_t ks_kdf_index;		/* KAC KDF key derivation target keyslot */
#endif
#if HAVE_SE_KAC_CLONE
		uint32_t ks_clone_src;		/* KAC CLONE source keyslot */
#endif
	};

	struct kac ks_kac;		/* KAC keyslot metadata fields (manifest, user, etc) */
};

#endif /* INCLUDED_TEGRA_SE_KAC_H */
