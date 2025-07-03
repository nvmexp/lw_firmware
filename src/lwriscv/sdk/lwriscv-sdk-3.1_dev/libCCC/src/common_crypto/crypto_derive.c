/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/*
 * Support CheetAh Security Engine Elliptic crypto algorithm ECDH
 *  key agreement.
 *
 * Note: plain Diffie-Hellman-Merkle (DH algorithm) is implemented as
 *  plain RSA private key decipher (in crypto_acipher.c). So it is not
 *  here even though it is also a key derivation algorithm. The crypto
 *  context dispatcher passes TE_ALG_DH directly to acipher
 *  functions. This avoids adding the RSA context to the
 *  se_derive_state_t (the RSA context would grow this state with
 *  about one kilobyte and make it DMA sensitive).
 *
 * + supports t19x AES encyption to keyslot based key derivations.
 * + supports T19x CMAC based key derivations with NIST 800-108 encoding.
 */

#include <crypto_system_config.h>

#if HAVE_CCC_KDF

#include <crypto_derive.h>
#include <crypto_ec.h>
#include <crypto_process_call.h>

#include <crypto_derive_proc.h>
#include <crypto_asig_proc.h>

#if HAVE_SE_KAC_KDF
#include <crypto_md_proc.h>
#endif /* HAVE_SE_KAC_KDF */

#if HAVE_SE_AES_KDF || HAVE_SE_CMAC_KDF
#include <crypto_cipher_proc.h>
#endif

#if CCC_WITH_NIST_KDF_COUNTER_ENCODER
#include <crypto_nist_kdf_counter.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CONTEXT_MEMORY
/* Class derive memory fixed minimal sizes for context memory.
 *
 * Object alignment: 64 bytes
 * Slice alignment: 64 bytes
 */
static const struct cm_layout_s derive_mem_specs = {
	.cm_lo_size = 2944U, /* Minimum size for context memory
			      * X25519 operation.
			      * XXX test longer lwrves (P521)!
			      *
			      * KDF encoder buffer fits here also.
			      */

#if CCC_WITH_CONTEXT_KEY_MEMORY
	.cm_lo_key_size = CMEM_KEY_MEMORY_STATIC_SIZE,
#endif

#if CCC_WITH_CONTEXT_DMA_MEMORY
	/* If separate DMA memory used, this is min size for that for
	 * AES based derivations.
	 */
	.cm_lo_dma_size = (CMEM_DESCRIPTOR_SIZE +
			   CCC_DMA_MEM_AES_BUFFER_SIZE),
#endif
};
#endif /* HAVE_CONTEXT_MEMORY */

#define SE_DERIVE_MAGIC 0x64657276U

/*****************************/

enum drv_selector_e {
	DRV_TYPE_UNKNOWN = 0,
	DRV_TYPE_EC,
	DRV_TYPE_MAC,
	DRV_TYPE_AES,
	DRV_TYPE_MAC_NIST_ENCODER,
	DRV_TYPE_CMAC_NIST_ENCODER,
};

#if HAVE_SE_AES_KDF

struct aes_kdf_s {
	struct se_aes_context aes_context;
	union {
		uint8_t	 ci_iv[MAX_IV_SIZE]; /**< IV for "normal" AES modes */
		struct cipher_aes_ctr_init_s ctr_mode;
	};
};
#endif /* HAVE_SE_AES_KDF */

#if HAVE_SE_CMAC_KDF

#define NEED_KDF_ENCODE_COUNTER_SET	/* CMAC KDF uses this */

struct cmac_kdf_s {
	struct se_aes_context aes_context;
	uint8_t	 pk1[MAX_AES_KEY_BYTE_SIZE];
	uint8_t	 pk2[MAX_AES_KEY_BYTE_SIZE];
};
#endif /* HAVE_SE_CMAC_KDF */

#if HAVE_SE_KAC_KDF
struct kac_kdf_s {
	struct se_sha_context kd_context;
	uint32_t	      kd_target_keyslot;

	/* Target keyslot manifest values */
	kac_manifest_purpose_t kd_mf_purpose;
	kac_manifest_user_t   kd_mf_user;
	uint32_t	      kd_mf_keybits; /**< 128, 192 or 256 bits */
	uint32_t	      kd_mf_sw;
};
#endif /* HAVE_SE_KAC_KDF */

struct se_derive_state_s {
	union {
#if CCC_WITH_ECDH || CCC_WITH_X25519
		struct se_ec_context ec_context;
#endif
#if HAVE_SE_KAC_KDF
		struct kac_kdf_s kac_kdf;
#endif
#if HAVE_SE_AES_KDF
		struct aes_kdf_s derive_aes_kdf;
#endif
#if HAVE_SE_CMAC_KDF
		struct cmac_kdf_s derive_cmac_kdf;
#endif
	};
	enum drv_selector_e derive_selector;
	uint32_t derive_magic;
};

#if CCC_WITH_NIST_KDF_COUNTER_ENCODER

/* NIST 800-108 Counter mode <COUNTER,LABEL,CONTEXT,KEYLEN> encoder
 */
static uint32_t encoding_len(uint32_t label_len, uint32_t context_len)
{
	/* 4 octet counter, 0x00, 4 octet L */
	const uint32_t meta_bytes = (KDF_VALUE_ENCODING_LENGTH + 1U + KDF_VALUE_ENCODING_LENGTH);
	uint32_t rval = 0U;

	if ((label_len <= (MAX_KDF_ENCODING_LENGTH - meta_bytes)) &&
	    (context_len <= (MAX_KDF_ENCODING_LENGTH - meta_bytes - label_len))) {
		rval = label_len + context_len + meta_bytes;
	}

	/* returns 0U on too large values */
	return rval;
}

/* Encode <LABEL,CONTEXT> as in NIST-800/108 Counter mode
 *
 * This encoder code version does not force/assume that LABEL and
 * CONTEXT are fixed length fields, the encoding result can be up
 * to MAX_KDF_ENCODING_LENGTH bytes long with variable length LABEL
 * and CONTEXT.
 *
 * The COUNTER value is a fixed length 4 byte big endian counter value
 * as requested for this encoding.
 */
static status_t kdf_nist_encode_get_buf_length(uint32_t kdf_label_len,
					       uint32_t kdf_context_len,
					       uint32_t *buf_len_p,
					       uint32_t max_len)
{
	status_t ret = NO_ERROR;
	uint32_t buf_len = 0U;

	LTRACEF("entry\n");

	/* These can be empty, but consistently with the length
	 */
	*buf_len_p = 0U;

	buf_len = encoding_len(kdf_label_len, kdf_context_len);
	if (buf_len > max_len) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	if (0U == buf_len) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	*buf_len_p = buf_len;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Encoded octet stream to DMA buf allocated here and passed to caller:
 *
 * OSTR4(0x00000001) || OSTR(Label) || OCTET(0x00) || OSTR(Context) || OSTR4({L})
 *
 * {L} is the bit length of the generated key as a bit vector:
 * See page 12 of NIST 800-108, symbols and abbreviations.
 *
 * HW only supports 128, 192 and 256 bit key lengths (key_bits).
 * So this function also encodes only those supported key lengths.
 *
 * Encoding the {L} as a byte aligned big endian bit vector.
 *
 * Consensus on encoding COUNTER and L as 4 octet big endian values.
 */
static status_t nist_encode_counter_mode(uint32_t counter,
					 uint8_t *buf,
					 uint32_t key_bits,
					 const uint8_t *kdf_label,
					 uint32_t kdf_label_len,
					 const uint8_t *kdf_context,
					 uint32_t kdf_context_len)
{
	status_t ret = NO_ERROR;
	uint32_t off = 0U;

	LTRACEF("entry\n");

	/* Big endian 4 byte counter into &buf[off]
	 */
	se_util_u32_to_buf(counter, &buf[off], true);
	off = off + KDF_VALUE_ENCODING_LENGTH;

	if (kdf_label_len > 0U) {
		se_util_mem_move(&buf[off], kdf_label, kdf_label_len);
		CCC_SAFE_ADD_U32(off, off, kdf_label_len);
	}

	buf[off] = 0x00U;
	CCC_SAFE_ADD_U32(off, off, 1U);

	if (kdf_context_len > 0U) {
		se_util_mem_move(&buf[off], kdf_context, kdf_context_len);
		CCC_SAFE_ADD_U32(off, off, kdf_context_len);
	}

	/* Encode length as 4 byte big endian value into &buf[off]
	 */
	se_util_u32_to_buf(key_bits, &buf[off], true);
	/* here off = off + KDF_VALUE_ENCODING_LENGTH */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_EXPORT_NIST_COUNTER_MODE_ENCODER == 0
static
#endif
status_t kdf_nist_encode_counter_mode(uint32_t counter,
				      uint8_t *buf,
				      uint32_t buf_len,
				      uint32_t key_bits,
				      const uint8_t *kdf_label,
				      uint32_t kdf_label_len,
				      const uint8_t *kdf_context,
				      uint32_t kdf_context_len)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((counter > KDF_COUNTER_MAX_VALUE) || (0U == counter)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (((NULL == kdf_context) && (kdf_context_len > 0U)) ||
	    ((NULL == kdf_label) && (kdf_label_len > 0U))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == buf) ||
	    (buf_len < encoding_len(kdf_label_len, kdf_context_len))) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	ret = nist_encode_counter_mode(counter, buf, key_bits,
				       kdf_label, kdf_label_len,
				       kdf_context, kdf_context_len);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(NEED_KDF_ENCODE_COUNTER_SET)
/* On an encoded counter value buffer inject the next counter value
 * replacing the current one. Other buffer bytes unmodified.
 *
 * If buffer value is not (counter-1) => fail.
 */
static status_t kdf_nist_encode_counter_set(uint32_t counter,
					    uint8_t *buf)
{
	status_t ret = NO_ERROR;
	uint32_t cval = 0U;

	LTRACEF("entry\n");

	if ((NULL == buf) ||
	    (counter > KDF_COUNTER_MAX_VALUE) ||
	    (0U == counter)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	cval = se_util_buf_to_u32(&buf[0], true);

	/* Buffer value must be one less than counter
	 */
	if (cval != (counter - 1U)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	se_util_u32_to_buf(counter, &buf[0], true);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* defined(NEED_KDF_ENCODE_COUNTER_SET) */
#endif /* CCC_WITH_NIST_KDF_COUNTER_ENCODER */

#if CCC_WITH_ECDH || CCC_WITH_X25519
static status_t ecdh_derive_init(const crypto_context_t *ctx,
				 struct context_mem_s *cmem,
				 struct se_derive_state_s *s,
				 te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = ec_context_static_init(ctx->ctx_domain, args->engine_hint,
				     ctx->ctx_algo, args->ec.lwrve_id,
				     &s->ec_context, cmem);
	CCC_ERROR_CHECK(ret);

	// XXXX Not sure if this is useful for ECDH callwlation (point_P contains
	// XXXX  a point specific LE flag, which is used) and the ECDH data
	// XXXX  is the point_P (peer pubkey or NULL (-> use my pubkey))
	//
	// XXXX  verify if useful => TODO
	//
	if ((args->ec.flags & INIT_FLAG_EC_DATA_LITTLE_ENDIAN) == 0U) {
		LTRACEF("EC data treated as big endian\n");
		s->ec_context.ec.ec_flags |= CCC_EC_FLAGS_BIG_ENDIAN_DATA;
	}

	// return a hint
	args->engine_hint = s->ec_context.ec.engine->e_id;
	s->derive_magic = SE_DERIVE_MAGIC;
	s->derive_selector = DRV_TYPE_EC;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdh_do_derivation(const crypto_context_t *ctx,
				   struct se_derive_state_s *s,
				   te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	/* EC operand size in bytes (== "lwrve size" in bytes) */
	LTRACEF("EC lwrve %s (%u) size %u\n",
		s->ec_context.ec.ec_lwrve->name, s->ec_context.ec.ec_lwrve->id,
		s->ec_context.ec.ec_lwrve->nbytes);

	if (NULL == s->ec_context.ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve undefined\n"));
	}

	/* if src is NULL, generate ECDH with my <key,pubkey> */
	if (NULL != args->src_point) {
		if (args->src_size < sizeof_u32(struct te_ec_point_s)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Unsupported EC pubkey arg length: %u bytes\n",
							       args->src_size));
		}

		input.point_P = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src_point);
		input.input_size  = args->src_size;
	} else {
		input.point_P     = NULL;
		input.input_size  = 0U;
	}

	input.dst = GET_DOMAIN_ADDR(ctx->ctx_domain, args->dst);
	input.output_size = args->dst_size;

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_DERIVE:
		ret = PROCESS_EC_DERIVE(&input, &s->ec_context);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("derive operation 0x%x failed: %d\n",
					  ctx->ctx_algo, ret);
			break;
		}
		args->dst_size = input.output_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("Generic (ECDH) derivation", s->ec_context.ec.perf_usec);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdh_set_key(struct se_derive_state_s *s,
			     te_crypto_key_t *key,
			     const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry\n");

	kdata = GET_DOMAIN_ADDR(s->ec_context.ec.domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((kdata->k_key_type != KEY_TYPE_EC_PUBLIC) &&
	    (kdata->k_key_type != KEY_TYPE_EC_PRIVATE)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Unsupported EC key type\n"));
	}

	ret = se_setup_ec_key(&s->ec_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup EC key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDH || CCC_WITH_X25519 */

#if CCC_WITH_NIST_KDF_COUNTER_ENCODER && (HAVE_SE_KAC_KDF || HAVE_SE_CMAC_KDF)
static status_t kdf_nist_counter_dma_buf_setup(uint8_t **dma_buf_p,
					       uint32_t *dma_buf_len_p,
					       struct context_mem_s *cmem,
					       uint32_t key_bits,
					       const uint8_t *kdf_label,
					       uint32_t kdf_label_len,
					       const uint8_t *kdf_context,
					       uint32_t kdf_context_len)
{
	status_t ret = NO_ERROR;
	uint8_t *kdf_nist_encoded_dma_buf = NULL;
	uint32_t kdf_encoded_len = 0U;

	LTRACEF("entry\n");

	ret = kdf_nist_encode_get_buf_length(kdf_label_len,
					     kdf_context_len,
					     &kdf_encoded_len,
					     MAX_KDF_ENCODING_LENGTH);
	CCC_ERROR_CHECK(ret);

	kdf_nist_encoded_dma_buf = CMTAG_MEM_GET_BUFFER(cmem,
							CMTAG_ALIGNED_DMA_BUFFER,
							BYTES_IN_WORD,
							kdf_encoded_len);
	if (NULL == kdf_nist_encoded_dma_buf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Current HW only supports these key lengths for target keyslot
	 */
	switch (key_bits) {
	case 128U:
	case 192U:
	case 256U:
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* On success this function returns an allocated DMA buffer
	 * in kdf_nist_encoded_dma_buf
	 *
	 * The buffer must be released by this caller.
	 *
	 * kd_mf_keybits holds the key length in bits.
	 *
	 * Current HW KDF implementation always uses counter with constant
	 * value NIST_KDF_COUNTER_MODE_CTR_VALUE
	 */
	ret = kdf_nist_encode_counter_mode(NIST_KDF_COUNTER_MODE_CTR_VALUE,
					   kdf_nist_encoded_dma_buf,
					   kdf_encoded_len,
					   key_bits,
					   kdf_label,
					   kdf_label_len,
					   kdf_context,
					   kdf_context_len);
	CCC_ERROR_CHECK(ret);

	/* Input encoded to kdf_nist_encoded_dma_buf as specified in
	 * NIST 800-108 5.1 KDF in Counter Mode
	 * for a single task call.
	 */
	*dma_buf_p     = kdf_nist_encoded_dma_buf;
	*dma_buf_len_p = kdf_encoded_len;

	kdf_nist_encoded_dma_buf = NULL;
fail:
	if (NULL != kdf_nist_encoded_dma_buf) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  kdf_nist_encoded_dma_buf);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_NIST_KDF_COUNTER_ENCODER && (HAVE_SE_KAC_KDF || HAVE_SE_CMAC_KDF) */

#if HAVE_SE_KAC_KDF

static kac_manifest_purpose_t kac_map_api_purpose(uint32_t purpose_arg)
{
	kac_manifest_purpose_t purpose = KAC_MANIFEST_PURPOSE_UNKNOWN;

	LTRACEF("Manifest purpose %u\n", purpose_arg);

	switch (purpose_arg) {
	case CCC_MANIFEST_PURPOSE_ENC:
		purpose = KAC_MANIFEST_PURPOSE_ENC;
		break;
	case CCC_MANIFEST_PURPOSE_CMAC:
		purpose = KAC_MANIFEST_PURPOSE_CMAC;
		break;
	case CCC_MANIFEST_PURPOSE_HMAC:
		purpose = KAC_MANIFEST_PURPOSE_HMAC;
		break;
	case CCC_MANIFEST_PURPOSE_KW:
		purpose = KAC_MANIFEST_PURPOSE_KW;
		break;
	case CCC_MANIFEST_PURPOSE_KUW:
		purpose = KAC_MANIFEST_PURPOSE_KUW;
		break;
	case CCC_MANIFEST_PURPOSE_KWUW:
		purpose = KAC_MANIFEST_PURPOSE_KWUW;
		break;
	case CCC_MANIFEST_PURPOSE_KDK:
		purpose = KAC_MANIFEST_PURPOSE_KDK;
		break;
	case CCC_MANIFEST_PURPOSE_KDD:
		purpose = KAC_MANIFEST_PURPOSE_KDD;
		break;
	case CCC_MANIFEST_PURPOSE_KDD_KUW:
		purpose = KAC_MANIFEST_PURPOSE_KDD_KUW;
		break;
	case CCC_MANIFEST_PURPOSE_XTS:
		purpose = KAC_MANIFEST_PURPOSE_XTS;
		break;
	case CCC_MANIFEST_PURPOSE_GCM:
		purpose = KAC_MANIFEST_PURPOSE_GCM;
		break;
	default:
		LTRACEF("Unknown manifest purpose value: %u\n", purpose_arg);
		break;
	}

	return purpose;
}

static kac_manifest_user_t kac_map_api_user(uint32_t user_arg)
{
	kac_manifest_user_t user = KAC_MANIFEST_USER_RESERVED;

	LTRACEF("Manifest user %u\n", user_arg);

	switch (user_arg) {
	case CCC_MANIFEST_USER_PSC:
		user = 	KAC_MANIFEST_USER_PSC;
		break;
	case CCC_MANIFEST_USER_TZ:
		user = 	KAC_MANIFEST_USER_TZ;
		break;
	case CCC_MANIFEST_USER_NS:
		user = 	KAC_MANIFEST_USER_NS;
		break;
	case CCC_MANIFEST_USER_FSI:
		user = 	KAC_MANIFEST_USER_FSI;
		break;
	default:
		LTRACEF("Unknown manifest user value: %u\n", user_arg);
		break;
	}

	return user;
}

/* Does not memset zero => do it in the caller */
static status_t kdf_derive_init(const crypto_context_t *ctx,
				struct context_mem_s *cmem,
				struct se_derive_state_s *s,
				te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	uint32_t kdf_flags = args->kdf.kdf_flags;

	LTRACEF("entry\n");

	ret = kdf_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo, &s->kac_kdf.kd_context,
				      cmem);
	CCC_ERROR_CHECK(ret);

	/* KDF target keyslot
	 */
	s->kac_kdf.kd_target_keyslot = args->kdf.kdf_index;

	s->kac_kdf.kd_mf_purpose = kac_map_api_purpose(args->kdf.kdf_purpose);
	if (KAC_MANIFEST_PURPOSE_UNKNOWN == s->kac_kdf.kd_mf_purpose) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid purpose %u\n", args->kdf.kdf_purpose));
	}

	s->kac_kdf.kd_mf_user    = kac_map_api_user(args->kdf.kdf_user);
	if (KAC_MANIFEST_USER_RESERVED == s->kac_kdf.kd_mf_user) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid user %u\n", args->kdf.kdf_user));
	}

	s->kac_kdf.kd_mf_keybits = args->kdf.kdf_key_bits;
	s->kac_kdf.kd_mf_sw      = args->kdf.kdf_sw;

	if ((s->kac_kdf.kd_mf_sw >> 16U) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Manifest SW value 0x%x does not fit in 16 bits\n",
					     s->kac_kdf.kd_mf_sw));
	}

	switch (s->kac_kdf.kd_mf_keybits) {
	case 128U:
	case 192U:
	case 256U:
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	// return a hint
	args->engine_hint = s->kac_kdf.kd_context.ec.engine->e_id;
	s->derive_magic = SE_DERIVE_MAGIC;
	s->derive_selector = DRV_TYPE_MAC;

	if ((kdf_flags & INIT_FLAG_KDF_NIST_ENCODE_INPUT) != 0U) {
#if  CCC_WITH_NIST_KDF_COUNTER_ENCODER
		s->derive_selector = DRV_TYPE_MAC_NIST_ENCODER;
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#endif
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* KAC KDF/KDD derivation is a single shot operation triggered
 * here.
 *
 * The input data gets encoded as in NIST 800-108 if the selector
 * specifies this to be a DRV_TYPE_MAC_NIST_ENCODER type operation.
 *
 * If it is not, input->src is passed verbatim to the KDF/KDD
 * and it is expected to be pre-encoded by the caller.
 * Encoding is not verified by CCC.
 */
static status_t kdf_do_derivation(const crypto_context_t *ctx,
				  struct se_derive_state_s *s,
				  te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
#if CCC_WITH_NIST_KDF_COUNTER_ENCODER
	uint8_t *kdf_encoded_dma_buf = NULL;
	uint32_t kdf_encoded_len = 0U;
#endif

	LTRACEF("entry\n");

	input.src        = NULL;
	input.input_size = 0U;

	if (DRV_TYPE_MAC_NIST_ENCODER == s->derive_selector) {
#if  CCC_WITH_NIST_KDF_COUNTER_ENCODER
		ret = kdf_nist_counter_dma_buf_setup(&kdf_encoded_dma_buf,
						     &kdf_encoded_len,
						     s->kac_kdf.kd_context.ec.cmem,
						     s->kac_kdf.kd_mf_keybits,
						     args->src_kdf_label,
						     args->src_kdf_label_len,
						     args->src_kdf_context,
						     args->src_kdf_context_len);
		CCC_ERROR_CHECK(ret);

		input.src        = kdf_encoded_dma_buf;
		input.input_size = kdf_encoded_len;
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#endif /*  CCC_WITH_NIST_KDF_COUNTER_ENCODER */
	} else {
		if ((NULL != args->src) && (0U != args->src_size)) {
			input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
			input.input_size  = args->src_size;
		}
	}

	/* KDF derives keys to keyslot, does not use dst buffer.
	 */
	input.dst = NULL;
	input.output_size = 0U;

	s->kac_kdf.kd_context.ec.is_last = true;

	switch (ctx->ctx_alg_mode) {
	case TE_ALG_MODE_DERIVE:
		ret = PROCESS_SHA_KDF(&input, &s->kac_kdf.kd_context);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("derive operation 0x%x failed: %d\n",
					  ctx->ctx_algo, ret);
			break;
		}
		args->dst_size = input.output_size;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
		show_elapsed_time("Generic (KDF) derivation",
				  s->kac_kdf.kd_context.ec.perf_usec);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
#if  CCC_WITH_NIST_KDF_COUNTER_ENCODER
	if (NULL != kdf_encoded_dma_buf) {
		if (kdf_encoded_len != 0U) {
			se_util_mem_set(kdf_encoded_dma_buf, 0U, kdf_encoded_len);
		}
		CMTAG_MEM_RELEASE(s->kac_kdf.kd_context.ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  kdf_encoded_dma_buf);
	}
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_kdf_keymat(struct se_engine_sha_context *econtext,
			      te_crypto_key_t *key,
			      const te_args_key_data_t *kdata)
{
	status_t ret = NO_ERROR;
	uint32_t kmat_size = kdata->k_byte_size;

	LTRACEF("entry\n");

	se_util_mem_move(key->k_kdf.kdk, kdata->k_kdf.kdk, kmat_size);

	/* KDF engine context uses these fields (KDK)
	 *
	 * This is word aligned already
	 */
	econtext->se_ks.ks_keydata = &key->k_kdf.kdk[0];

	if (TE_ALG_KEYOP_KDF_2KEY == econtext->algorithm) {
		/* 2 separate keys in the key material
		 *
		 * XXXX FIXME: Now the KDK and KDD keys must be same
		 * XXXX  size, but this is not really acceptable.
		 */
		se_util_mem_move(key->k_kdf.kdd, kdata->k_kdf.kdd, kmat_size);

		/* KDF engine context uses these fields (KDD)
		 *
		 * This is word aligned already
		 */
		econtext->se_ks.ks_kdd_key = &key->k_kdf.kdd[0];
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set the key material to the sha engine context se_key_slot se_ks
 * field for the key derivation
 */
static status_t se_setup_kdf_key(struct se_derive_state_s *s,
				 struct se_engine_sha_context *econtext,
				 te_crypto_key_t *key,
				 const te_args_key_data_t *kdata)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	key->k_key_type  = kdata->k_key_type;
	key->k_flags     = kdata->k_flags;
	key->k_keyslot   = kdata->k_keyslot;
	key->k_kdd_keyslot = ILWALID_KEYSLOT_INDEX;

	if ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) {
		econtext->sha_flags |= SHA_FLAGS_LEAVE_KEY_TO_KEYSLOT;
	}

	key->k_byte_size = kdata->k_byte_size;

	/* KDF only supports these key sizes
	 *
	 * This is not "AES mode", do not drop 192 bit key support even from
	 * automotive.
	 */
	if (! BOOL_IS_TRUE(IS_VALID_AES_KEYSIZE_BYTES(key->k_byte_size))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Unsupported KDF key byte size %u\n",
					     key->k_byte_size));
	}

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		/* Engine code must not write the key */
		econtext->sha_flags |= SHA_FLAGS_USE_PRESET_KEY;
	} else {
		ret = se_kdf_keymat(econtext, key, kdata);
		CCC_ERROR_CHECK(ret);
	}

	econtext->se_ks.ks_bitsize = (key->k_byte_size * 8U);
	econtext->se_ks.ks_slot = key->k_keyslot;

	if (TE_ALG_KEYOP_KDF_2KEY == econtext->algorithm) {
		key->k_kdd_keyslot = kdata->k_kdd_keyslot;
	}

	/* Naming convention: KDD keyslot is in ks_subkey_slot for
	 * KDF 2KEY operation
	 */
	econtext->se_ks.ks_subkey_slot = key->k_kdd_keyslot;
	econtext->se_ks.ks_kdf_index   = s->kac_kdf.kd_target_keyslot;

	/* KDF target keyslot manifest settings
	 */
	switch (s->kac_kdf.kd_mf_keybits) {
	case 128U:
		econtext->kdf_mf_keybits = KAC_MANIFEST_SIZE_128;
		break;
	case 192U:
		econtext->kdf_mf_keybits = KAC_MANIFEST_SIZE_192;
		break;
	case 256U:
		econtext->kdf_mf_keybits = KAC_MANIFEST_SIZE_256;
		break;
	default:
		/* Can never happen */
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	econtext->kdf_mf_purpose = s->kac_kdf.kd_mf_purpose;
	econtext->kdf_mf_user	 = s->kac_kdf.kd_mf_user;
	econtext->kdf_mf_sw	 = s->kac_kdf.kd_mf_sw;

	/* XXXX TODO: ctrl field, add locking the target keyslot support?
	 */
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kdf_set_key(struct se_derive_state_s *s,
			    te_crypto_key_t *key,
			    const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry\n");

	kdata = GET_DOMAIN_ADDR(s->kac_kdf.kd_context.ec.domain, key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (kdata->k_key_type != KEY_TYPE_KDF) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Unsupported KDF key type\n"));
	}

	ret = se_setup_kdf_key(s, &s->kac_kdf.kd_context.ec, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup KDF key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#endif /* HAVE_SE_KAC_KDF */

#if HAVE_SE_AES_KDF
static status_t aes_kdf_algo_setup(struct se_engine_aes_context *econtext,
				   struct aes_kdf_init_args_s *aes_kdf)
{
	status_t ret = NO_ERROR;

	switch(econtext->algorithm) {
	case TE_ALG_AES_ECB_NOPAD:
		break;

	case TE_ALG_AES_CBC_NOPAD:
		se_util_mem_move((uint8_t *)econtext->iv_stash, aes_kdf->ci_iv, AES_IV_SIZE);
		break;

#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
		/* Treat AES-CTR zero increment as 1 (XXX AFAIK, this
		 * is the standard default increment). Because this
		 * default: increment does not need to be explicitly set
		 * in call.
		 */
		if (0U == aes_kdf->ctr_mode.ci_increment) {
			aes_kdf->ctr_mode.ci_increment = 1U;
		}

		econtext->ctr.increment =
			aes_kdf->ctr_mode.ci_increment;

		/* Treat the AES-CTR mode counter as "big endian"
		 * and do big_endian increment to it by
		 * default (i.e. field == 0); this is what NIST
		 * AES-CTR requires.
		 *
		 * If you need non-standard SE HW behaviour with
		 * little endian counters please let me know why you
		 * need to do so and add a requirement.  The HW
		 * "little endian" counter setup is confusing and does
		 * not actually support little endian counters. Since
		 * this mode has been also removed from the new SOCs
		 * the support is disabled in code as well (feature
		 * was not used by any known subsytem).
		 */
		if (0U == aes_kdf->ctr_mode.ci_counter_little_endian) {
			econtext->aes_flags |= AES_FLAGS_CTR_MODE_BIG_ENDIAN;
		} else {
			/* Do not support "LE" counters; they are never required
			 * and the HW feature is not useful.
			 *
			 * It would be possible to swap the counter bytes, but
			 * there is no real point in such support.
			 */
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
		}

		se_util_mem_move((uint8_t *)econtext->ctr.counter,
				 aes_kdf->ctr_mode.ci_counter,
				 sizeof_u32(econtext->ctr.counter));

		LTRACEF("AES KDF counter value set: increment %u\n",
			econtext->ctr.increment);
		break;
#endif /* HAVE_AES_CTR */

	default:
		/* Normal exelwtion path: this is unreachable */
		CCC_ERROR_MESSAGE("AES KDF with algorithm 0x%x unsupported\n",
				  econtext->algorithm);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_kdf_init_econtext(struct se_engine_aes_context *econtext,
				      struct aes_kdf_init_args_s *aes_kdf)
{
	status_t ret = NO_ERROR;
	uint32_t ksize_flag = 0U;

	LTRACEF("entry\n");

	ret = aes_kdf_algo_setup(econtext, aes_kdf);
	CCC_ERROR_CHECK(ret);

	switch (aes_kdf->kdf_key_bit_size) {
	case 128U: ksize_flag = AES_FLAGS_DERIVE_KEY_128; break;
	case 192U:
#if HAVE_AES_KEY192 == 0
		/* 192 bit AES keys are not supported by configuration */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#else
		ksize_flag = AES_FLAGS_DERIVE_KEY_192;
#endif
		break;
	case 256U: ksize_flag = AES_FLAGS_DERIVE_KEY_256; break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (aes_kdf->kdf_index >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid AES keyslot target index %u",
					     aes_kdf->kdf_index));
	}

	/* Deriving N bit key value to keyslot X
	 *
	 * Proper data sizes must be verified by deriving function,
	 * not available during init.
	 */
	econtext->aes_flags = (econtext->aes_flags | AES_FLAGS_DST_KEYSLOT | ksize_flag);
	econtext->se_ks.ks_target_keyslot = aes_kdf->kdf_index;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_kdf_derive_init(const crypto_context_t *ctx,
				    struct context_mem_s *cmem,
				    struct se_derive_state_s *s,
				    te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo,
				      &s->derive_aes_kdf.aes_context, cmem);
	CCC_ERROR_CHECK(ret);

	ret = aes_kdf_init_econtext(&s->derive_aes_kdf.aes_context.ec,
				    &args->aes_kdf);
	CCC_ERROR_CHECK(ret);

	// return a hint
	args->engine_hint = s->derive_aes_kdf.aes_context.ec.engine->e_id;
	s->derive_magic = SE_DERIVE_MAGIC;
	s->derive_selector = DRV_TYPE_AES;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

// Only accepts 128 and 256 bit keys length inpud data for nopad AES
// for ECB/CBC.
//
static status_t aes_kdf_do_derivation(const crypto_context_t *ctx,
				      struct se_derive_state_s *s,
				      te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t key_blen = 0U;
	struct se_aes_context *aes_context = NULL;

	LTRACEF("entry\n");

	if (TE_ALG_MODE_DERIVE != ctx->ctx_alg_mode) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	aes_context = &s->derive_aes_kdf.aes_context;

	if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_128) != 0U) {
		key_blen = 16U;
	} else if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_192) != 0U) {
		/* Require 256 bit derivation data for 192 bit keys.
		 * This writes the key as 256 bit key; user may choose to use
		 * only the first 192 bits of it as key.
		 */
		key_blen = 32U;
	} else if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_256) != 0U) {
		key_blen = 32U;
	} else {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (args->src_size != key_blen) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Input data size does not match derived key length\n"));
	}

	/* Outputs to keyslot, not memory; force dst <NULL,0> even if
	 * it would not be such by caller. Caller dst buffer will not
	 * get modified by AES key derivation.
	 */
	input.src = GET_DOMAIN_ADDR(ctx->ctx_domain, args->src);
	input.input_size = args->src_size;
	input.dst = NULL;
	input.output_size = 0U;

	aes_context->ec.is_last = true;

	ret = PROCESS_AES(&input, aes_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("derive operation 0x%x failed: %d\n",
				ctx->ctx_algo, ret));

	/* Even for 192 bit key wrote 32 bytes to keyslot */
	args->dst_size = key_blen;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("Generic (AES KDF) derivation", aes_context->ec.perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_kdf_set_key(struct se_derive_state_s *s,
				te_crypto_key_t *key,
				const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry\n");

	kdata = GET_DOMAIN_ADDR(s->derive_aes_kdf.aes_context.ec.domain,
				key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (kdata->k_key_type != KEY_TYPE_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Unsupported AES key type\n"));
	}

	ret = setup_aes_key(&s->derive_aes_kdf.aes_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AES key\n"));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES_KDF */

#if HAVE_SE_CMAC_KDF
static status_t cmac_kdf_init_econtext(struct se_engine_aes_context *econtext,
				       struct cmac_kdf_init_args_s *cmac_kdf)
{
	status_t ret = NO_ERROR;
	uint32_t ksize_flag = 0U;

	LTRACEF("entry\n");

	switch (cmac_kdf->kdf_key_bit_size) {
	case 128U: ksize_flag = AES_FLAGS_DERIVE_KEY_128; break;
	case 192U:
#if HAVE_AES_KEY192 == 0
		/* 192 bit AES keys are not supported by configuration
		 * Since the symmetric keyslot are only used for AES in
		 * T19X do not allow to even derive such keys if 192
		 * bit keys are disabled.
		 */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#else
		ksize_flag = AES_FLAGS_DERIVE_KEY_192;
#endif
		break;
	case 256U: ksize_flag = AES_FLAGS_DERIVE_KEY_256; break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (cmac_kdf->kdf_index >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid CMAC-AES keyslot target index %u",
					     cmac_kdf->kdf_index));
	}

	/* Deriving N bit key value to keyslot X
	 *
	 * Proper data sizes must be verified by deriving function,
	 * not available during init.
	 */
	econtext->aes_flags = (econtext->aes_flags | AES_FLAGS_DST_KEYSLOT | ksize_flag);
	econtext->se_ks.ks_target_keyslot = cmac_kdf->kdf_index;
	econtext->is_hash    = true;
	econtext->is_encrypt = false;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t nist_cmac_kdf_incompatible_flags(te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((args->mac.mac_flags & INIT_FLAG_MAC_DST_KEYSLOT) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NIST CMAC KDF derive with INIT_FLAG_MAC_DST_KEYSLOT\n"));
	}

	if ((args->mac.mac_flags & INIT_FLAG_MAC_DST_UPPER_QUAD) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NIST CMAC KDF derive with INIT_FLAG_MAC_UPPER_QUAD\n"));
	}

	if ((args->mac.mac_flags & INIT_FLAG_MAC_KEY_SIZE_MASK) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("NIST CMAC KDF derive with kdf key size flags\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_kdf_derive_init(const crypto_context_t *ctx,
				     struct context_mem_s *cmem,
				     struct se_derive_state_s *s,
				     te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = nist_cmac_kdf_incompatible_flags(args);
	CCC_ERROR_CHECK(ret);

	ret = aes_context_static_init(ctx->ctx_domain, args->engine_hint,
				      ctx->ctx_algo,
				      &s->derive_cmac_kdf.aes_context, cmem);
	CCC_ERROR_CHECK(ret);

	ret = cmac_kdf_init_econtext(&s->derive_cmac_kdf.aes_context.ec,
				     &args->cmac_kdf);
	CCC_ERROR_CHECK(ret);

	/* Setup  space for the CMAC subkey and CMAC intermediate/final value is
	 * callwlated to the phash buffer in aes context. This overlaps
	 * the iv_stash buffer used for other aes modes so it does not take up space.
	 *
	 * The engine layer uses the phash but it does not need the subkeys; the subkeys
	 * are used by the process layer.
	 */
	s->derive_cmac_kdf.aes_context.cmac.pk1 = &s->derive_cmac_kdf.pk1[0];
	s->derive_cmac_kdf.aes_context.cmac.pk2 = &s->derive_cmac_kdf.pk2[0];

	// return a hint
	args->engine_hint = s->derive_cmac_kdf.aes_context.ec.engine->e_id;
	s->derive_magic = SE_DERIVE_MAGIC;

	/* CMAC KDF with TE_ALG_MODE_DERIVATION always uses NIST 800-108 encoder
	 * for input data. If you do not want this, you need to use the TE_ALG_MODE_MAC
	 * operation in which you can set the destination to be a keyslot.
	 * That mode allows CMAC-AES key derivation without NIST encoder.
	 */
	s->derive_selector = DRV_TYPE_CMAC_NIST_ENCODER;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_cmac_derive_bitlen(const struct se_aes_context *aes_context,
				       uint32_t *key_bitlen_p)
{
	status_t ret = NO_ERROR;
	uint32_t key_bitlen = 0U;

	LTRACEF("entry\n");

	*key_bitlen_p = 0U;

	if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_128) != 0U) {
		key_bitlen = 128U;
	} else if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_192) != 0U) {
		/* Require 256 bit derivation data for 192 bit keys.
		 * This writes the key as 256 bit key (two blocks);
		 * caller may choose to use only the first 192 bits of
		 * it as key.
		 */
		key_bitlen = 256U;
	} else if ((aes_context->ec.aes_flags & AES_FLAGS_DERIVE_KEY_256) != 0U) {
		key_bitlen = 256U;
	} else {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*key_bitlen_p = key_bitlen;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}


static status_t cmac_kdf_derive_upper_quad(struct se_aes_context *aes_context,
					   struct se_data_params *input)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Need to derive UPPER QUAD keyslot value with a second CMAC.
	 */
	aes_context->ec.aes_flags |= AES_FLAGS_DERIVE_KEY_HIGH;

	aes_context->ec.is_first  = true;
	aes_context->ec.is_last   = true;
	aes_context->used = 0U;

	/* Need to derive UPPER QUAD keyslot value
	 */
	ret = PROCESS_AES_CMAC(input, aes_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("CMAC upper quad derive operation failed: %d\n",
				ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_kdf_do_derivation_cleanup(const struct se_aes_context *aes_context,
					       uint8_t *kdf_encoded_dma_buf,
					       uint32_t kdf_encoded_len,
					       bool clear_kslots)
{
	status_t ret = NO_ERROR;

	if (NULL != kdf_encoded_dma_buf) {

		if (0U != kdf_encoded_len) {
			se_util_mem_set(kdf_encoded_dma_buf, 0U, kdf_encoded_len);
		}

		CMTAG_MEM_RELEASE(aes_context->ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  kdf_encoded_dma_buf);
	}

	if (BOOL_IS_TRUE(clear_kslots)) {
		status_t retu = NO_ERROR;
		bool leave_key = false;

		if (NULL == aes_context) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		/* Clear dst keyslot on error when potentially wrote part of it.
		 *
		 * Also check derivation leave_key if the derivation key needs to be cleared.
		 * Due to two step nature of long key derivation an error in the middle
		 * might not have erased this even though caller intended so.
		 */
		leave_key = ((aes_context->ec.aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U);

		HW_MUTEX_TAKE_ENGINE(aes_context->ec.engine);

		if (!BOOL_IS_TRUE(leave_key)) {
			ret = aes_write_key_iv_locked(aes_context->ec.engine,
						      aes_context->ec.se_ks.ks_slot, 256U,
						      SE_AES_KSLOT_SEL_KEY, NULL);
		}

		retu = aes_write_key_iv_locked(aes_context->ec.engine,
					       aes_context->ec.se_ks.ks_target_keyslot, 256U,
					       SE_AES_KSLOT_SEL_KEY, NULL);

		HW_MUTEX_RELEASE_ENGINE(aes_context->ec.engine);

		if (NO_ERROR == ret) {
			ret = retu;
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* aes flags to derive lower quadword value of the keyslot first
 */
static void cmac_kdf_set_lower_quad_flags(struct se_aes_context *aes_context,
					  uint32_t key_bitlen)
{
	aes_context->ec.aes_flags &= ~AES_FLAGS_DERIVE_KEY_HIGH;

	if (key_bitlen > 128U) {
		if ((aes_context->ec.aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
			aes_context->ec.aes_flags |= AES_FLAGS_LEAVE_KEY_TO_KEYSLOT;
		}
	}
}

/* Only support NIST 800-108 Counter mode for CMAC-AES key derivation
 * with TE_ALG_MODE_DERIVATION for 128 or 256 bit keys. 192 bit keys
 * (if supported by configuration) are written as 256 bit keys to keyslots,
 * the client may use them as 192 bit keys, as T19X HW does not limit
 * key use with any supported size.
 *
 * For non-encoded CMAC-AES key derivation, use TE_ALG_MODE_MAC and set the destination
 * to be a keyslot. This is in crypto_mac.c. That version writes the CMAC output to
 * either lower or upper quadswords of the destination keyslot.
 */
static status_t cmac_kdf_do_derivation(const crypto_context_t *ctx,
				       struct se_derive_state_s *s,
				       te_args_data_t *args)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t key_bitlen = 0U;
	struct se_aes_context *aes_context = NULL;
	uint8_t *kdf_encoded_dma_buf = NULL;
	uint32_t kdf_encoded_len = 0U;
	uint32_t saved_aes_flags = 0U;
	bool clear_kslots = false;

	LTRACEF("entry\n");

	if (TE_ALG_MODE_DERIVE != ctx->ctx_alg_mode) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	aes_context = &s->derive_cmac_kdf.aes_context;

	ret = get_cmac_derive_bitlen(aes_context, &key_bitlen);
	CCC_ERROR_CHECK(ret);

	input.src = NULL;
	input.input_size = 0U;

	if (DRV_TYPE_CMAC_NIST_ENCODER != s->derive_selector) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Invalid selector for CMAC-AES with NIST encoder\n"));
	}

	ret = kdf_nist_counter_dma_buf_setup(&kdf_encoded_dma_buf,
					     &kdf_encoded_len,
					     s->derive_cmac_kdf.aes_context.ec.cmem,
					     key_bitlen,
					     args->src_kdf_label,
					     args->src_kdf_label_len,
					     args->src_kdf_context,
					     args->src_kdf_context_len);
	CCC_ERROR_CHECK(ret);

	input.src        = kdf_encoded_dma_buf;
	input.input_size = kdf_encoded_len;

	LOG_HEXBUF("NIST encoded for CMAC", input.src, input.input_size);

	/* Outputs to keyslot, not memory; force dst <NULL,0> even if
	 * it would not be such by caller. Caller dst buffer will not
	 * get modified by AES key derivation.
	 */
	input.dst = NULL;
	input.output_size = 0U;

	aes_context->ec.is_last = true;

	saved_aes_flags = aes_context->ec.aes_flags;

	cmac_kdf_set_lower_quad_flags(aes_context, key_bitlen);

	/* Potentially need to clear both derivation and target kslot */
	clear_kslots = true;

	/* Need to derive LOWER QUAD keyslot value
	 */
	ret = PROCESS_AES_CMAC(&input, aes_context);

	/* Restore aes flags before result check */
	aes_context->ec.aes_flags = saved_aes_flags;

	CCC_ERROR_CHECK(ret,
			LTRACEF("CMAC derive operation failed: %d\n",
				ret));

	if (key_bitlen > 128U) {
		/* Bump up NIST counter value to 2 for the upper quad key derivation
		 *
		 * Run a second CMAC (independent CMAC-AES) with a modified
		 * input data octet stream: the NIST 800-108 counter field
		 * needs to be bumbed from (as 4 byte big endian) value 1 to 2.
		 *
		 * Then a second CMAC key derivation is run after setting the
		 * AES_FLAGS_DERIVE_KEY_HIGH which instructs the low level
		 * code to place the 16 byte result to the keyslot upper
		 * quad. Otherwise the same CMAC contextx are used.
		 */
		LOG_INFO ("Upper quad handler\n");

		ret = kdf_nist_encode_counter_set(2U, kdf_encoded_dma_buf);
		CCC_ERROR_CHECK(ret);

		ret = cmac_kdf_derive_upper_quad(aes_context, &input);
		CCC_ERROR_CHECK(ret);
	}

	/* Even for 192 bit key wrote 32 bytes to keyslot */
	args->dst_size = (key_bitlen / 8U);

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	show_elapsed_time("Generic (CMAC KDF) derivation", aes_context->ec.perf_usec);
#endif

	clear_kslots = false;
fail:
	cmac_kdf_do_derivation_cleanup(aes_context, kdf_encoded_dma_buf,
				       kdf_encoded_len, clear_kslots);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Since over 128 bit key derivation requires two separate
 * CMAC-AES operations, each of which need to use the same
 * keyslot which is set by the CMAC-AES key derivation =>
 * not possible to overwrite the keyslot used for the CMAC
 * operation.
 *
 * If 128 bit key is derived => in that case overwriting
 * the derivation keyslot is supported.
 */
static status_t cmac_kdf_check_keyslot_overwrite(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	if ((econtext->aes_flags & AES_FLAGS_DERIVE_KEY_128) == 0U) {
		/* Must be deriving one of these key sizes here,
		 * these can not all be unset.
		 */
		if ((econtext->aes_flags & (AES_FLAGS_DERIVE_KEY_192 |
					    AES_FLAGS_DERIVE_KEY_256)) == 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		if (econtext->se_ks.ks_target_keyslot == econtext->se_ks.ks_slot) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("CMAC-AES KDF key overwrite not supported with long keys\n"));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_kdf_set_key(struct se_derive_state_s *s,
				 te_crypto_key_t *key,
				 const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	const te_args_key_data_t *kdata = NULL;

	LTRACEF("entry\n");

	kdata = GET_DOMAIN_ADDR(s->derive_cmac_kdf.aes_context.ec.domain,
				key_args);
	if (NULL == kdata) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (kdata->k_key_type != KEY_TYPE_AES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Unsupported AES key type\n"));
	}

	ret = setup_aes_key(&s->derive_cmac_kdf.aes_context, key, kdata);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to setup AES key\n"));

	ret = cmac_kdf_check_keyslot_overwrite(&s->derive_cmac_kdf.aes_context.ec);
	CCC_ERROR_CHECK(ret);

	ret = cmac_process_derive_subkeys(&s->derive_cmac_kdf.aes_context);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_CMAC_KDF */

static status_t se_crypto_derive_init_check_args(const crypto_context_t *ctx,
						 const te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ctx) || (NULL == args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL != ctx->ctx_run_state.rs.s_object) ||
	    (RUN_STATE_SELECT_NONE != ctx->ctx_run_state.rs_select)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("derive init: state already set up\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_derive_algo_init(crypto_context_t *ctx,
				    struct context_mem_s *cmem,
				    te_args_init_t *args,
				    struct se_derive_state_s *s)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch(ctx->ctx_algo) {
#if CCC_WITH_ECDH
	case TE_ALG_ECDH:
		ret = ecdh_derive_init(ctx, cmem, s, args);
		break;
#endif

#if CCC_WITH_X25519
	case TE_ALG_X25519:
		ret = ecdh_derive_init(ctx, cmem, s, args);
		break;
#endif

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
		ret = kdf_derive_init(ctx, cmem, s, args);
		break;
#endif

#if HAVE_SE_AES_KDF
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
		ret = aes_kdf_derive_init(ctx, cmem, s, args);
		break;
#endif

#if HAVE_SE_CMAC_KDF
	case TE_ALG_CMAC_AES:
		ret = cmac_kdf_derive_init(ctx, cmem, s, args);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("derivation algorithm 0x%x unsupported\n",
				  ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
static void release_derive_context_dma_buf(struct se_derive_state_s *s)
{
	if (NULL != s) {
#if HAVE_SE_KAC_KDF
		if ((DRV_TYPE_MAC == s->derive_selector) ||
		    (DRV_TYPE_MAC_NIST_ENCODER == s->derive_selector)) {
			sha_context_dma_buf_release(&s->kac_kdf.kd_context);
		}
#endif
#if HAVE_SE_AES_KDF
		if (DRV_TYPE_AES == s->derive_selector) {
			aes_context_dma_buf_release(&s->derive_aes_kdf.aes_context);
		}
#endif
#if HAVE_SE_CMAC_KDF
		if (DRV_TYPE_CMAC_NIST_ENCODER == s->derive_selector) {
			aes_context_dma_buf_release(&s->derive_cmac_kdf.aes_context);
		}
#endif
		/* Other derive contexts not using separate dma buffers
		 * or types are not enabled => no need to release.
		 */
	}
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

status_t se_crypto_derive_init(crypto_context_t *ctx, te_args_init_t *args)
{
	status_t ret = NO_ERROR;
	struct se_derive_state_s *s = NULL;
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	MEASURE_MEMORY_START("Generic derivation");

	ret = se_crypto_derive_init_check_args(ctx, args);
	CCC_ERROR_CHECK(ret);

#if HAVE_CONTEXT_MEMORY
	/* CMEM may be NULL or checked cmem setup after this call.
	 *
	 * If NULL, CCC uses subsystem memory functions for this context.
	 *
	 * If not NULL, CCC manages the CMEM object internally and does not use
	 * subsystem memory functions for this context.
	 */
	ret = context_memory_get_init(&ctx->ctx_mem, &cmem, &derive_mem_specs);
	CCC_ERROR_CHECK(ret);
#endif

	/*
	 * Use contiguous memory, aligned by cache line.
	 */
	s = CMTAG_MEM_GET_OBJECT(cmem,
				 CMTAG_API_STATE,
				 CCC_DMA_MEM_ALIGN,
				 struct se_derive_state_s,
				 struct se_derive_state_s *);
	if (NULL == s) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	if (TE_ALG_MODE_DERIVE != ctx->ctx_alg_mode) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("derivation algo 0x%x mode %u unsupported\n",
						       ctx->ctx_algo, ctx->ctx_alg_mode));
	}

	ret = context_get_key_mem(ctx);
	CCC_ERROR_CHECK(ret);

	ret = se_derive_algo_init(ctx, cmem, args, s);
	CCC_ERROR_CHECK(ret);

	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_DRV;
	ctx->ctx_run_state.rs.s_drv = s;

	s = NULL;

fail:
	if (NULL != s) {
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		release_derive_context_dma_buf(s);
#endif
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_API_STATE,
				  s);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_derive_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				  te_args_data_t *args, te_args_init_t *init_args)
{
	status_t ret = NO_ERROR;
	struct se_derive_state_s *s = NULL;

	(void)init_args;

	LTRACEF("entry\n");

	if ((ctx->ctx_handle_state & TE_HANDLE_FLAG_KEY_SET) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Derive key not defined\n"));
	}

	LTRACEF("DST: %p, dst_size %u\n", args->dst, args->dst_size);
	LTRACEF("SRC: %p, src_size %u\n", args->src, args->src_size);

	if (RUN_STATE_SELECT_DRV != run_state->rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for DRV\n",
					     run_state->rs_select));
	}

	s = run_state->rs.s_drv;
	if ((NULL == s) || (s->derive_magic != SE_DERIVE_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Derive magic invalid\n"));
	}

	switch(ctx->ctx_algo) {
#if CCC_WITH_ECDH
	case TE_ALG_ECDH:
		ret = ecdh_do_derivation(ctx, s, args);
		break;
#endif
#if CCC_WITH_X25519
	case TE_ALG_X25519:
		ret = ecdh_do_derivation(ctx, s, args);
		break;
#endif

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
		/* Derives new key, the destination is a keyslot for these HW ops
		 */
		ret = kdf_do_derivation(ctx, s, args);
		break;
#endif

#if HAVE_SE_AES_KDF
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
		ret = aes_kdf_do_derivation(ctx, s, args);
		break;
#endif

#if HAVE_SE_CMAC_KDF
	case TE_ALG_CMAC_AES:
		ret = cmac_kdf_do_derivation(ctx, s, args);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("derivation algorithm 0x%x unsupported\n",
				  ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_derive_reset(crypto_context_t *ctx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ctx) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ctx->ctx_class != TE_CLASS_KEY_DERIVATION) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("Attempt to DERIVE reset with %u class object\n",
						       ctx->ctx_class));
	}

	if (NULL != ctx->ctx_run_state.rs.s_object) {
		if (RUN_STATE_SELECT_DRV != ctx->ctx_run_state.rs_select) {
			LTRACEF("Invalid state selector 0x%x for DRV\n",
				ctx->ctx_run_state.rs_select);
		} else {
#if CCC_WITH_ECDH || CCC_WITH_X25519
			struct se_derive_state_s *s = ctx->ctx_run_state.rs.s_drv;

			if ((SE_DERIVE_MAGIC == s->derive_magic) &&
			    (DRV_TYPE_EC == s->derive_selector)) {
				/* EC PKA engine object may need cleanup
				 * TODO => could make more generic
				 *
				 * (could add a reset function to all
				 * engines and just call them in a
				 * more generic way, if not NULL)
				 */
				PKA_DATA_EC_RELEASE(&s->ec_context.ec);
			}
#endif /* CCC_WITH_ECDH || CCC_WITH_X25519 */

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
			release_derive_context_dma_buf(s);
#endif
			se_util_mem_set(ctx->ctx_run_state.rs.s_object, 0U, sizeof_u32(struct se_derive_state_s));
		}
		CMTAG_MEM_RELEASE(context_memory_get(&ctx->ctx_mem),
				  CMTAG_API_STATE,
				  ctx->ctx_run_state.rs.s_object);
	}
	ctx->ctx_run_state.rs_select = RUN_STATE_SELECT_NONE;

fail:
	MEASURE_MEMORY_STOP;

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_crypto_derive_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				  const te_args_key_data_t *key_args)
{
	status_t ret = NO_ERROR;
	struct se_derive_state_s *s = NULL;

	LTRACEF("entry: ctx %p\n", ctx);

	if ((NULL == ctx) || (NULL == key) || (NULL == key_args)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (RUN_STATE_SELECT_DRV != ctx->ctx_run_state.rs_select) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Invalid selector 0x%x for DRV\n",
					     ctx->ctx_run_state.rs_select));
	}

	s = ctx->ctx_run_state.rs.s_drv;
	if ((NULL == s) || (s->derive_magic != SE_DERIVE_MAGIC)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	switch(ctx->ctx_algo) {
#if CCC_WITH_X25519
	case TE_ALG_X25519:
		ret = ecdh_set_key(s, key, key_args);
		break;
#endif
#if CCC_WITH_ECDH
	case TE_ALG_ECDH:
		ret = ecdh_set_key(s, key, key_args);
		break;
#endif

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
		/* Set the key(s) for KDF key derivation.
		 */
		ret = kdf_set_key(s, key, key_args);
		break;
#endif

#if HAVE_SE_AES_KDF
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
		ret = aes_kdf_set_key(s, key, key_args);
		break;
#endif

#if HAVE_SE_CMAC_KDF
	case TE_ALG_CMAC_AES:
		ret = cmac_kdf_set_key(s, key, key_args);
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("derivation algorithm 0x%x unsupported\n",
				  ctx->ctx_algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CCC_KDF */
