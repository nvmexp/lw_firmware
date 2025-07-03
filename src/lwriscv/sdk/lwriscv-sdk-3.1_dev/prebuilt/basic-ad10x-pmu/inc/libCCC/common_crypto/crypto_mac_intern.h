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

#ifndef INCLUDED_CRYPTO_MAC_INTERN_H
#define INCLUDED_CRYPTO_MAC_INTERN_H

#if HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5

/* CMAC supported by the HW in T19X/T18X.
 * CMAC has full HW support in T23x.
 *
 * (But I guess it was not designed to support multiple CMAC data injections for one CMAC result,
 *  i.e. continuation of the CMAC operations). OTOH, I guess I got that done in the HW dependent
 *  code by adding one manual block xor => anyway, verify this from the HW folks).
 *
 * T23x has HW support for fixed key length HMAC-SHA2 (HAVE_HW_HMAC_SHA).
 * HMAC is also supported with SW wrapper layer with SHA HW operations (HAVE_SW_HMAC_SHA).
 */
enum mac_type_e {
	MAC_TYPE_NONE		= 0,
	MAC_TYPE_CMAC_AES	= 1,
	MAC_TYPE_HMAC_SHA1	= 2,
	MAC_TYPE_HMAC_SHA2	= 3,
	MAC_TYPE_HMAC_MD5	= 4,
	MAC_TYPE_HW_HMAC_SHA2	= 5,
};

/* MAX_MD_ALIGNED_SIZE > AES_BLOCK_SIZE so align by MAX_MD_ALIGNED_SIZE if
 * required to keep contiguous
 */
struct se_mac_state_s {
	/* MAC type specific state */
	union {
#if HAVE_CMAC_AES
		struct {
			/* First field in the object (alignment is smaller) */
			struct se_aes_context context_aes;	// HW state

			/* CMAC subkey buffers. To keep the AES
			 * context smaller they are not there and so
			 * need to be set up separately when using
			 * CMAC.
			 */
			uint8_t	 pk1[MAX_AES_KEY_BYTE_SIZE];
			uint8_t	 pk2[MAX_AES_KEY_BYTE_SIZE];
		} cmac;
#endif

#if HAVE_HMAC_SHA || HAVE_HMAC_MD5
		struct {
			/* HMAC block size is identical to the digest
			 *  block size => using that.
			 */
			union {
#if HAVE_HMAC_SHA
				/* SHA context used for both hybrid
				 * SW+SHA engine and HW HMAC-SHA2 engine
				 * (T23X only) operations.
				 */
				struct se_sha_context context_sha;	// HW state
#endif
#if HAVE_HMAC_MD5
				struct soft_md5_context context_md5;	// SW state (no MD5 engine)
#endif
			};
			const uint8_t *hmac_key;
			uint32_t hmac_key_size;
			bool     ipad_ok;
		} hmac;
#endif /* HAVE_HMAC_SHA || HAVE_HMAC_MD5 */
	};

	uint32_t   mac_magic;
	enum mac_type_e mac_type;	// Also used as selector
};

#if HAVE_SW_HMAC_SHA || HAVE_HMAC_MD5

#define HMAC_IPAD_VALUE 0x36U
#define HMAC_OPAD_VALUE 0x5LW

status_t hmac_handle_ipad(struct se_mac_state_s *s);

status_t hmac_handle_opad(struct se_mac_state_s *s);
#endif /* HAVE_SW_HMAC_SHA || HAVE_HMAC_MD5 */

#if HAVE_HMAC_SHA || HAVE_HMAC_MD5
status_t handle_hmac_result(const crypto_context_t *ctx,
			    uint8_t *result,
			    uint32_t result_size,
			    te_args_data_t *args,
			    status_t *rbad_p);
#endif /* HAVE_HMAC_SHA || HAVE_HMAC_MD5 */

#endif /* HAVE_CMAC_AES || HAVE_HMAC_SHA || HAVE_HMAC_MD5 */
#endif /* INCLUDED_CRYPTO_MAC_INTERN_H */
