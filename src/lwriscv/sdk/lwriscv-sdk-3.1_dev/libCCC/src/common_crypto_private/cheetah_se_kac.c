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

/* All SE engine HW related code should be in this file.
 * If not => move it here and move other code out of here (TODO).
 */

/* NOTE: This file is now only used in T23X platform devices and it is
 * still UNDER CONSTRUCTION
 */

/* T23X CMOD does not set the IV back to linear counter regs.  Also,
 * HW does not recommend to use that feature even if it would set it
 * in this SoC => CCC will not use the register read-back feature in
 * this platform.
 *
 * FIXME => TODO => potential issue if platform CPU can not access
 *  all of SE DMA memory range!
 *
 * => This would not work if subsystem CPU can not access the memory
 *    used by SE AES DMA engine!
 *
 *    TODO: Verify if this can happen in any platform.
 */

/* NOTE about key slot usage. This code does not specify keyslot
 * allocation, it uses keyslots as requested by the client calls.
 *
 * Any keyslot allocation needs to be done externally. If such
 * allocation can be agreed, then CCC could then check that it only
 * accesses the allowed keyslots. But there is no such agreement and
 * CCC will do as requested by the client calls.
 */

// R&D FEATURE!!! (Probably no need in production) => REMOVE
#define HAVE_KAC_READ 0	// XXX FIXME: Remove this => dump KAC keyslot buffers (R&D feature only)

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#include <include/tegra_se.h>
#include <tegra_se_gen.h>
#include <tegra_se_kac_intern.h>

#ifndef USE_HW_CMAC
#define USE_HW_CMAC 0
#endif

#if CCC_WITH_AES_GCM
#include <tegra_se_kac_gcm.h>
#endif

#if HAVE_SOC_FEATURE_KAC == 0
#error "HAVE_SOC_FEATURE_KAC not set when compiling tegra_se_kac.c"
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if defined(CCC_SET_SE_APERTURE) && !defined(CCC_SET_SE_APERTURE_PROTO_DECLARED)
/* T23X PSC memory aperture management function defined by the sybsystem.
 *
 * This is the protype of that function. See PSC IAS for details.
 *
 * From CCC this external function is only called from this file via
 * function se_set_aperture()
 */
status_t CCC_SET_SE_APERTURE(PHYS_ADDR_T src_paddr, uint32_t slen,
			     PHYS_ADDR_T dst_paddr, uint32_t dlen);
#endif

#if HAVE_SE_SHA || HAVE_SE_AES

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/****************************/

#if LOCAL_TRACE
static void rd_print_manifest(const struct kac_manifest *mf)
{
	const char *origin[] = { "HW", "PSC", "TZ", "NS", "FSI", };
	const char *user[] = { "RESERVED", "PSC", "TZ", "NS", "FSI", };
	const char *purpose[] = { "ENC", "CMAC", "HMAC", "KW", "KUW", "KWUW",
				  "KDK", "KDD", "KDD_KUW", "XTS", "GCM", };
	const char *size[] = { "128", "192", "256", };

	if (NULL != mf) {
		LOG_INFO("Manifest:\n");
		LOG_INFO(" origin : %s\n", origin[mf->mf_origin]);
		LOG_INFO(" user   : %s\n", user[mf->mf_user]);
		LOG_INFO(" purpose: %s\n", purpose[mf->mf_purpose]);
		LOG_INFO(" size   : %s\n", size[mf->mf_size]);
	}
}
#endif /* LOCAL_TRACE */

static status_t se_map_algorithm_to_purpose(te_crypto_algo_t algorithm,
					    kac_manifest_purpose_t *purpose_p,
					    uint32_t kac_flags)
{
	status_t ret = NO_ERROR;
	kac_manifest_purpose_t kac_purpose = KAC_MANIFEST_PURPOSE_UNKNOWN;

	(void)kac_flags;

	LTRACEF("entry\n");

	if (NULL == purpose_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*purpose_p = KAC_MANIFEST_PURPOSE_UNKNOWN;

	/* Only algorithms that have HW keyslot support are passing here
	 */
	switch (algorithm) {
#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
#if HAVE_AES_CTS
	case TE_ALG_AES_CTS:	// => using CBC mode keyslot
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
	case TE_ALG_AES_CTR:
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
		kac_purpose = KAC_MANIFEST_PURPOSE_ENC;
		break;

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		kac_purpose = KAC_MANIFEST_PURPOSE_XTS;
		break;
#endif

#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES:
#if USE_HW_CMAC
		/* CMAC purpose is only allowing HW CMAC operations
		 * in HW (VDK did not seem to care).
		 *
		 * Due to HW change this is no longer allowed for
		 * AES encryption cases.
		 */
		kac_purpose = KAC_MANIFEST_PURPOSE_CMAC;
#else
		/* use old SW+CBC-MAC implementation of CMAC.
		 *
		 * In T23X needs both CBC-MAC and ENC operations.
		 * Use PURPOSE_ENC since HW may trap it otherwise.
		 */
		kac_purpose = KAC_MANIFEST_PURPOSE_ENC;
#endif
		break;
#endif

#if HAVE_HW_HMAC_SHA
#if HAVE_SHA1
		/* TODO: Note that HW does not support HMAC-SHA-1,
		 * it only supports SHA-2 with the HMAC engine.
		 *
		 * If used, must be handled by hybrid SW HMAC code.
		 */
	case TE_ALG_HMAC_SHA1:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
#endif /* HAVE_SHA1 */

#if HAVE_SHA224
	case TE_ALG_HMAC_SHA224:
#endif
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
		kac_purpose = KAC_MANIFEST_PURPOSE_HMAC;
		break;
#endif /* HAVE_HW_HMAC_SHA */

#if CCC_WITH_AES_GCM
	case TE_ALG_AES_GCM:
	case TE_ALG_GMAC_AES:
		kac_purpose = KAC_MANIFEST_PURPOSE_GCM;
		break;
#endif

#if HAVE_SE_KAC_KEYOP
	case TE_ALG_KEYOP_KWUW:
		kac_purpose = KAC_MANIFEST_PURPOSE_KWUW;
		break;

	case TE_ALG_KEYOP_KW:
		kac_purpose = KAC_MANIFEST_PURPOSE_KW;
		break;

	case TE_ALG_KEYOP_KUW:
		kac_purpose = KAC_MANIFEST_PURPOSE_KUW;
		break;
#endif /* HAVE_SE_KAC_KEYOP */

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_1KEY:
		/* KDK purpose is always used/mapped
		 * for TE_ALG_KEYOP_KDF_1KEY algo.
		 */
		kac_purpose = KAC_MANIFEST_PURPOSE_KDK;
		break;

	case TE_ALG_KEYOP_KDF_2KEY:
		/*
		 * For 2KEY algorithm both KDK and KDD keys need to be
		 * set with matching purposes, so the 2KEY code will
		 * call key setting twice: Once for KDK key and once for
		 * the KDD key.
		 *
		 * Either use TE_ALG_KEYOP_KDF_1KEY for the call setting KDK key
		 * or use TE_ALG_KEYOP_KDF_2KEY but also set
		 * SE_KAC_FLAGS_PURPOSE_SECONDARY_KEY flag =>
		 * this will set the key purpose to KDK as well.
		 *
		 * In normal case (no flag) TE_ALG_KEYOP_KDF_2KEY will get
		 * mapped to KDD purpose.
		 */
		if ((kac_flags & SE_KAC_FLAGS_PURPOSE_SECONDARY_KEY) != 0U) {
			/* "Secondary key" for 2KEY algo is KDK (KEY1_INDEX key),
			 * the "primary key" for 2KEY is KDD key (KEY2_INDEX key).
			 *
			 * Note: This is just a naming convention....
			 */
			kac_purpose = KAC_MANIFEST_PURPOSE_KDK;
		} else {
			kac_purpose = KAC_MANIFEST_PURPOSE_KDD;
		}
		break;
#endif /* HAVE_SE_KAC_KDF */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*purpose_p = kac_purpose;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Keyslot operations (used by AES, CMAC, HMAC, SHA and KAC read operations.
 */
static status_t kslot_field_select(se_aes_keyslot_id_sel_t id_sel,
				   uint32_t sel_key,
				   uint32_t sel_manifest,
				   uint32_t sel_ctrl,
				   uint32_t *sel_p)
{
	status_t ret = NO_ERROR;

	switch (id_sel) {
	case SE_KSLOT_SEL_KEYBUF:
		*sel_p = sel_key;
		break;

	case SE_KSLOT_SEL_KEYMANIFEST:
		*sel_p = sel_manifest;
		break;

	case SE_KSLOT_SEL_KEYCTRL:
		*sel_p = sel_ctrl;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	return ret;
}

/*
 * @brief Generate the value of SE_CRYPTO_KEYIV_PKT for keyslot KAC operations.
 *
 * @param engine the registers
 * @param keyslot SE AES key slot number.
 * @param id_sel Select a Key, original IV or updated IV. Use SE_KSLOT_SEL_*.
 * @param word_sel Select the word number to access in the AES key or IV.
 * @param [out]se_keyiv_pkt value of SE_CRYPTO_KEYIV_PKT expected
 *
 * @return NO_ERROR if success, error if fails
 *
 * NOTE for T23X:
 *
 * WRITE =>
 *  SEL inored.
 *  KEY_INDEX ignored.
 *  HW only writes to KEYBUF, not to keyslot nor to metadata like this.
 *
 * READ =>
 * SEL selects if reading KEYBUF, KEYMANIFEST or KEYCTRL.
 * KEY_INDEX ignored if KEYBUF.
 * KEY_INDEX specifies which keyslot metadata to read on KEYMANIFEST and KEYCTRL.
 *
 * Fields are ignored on writes: setup all like this would be a READ because it is most generic.
 *
 * The offsets to the KEYTABLE_ADDR pkt entries are identical for
 * AES and SHA keytables => use AES0 KEYTABLE_ADDR for all operations.
 */
static status_t se_create_keyslot_pkt(
	engine_id_t engine_id,
	uint32_t keyslot, se_aes_keyslot_id_sel_t id_sel,
	uint32_t word_sel, uint32_t *se_keyiv_pkt)
{
	status_t ret = NO_ERROR;
	uint32_t sel_type = 0U;

	LTRACEF("entry\n");

	*se_keyiv_pkt = 0U;

	if (word_sel > 0xFFU) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* These constant fields are actually identical for every
	 * engine.
	 *
	 * Just for the sake of it, make SHA and AES engines use
	 * different defines.
	 */
	switch (engine_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:
		ret = kslot_field_select(id_sel,
					 LW_DRF_DEF(SE0, AES0_CRYPTO_KEYTABLE_ADDR, SEL, KEYBUF),
					 LW_DRF_DEF(SE0, AES0_CRYPTO_KEYTABLE_ADDR, SEL, KEYMANIFEST),
					 LW_DRF_DEF(SE0, AES0_CRYPTO_KEYTABLE_ADDR, SEL, KEYCTRL),
					 &sel_type);
		break;

	case CCC_ENGINE_SE0_SHA:
		ret = kslot_field_select(id_sel,
					 LW_DRF_DEF(SE0, SHA_CRYPTO_KEYTABLE_ADDR, SEL, KEYBUF),
					 LW_DRF_DEF(SE0, SHA_CRYPTO_KEYTABLE_ADDR, SEL, KEYMANIFEST),
					 LW_DRF_DEF(SE0, SHA_CRYPTO_KEYTABLE_ADDR, SEL, KEYCTRL),
					 &sel_type);
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*se_keyiv_pkt = (
		(keyslot  << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
		(word_sel << SE_CRYPTO_KEYIV_PKT_KEY_WORD_SHIFT) |
		sel_type);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if defined(HAVE_KAC_READ) && HAVE_KAC_READ && 0
/* KSLOT info not read by anything yet => disable the function for now.
 * Maybe only used for testing later...
 */
static status_t kac_read_keyslot_locked(
	const engine_t *engine,
	struct se_key_slot *kslot)
{
	status_t ret = NO_ERROR;
	uint32_t word_inx = 0U;
	uint32_t kt_addr_reg = 0U;
	uint32_t kt_data_reg = 0U;
	uint32_t words = 0U;
	uint32_t manifest_value = 0U;
	uint32_t ctrl_value = 0U;
	uint32_t se_keytable_addr = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if ((NULL == engine) || (NULL == kslot)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!IS_VALID_AES_KEYSIZE_BITS(kslot->ks_bitsize)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	words = kslot->ks_bitsize / BITS_IN_WORD;

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:	/* regmapping deals with AES1 => TODO: verify t23X reg offsets!!! */
		kt_addr_reg  = SE0_AES0_CRYPTO_KEYTABLE_ADDR_0;
		kt_data_reg  = SE0_AES0_CRYPTO_KEYTABLE_DATA_0;
		break;

	case CCC_ENGINE_SE0_SHA:
		kt_addr_reg  = SE0_SHA_CRYPTO_KEYTABLE_ADDR_0;
		kt_data_reg  = SE0_SHA_CRYPTO_KEYTABLE_DATA_0;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Read %e KAC slot %u\n", engine->e_name, kslot->ks_slot);

	/* Read keytable KEYMANIFEST via KT_ADDR/KT_DATA
	 */
	se_keytable_addr = 0U;
	ret = se_create_keyslot_pkt(engine->e_id, kslot->ks_slot, SE_KSLOT_SEL_KEYMANIFEST,
				    0U, &se_keytable_addr);
	CCC_ERROR_CHECK(ret);

	LTRACEF("MANIFEST PKT SEL: 0x%x\n", se_keytable_addr);

	tegra_engine_set_reg(engine, kt_addr_reg, se_keytable_addr);
	manifest_value = tegra_engine_get_reg(engine, kt_data_reg);

	/* Read keytable KEYCTRL from KT/ADDR/KT_DATA
	 */
	se_keytable_addr = 0U;
	ret = se_create_keyslot_pkt(engine->e_id, kslot->ks_slot, SE_KSLOT_SEL_KEYCTRL,
				    0U, &se_keytable_addr);
	CCC_ERROR_CHECK(ret);

	LTRACEF("CTRL PKT SEL: 0x%x\n", se_keytable_addr);

	tegra_engine_set_reg(engine, kt_addr_reg, se_keytable_addr);
	ctrl_value = tegra_engine_get_reg(engine, kt_data_reg);

	(void)manifest_value;
	(void)ctrl_value;

	/**
	 * SE_CRYPTO_KEYTABLE_ADDR has to be written with the
	 * keyslot configuration for the particular KEY_WORD
	 * you are reading from. The data to be read from the keyslot
	 * metadata or from the KEYBUF (temp buffer for injecting keys).
	 *
	 * Keyslot keymaterial can never be read; blocked by HW.
	 */
	for (word_inx = 0U; word_inx < words; word_inx++) {
		uint32_t ksdata_value;

		ret = se_create_keyslot_pkt(engine->e_id, kslot->ks_slot, SE_KSLOT_SEL_KEYBUF,
					    word_inx, &se_keytable_addr);
		if (NO_ERROR != ret) {
			break;
		}

		tegra_engine_set_reg(engine, kt_addr_reg, se_keytable_addr);
		ksdata_value = tegra_engine_get_reg(engine, kt_data_reg);

		// Do not save these anywhere for now...
		(void)ksdata_value;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_KAC_READ */

/* All engines have same manifest fields, uses AES0 for all.
 */
static status_t kac_pack_manifest(const struct kac_manifest *manifest, uint32_t *mf)
{
	status_t ret = NO_ERROR;
	uint32_t m = 0U;

	LTRACEF("entry\n");

	if ((NULL == manifest) || (NULL == mf)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if LOCAL_TRACE
	rd_print_manifest(manifest);
#endif

	m = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_KEYMANIFEST,
			       PURPOSE, manifest->mf_purpose, m);
	m = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_KEYMANIFEST,
			       USER, manifest->mf_user, m);
	{
		uint32_t mf_exportable = 0U;
		if (BOOL_IS_TRUE(manifest->mf_exportable)) {
			mf_exportable = 1U;
		}
		m = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_KEYMANIFEST,
				       EX, mf_exportable, m);
	}
	m = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_KEYMANIFEST,
			       SIZE, manifest->mf_size, m);
	m = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_KEYMANIFEST,
			       SW, manifest->mf_sw, m);
	*mf = m;

	LTRACEF("KAC packed manifest: 0x%x\n", m);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kac_pack_ctrl(const struct kac_ctrl *ctrl, uint32_t *ct)
{
	status_t ret = NO_ERROR;
	uint32_t c = 0U;

	LTRACEF("entry\n");

	if ((NULL == ctrl) || (NULL == ct)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* VALID bit is R/O, set only the LOCK if requested.
	 */
	if (BOOL_IS_TRUE(ctrl->ctrl_lock)) {
		c = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_KEYTABLE_CTRL,
				       LOCK, TRUE, c);
	}
	*ct = c;

	LTRACEF("KAC packed KEYCTRL: 0x%x\n", c);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define MAX_SE_KEY_BITS 256U

status_t se_write_keybuf(const engine_t *engine, uint32_t keyslot,
			 const uint8_t *ksdata, uint32_t key_words_arg)
{
	status_t ret = NO_ERROR;
	uint32_t kt_addr_reg = 0U;
	uint32_t kt_data_reg = 0U;
	uint32_t word_inx = 0U;
	uint32_t key_words = key_words_arg;

	LTRACEF("entry\n");

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:	/* regmapping deals with AES1 => TODO: verify t23X reg offsets!!! */
		kt_addr_reg  = SE0_AES0_CRYPTO_KEYTABLE_ADDR_0;
		kt_data_reg  = SE0_AES0_CRYPTO_KEYTABLE_DATA_0;
		break;

	case CCC_ENGINE_SE0_SHA:
		kt_addr_reg  = SE0_SHA_CRYPTO_KEYTABLE_ADDR_0;
		kt_data_reg  = SE0_SHA_CRYPTO_KEYTABLE_DATA_0;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NULL == ksdata) {
		// XXXX TODO =>
		//  Verify the max number of bits SE keyslot can hold!!!
		//  Clear all of those with this function
		key_words = MAX_SE_KEY_BITS / BITS_IN_WORD;
	}

	if (key_words > (MAX_SE_KEY_BITS / BITS_IN_WORD)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	/**
	 * Write the internal KEYBUF with keymaterial.
	 *
	 * SE_CRYPTO_KEYTABLE_ADDR has to be written with the
	 * keyslot configuration for the particular KEY_WORD
	 * you are writing to. The data to be written to the keyslot
	 * metadata or to the KEYBUF (temp buffer for injecting keys).
	 *
	 * Write number of key_words to the keybuf.
	 */
	for (word_inx = 0U; word_inx < key_words; word_inx++) {
		uint32_t se_keytable_addr = 0U;
		uint32_t kd = 0U;

		ret = se_create_keyslot_pkt(engine->e_id, keyslot, SE_KSLOT_SEL_KEYBUF,
					    word_inx, &se_keytable_addr);
		if (NO_ERROR != ret) {
			break;
		}

		tegra_engine_set_reg(engine, kt_addr_reg, se_keytable_addr);

		if (NULL != ksdata) {
			kd = se_util_buf_to_u32(&ksdata[word_inx * 4U], CFALSE);
		}

		tegra_engine_set_reg(engine, kt_data_reg, kd);
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kac_kslot_write_kbuf_locked(const engine_t *engine,
					    const struct se_key_slot *kslot,
					    uint32_t *manifest_value_p,
					    uint32_t *ctrl_value_p)
{
	status_t ret = NO_ERROR;
	uint32_t key_words = 0U;
	const uint8_t *ksdata = kslot->ks_keydata;

	LTRACEF("entry\n");

	if (NULL == ksdata) {
		LTRACEF("Ilwalidating keyslot %u, setting manifest value 0x0\n",
			kslot->ks_slot);

		/* Override manifest and ctrl when clearing keybuf
		 * XXXXX TODO Verify this is ok in all cases
		 */
		*manifest_value_p = 0x0U;
		*ctrl_value_p     = 0x0U;
	}

	if (!IS_VALID_AES_KEYSIZE_BITS(kslot->ks_bitsize)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	key_words = kslot->ks_bitsize / BITS_IN_WORD;

	/* Note: Writes the intermediate buffer, not the keyslot!
	 */
	ret = se_write_keybuf(engine, kslot->ks_slot, ksdata, key_words);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* kslot->ks_keydata can be NULL. In that case the keyslot is
 * "ilwalidated": 256 zero bits written to the KEYBUF
 * (ignores kslot->ks_bitsize).
 *
 * Other kslot fields need to be prepared before calling this function.
 * This writes keytable manifest, keytable_cntrl, keytable_keybuf.
 */
status_t kac_setup_keyslot_data_locked(
	const engine_t *engine,
	const struct se_key_slot *kslot,
	bool write_keybuf)
{
	status_t ret = NO_ERROR;
	uint32_t manifest_reg = 0U;
	uint32_t ctrl_reg = 0U;
	uint32_t manifest_value = 0U;
	uint32_t ctrl_value = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if ((NULL == engine) || (NULL == kslot)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Setup keyslot metadata for key slot %u (%s writing keybuf)\n",
		kslot->ks_slot, (write_keybuf ? "is" : "not"));

	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:	/* regmapping deals with AES1 offsetting */
		manifest_reg = SE0_AES0_CRYPTO_KEYTABLE_KEYMANIFEST_0;
		ctrl_reg     = SE0_AES0_CRYPTO_KEYTABLE_CTRL_0;
		break;

	case CCC_ENGINE_SE0_SHA:
		manifest_reg = SE0_SHA_CRYPTO_KEYTABLE_KEYMANIFEST_0;
		ctrl_reg     = SE0_SHA_CRYPTO_KEYTABLE_CTRL_0;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* When writing a keyslot with key (from KEYBUF) the
	 * KEYMANIFEST and KEYCTRL must also be set at the same time.
	 *
	 * The KEYBUF is filled by writing with tuple <*_KEYTABLE_ADDR,*_KEYTABLE_DATA>
	 * but the KEYMANIFEST and KEYCTLR are not written this way. They have separate
	 * registers: SE0_*_CRYPTO_KEYTABLE_KEYMANIFEST_0 and
	 * SE0_*_CRYPTO_KEYTABLE_KEYCTRL_0.
	 *
	 * The KT_ADDR/KT_DATA path to MANIFEST and CTRL is for readout only,
	 * not for setting the values! (I do not know if writing works at all)
	 */

	ret = kac_pack_manifest(&kslot->ks_kac.k_manifest, &manifest_value);
	CCC_ERROR_CHECK(ret);

	ret = kac_pack_ctrl(&kslot->ks_kac.k_ctrl, &ctrl_value);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(write_keybuf)) {
		ret = kac_kslot_write_kbuf_locked(engine, kslot,
						  &manifest_value, &ctrl_value);
		CCC_ERROR_CHECK(ret);
	}

	/* First set the CRYPTO_KEYTABLE_KEYMANIFEST and
	 * CRYPTO_KEYTABLE_CTRL registers
	 */
	tegra_engine_set_reg(engine, manifest_reg, manifest_value);
	tegra_engine_set_reg(engine, ctrl_reg, ctrl_value);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_kac_keyslot_op_aes_config_0(te_crypto_algo_t op_algo,
						uint32_t *se_config_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config = 0U;

	LTRACEF("entry\n");

	*se_config_p = 0U;

	se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, DEC_ALG, NOP,
				       se_config);

	switch (op_algo) {
	case TE_ALG_KEYOP_INS:
		se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, ENC_ALG, INS,
					       se_config);
		break;

	case TE_ALG_KEYOP_GENKEY:
#if HAVE_SE_KAC_GENKEY
		se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, ENC_ALG, RNG,
					       se_config);
		se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, DST, KEYTABLE,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_CLONE:
#if HAVE_SE_KAC_CLONE
		se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, ENC_ALG, CLONE,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_LOCK:
#if HAVE_SE_KAC_LOCK
		se_config = LW_FLD_SET_DRF_DEF(SE0, AES0_CONFIG, ENC_ALG, LOCK,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*se_config_p = se_config;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_kac_keyslot_op_sha_config_0(te_crypto_algo_t op_algo,
						uint32_t *se_config_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config = 0U;

	LTRACEF("entry\n");

	*se_config_p = 0U;

	se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DEC_ALG, NOP,
				       se_config);

	switch (op_algo) {
	case TE_ALG_KEYOP_INS:
		se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, INS,
					       se_config);
		break;

	case TE_ALG_KEYOP_GENKEY:
#if HAVE_SE_KAC_GENKEY
		se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, RNG,
					       se_config);
		se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, DST, KEYTABLE,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_CLONE:
#if HAVE_SE_KAC_CLONE
		se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, CLONE,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_LOCK:
#if HAVE_SE_KAC_LOCK
		se_config = LW_FLD_SET_DRF_DEF(SE0, SHA_CONFIG, ENC_ALG, LOCK,
					       se_config);
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*se_config_p = se_config;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* INS / GENKEY / LOCK / CLONE setting for CONFIG_0 register.
 */
static status_t get_kac_keyslot_op_config_0(const engine_t *engine,
					    te_crypto_algo_t op_algo,
					    uint32_t *se_config_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	*se_config_p = 0U;

	if (IS_AES_ENGINE_ID(engine->e_id)) {
		ret = get_kac_keyslot_op_aes_config_0(op_algo,
						      se_config_p);
		CCC_ERROR_CHECK(ret);
	} else if (CCC_ENGINE_SE0_SHA == engine->e_id) {
		ret = get_kac_keyslot_op_sha_config_0(op_algo,
						      se_config_p);
		CCC_ERROR_CHECK(ret);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Engine id %u used for KAC key op\n",
					     engine->e_id));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Setup regs for keyslot INS/GENKEY/LOCK/CLONE operation with the given engine.
 */
static status_t setup_kac_engine_write_keyslot_regs(const engine_t *engine,
						    const struct se_key_slot *kslot,
						    te_crypto_algo_t op_algo,
						    bool write_kbuf)
{
	status_t ret = NO_ERROR;
	uint32_t se_config = 0U;
	uint32_t key_index = 0U;

	uint32_t config_0_reg = SE0_AES0_CONFIG_0;
	uint32_t kt_dst_reg = SE0_AES0_CRYPTO_KEYTABLE_DST_0;

#if HAVE_SE_KAC_CLONE
	uint32_t crypto_config_0_reg = SE0_AES0_CRYPTO_CONFIG_0;
	uint32_t crypto_config = 0U;
#endif


	switch (engine->e_id) {
	case CCC_ENGINE_SE0_AES0:
	case CCC_ENGINE_SE0_AES1:
		key_index = LW_FLD_SET_DRF_NUM(SE0, AES0_CRYPTO_KEYTABLE_DST,
					       KEY_INDEX, kslot->ks_slot,
					       key_index);

#if HAVE_SE_KAC_CLONE
		/* CLONE operation source keyslot (unused if not CLONE) */
		crypto_config = LW_FLD_SET_DRF_NUM(SE0, AES0_CRYPTO_CONFIG,
						   KEY_INDEX, kslot->ks_clone_src,
						   crypto_config);
#endif
		break;

	case CCC_ENGINE_SE0_SHA:
		config_0_reg = SE0_SHA_CONFIG_0;
		kt_dst_reg = SE0_SHA_CRYPTO_KEYTABLE_DST_0;

		key_index = LW_FLD_SET_DRF_NUM(SE0, SHA_CRYPTO_KEYTABLE_DST,
					       KEY_INDEX, kslot->ks_slot,
					       key_index);

#if HAVE_SE_KAC_CLONE
		crypto_config_0_reg = SE0_SHA_CRYPTO_CONFIG_0;
		/* CLONE operation source keyslot (unused if not CLONE) */
		crypto_config = LW_FLD_SET_DRF_NUM(SE0, SHA_CRYPTO_CONFIG,
						   KEY_INDEX, kslot->ks_clone_src,
						   crypto_config);
#endif
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = get_kac_keyslot_op_config_0(engine, op_algo, &se_config);
	CCC_ERROR_CHECK(ret);

	tegra_engine_set_reg(engine, config_0_reg, se_config);

#if HAVE_SE_KAC_CLONE
	/* Write the ks source KEY_INDEX to CRYPTO_CONFIG if CLONE
	 */
	if (TE_ALG_KEYOP_CLONE == op_algo) {
		tegra_engine_set_reg(engine, crypto_config_0_reg, crypto_config);
	}
#endif

	/* Write the ks destination key index
	 * INS / GENKEY / LOCK / CLONE operations
	 */
	tegra_engine_set_reg(engine, kt_dst_reg, key_index);

	/* Write the manifest/keydata if not LOCK.
	 */
	if (TE_ALG_KEYOP_LOCK != op_algo) {
		/* Setup engine for starting the INS / GENKEY / CLONE operation
		 * (manifest, cntrl, keybuf)
		 *
		 * Write keybuf only with INS.
		 */
		ret = kac_setup_keyslot_data_locked(engine, kslot, write_kbuf);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Runs the keyslot INS/GENKEY/LOCK/CLONE operation with the given engine.
 */
static status_t kac_engine_write_keyslot_locked(const engine_t *engine,
						const struct se_key_slot *kslot,
						te_crypto_algo_t op_algo)
{
	status_t ret = NO_ERROR;
	bool write_kbuf = (TE_ALG_KEYOP_INS == op_algo);

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = setup_kac_engine_write_keyslot_regs(engine, kslot,
						  op_algo, write_kbuf);
	CCC_ERROR_CHECK(ret);

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(engine, 0U);
	CCC_ERROR_CHECK(ret);

	ret = se_wait_engine_free(engine);
	CCC_ERROR_CHECK(ret);

	CCC_ENGINE_ERR_CHECK(engine, "Keyop done");

fail:
	if (BOOL_IS_TRUE(write_kbuf) && (NULL != kslot) && (NULL != engine)) {
		status_t fret = se_write_keybuf(engine, kslot->ks_slot, NULL, 0U);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear keybuf: %d\n", fret);
			/* Does not fail the operation in case keybuf clear fails
			 * since it would not fix anything.
			 */
		}
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* TODO: Could also add checking se_kac_flags for only containing supported bits
 */
static status_t kac_set_keyslot_manifest_user_sw_check(kac_manifest_user_t mf_kac_user,
						       uint32_t se_kac_sw)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((KAC_MANIFEST_USER_RESERVED == mf_kac_user)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!KAC_MANIFEST_SW_VALUE_IS_VALID(se_kac_sw)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Manifest SW value larger than field size: 0x%x\n",
					     se_kac_sw));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kac_set_keyslot_fields_check_args(const engine_t *engine,
						  const uint8_t *keydata,
						  kac_manifest_user_t mf_kac_user,
						  kac_manifest_purpose_t mf_kac_purpose,
						  uint32_t se_kac_sw,
						  te_crypto_algo_t op_algo)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	// TODO: verify if this can be NULL in INS (=> clear keyslot)
	(void)keydata;

	if ((NULL == engine) ||
	    (KAC_MANIFEST_PURPOSE_UNKNOWN == mf_kac_purpose)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = kac_set_keyslot_manifest_user_sw_check(mf_kac_user, se_kac_sw);
	CCC_ERROR_CHECK(ret);

	switch (op_algo) {
	case TE_ALG_KEYOP_INS:
		break;

	case TE_ALG_KEYOP_LOCK:
	case TE_ALG_KEYOP_CLONE:
		/* These are not handled here */
		ret = SE_ERROR(ERR_BAD_STATE);
		break;

#if HAVE_SE_KAC_GENKEY
	case TE_ALG_KEYOP_GENKEY:
		if (NULL != keydata) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
		}
		break;
#endif

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ks_set_manifest_size(struct se_key_slot *ks,
				     uint32_t keysize_bits)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (keysize_bits) {
	case 128U:
		ks->ks_kac.k_manifest.mf_size = KAC_MANIFEST_SIZE_128;
		break;
	case 192U:
		ks->ks_kac.k_manifest.mf_size = KAC_MANIFEST_SIZE_192;
		break;
	case 256U:
		ks->ks_kac.k_manifest.mf_size = KAC_MANIFEST_SIZE_256;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Caller checks for error value */
static kac_manifest_user_t ks_map_manifest_user(uint32_t se_kac_user)
{
	kac_manifest_user_t mf_kac_user = KAC_MANIFEST_USER_RESERVED;

	LTRACEF("entry\n");

	switch (se_kac_user) {
	case CCC_MANIFEST_USER_PSC:
		mf_kac_user = KAC_MANIFEST_USER_PSC;
		break;
	case CCC_MANIFEST_USER_TZ:
		mf_kac_user = KAC_MANIFEST_USER_TZ;
		break;
	case CCC_MANIFEST_USER_NS:
		mf_kac_user = KAC_MANIFEST_USER_NS;
		break;
	case CCC_MANIFEST_USER_FSI:
		mf_kac_user = KAC_MANIFEST_USER_FSI;
		break;
	default:
		mf_kac_user = KAC_MANIFEST_USER_RESERVED;
		break;
	}

	LTRACEF("exit\n");
	return mf_kac_user;
}

/* Caller checks for error value */
static kac_manifest_purpose_t ks_map_manifest_purpose(uint32_t se_kac_purpose)
{
	kac_manifest_purpose_t mf_kac_purpose = KAC_MANIFEST_PURPOSE_UNKNOWN;

	LTRACEF("entry\n");

	switch (se_kac_purpose) {
	case CCC_MANIFEST_PURPOSE_ENC:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_ENC;
		break;
	case CCC_MANIFEST_PURPOSE_CMAC:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_CMAC;
		break;
	case CCC_MANIFEST_PURPOSE_HMAC:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_HMAC;
		break;
	case CCC_MANIFEST_PURPOSE_KW:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KW;
		break;
	case CCC_MANIFEST_PURPOSE_KUW:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KUW;
		break;
	case CCC_MANIFEST_PURPOSE_KWUW:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KWUW;
		break;
	case CCC_MANIFEST_PURPOSE_KDK:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KDK;
		break;
	case CCC_MANIFEST_PURPOSE_KDD:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KDD;
		break;
	case CCC_MANIFEST_PURPOSE_KDD_KUW:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_KDD_KUW;
		break;
	case CCC_MANIFEST_PURPOSE_XTS:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_XTS;
		break;
	case CCC_MANIFEST_PURPOSE_GCM:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_GCM;
		break;
	default:
		mf_kac_purpose = KAC_MANIFEST_PURPOSE_UNKNOWN;
		break;
	}

	LTRACEF("exit\n");
	return mf_kac_purpose;
}


#if HAVE_SE_KAC_CLONE
static status_t ks_set_for_clone(struct se_key_slot *ks,
				 uint32_t cloned_keyslot,
				 kac_manifest_user_t mf_kac_user,
				 uint32_t se_kac_sw,
				 uint32_t se_kac_flags)
{
	status_t ret = NO_ERROR;

	if (cloned_keyslot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	ks->ks_clone_src = cloned_keyslot;

	ret = kac_set_keyslot_manifest_user_sw_check(mf_kac_user,
						     se_kac_sw);
	CCC_ERROR_CHECK(ret);

	// mf_origin, mf_purpose, mf_size => set by HW
	//
	ks->ks_kac.k_manifest.mf_user = mf_kac_user;

	ks->ks_kac.k_manifest.mf_exportable =
		((se_kac_flags & SE_KAC_FLAGS_EXPORTABLE) != 0U);

	ks->ks_kac.k_manifest.mf_sw =
		(se_kac_sw & KAC_MANIFEST_SW_FIELD_MASK);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_CLONE */

static status_t ks_set_for_algo(struct se_key_slot *ks,
				te_crypto_algo_t op_algo,
				const engine_t *engine,
				const uint8_t *keydata,
				uint32_t keysize_bits,
				kac_manifest_user_t mf_kac_user,
				kac_manifest_purpose_t mf_kac_purpose,
				uint32_t se_kac_sw,
				uint32_t se_kac_flags)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = kac_set_keyslot_fields_check_args(engine, keydata,
						mf_kac_user,
						mf_kac_purpose,
						se_kac_sw, op_algo);
	CCC_ERROR_CHECK(ret);

	ret = ks_set_manifest_size(ks, keysize_bits);
	CCC_ERROR_CHECK(ret);

	ks->ks_bitsize = keysize_bits;

	// mf_origin => set by HW
	//
	ks->ks_kac.k_manifest.mf_user = mf_kac_user;
	ks->ks_kac.k_manifest.mf_purpose = mf_kac_purpose;
	ks->ks_kac.k_manifest.mf_exportable =
		((se_kac_flags & SE_KAC_FLAGS_EXPORTABLE) != 0U);
	ks->ks_kac.k_manifest.mf_sw =
		(se_kac_sw & KAC_MANIFEST_SW_FIELD_MASK);

#if HAVE_KAC_KEYSLOT_LOCK
	/* If client is allowed to LOCK a keyslot, define
	 * HAVE_KAC_KEYSLOT_LOCK nonzero. Default: Not allowed.
	 */
	ks->ks_kac.k_ctrl.ctrl_lock = ((se_kac_flags & SE_KAC_FLAGS_LOCK) != 0U);
#else
	ks->ks_kac.k_ctrl.ctrl_lock = false;
#endif /* HAVE_KAC_KEYSLOT_LOCK */

	ks->ks_keydata = keydata;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kac_keyop_purpose_locked(const engine_t *engine,
					 uint32_t keyslot,
					 const uint8_t *keydata,
					 uint32_t keysize_bits,
					 uint32_t se_kac_user,
					 kac_manifest_purpose_t mf_kac_purpose,
					 uint32_t se_kac_flags,
					 uint32_t se_kac_sw,
					 te_crypto_algo_t op_algo,
					 uint32_t cloned_keyslot)
{
	status_t ret = NO_ERROR;
	struct se_key_slot ks = { .ks_keydata = NULL };
	kac_manifest_user_t mf_kac_user = KAC_MANIFEST_USER_RESERVED;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if ((NULL == engine) || (keyslot >= SE_AES_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	mf_kac_user = ks_map_manifest_user(se_kac_user);

	ks.ks_slot = keyslot;

	switch (op_algo) {
	case TE_ALG_KEYOP_LOCK:
#if HAVE_SE_KAC_LOCK
		/* Nothing */
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	case TE_ALG_KEYOP_CLONE:
#if HAVE_SE_KAC_CLONE
		ret = ks_set_for_clone(&ks, cloned_keyslot,
				       mf_kac_user, se_kac_sw,
				       se_kac_flags);
#else
		(void)cloned_keyslot;
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif
		break;

	default:
		ret = ks_set_for_algo(&ks, op_algo, engine,
				      keydata, keysize_bits,
				      mf_kac_user,
				      mf_kac_purpose,
				      se_kac_sw,
				      se_kac_flags);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = kac_engine_write_keyslot_locked(engine, &ks, op_algo);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t kac_keyop_locked(const engine_t *engine,
				 uint32_t keyslot,
				 const uint8_t *keydata,
				 uint32_t keysize_bits,
				 uint32_t se_kac_user,
				 te_crypto_algo_t se_kac_algorithm,
				 uint32_t se_kac_flags,
				 uint32_t se_kac_sw,
				 te_crypto_algo_t op_algo,
				 uint32_t cloned_keyslot)
{
	status_t ret = NO_ERROR;
	kac_manifest_purpose_t mf_kac_purpose = KAC_MANIFEST_PURPOSE_UNKNOWN;

	LTRACEF("entry\n");

	if ((TE_ALG_KEYOP_LOCK != op_algo) &&
	    (TE_ALG_KEYOP_CLONE != op_algo)) {
		/* LOCK and CLONE do not set target manifest fields, so
		 * they pass "filler values" here.
		 * Do not map TE_ALG_NONE to purpose, just leave it as "UNKNOWN".
		 */
		ret = se_map_algorithm_to_purpose(se_kac_algorithm,
						  &mf_kac_purpose,
						  se_kac_flags);
		CCC_ERROR_CHECK(ret);
	}

	ret = kac_keyop_purpose_locked(engine, keyslot, keydata, keysize_bits,
				       se_kac_user,
				       mf_kac_purpose,
				       se_kac_flags,
				       se_kac_sw,
				       op_algo,
				       cloned_keyslot);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* INS */

status_t se_set_engine_kac_keyslot_locked(const engine_t *engine,
					  uint32_t keyslot,
					  const uint8_t *keydata,
					  uint32_t keysize_bits,
					  uint32_t se_kac_user,
					  uint32_t se_kac_flags,
					  uint32_t se_kac_sw,
					  te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = kac_keyop_locked(engine, keyslot, keydata, keysize_bits,
			       se_kac_user, se_kac_algorithm,
			       se_kac_flags, se_kac_sw,
			       TE_ALG_KEYOP_INS, SE_AES_MAX_KEYSLOTS);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_set_engine_kac_keyslot(const engine_t *engine,
				   uint32_t keyslot,
				   const uint8_t *keydata,
				   uint32_t keysize_bits,
				   uint32_t se_kac_user,
				   uint32_t se_kac_flags,
				   uint32_t se_kac_sw,
				   te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_set_engine_kac_keyslot_locked(engine, keyslot, keydata, keysize_bits,
					       se_kac_user, se_kac_flags,
					       se_kac_sw, se_kac_algorithm);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_set_device_kac_keyslot(se_cdev_id_t device_id, uint32_t keyslot,
				   const uint8_t *keydata, uint32_t keysize_bits,
				   uint32_t se_kac_user,
				   uint32_t se_kac_flags,
				   uint32_t se_kac_sw,
				   te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;
	uint32_t engine_class = CCC_ENGINE_CLASS_AES;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	// TODO: Add sanity checks for the USER field. Makes no sense
	// to e.g. allow NS or TZ to push keys used in more secure layers.

	// SE KAC keyslots are shared between all engines of the T23X SE engine
	// So, use the SHA engine to do this operation if SHA
	//  defined, otherwise lookup AES.
	//

	// FIXME: Need to support HMAC_SHA engine key settings as well

	// engine_class = CCC_ENGINE_CLASS_SHA; (SHA engines support HMAC-SHA)
	// if these differ!!! Now the same keyslots are used for both and
	// it should not matter what the engine is.
	//
	ret = ccc_select_device_engine(device_id, &engine, engine_class);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an SE KAC keyslot)\n",
					  device_id));

	ret = se_set_engine_kac_keyslot(engine, keyslot, keydata, keysize_bits,
					se_kac_user, se_kac_flags, se_kac_sw, se_kac_algorithm);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set keyslot using manifest field value for keyslot purpose instead of
 * mapping it from algorithm.
 */
status_t se_set_eng_kac_keyslot_purpose_locked(const engine_t *engine,
					       uint32_t keyslot,
					       const uint8_t *keydata,
					       uint32_t keysize_bits,
					       uint32_t se_kac_user,
					       uint32_t se_kac_flags,
					       uint32_t se_kac_sw,
					       uint32_t se_kac_purpose)
{
	status_t ret = NO_ERROR;
	kac_manifest_purpose_t mf_kac_purpose =
		ks_map_manifest_purpose(se_kac_purpose);

	LTRACEF("entry\n");

	/* INSert a key to specified keyslot
	 */
	ret = kac_keyop_purpose_locked(engine, keyslot, keydata, keysize_bits,
				       se_kac_user,
				       mf_kac_purpose,
				       se_kac_flags,
				       se_kac_sw,
				       TE_ALG_KEYOP_INS, SE_AES_MAX_KEYSLOTS);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_set_eng_kac_keyslot_purpose(const engine_t *engine,
					uint32_t keyslot,
					const uint8_t *keydata,
					uint32_t keysize_bits,
					uint32_t se_kac_user,
					uint32_t se_kac_flags,
					uint32_t se_kac_sw,
					uint32_t se_kac_purpose)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_set_eng_kac_keyslot_purpose_locked(engine, keyslot, keydata,
						    keysize_bits,
						    se_kac_user, se_kac_flags,
						    se_kac_sw, se_kac_purpose);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_set_device_kac_keyslot_purpose(se_cdev_id_t device_id, uint32_t keyslot,
					   const uint8_t *keydata, uint32_t keysize_bits,
					   uint32_t se_kac_user,
					   uint32_t se_kac_flags,
					   uint32_t se_kac_sw,
					   uint32_t se_kac_purpose)
{
	status_t ret = NO_ERROR;
	uint32_t engine_class = CCC_ENGINE_CLASS_AES;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	// TODO: Add sanity checks for the USER field. Makes no sense
	// to e.g. allow NS or TZ to push keys used in more secure layers.

	// SE KAC keyslots are shared between all engines of the T23X SE engine
	// So, use the SHA engine to do this operation if SHA
	//  defined, otherwise lookup AES.
	//
	ret = ccc_select_device_engine(device_id, &engine, engine_class);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an SE KAC keyslot)\n",
					  device_id));

	ret = se_set_eng_kac_keyslot_purpose(engine, keyslot, keydata, keysize_bits,
					     se_kac_user,
					     se_kac_flags,
					     se_kac_sw,
					     se_kac_purpose);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_KAC_GENKEY

/* Keyslot GENKEY (random key) */

status_t se_engine_kac_keyslot_genkey_locked(const engine_t *engine,
					     uint32_t keyslot,
					     uint32_t keysize_bits,
					     uint32_t se_kac_user,
					     uint32_t se_kac_flags,
					     uint32_t se_kac_sw,
					     te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = kac_keyop_locked(engine, keyslot, NULL, keysize_bits,
			       se_kac_user, se_kac_algorithm,
			       se_kac_flags, se_kac_sw,
			       TE_ALG_KEYOP_GENKEY, SE_AES_MAX_KEYSLOTS);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_engine_kac_keyslot_genkey(const engine_t *engine,
				      uint32_t keyslot,
				      uint32_t keysize_bits,
				      uint32_t se_kac_user,
				      uint32_t se_kac_flags,
				      uint32_t se_kac_sw,
				      te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_engine_kac_keyslot_genkey_locked(engine, keyslot, keysize_bits,
						  se_kac_user, se_kac_flags, se_kac_sw,
						  se_kac_algorithm);
	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_device_kac_keyslot_genkey(se_cdev_id_t device_id, uint32_t keyslot,
				      uint32_t keysize_bits,
				      uint32_t se_kac_user,
				      uint32_t se_kac_flags,
				      uint32_t se_kac_sw,
				      te_crypto_algo_t se_kac_algorithm)
{
	status_t ret = NO_ERROR;
	uint32_t engine_class = CCC_ENGINE_CLASS_AES;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	// Now the same keyslots are used for both AES/SHA engines and
	// it should really not matter what the engine is used since only
	// 128, 192 and 256 bit keys are supported for both algorithm classes in HW.
	//
	ret = ccc_select_device_engine(device_id, &engine, engine_class);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an SE KAC keyslot)\n",
					  device_id));

	ret = se_engine_kac_keyslot_genkey(engine, keyslot, keysize_bits,
					   se_kac_user, se_kac_flags, se_kac_sw,
					   se_kac_algorithm);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_GENKEY */

#if HAVE_SE_KAC_LOCK

/* Keyslot LOCK */

status_t se_engine_kac_keyslot_lock_locked(const engine_t *engine,
					   uint32_t keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = kac_keyop_locked(engine, keyslot, NULL, 0U,
			       KAC_MANIFEST_USER_RESERVED, TE_ALG_NONE,
			       SE_KAC_FLAGS_NONE, 0U,
			       TE_ALG_KEYOP_LOCK, SE_AES_MAX_KEYSLOTS);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_engine_kac_keyslot_lock(const engine_t *engine,
				    uint32_t keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_engine_kac_keyslot_lock_locked(engine, keyslot);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_device_kac_keyslot_lock(se_cdev_id_t device_id,
				    uint32_t keyslot)
{
	status_t ret = NO_ERROR;
	uint32_t engine_class = CCC_ENGINE_CLASS_AES;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	// Now the same keyslots are used for both AES/SHA engines and
	// it should really not matter what the engine is used.
	//
	ret = ccc_select_device_engine(device_id, &engine, engine_class);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an SE KAC keyslot)\n",
					  device_id));

	ret = se_engine_kac_keyslot_lock(engine, keyslot);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_LOCK */

#if HAVE_SE_KAC_CLONE

/* Keyslot CLONE */

status_t se_engine_kac_keyslot_clone_locked(const engine_t *engine,
					    uint32_t dst_keyslot,
					    uint32_t src_keyslot,
					    uint32_t clone_user,
					    uint32_t clone_sw,
					    uint32_t clone_kac_flags)
{
	status_t ret = NO_ERROR;
	uint32_t kac_flags = 0U;
	kac_manifest_user_t mf_user = KAC_MANIFEST_USER_RESERVED;

	LTRACEF("entry\n");

	/* clone_sw is checked down the call chain.
	 */
	if (CCC_MANIFEST_FLAGS_EXPORTABLE == clone_kac_flags) {
		kac_flags = SE_KAC_FLAGS_EXPORTABLE;
	}

	switch (clone_user) {
	case CCC_MANIFEST_USER_PSC:
		mf_user = KAC_MANIFEST_USER_PSC;
		break;
	case CCC_MANIFEST_USER_TZ:
		mf_user = KAC_MANIFEST_USER_TZ;
		break;
	case CCC_MANIFEST_USER_NS:
		mf_user = KAC_MANIFEST_USER_NS;
		break;
	case CCC_MANIFEST_USER_FSI:
		mf_user = KAC_MANIFEST_USER_FSI;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = kac_keyop_locked(engine, dst_keyslot, NULL, 0U,
			       mf_user,
			       TE_ALG_NONE,
			       kac_flags, clone_sw,
			       TE_ALG_KEYOP_CLONE, src_keyslot);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_engine_kac_keyslot_clone(const engine_t *engine,
				     uint32_t dst_keyslot,
				     uint32_t src_keyslot,
				     uint32_t clone_user,
				     uint32_t clone_sw,
				     uint32_t clone_kac_flags)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = se_engine_kac_keyslot_clone_locked(engine,
						 dst_keyslot,
						 src_keyslot,
						 clone_user,
						 clone_sw,
						 clone_kac_flags);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

status_t se_device_kac_keyslot_clone(se_cdev_id_t device_id,
				     uint32_t dst_keyslot,
				     uint32_t src_keyslot,
				     uint32_t clone_user,
				     uint32_t clone_sw,
				     uint32_t clone_kac_flags)
{
	status_t ret = NO_ERROR;
	uint32_t engine_class = CCC_ENGINE_CLASS_AES;
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	// Now the same keyslots are used for both AES/SHA engines and
	// it should really not matter what the engine is used.
	//
	ret = ccc_select_device_engine(device_id, &engine, engine_class);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u with an SE KAC keyslot)\n",
					  device_id));

	ret = se_engine_kac_keyslot_clone(engine,
					  dst_keyslot,
					  src_keyslot,
					  clone_user,
					  clone_sw,
					  clone_kac_flags);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_CLONE */

#if HAVE_SE_APERTURE
/* Use this intermediate for now in case want to do something else with apertures.
 */
status_t se_set_aperture(PHYS_ADDR_T src_paddr, uint32_t slen,
			 PHYS_ADDR_T dst_paddr, uint32_t dlen)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("Apertures SRC@%p[%u] => DST@%p[%u]\n",
		(const uint8_t *)src_paddr, slen, (const uint8_t *)dst_paddr, dlen);

	ret = CCC_SET_SE_APERTURE(src_paddr, slen, dst_paddr, dlen);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_APERTURE */

#endif /*  HAVE_SE_SHA || HAVE_SE_AES */

#if HAVE_SE_AES

/* For T23X the IV is no longer in the keyslots, it is stored in the
 * linear counter registers from which the AES engine reads it.
 *
 * If IV is NULL it is cleared.
 *
 * CCC does not assume HW would update the counter registers =>
 * counters and IV values are maintained in SW or extracted
 * from memory.
 */
static status_t aes_write_iv_locked(const engine_t *engine, const uint32_t *iv)
{
	status_t ret = se_write_linear_counter(engine, iv);
	return ret;
}

static status_t engine_aes_set_op_mode_encrypt_crypto_config(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	uint32_t *se_config_reg_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;
	bool set_core_sel = true;
	uint32_t counter_increment = 1U;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == se_config_reg_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_AES_CTR
	/* Modify the CTR mode increment if allowed by counter mode.
	 * Used by AES counter variants in CTR_CNTN field.
	 */
	if ((TE_ALG_AES_CTR == algorithm) &&
	    (0U != econtext->ctr.increment)) {
		counter_increment = econtext->ctr.increment;
	}
#else
	(void)counter_increment;
#endif

	switch (algorithm) {
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
#endif
	case TE_ALG_AES_ECB_NOPAD:
		/* ECB does not use IV
		 * VCTRAM_SEL is not used, leave field zero.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, BYPASS, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   MEMORY, se_config_reg);
		break;

	case TE_ALG_CMAC_AES:
		/* CMAC-AES: enable HASH mode, otherwise
		 * HASH_ENB(DISABLE) (zero) by default.  CMAC uses
		 * AES-CBC settings otherwise.
		 *
		 * Note: v0.7 IAS added a native CMAC-AES HW which is
		 * programmed differently => this (old) code is using
		 * AES-CBC-MAC for implementing CMAC-AES (and AES-CCM).
		 *
		 * The AES-CBC-MAC mode was called "CMAC-AES" mode in
		 * earlier engines.
		 *
		 * TODO: Native HW CMAC-AES mode is not yet supported
		 * in CCC => add support.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   HASH_ENB, ENABLE, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, TOP, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   MEMORY, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   VCTRAM_SEL, INIT_AESOUT,
						   se_config_reg);
		break;

	case TE_ALG_AES_CTS:
#if HAVE_AES_PADDING
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_CBC_NOPAD:
		/*
		 * CBC XORs IV with plaintext.
		 * CTS is NIST CBC-CS2 => uses CBC programming.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, TOP, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   MEMORY, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   VCTRAM_SEL, INIT_AESOUT,
						   se_config_reg);
		break;

#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
		/* OFB XORs ciphered text with plaintext (result used also as next IV)
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, BOTTOM, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   INIT_AESOUT, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   VCTRAM_SEL, MEMORY,
						   se_config_reg);
		break;
#endif

#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
		/* For HW GCM support no additional CRYPTO_CONFIG settings matter,
		 * CORE_SEL is ignored (not set) and KEY_INDEX is set by caller.
		 *
		 * For SW (hybrid) implementation AES-CTR HW engine is used.
		 *
		 * XXX Only KEY_INDEX/SCC_EN are used for GCM in crypto config.
		 */
		set_core_sel = false;
		break;
#endif /* HAVE_AES_GCM */

#if HAVE_AES_CCM || HAVE_AES_CTR
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
	{
		/* CTR XORs ciphered text with plaintext. AES input is the value
		 * of the counter from SE_CRYPTO_LINEAR_CTR[0-3].
		 *
		 * This counter value is incremented by programmed value in
		 * SE_CRYPTO_CONFIG.CTR_CNT
		 *
		 * Note: In T186 the counter value can no longer be (easily)
		 * read back from HW => SW must track it out-of-band.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, BOTTOM, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   LINEAR_CTR, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   VCTRAM_SEL, MEMORY,
						   se_config_reg);

		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
						   CTR_CNTN, counter_increment,
						   se_config_reg);
	}
	break;
#endif /* HAVE_AES_CCM || HAVE_AES_CTR */

	default:
		/* [COVERITY] It is true that this is lwrrently deadcode, but if
		 * new algorithms are added to the enum then this would catch
		 * those before support is added.
		 */
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(set_core_sel)) {
		/* common configs */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   CORE_SEL, ENCRYPT,
						   se_config_reg);
	}

	*se_config_reg_p = se_config_reg;

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t engine_aes_set_op_mode_decrypt_crypto_config(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	uint32_t *se_config_reg_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == se_config_reg_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (algorithm) {
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
#endif
	case TE_ALG_AES_ECB_NOPAD:
		/* ECB does not use IV
		 * VCTRAM_SEL is not used, does not set it.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, BYPASS, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   MEMORY, se_config_reg);

		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   CORE_SEL, DECRYPT,
						   se_config_reg);
		break;

	case TE_ALG_AES_CTS:
#if HAVE_AES_PADDING
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_CBC_NOPAD:
		/* CMAC is never using decipher => trapped below if attempted.
		 * CBC XORs IV with plaintext
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   XOR_POS, BOTTOM, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, INPUT_SEL,
						   MEMORY, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   VCTRAM_SEL, INIT_PREV_MEMORY,
						   se_config_reg);

		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   CORE_SEL, DECRYPT,
						   se_config_reg);
		break;

#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
		/* HW has ENCRYPT/DECRYPT modes for AES-GCM.
		 *
		 * Only KEY_INDEX/SCC_EN fields are used for GCM in CRYPTO_CONFIG
		 * but they are set elsewhere.
		 */
		break;
#endif

		/* Stream cipher modes && CMAC use encrypt settings for deciphering
		 * so TE_ALG_CMAC_AES, TE_ALG_AES_OFB and TE_ALG_AES_CTR will not
		 * enter here. If they do, the default will catch it and trap an error.
		 */
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* common configs */
	*se_config_reg_p = se_config_reg;

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t engine_aes_set_op_mode_check_args(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	uint32_t config_dst)
{
	status_t ret = NO_ERROR;

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* XXX Should support keyslot targets later as well.
	 * XXX TODO (but => NOT FOR T23X anymore, the HW feature is redesigned...)
	 */
	if (SE0_AES0_CONFIG_0_DST_MEMORY != config_dst) {
		bool failed = true;

		/* AES MAC destination can be HASH_REG */
		if (AES_MAC_ALGO(algorithm) &&
		    (SE0_AES0_CONFIG_0_DST_HASH_REG == config_dst)) {
			failed = false;
		}

		if (BOOL_IS_TRUE(failed)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

fail:
	return ret;
}

#if HAVE_AES_XTS
static status_t engine_aes_op_mode_xts(
	const struct se_engine_aes_context *econtext,
	bool is_encrypt,
	uint32_t pkt_mode)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(is_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   AES_ENC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
						   pkt_mode, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   NOP, se_config_reg);
	} else {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   AES_DEC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DEC_MODE,
						   pkt_mode, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   NOP, se_config_reg);
	}

	/* XTS only supports output to memory */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST,
					   MEMORY, se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	se_config_reg = 0U;

	if (BOOL_IS_TRUE(is_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   CORE_SEL, ENCRYPT,
						   se_config_reg);
	}

	/* XTS is a two key tweakable AES mode, otherwise much
	 * like AES-CTR.  AES input is the value of the
	 * counter from SE_CRYPTO_LINEAR_CTR[0-3].
	 *
	 * Based on some texts AES-XTS was not designed to be
	 * "two key" cipher, it is made so by a mistake
	 * interpreting the paper defining it. But since all
	 * XTS implementations use two keys => this
	 * one also does. If you want the two keys to be
	 * identical (key and subkey) just use identical values.
	 */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   XOR_POS, BOTH,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   INPUT_SEL, MEMORY,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   VCTRAM_SEL, TWEAK,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   IV_SELECT, REG,
					   se_config_reg);

	/* Set the keyslot used. */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
					   KEY_INDEX, econtext->se_ks.ks_slot,
					   se_config_reg);

	/* Older platform uses catenated XTS key from econtext->se_ks.ks_slot,
	 * there is a new field for the second key in the new platforms.
	 */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
					   KEY2_INDEX, econtext->se_ks.ks_subkey_slot,
					   se_config_reg);

#if HAVE_SE_SCC
	/* ENABLE SE SCC equivalent to disable SCC_DIS */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   SCC_DIS, DISABLE,
					   se_config_reg);
#else
	/* DISABLE SE SCC (enable SCC_DIS) */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   SCC_DIS, ENABLE,
					   se_config_reg);
#endif /* HAVE_SE_SCC */

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

	/* From T194 IAS (page 158):
	 * HW TWEAK is derived once per START operation at the beginning of task.
	 * The implication is that SW must schedule one task per sector!
	 */
	ret = se_write_linear_counter(econtext->engine, econtext->xts.tweak);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}
#endif /* HAVE_AES_XTS */

static uint32_t aes_set_iv_select_field(te_crypto_algo_t algorithm,
					uint32_t crypto_config_value,
					bool set_default,
					bool use_orig_iv)
{
	uint32_t crypto_config = crypto_config_value;

	if (BOOL_IS_TRUE(set_default)) {
		/* Set the IV_SELECT field to DEFAULT (zero) for any
		 * algorithm using internal context in a keyslot, like
		 * HW AES-GCM.
		 *
		 * TODO: HW AES-CMAC must drop here as well, when
		 * T23X HW CMAC mode used.
		 */
		crypto_config = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   IV_SELECT, DEFAULT,
						   crypto_config);
	} else if (TE_AES_MODE_USES_IV(algorithm)) {
		if (BOOL_IS_TRUE(use_orig_iv)) {
			/* Caller must set up the proper registers for
			 * each operation.
			 *
			 * For CMAC => SE0_AES0_CMAC_RESULT must be
			 * cleared.  For other AES modes =>
			 * SE0_AES0_CRYPTO_LINEAR_CTR must be set
			 */
			crypto_config = LW_FLD_SET_DRF_DEF(SE0_AES0,
							   CRYPTO_CONFIG,
							   IV_SELECT, REG,
							   crypto_config);
		} else {
			crypto_config = LW_FLD_SET_DRF_DEF(SE0_AES0,
							   CRYPTO_CONFIG,
							   IV_SELECT, UPDATED,
							   crypto_config);
		}
	} else if (TE_AES_MODE_COUNTER(algorithm) ||
		   (TE_ALG_CMAC_AES == algorithm)) {
		/* Algorithms using AES-CBC-MAC mode should also drop here,
		 * like old mode CMAC-AES and AES-CCM (which is also
		 * a COUNTER mode).
		 */
		crypto_config = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   IV_SELECT, REG,
						   crypto_config);
	} else {
		/* No need to set IV */
	}

	return crypto_config;
}

status_t engine_aes_op_mode_normal(const struct se_engine_aes_context *econtext,
				   te_crypto_algo_t algorithm,
				   bool is_encrypt, bool use_orig_iv,
				   uint32_t pkt_mode, uint32_t config_dst)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;
	bool has_internal_context = ((econtext->aes_flags & AES_FLAGS_HW_CONTEXT) != 0U);

	/* Set SE0_AES0_CONFIG_0 register fields */

	se_config_reg = 0U;

	/* KAC change: ENC_MODE and DEC_MODE are NOP, except for
	 * GCM/GMAC/KWUW => pass 0U in pkt_mode for algos that do not
	 * need them.
	 */
	if (BOOL_IS_TRUE(is_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   AES_ENC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
						   pkt_mode, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   NOP, se_config_reg);
	} else {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   AES_DEC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DEC_MODE,
						   pkt_mode, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   NOP, se_config_reg);
	}

	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DST, config_dst,
					   se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	/* Set SE0_AES0_CRYPTO_CONFIG_0 register fields */

	se_config_reg = 0U;
	if (BOOL_IS_TRUE(is_encrypt)) {
		ret = engine_aes_set_op_mode_encrypt_crypto_config(econtext,
								   algorithm,
								   &se_config_reg);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = engine_aes_set_op_mode_decrypt_crypto_config(econtext,
								   algorithm,
								   &se_config_reg);
		CCC_ERROR_CHECK(ret);
	}

	/* Set the IV_SELECT field for CRYPTO_CONFIG */
	se_config_reg = aes_set_iv_select_field(algorithm, se_config_reg,
						has_internal_context,
						use_orig_iv);

	/* Inject the used keyslot. */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
					   KEY_INDEX, econtext->se_ks.ks_slot,
					   se_config_reg);

#if HAVE_SE_SCC
	/* ENABLE SE SCC (disable SCC_DIS) */

	/* Jerry: AES CRYPTO CONFIG => SCC_DIS must be set to 0 to enable SCC (confirm please!)
	 * FIXME: remove this comment...
	 */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   SCC_DIS, DISABLE,
					   se_config_reg);
#else
	/* DISABLE SE SCC (enable SCC_DIS) */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   SCC_DIS, ENABLE,
					   se_config_reg);
#endif /* HAVE_SE_SCC */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

	ret = engine_aes_op_mode_write_linear_counter(econtext, algorithm);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_AES_GCM
/* AES-GCM DEC_MODE / ENC_MODE field values in CONFIG register
 *
 * SE_MODE_PKT_AESMODE_GMAC
 * SE_MODE_PKT_AESMODE_GCM
 * SE_MODE_PKT_AESMODE_GCM_FINAL
 *
 * NOTE:
 * With GMAC_AES the is_hash is true and it is NOT handled in this file.
 */
static status_t engine_aes_op_mode_gcm(const struct se_engine_aes_context *econtext,
				       te_crypto_algo_t algorithm,
				       bool is_encrypt_arg, bool use_orig_iv,
				       uint32_t config_dst)
{
	status_t ret = NO_ERROR;
	uint32_t pkt_mode = 0U;
	bool is_encrypt = is_encrypt_arg;

	LTRACEF("entry\n");

	if ((econtext->aes_flags & AES_FLAGS_AE_AAD_DATA) != 0U) {
		pkt_mode = SE_MODE_PKT_AESMODE_GMAC;
	} else {
		pkt_mode = SE_MODE_PKT_AESMODE_GCM;
	}

	ret = engine_aes_op_mode_normal(econtext, algorithm, is_encrypt, use_orig_iv,
					pkt_mode, config_dst);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_GCM */

/* Setup the operating mode registers for AES (modes)
 * encryption/decryption and CMAC Hash
 */
status_t setup_aes_op_mode_locked(
	const struct se_engine_aes_context *econtext, te_crypto_algo_t algorithm,
	bool is_encrypt, bool use_orig_iv, uint32_t config_dst)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry: algo 0x%x, is_encrypt=%u, orig_iv=%u\n",
		algorithm, is_encrypt, use_orig_iv);

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_aes_set_op_mode_check_args(econtext,
						algorithm,
						config_dst);
	CCC_ERROR_CHECK(ret);

	switch (algorithm) {
#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES: /* MAC */
		if (!BOOL_IS_TRUE(is_encrypt)) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		/* Always in encrypt mode */
		ret = engine_aes_op_mode_normal(econtext, algorithm, CTRUE, use_orig_iv, 0U,
						config_dst);
		break;
#endif

#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
		ret = engine_aes_op_mode_gcm(econtext, algorithm, is_encrypt, use_orig_iv,
					     config_dst);
		break;
#endif

#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_OFB
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM: /* Like AES-CTR */
#endif
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
		/* decipher and cipher use the same SE_CONFIG and SE_CRYPTO_CONFIG settings
		 * for CMAC, CTR, OFB and GCM.
		 *
		 * Always in encrypt mode
		 */
		ret = engine_aes_op_mode_normal(econtext, algorithm, CTRUE, use_orig_iv, 0U,
						config_dst);
		break;
#endif /* HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_OFB */

	case TE_ALG_AES_CTS:
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
		ret = engine_aes_op_mode_normal(econtext, algorithm, is_encrypt, use_orig_iv, 0U,
						config_dst);
		break;

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		ret = engine_aes_op_mode_xts(econtext, is_encrypt, 0U);
		break;
#endif /* HAVE_AES_XTS */

	default:
		/* [COVERITY] It is true that this is lwrrently deadcode, but if
		 * new algorithms are added to the enum then this would catch
		 * those before the support is added.
		 */
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**
 * @brief Write an AES Key to an AES key slot in the SE.
 *	If keydata == NULL, this function sets the keyslot or IV to all zeroes.
 *
 * @param engine seleted engine
 * @param keyslot SE key slot number.
 * @param keysize_bits_param AES keysize in bits.
 * @param ksdata word pointer to key data. Must be valid memory location with valid
 *				data or NULL. If NULL set the keyslot entity to all zeroes.
 * @param algorithm Algorithm using the keyslot being written
 *
 * @return NO_ERROR in case of no error.
 *	returns specific error in case of any error.
 *
 */
static status_t aes_set_key_locked(
	const engine_t *engine,
	uint32_t keyslot,
	uint32_t keysize_bits_param,
	const uint8_t *ksdata,
	te_crypto_algo_t algorithm)
{
	status_t ret = NO_ERROR;
	uint32_t se_kac_flags = SE_KAC_FLAGS_NONE;
	const uint32_t se_kac_sw = 0U;

	LTRACEF("entry\n");
	ret = se_set_engine_kac_keyslot_locked(engine, keyslot, ksdata,
					       keysize_bits_param,
					       SE_KAC_KEYSLOT_USER,
					       se_kac_flags,
					       se_kac_sw,
					       algorithm);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CCC_LIBRARY_MODE_CALLS
/* This function exists only for backwards compatibility
 * TODO => rework crypto_ks_se.c to get rid of this for T23x.
 *
 * Use only to set legacy AES mode keys.
 *
 * Does not work for setting XTS or GCM keys.
 *
 * Since the algorithm is not passed by caller => use AES-CBC for setting the AES keyslot
 * key (key setting is identical for other legacy AES modes: ECB, CTR, OFB).
 */
status_t aes_write_key_iv_locked(
	const struct engine_s *engine,
	uint32_t keyslot, uint32_t keysize_bits_param,
	se_aes_keyslot_id_sel_t id_sel,
	const uint8_t *keydata)
{
	status_t ret = NO_ERROR;
	uint32_t keysize_bits = keysize_bits_param;

	LTRACEF("entry\n");

	if (id_sel != SE_AES_KSLOT_SEL_KEY) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Obsolete way of setting keyslot field %u used\n",
					     (uint32_t)id_sel));
	}

	ret = aes_set_key_locked(engine,
				 keyslot,
				 keysize_bits,
				 keydata,
				 TE_ALG_AES_CBC);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CCC_LIBRARY_MODE_CALLS */

static status_t aes_adjust_blocks(const struct se_engine_aes_context *econtext,
				  uint32_t unaligned_bytes,
				  uint32_t *num_blocks_p, uint32_t *last_block_p)
{
	status_t ret = NO_ERROR;
	uint32_t num_blocks = *num_blocks_p;
	uint32_t last_block = *last_block_p;

	(void)econtext;

	LTRACEF("entry\n");

	if (num_blocks != last_block) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Unexpected state %u != %u\n",
					     num_blocks, last_block));
	}

	if (0U != unaligned_bytes) {
		/* In case XTS/GCM has non-block aligned
		 * bytes => need to handle one partial
		 * AES block more
		 *
		 * last_block already is ok the HW used value
		 * is num_blocks - 1 after this increment.
		 */
		num_blocks++;
	} else {
		if (0U == num_blocks) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
					     LTRACEF("AES 0x%x zero block count\n",
						     econtext->algorithm));
		} else {
			/* LAST_BLOCK = number_of_blocks - 1 in general case */
			last_block--;
		}
	}

	*num_blocks_p = num_blocks;
	*last_block_p = last_block;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_AES_XTS
static status_t aes_xts_residual_bit_count(const struct se_engine_aes_context *econtext,
					   uint32_t unaligned_bytes,
					   uint32_t *residual_bits_p)
{
	status_t ret = NO_ERROR;
	uint32_t residual_bits = *residual_bits_p;

	LTRACEF("entry\n");

	if (0U != unaligned_bytes) {
		/* 1..15 bytes in last block */
		residual_bits = unaligned_bytes * 8U;
	}

	if (0U != econtext->xts.last_byte_unused_bits) {
		if (econtext->xts.last_byte_unused_bits > 7U) {
			/* Client args trapped in crypto_cipher
			 * when setting this. If true here =>
			 * some other issue...
			 */
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		/* 1..7 trailing bits not used in last byte */
		residual_bits -= econtext->xts.last_byte_unused_bits;
	}

	*residual_bits_p = residual_bits;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_XTS */

#if HAVE_SE_KAC_KEYOP || CCC_WITH_AES_GCM
static void aes_residual_bit_count(uint32_t unaligned_bytes,
				   uint32_t *residual_bits_p)
{
	uint32_t residual_bits = *residual_bits_p;

	if (0U != unaligned_bytes) {
		/* 1..15 bytes in last block */
		residual_bits = unaligned_bytes * 8U;
	}

	*residual_bits_p = residual_bits;
}
#endif /* HAVE_SE_KAC_KEYOP || CCC_WITH_AES_GCM */

/*
 * The SE0_AES0_CRYPTO_LAST_BLOCK_0 value is callwlated as follows
 * given in the SE IAS section 3.2.3.1 AES Input Data Size.
 *
 * Input Bytes    = 16 bytes * (1 + CRYPTO_LAST_BLOCK)
 * num_blocks*16  = 16 * (1 + CRYPTO_LAST_BLOCK)
 * num_blocks - 1 = CRYPTO_LAST_BLOCK
 *
 * This is slightly more complex for AES-XTS since the data do
 * not necessarily have to be block aligned: AES-XTS may have
 * one block more if num_bytes is not block aligned (adjusted
 * below).
 *
 * Callwlates the value os SE engine AES last block register
 * This is one less than the number of blocks.
 *
 * CCC supports the engine feature of specifying bit lengths for
 * the AES-XTS algorithm data. This is dealt with below:
 * the last byte may optionally have "unused bits" which are
 * excluded from the SE0_AES0_CRYPTO_LAST_BLOCK_0 setup.
 *
 * From IAS 0.7 and later, AES engines are identical in T23X.
 * [ All operations, including AES-XTS are supported with both AES engines ]
 */
status_t engine_aes_set_block_count(
	const struct se_engine_aes_context *econtext,
	uint32_t num_bytes,
	uint32_t *num_blocks_p)
{
	status_t ret = NO_ERROR;
	uint32_t num_blocks = 0U;
	uint32_t last_block = 0U;

	/* Expect last block aligned... */
	uint32_t residual_bits = (SE_AES_BLOCK_LENGTH * 8U);
	uint32_t unaligned_bytes = num_bytes % SE_AES_BLOCK_LENGTH;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == num_blocks_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	num_blocks = num_bytes / SE_AES_BLOCK_LENGTH;
	last_block = num_blocks;

	ret = aes_adjust_blocks(econtext, unaligned_bytes, &num_blocks, &last_block);
	CCC_ERROR_CHECK(ret);

	switch (econtext->algorithm) {
#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		/* HW uses cipher text stealing to manage the last two AES-XTS blocks
		 * (in case the length is not aligned). If it is aligned, no swap.
		 *
		 * Much like in NIST-CBC-CS2 mode for AES-CTS.
		 */
		ret = aes_xts_residual_bit_count(econtext,
						 unaligned_bytes,
						 &residual_bits);
		break;
#endif /* HAVE_AES_XTS */

#if HAVE_SE_KAC_KEYOP
	case TE_ALG_KEYOP_KWUW:
	case TE_ALG_KEYOP_KUW:
	case TE_ALG_KEYOP_KW:
		aes_residual_bit_count(unaligned_bytes, &residual_bits);

		/* Assume AAD is byte size multiple => no need to add bit length support */
		break;
#endif /* HAVE_SE_KAC_KEYOP */

#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
#if HAVE_AES_CTS
	case TE_ALG_AES_CTS: /* From engine point of view CTS identical to CBC */
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
	case TE_ALG_AES_CTR:
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
		if (unaligned_bytes != 0U) {
			LTRACEF("AES algo 0x%x last block not size aligned for HW\n",
				econtext->algorithm);
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
		break;

#if CCC_WITH_AES_GCM
	case TE_ALG_AES_GCM:
	case TE_ALG_GMAC_AES:
		/* Lwrrently only supports byte aligned last block bits...
		 */
		aes_residual_bit_count(unaligned_bytes, &residual_bits);
		break;
#endif

	default:
		/* All AES modes need to be handled above for last block settings.
		 * when supported.
		 */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if ((SE_AES_BLOCK_LENGTH * 8U) != residual_bits) {
		/* KW/KUW/KWUW/GCM/GMAC/XTS now supported on both AES0 && AES1.
		 *
		 * RESIDUAL_BITS related syms are defined in KAC
		 * enabled units; not the case for e.g. T18X.
		 */
		last_block = LW_FLD_SET_DRF_NUM(SE0_AES0,
						CRYPTO_LAST_BLOCK,
						RESIDUAL_BITS,
						residual_bits,
						last_block);

		LTRACEF("AES mode unaligned last block contains %u bytes (%u bits)\n",
			unaligned_bytes, residual_bits);
	}

	tegra_engine_set_reg(econtext->engine,
			     SE0_AES0_CRYPTO_LAST_BLOCK_0, last_block);
	*num_blocks_p = num_blocks;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_encrypt_decrypt_arg_check(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ac) || (NULL == ac->econtext) ||
	    (NULL ==  ac->input.src) ||
	    (0U == ac->input.input_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((ac->econtext->aes_flags & AES_FLAGS_AE_AAD_DATA) == 0U) {
		if (NULL == ac->input.dst) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_output_paddr(async_aes_ctx_t *ac,
				 PHYS_ADDR_T paddr_src_addr,
				 PHYS_ADDR_T *paddr_dst_addr_p)
{
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr_dst_addr = *paddr_dst_addr_p;

	LTRACEF("entry\n");

	if (ac->input.src != ac->input.dst) {
		paddr_dst_addr = DOMAIN_VADDR_TO_PADDR(ac->econtext->domain,
						       ac->input.dst);
		SE_PHYS_RANGE_CHECK(paddr_dst_addr,
				    ac->flush_dst_size);

		/* Just flush for the DST here; it will get
		 * ilwalidated only once after the AES operation
		 * completes. This makes a difference in a
		 * write-through cache system.
		 */
		SE_CACHE_FLUSH((VIRT_ADDR_T)ac->input.dst, ac->flush_dst_size);
	} else {
		/* Buffers overlap with same address, use the src
		 * mapping done above.
		 */
		paddr_dst_addr = paddr_src_addr;
	}

	if (ac->input.output_size < ac->input.input_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	*paddr_dst_addr_p = paddr_dst_addr;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_input_output_regs(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	PHYS_ADDR_T paddr_src_addr = PADDR_NULL;
	PHYS_ADDR_T paddr_dst_addr = PADDR_NULL;
	uint32_t se_config_reg = 0U;
	uint32_t out_size = 0U;

	LTRACEF("entry\n");

	if (NULL == ac->input.src) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (MAX_AES_ENGINE_CHUNK_SIZE < ac->input.input_size) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	paddr_src_addr = DOMAIN_VADDR_TO_PADDR(ac->econtext->domain,
					       ac->input.src);
	SE_PHYS_RANGE_CHECK(paddr_src_addr, ac->input.input_size);

	SE_CACHE_FLUSH((VIRT_ADDR_T)ac->input.src,
		       DMA_ALIGN_SIZE(ac->input.input_size));

	/* Program input message address. */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_src_addr));

	/* Program input message buffer size into
	 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b dma
	 * addr into MSB field
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, SZ, ac->input.input_size,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_src_addr),
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_HI_0,
			     se_config_reg);

	/* If passing in AES-GCM AAD, dst is NULL
	 */
	if (NULL != ac->input.dst) {
		ret = aes_output_paddr(ac, paddr_src_addr, &paddr_dst_addr);
		CCC_ERROR_CHECK(ret);

		/* Using input_size as output size in normal cases */
		out_size = ac->input.input_size;
	}

	/* Program output address. If <NULL,0U> program that */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_dst_addr));

	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, out_size,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_dst_addr),
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_HI_0,
			     se_config_reg);

#if HAVE_SE_APERTURE
	ret = se_set_aperture(paddr_src_addr, ac->input.input_size,
			      paddr_dst_addr, ac->input.output_size);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_enc_dec_engine_setup_locked(async_aes_ctx_t *ac,
						uint32_t *op_preset_p)
{
	status_t ret = NO_ERROR;

	(void)op_preset_p;

	LTRACEF("entry\n");

	/* Setup SE engine parameters for AES encrypt operation. */
	ret = setup_aes_op_mode_locked(
		ac->econtext, ac->econtext->algorithm, ac->econtext->is_encrypt, ac->econtext->is_first,
		SE0_AES0_CONFIG_0_DST_MEMORY);
	CCC_ERROR_CHECK(ret);


	/* Callwlate the number of blocks written to the engine and write the
	 * SE engine SE_CRYPTO_LAST_BLOCK_0
	 */
	ret = engine_aes_set_block_count(ac->econtext, ac->input.input_size, &ac->num_blocks);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LTRACEF("[num_blocks %u (bytes %u) (src= %p, dst=%p), algo=0x%x]\n",
		ac->num_blocks, ac->input.input_size, ac->input.src,
		ac->input.dst, ac->econtext->algorithm);
#endif

#if HAVE_AES_COUNTER_OVERFLOW_CHECK
	ret = do_aes_counter_overflow_check(ac);
	CCC_ERROR_CHECK(ret);
#endif

#if HAVE_AES_GCM
	if (TE_ALG_AES_GCM == ac->econtext->algorithm) {
		ret = eng_aes_gcm_get_operation_preset(ac->econtext, op_preset_p);
		CCC_ERROR_CHECK(ret);
	}
#endif

	ret = aes_input_output_regs(ac);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_enc_dec_engine_start_locked(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t op_preset = 0U;

	LTRACEF("entry\n");

	ret = aes_enc_dec_engine_setup_locked(ac, &op_preset);
	CCC_ERROR_CHECK(ret);

	AES_ERR_CHECK(ac->econtext->engine, "AES START");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	ac->gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(ac->econtext->engine, op_preset);
	CCC_ERROR_CHECK(ret);

	ac->started = true;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_enc_dec_algo_setup(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ac->econtext->algorithm) {
#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM
#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
		ac->update_ctr = true;

		if ((ac->econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) == 0U) {
			LTRACEF("Little endian counters are not supported\n");
			ret = SE_ERROR(ERR_NOT_SUPPORTED);
			break;
		}
		break;
#endif /* HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM */

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		if (ac->input.input_size < SE_AES_BLOCK_LENGTH) {
			LTRACEF("AES-XTS engine requires at least 16 bytes of data\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
		break;
#endif /* HAVE_AES_XTS */

	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_CTS: /* From engine point of view CTS identical to CBC */
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Encrypt/Decrypt given input data
 *
 * XXXX AES ENGINE output buffer must be cache line aligned to avoid issues.
 */
static status_t aes_encrypt_decrypt_locked_async_start(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_encrypt_decrypt_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AES operation context already started\n"));
	}

	ac->update_ctr = false;

	if (MAX_AES_ENGINE_CHUNK_SIZE < ac->input.input_size) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	ac->flush_dst_size = ac->input.input_size;

	/* Padding increasing output size to next block boundary
	 */
	if (BOOL_IS_TRUE(ac->econtext->is_encrypt) &&
	    TE_AES_MODE_PADDING(ac->econtext->algorithm)) {
		uint32_t remain = ac->input.input_size % SE_AES_BLOCK_LENGTH;

		ac->flush_dst_size += (SE_AES_BLOCK_LENGTH - remain);
	}
	ac->flush_dst_size = DMA_ALIGN_SIZE(ac->flush_dst_size);

	ret = aes_enc_dec_algo_setup(ac);
	CCC_ERROR_CHECK(ret);

	ret = aes_enc_dec_engine_start_locked(ac);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_encrypt_decrypt_locked_async_finish(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

#if HAVE_SE_ASYNC_AES
	ret = aes_encrypt_decrypt_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("Async AES not started\n"));
	}

	/* This can be called only once */
	ac->started = false;
#endif

	ret = se_wait_engine_free(ac->econtext->engine);
	CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	if (0U != ac->gen_op_perf_usec) {
		show_se_gen_op_entry_perf("AES", get_elapsed_usec(ac->gen_op_perf_usec));
	}
#endif

	AES_ERR_CHECK(ac->econtext->engine, "AES DONE");

	/* This ilwalidation MUST BE LEFT IN PLACE since the CPU may
	 * prefetch the data while the SE AES DMA writes the phys mem
	 * values causing inconsistencies.
	 *
	 * Without this ilwalidate the prefetched cache content may
	 * cause the AES result to be incorrect when the CPU reads it.
	 *
	 * Yes, this actually happens! So general case must not remove
	 * this. If the SE DMA keeps caches coherent, then that
	 * particular case can reconsider all these cache management
	 * ops saving a lot of time.
	 */
	SE_CACHE_ILWALIDATE((VIRT_ADDR_T)ac->input.dst, ac->flush_dst_size);

#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM
	if (BOOL_IS_TRUE(ac->update_ctr)) {
		ret = se_update_linear_counter(ac->econtext, ac->num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_process_arg_check_rest(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* AES-XTS and AES-GCM (&& GMAC) engines expect
	 * the last block size to be correct, so SW must not
	 * pad the length to AES block size.
	 */
	if ((TE_ALG_AES_XTS != ac->econtext->algorithm) &&
	    !TE_AES_GCM_MODE(ac->econtext->algorithm)) {
		/* AES-CBC-CS2 (TE_ALG_AES_CTS) which also
		 * swaps the last blocks (unless len % 16 == 0)
		 * much like XTS with unaligned buffers.
		 *
		 * But for XTS the HW engine deals with this, for AES_CTS
		 * the process layer handles this and engine
		 * only sees full blocks in CTS mode.
		 */
		if ((ac->input.input_size % SE_AES_BLOCK_LENGTH) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

	if ((ac->econtext->aes_flags & AES_FLAGS_AE_AAD_DATA) == 0U) {
		if (NULL == ac->input.dst) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		if (ac->input.output_size < ac->input.input_size) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_process_arg_check(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(ac->econtext->is_hash)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!IS_VALID_AES_KEYSIZE_BITS(ac->econtext->se_ks.ks_bitsize)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (ac->econtext->se_ks.ks_slot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ac->input.src) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = engine_aes_process_arg_check_rest(ac);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_AES
status_t engine_aes_locked_async_done(const async_aes_ctx_t *ac, bool *is_idle_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == ac) || (NULL == ac->econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = engine_aes_process_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (NULL == is_idle_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					 CCC_ERROR_MESSAGE("Async AES operation has not been started\n"));
	}

	if (AES_ASYNC_MAGIC != ac->magic) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("AES AC magic: 0x%x",
						       ac->magic));
	}

	ret = se_async_check_engine_free(ac->econtext->engine, is_idle_p);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_ASYNC_AES */

/* Write the SE engine AES keyslot key material.
 *
 * For AES-XTS the input subkey is concatenated after the master AES pkey
 * of se_ks.ks_bitsize bits.
 */
static status_t engine_aes_write_keyslot(struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_set_key_locked(
		econtext->engine,
		econtext->se_ks.ks_slot, econtext->se_ks.ks_bitsize,
		econtext->se_ks.ks_keydata,
		econtext->algorithm);
	CCC_ERROR_CHECK(ret);

#if HAVE_AES_XTS
	// XXXX T23X uses two keyslots, no catenated subkey like in earlier units
	//
	if (TE_ALG_AES_XTS == econtext->algorithm) {
		/* AES XTS mode subkey is catenated after the aes_key */
		ret = aes_set_key_locked(
			econtext->engine,
			econtext->se_ks.ks_subkey_slot,
			econtext->se_ks.ks_bitsize,
			&econtext->se_ks.ks_keydata[econtext->se_ks.ks_bitsize / 8U],
			TE_ALG_AES_XTS);
		CCC_ERROR_CHECK(ret);
	}
#endif

	/* Some operations like HW AES-GCM internally need to use
	 * HW keyslot context until context is reset.
	 *
	 * So if the SW sets the key initially from now on this
	 * context needs to switch to using that key setup until
	 * the context is reset. If the key is incorrectly set
	 * in the middle of GCM operation, the on-going operation
	 * intermediate context will be lost.
	 */
	if ((econtext->aes_flags & AES_FLAGS_HW_CONTEXT) != 0U) {
		econtext->aes_flags |= AES_FLAGS_USE_PRESET_KEY;
		LTRACEF("HW context key set to %u, switching to HW context key\n",
			econtext->se_ks.ks_slot);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_write_iv(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_write_iv_locked(econtext->engine, econtext->iv_stash);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to set IV\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_aes_setup_key_iv(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((ac->econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* Commit the pre-existing keyslot */
		ac->use_preset_key = true;
	} else {
		if (NULL == ac->econtext->se_ks.ks_keydata) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AES key undefined\n"));
		}

		/* commit keyslot; forces slot clear even if write fails */
		ac->key_written = true;

		ret = engine_aes_write_keyslot(ac->econtext);
		CCC_ERROR_CHECK(ret);
	}

	/* If the loaded key must be left to keyslot after the operation
	 */
	if ((ac->econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U) {
		ac->leave_key_to_keyslot = true;
	}

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		ret = engine_aes_write_iv(ac->econtext);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_process_locked_async_start(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if ((NULL == ac) || (NULL == ac->econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = engine_aes_process_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AES operation context already started\n"));
	}

	/* AES engine can only handle up to ((2^24 / 16) * 16) bytes
	 * in one call (Largest number of 16 byte blocks in (2^24-1)
	 * bytes).
	 */
	if (ac->input.input_size > MAX_AES_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("AES input chunk size too big: 0x%x\n",
					     ac->input.input_size));
	}

	ret = engine_aes_setup_key_iv(ac);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LOG_HEXBUF("AES src:", ac->input.src, ac->input.input_size);
	LTRACEF("AES dst: %p, dst size: %u\n", ac->input.dst, ac->input.output_size);
#endif

	ret = aes_encrypt_decrypt_locked_async_start(ac);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_AES_OFB
/* In AES-OFB the updated_iv value is raw AES output, which is value of
 * XOR(mem_block, last PT/CT block).
 */
static status_t aes_ofb_extract_iv(const async_aes_ctx_t *ac, uint8_t *iv)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	const uint8_t *xor_buf = NULL;

	LTRACEF("entry\n");

	if (ac->input.input_size < SE_AES_BLOCK_LENGTH) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("AES-OFB input size less than one block %u\n",
					     ac->input.input_size));
	}

	/* Xor PT/CT block from input (PT or CT, per direction) */
	xor_buf = &ac->input.src[ac->input.input_size - SE_AES_BLOCK_LENGTH];

	for (; inx < SE_AES_BLOCK_LENGTH; inx++) {
		uint32_t tiv = iv[inx];
		uint32_t txor  = xor_buf[inx];

		tiv = tiv ^ txor;
		iv[inx] = BYTE_VALUE(tiv);
	}
	LOG_HEXBUF("AES-OFB updated IV", iv, SE_AES_BLOCK_LENGTH);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_OFB */

static status_t aes_get_updated_iv_from_memory(const async_aes_ctx_t *ac, uint8_t *iv)
{
	status_t ret = NO_ERROR;
	const uint8_t *iv_buf = NULL;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(ac->econtext->is_encrypt)) {
		// Block encryption size invalid
		if ((ac->input.output_size % SE_AES_BLOCK_LENGTH) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN, LTRACEF("AES ctext dst size %d\n",
								  ac->input.output_size));
		}

		if (0U == ac->input.output_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN, LTRACEF("AES ctext empty\n"));
		}

		iv_buf = &ac->input.dst[ac->input.output_size - SE_AES_BLOCK_LENGTH];
	} else {
		// Block decryption size invalid
		if ((ac->input.input_size % SE_AES_BLOCK_LENGTH) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN, LTRACEF("AES ctext src size %d\n",
								  ac->input.input_size));
		}

		if (0U == ac->input.input_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN, LTRACEF("AES ctext empty\n"));
		}

		iv_buf = &ac->input.src[ac->input.input_size - SE_AES_BLOCK_LENGTH];
	}

	LOG_HEXBUF("AES IV read from memory", iv_buf, SE_AES_BLOCK_LENGTH);

	se_util_mem_move(iv, iv_buf, SE_AES_BLOCK_LENGTH);

#if HAVE_AES_OFB
	/* In AES-OFB the updated_iv value is raw AES output, which is value of
	 * XOR(mem_block, last PT/CT block).
	 */
	if (TE_ALG_AES_OFB == ac->econtext->algorithm) {
		ret = aes_ofb_extract_iv(ac, iv);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* If key was committed and not flagged to leave in keyslot => clear slot
 * If iret has an error on input, it will be returned as error from this call.
 */
static status_t aes_ac_clear_keyslot(async_aes_ctx_t *ac, status_t iret)
{
	status_t ret = iret;

	/* HW CONTEXT will prevent AES keyslot clear unless there is an error.
	 */
	bool hw_context_gate =
		(((ac->econtext->aes_flags & AES_FLAGS_HW_CONTEXT) != 0U) &&
		 (NO_ERROR == iret));
	bool key_in_slot = (BOOL_IS_TRUE(ac->use_preset_key) || BOOL_IS_TRUE(ac->key_written));

	/* Clear existing key if error on entry or if leave_key is not set.
	 * The hw context gate can prevent this clearing if no error condition.
	 */
	if (BOOL_IS_TRUE(key_in_slot) &&
	    (!BOOL_IS_TRUE(ac->leave_key_to_keyslot) &&
	     !BOOL_IS_TRUE(hw_context_gate))) {

		/* Even with ECB/ECB_NOPAD also clear the IV (or linear_counter_regs)
		 * (Could use TE_AES_MODE_USES_IV(algo) || TE_ALG_MODE_COUNTER(algo)
		 *  instead of "true")
		 */
		status_t fret = tegra_se_clear_aes_keyslot_locked(
			ac->econtext->engine, ac->econtext->se_ks.ks_slot, true);

		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear engine %u AES keyslot %u\n",
				ac->econtext->engine->e_id, ac->econtext->se_ks.ks_slot);
			if (NO_ERROR == ret) {
				ret = fret;
			}
			/* FALLTHROUGH */
		}
	}

	return ret;
}

status_t aes_process_locked_async_finish(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* AC checked by this callee */
	ret = aes_encrypt_decrypt_locked_async_finish(ac);
	CCC_ERROR_CHECK(ret);

	ac->econtext->is_first = false;

	if (NULL != ac->input.dst) {
		ac->input.output_size = ac->input.input_size;
	}

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		ret = aes_get_updated_iv_from_memory(ac, (uint8_t *)&ac->econtext->iv_stash[0]);
		CCC_ERROR_CHECK(ret);
	}

#if SE_RD_DEBUG
	if (NULL != ac->input.dst) {
		LOG_HEXBUF("AES dst:", ac->input.dst, ac->input.output_size);
	}
#endif

fail:
	ret = aes_ac_clear_keyslot(ac, ret);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if ASYNC_FUNCTION_EXPOSE

status_t engine_aes_locked_async_start(async_aes_ctx_t *ac)
{
	return aes_process_locked_async_start(ac);
}

status_t engine_aes_locked_async_finish(async_aes_ctx_t *ac)
{
	return aes_process_locked_async_finish(ac);
}

#if HAVE_LONG_ASYNC_NAMES
/* Backwards compatibility only */
status_t engine_aes_process_locked_async_done(const async_aes_ctx_t *ac, bool *is_idle_p)
{
	return engine_aes_locked_async_done(ac, is_idle_p);
}

status_t engine_aes_process_block_locked_async_start(async_aes_ctx_t *ac)
{
	return engine_aes_locked_async_start(ac);
}

status_t engine_aes_process_block_locked_async_finish(async_aes_ctx_t *ac)
{
	return engine_aes_locked_async_finish(ac);
}
#endif /* HAVE_LONG_ASYNC_NAMES*/

#endif /* ASYNC_FUNCTION_EXPOSE */

status_t engine_aes_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	async_aes_ctx_t ac = { .magic = 0U };
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ac.input = *input_params;
	ac.econtext = econtext;

	CCC_LOOP_BEGIN {
#if CCC_WITH_AES_GCM
		if (TE_AES_GCM_MODE(econtext->algorithm)) {
			ret = eng_aes_gcm_process_block_locked(&ac);
			CCC_LOOP_STOP;
		}
#endif /* CCC_WITH_AES_GCM */

		ret = aes_process_locked_async_start(&ac);
		CCC_ERROR_END_LOOP(ret);

		ret = aes_process_locked_async_finish(&ac);
		CCC_ERROR_END_LOOP(ret);

		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	if ((NO_ERROR != ret) && (NULL != ac.econtext)) {
		(void)aes_ac_clear_keyslot(&ac, NO_ERROR);
	}

	se_util_mem_set((uint8_t *)&ac, 0U, sizeof_u32(ac));

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_aes_close_locked(void)
{
	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	return;
}
#endif

static status_t engine_aes_clear_iv(const engine_t *engine, uint32_t keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)keyslot;

	ret = aes_write_iv_locked(engine, NULL);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set clear_ivs false only when there is no security concern with leaking
 * IV values.
 *
 * Note that AES_XTS mode sets the subkey to IV fields => you should
 * always clear the IVs when using XTS! (that is one such security
 * concern!) This call does not know the algorithm/mode => caller must
 * set clear_ivs true in that case.
 */
status_t tegra_se_clear_aes_keyslot_locked(const engine_t *engine,
					   uint32_t keyslot,
					   bool clear_ivs)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	AES_ERR_CHECK(engine, "before kslot clear");

	if ((engine->e_class & CCC_ENGINE_CLASS_AES) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("AES key clear with non-aes engine (%s)\n",
						       engine->e_name));
	}

	/* Clear key entity in keyslot */
	ret = aes_set_key_locked(engine, keyslot, 256U, NULL, TE_ALG_AES_CBC);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(clear_ivs)) {
		ret = engine_aes_clear_iv(engine, keyslot);
		CCC_ERROR_CHECK(ret);
	}


	AES_ERR_CHECK(engine, "after kslot clear");
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES */
