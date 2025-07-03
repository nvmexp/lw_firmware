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

/* Support for symmetric block ciphers with SE engine (AES-*)
 *
 * Other symmetric cipher algorithms can be supported in SW (fully or partially)
 * by adding calls from here.
 */
#include <crypto_system_config.h>

#if HAVE_SE_AES

#include <crypto_cipher_proc.h>
#include <crypto_process_call.h>

#if HAVE_AES_CCM
#include <aes-ccm/crypto_ops_ccm.h>
#endif

#if HAVE_AES_GCM
#include <aes-gcm/crypto_ops_gcm.h>
#endif

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t call_aes_process_handler(struct se_data_params *input,
				  struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
#if HAVE_AES_CCM
		if (TE_ALG_AES_CCM == aes_context->ec.algorithm) {
			/* Special handler before HW switch call */
			ret = PROCESS_AES_CCM(input, aes_context);
			CCC_LOOP_STOP;
		}
#endif

#if HAVE_AES_GCM
		if (TE_ALG_AES_GCM == aes_context->ec.algorithm) {
			/* Special handler before HW switch call */
			ret = PROCESS_AES_GCM(input, aes_context);
			CCC_LOOP_STOP;
		}
#endif

		ret = PROCESS_AES(input, aes_context);
		break;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_engine_context_static_init_cm(te_crypto_domain_t domain,
					   engine_id_t engine_id,
					   te_crypto_algo_t algo,
					   struct se_engine_aes_context *aes_econtext,
					   struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	engine_class_t engine_class_aes = CCC_ENGINE_CLASS_AES;

	LTRACEF("entry\n");
	if (NULL == aes_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (algo) {
#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES:
		/* Need to select an engine with CMAC support */
		engine_class_aes = CCC_ENGINE_CLASS_CMAC_AES;
		break;
#endif

	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_OFB:
#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
#endif
#if HAVE_GMAC_AES
	case TE_ALG_GMAC_AES:
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
		break;

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		engine_class_aes = CCC_ENGINE_CLASS_AES_XTS;
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case TE_ALG_KEYOP_KWUW:
	case TE_ALG_KEYOP_KW:
	case TE_ALG_KEYOP_KUW:
		engine_class_aes = CCC_ENGINE_CLASS_KW;
		break;
#endif

		// Unfortunately no time yet to implement any of these
	case TE_ALG_AES_CTS:
	default:
		CCC_ERROR_MESSAGE("Unsupported/unimplemented AES algorithm mode 0x%x\n",
			      algo);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	CCC_ERROR_CHECK(ret);

	aes_econtext->algorithm = algo;
	aes_econtext->is_first = true;
	aes_econtext->is_last  = false;
	aes_econtext->is_hash  = false;
	aes_econtext->domain   = domain;
	aes_econtext->cmem     = cmem;

#if TEGRA_MEASURE_PERFORMANCE
	aes_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

#if HAVE_CMAC_AES
	if (TE_ALG_CMAC_AES == algo) {
		aes_econtext->is_hash = true;
	}
#endif

#if HAVE_GMAC_AES
	if (TE_ALG_GMAC_AES == algo) {
		aes_econtext->is_hash = true;
	}
#endif

	ret = ccc_select_engine(&aes_econtext->engine, engine_class_aes, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES engine selection failed: %u\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CMAC_AES
static status_t cinit_cmac(te_crypto_domain_t domain,
			   struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	(void)domain;
	(void)aes_context;

	LTRACEF("entry\n");

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_AES */

#if HAVE_AES_CCM
static status_t cinit_ccm(te_crypto_domain_t domain,
			  struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)aes_context;
	(void)domain;

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_CCM */

#if HAVE_SE_KAC_KEYOP
static status_t cinit_kwuw(te_crypto_domain_t domain,
			   struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)domain;

	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Prefer not to const the argument.
	 */
	FI_BARRIER(u32, aes_context->used);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KEYOP */

static status_t cinit_aes(te_crypto_domain_t domain,
			  struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	(void)domain;

	LTRACEF("entry\n");

	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Prefer not to const the argument.
	 */
	FI_BARRIER(u32, aes_context->used);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
static status_t aes_context_dma_buf_alloc(struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	uint8_t *buf = NULL;

	LTRACEF("entry\n");
	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != aes_context->buf) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	buf = CMTAG_MEM_GET_BUFFER(aes_context->ec.cmem,
				   CMTAG_ALIGNED_DMA_BUFFER,
				   CCC_DMA_MEM_ALIGN,
				   CCC_DMA_MEM_AES_BUFFER_SIZE);
	if (NULL == buf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY,
				     LTRACEF("AES context dma memory alloc failed: %u bytes\n",
					     CCC_DMA_MEM_AES_BUFFER_SIZE));
	}
	aes_context->buf = buf;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void aes_context_dma_buf_release(struct se_aes_context *aes_context)
{
	LTRACEF("entry\n");
	if ((NULL != aes_context) && (NULL != aes_context->buf)) {
		se_util_mem_set(aes_context->buf, 0U, CCC_DMA_MEM_AES_BUFFER_SIZE);
		CMTAG_MEM_RELEASE(aes_context->ec.cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  aes_context->buf);
	}
	LTRACEF("exit\n");
}
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

status_t aes_context_static_init(te_crypto_domain_t domain,
				 engine_id_t engine_id, te_crypto_algo_t algo,
				 struct se_aes_context *aes_context,
				 struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (algo) {
	case TE_ALG_CMAC_AES:
#if HAVE_CMAC_AES
		ret = cinit_cmac(domain, aes_context);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_AES_GCM:
	case TE_ALG_GMAC_AES:
#if CCC_WITH_AES_GCM
		ret = cinit_gcm(domain, aes_context);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_AES_CCM:
#if HAVE_AES_CCM
		ret = cinit_ccm(domain, aes_context);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_KWUW:
	case TE_ALG_KEYOP_KW:
	case TE_ALG_KEYOP_KUW:
#if HAVE_SE_KAC_KEYOP
		ret = cinit_kwuw(domain, aes_context);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = cinit_aes(domain, aes_context);
		break;
	}

	CCC_ERROR_CHECK(ret);

	ret = aes_engine_context_static_init_cm(domain, engine_id, algo,
						&aes_context->ec, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES engine context setup failed: %u\n", ret));

	/* CMAC users note: The caller must set up these !!!
	 */
	aes_context->cmac.pk1   = NULL;
	aes_context->cmac.pk2   = NULL;

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
	/* HW engines for AES access to context buffers
	 * with a DMA engine.
	 */
	ret = aes_context_dma_buf_alloc(aes_context);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_init_econtext(struct se_engine_aes_context *econtext,
			   te_args_init_t *args)
{
	status_t ret = NO_ERROR;

	(void)args;

	LTRACEF("entry\n");

	switch(econtext->algorithm) {
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
	case TE_ALG_AES_OFB:
	case TE_ALG_AES_CTS:
		break;

#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
		/* Treat AES-CTR zero increment as 1 (XXX AFAIK, this
		 * is the standard default increment). Because this
		 * default: increment does not need to be explicitly set
		 * in call.
		 */
		if (0U == args->aes.ctr_mode.ci_increment) {
			args->aes.ctr_mode.ci_increment = 1U;
		}

		econtext->ctr.increment =
			args->aes.ctr_mode.ci_increment;

		/* Treat the AES-CTR mode counter as "big endian" by
		 * default and do big_endian increment to it by
		 * default (i.e. field == 0); this is what NIST
		 * AES-CTR requires.
		 *
		 * If you need non-standard SE HW behaviour, set this LE
		 * field non-zero in the call (and please let me know
		 * why you need to do so)!
		 */
		if (0U == args->aes.ctr_mode.ci_counter_little_endian) {
			econtext->aes_flags |= AES_FLAGS_CTR_MODE_BIG_ENDIAN;
		} else {
			/* "LE" counters are not supported
			 * and the HW feature is not useful.
			 *
			 * It would be possible to swap the counter bytes, but
			 * there is no real point in such support.
			 */
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
		}

		se_util_mem_move((uint8_t *)econtext->ctr.counter,
				 args->aes.ctr_mode.ci_counter,
				 sizeof_u32(econtext->ctr.counter));

		LTRACEF("AES-CTR value set, increment %u\n",
			econtext->ctr.increment);
		break;
#endif /* HAVE_AES_CTR */

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		/* Valid "unused bits" range (number of bits to ignore
		 * in last byte): 0..7
		 */
		if (args->aes.xts_mode.ci_xts_last_byte_unused_bits > 7U) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		econtext->xts.last_byte_unused_bits =
			args->aes.xts_mode.ci_xts_last_byte_unused_bits;

		if (0U != econtext->xts.last_byte_unused_bits) {
			LTRACEF("AES-XTS mode data not byte aligned, last byte has %u unused bits\n",
				econtext->xts.last_byte_unused_bits);
		}

		/* AES-XTS tweak is always taken as LITTLE ENDIAN
		 * (TODO: could support also big endian tweaks => add a flag if required).
		 */
		se_util_mem_move((uint8_t *)econtext->xts.tweak,
				 args->aes.xts_mode.ci_xts_tweak,
				 sizeof_u32(econtext->xts.tweak));
		LOG_HEXBUF("AES-XTS tweak value set", econtext->xts.tweak, 16U);
		break;
#endif /* HAVE_AES_XTS */

#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
		// padding algorithms...
		break;
#endif

	default:
		CCC_ERROR_MESSAGE("Cipher algorithm 0x%x unsupported\n",
			      econtext->algorithm);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit, ret: %d\n", ret);
	return ret;
}

static status_t setup_aes_key_arg_check(const struct se_aes_context *aes_context,
					const te_crypto_key_t *key,
					const te_args_key_data_t *kargs)
{
	status_t ret = NO_ERROR;

	if ((NULL == aes_context) || (NULL == key) || (NULL == kargs)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}


	switch (kargs->k_byte_size) {
	case (192U / 8U):
#if HAVE_AES_KEY192 == 0
		/* Some configurations disable 192 bit keys for AES.
		 */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case (128U / 8U):
	case (256U / 8U):
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Unsupported AES key length\n"));

#if HAVE_AES_XTS
	/* HW may work but the spec does not define XTS with 192 bit AES key size
	 * TODO: Consider supporting this (why is it not defined in the spec)?
	 */
	if ((TE_ALG_AES_XTS == aes_context->ec.algorithm) &&
	    ((192U / 8U) == kargs->k_byte_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-XTS does not support 192 bit keysize\n"));
	}
#endif /* HAVE_AES_XTS */

fail:
	return ret;
}

static status_t get_aes_keyslot(const struct se_aes_context *aes_context,
				te_crypto_key_t *key,
				const te_args_key_data_t *kargs)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)kargs;
	(void)aes_context;

	if ((key->k_flags & KEY_FLAG_DYNAMIC_SLOT) != 0U) {
		if (((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) ||
		    ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Dynamic keyslot can not use persistent values\n"));
		}
#if HAVE_CRYPTO_KCONFIG || defined(SE_AES_PREDEFINED_DYNAMIC_KEYSLOT)
		/* CCC needs a dynamic keyslot.
		 *
		 * If not enabled, the operation fails since there
		 * are no AES engines that operate without keyslots.
		 */
		ret = aes_engine_get_dynamic_keyslot(&aes_context->ec,
						     &key->k_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("AES dynamic keyslot not set up\n"));
#else /* HAVE_CRYPTO_KCONFIG || SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
			    LTRACEF("AES dynamic keyslot not supported"));
#endif /* HAVE_CRYPTO_KCONFIG || SE_RSA_PREDEFINED_DYNAMIC_KEYSLOT */
	}

	if (key->k_keyslot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("AES key slot value too large (%u) and slot not dynamic\n",
						   key->k_keyslot));
	}

	/* XXXX TODO: Adapt the dynamic keyslot code for T23X
	 * XXXX  XTS which needs two separate keyslots.
	 */
#if HAVE_SOC_FEATURE_XTS_SUBKEY_SLOT
	if (TE_ALG_AES_XTS == aes_context->ec.algorithm) {
		// Now uses client provided value for XTS SUBKEY slot
		key->k_tweak_keyslot = kargs->k_tweak_keyslot;

		if (key->k_tweak_keyslot >= SE_AES_MAX_KEYSLOTS) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("AES subkey slot value too large (%u) and slot not dynamic\n",
							       key->k_tweak_keyslot));
		}
	}
#else
	/* There is no separate keyslot for subkeys in older SoCs.
	 * So just use the appended key from master keyslot for both
	 * (One keyslot contains two concatenated keys).
	 */
	key->k_tweak_keyslot = key->k_keyslot;
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Shared between aes key users. */
status_t setup_aes_key(struct se_aes_context *aes_context,
		       te_crypto_key_t *key,
		       const te_args_key_data_t *kargs)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = setup_aes_key_arg_check(aes_context, key, kargs);
	CCC_ERROR_CHECK(ret);

	key->k_byte_size = kargs->k_byte_size;
	if (key->k_byte_size > (256U / 8U)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}
	aes_context->ec.se_ks.ks_bitsize = (key->k_byte_size * 8U);

	key->k_key_type  = kargs->k_key_type;
	key->k_flags     = kargs->k_flags;
	key->k_keyslot   = kargs->k_keyslot;

	ret = get_aes_keyslot(aes_context, key, kargs);
	CCC_ERROR_CHECK(ret);

	if ((key->k_flags & KEY_FLAG_USE_KEYSLOT_KEY) != 0U) {
		LTRACEF("Using existing AES key in slot: %u\n", key->k_keyslot);
		aes_context->ec.aes_flags |= AES_FLAGS_USE_PRESET_KEY;
		aes_context->ec.se_ks.ks_keydata = NULL;
	} else {
		uint32_t kmat_size = key->k_byte_size;

		/* Pre-set IV not allowed when CCC writes the key to the keyslot, so
		 * if using pre-set IV the key must be pre-set as well.
		 */
		if ((aes_context->ec.aes_flags & AES_FLAGS_USE_KEYSLOT_IV) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AES pre-set IV only usable with pre-set key\n"));
		}

#if HAVE_AES_XTS
		/*
		 * For AES-XTS, the subkey is just catenated after the AES key
		 * so the key material is twice as long as the AES keylength.
		 *
		 * This is just a handy convention which seems to be
		 * used also in NIST AES-XTS test suite tests providing
		 * a catenated key this way.
		 */
		if (aes_context->ec.algorithm == TE_ALG_AES_XTS) {
			kmat_size = key->k_byte_size * 2U;
		}
#endif
		/* Copy the caller AES key material to context.
		 * key->k_aes_key is word aligned.
		 */
		se_util_mem_move(key->k_aes_key, kargs->k_aes_key, kmat_size);
		aes_context->ec.se_ks.ks_keydata = &key->k_aes_key[0];
#if SE_RD_DEBUG
		LTRACEF("Defining AES key for slot: %u\n", key->k_keyslot);
		LOG_HEXBUF("AES key", aes_context->ec.se_ks.ks_keydata, key->k_byte_size);
#if HAVE_AES_XTS
		if (TE_ALG_AES_XTS == aes_context->ec.algorithm) {
			LOG_HEXBUF("AES-XTS subkey", &aes_context->ec.se_ks.ks_keydata[key->k_byte_size ], key->k_byte_size);
		}
#endif
#endif /* SE_RD_DEBUG */
	}

	aes_context->ec.se_ks.ks_slot = key->k_keyslot;
	aes_context->ec.se_ks.ks_subkey_slot = key->k_tweak_keyslot;

	if ((key->k_flags & KEY_FLAG_LEAVE_KEY_IN_KEYSLOT) != 0U) {
		aes_context->ec.aes_flags |= AES_FLAGS_LEAVE_KEY_TO_KEYSLOT;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES */
