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

/* NOTE about key slots.  As a convention we should assume that one
 * out of the 4 rsa keyslots is reserved for secure code (e.g. this
 * driver) and this reg shyould then be set TZ access only ( which is
 * a sticky to reset setting). Otherwise the SE driver can not do any
 * RSA operations with the SE PKA0 unit.
 *
 * As a convention (as dislwssed and suggested by me ;-) I will take
 * the last RSA key slot (slot 3) and will make it TZ only at some
 * point of time before releasing this driver...
 *
 * Same thing for AES key slots => I need at least one keyslot for
 * this code, but I do not know which number I can start to use =>
 * more dislwssions required. Now using AES key slot 2 without
 * agreements of any kind for testing (but hey, it seems to
 * work!).
 *
 * What ever slot is reserved for this it will most likely be
 * the only keyslot this driver can use to write keys. Existing key
 * use will need some authorization mechanism (as dislwssed the
 * dynloader adds simple manifest authorizations) but theese are not
 * yet merged. Lwrrently any client can use any keyslot (which is not
 * really that fine, even though all clients are secure).
 *
 * In case I do the keystore using ciphered keys => then it might be
 * required to allocate one more AES keyslot for this purpose.
 */

/* XXX TODO set "TZ use only" to any keyslot we set in this code before setting it...
 */

#include <crypto_system_config.h>

#if HAVE_SOC_FEATURE_KAC
#error "HAVE_SOC_FEATURE_KAC set when compiling tegra_se_aes.c"
#endif

#if HAVE_SE_AES

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#ifndef SE0_AES0_SPARE_0
/* This file only exists in T194 and newer; e.g. this define
 * has been moved to the private version.
 */
#include <arse0_private.h>
#endif

#include <crypto_ops.h>
#include <tegra_se_gen.h>
#include <tegra_se_aes.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/**** AES operations (also used by CMAC-AES) ****/

/*
 * @brief Generate the value of SE_CRYPTO_KEYIV_PKT for SE AES key operations.
 *
 * @param keyslot SE AES key slot number.
 * @param id_sel Select a Key, original IV or updated IV. Use SE_AES_KSLOT_SEL_*.
 * @param word_sel Select the word number to access in the AES key or IV.
 * @param [out]se_keyiv_pkt value of SE_CRYPTO_KEYIV_PKT expected
 *
 * @return NO_ERROR if success, error if fails
 */
static status_t se_create_keyiv_pkt(
	uint32_t keyslot, se_aes_keyslot_id_sel_t id_sel,
	uint32_t word_sel, uint32_t *se_keyiv_pkt)
{
	status_t ret = NO_ERROR;
	uint32_t keyiv_pkt = 0U;
	uint32_t iv_sel = SE_CRYPTO_KEYIV_PKT_IV_SEL_ORIGINAL;

	LTRACEF("entry\n");
	*se_keyiv_pkt = 0U;

	switch (id_sel) {
	case SE_AES_KSLOT_SEL_KEY:
		/* IV_SEL and IV_WORD aren't use in the key case, and overlap with
		 * KEY_WORD in SE_CRYPTO_KEYIV_PKT.
		 */
		keyiv_pkt = (
			(keyslot << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
			(SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_KEY << SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
			(word_sel << SE_CRYPTO_KEYIV_PKT_KEY_WORD_SHIFT)
			);
		break;

#if HAVE_AES_XTS

// XXXXXX FIXME: Do I need to set keytable access CIPHER_MODE => XTS when using XTS and then some other
// value when not using XTS (or is it sticky to reset?)???
//
// XXXXXX FIXME: need to set LAST_BLOCK field RESIDUAL_BITS if the AES-XTS last aes block is not 16 bytes.
// zero == 16 bytes. The bits for this field are 26:20. HW supports cipher block stealing to manage this.
// XXXXX Note: engine only support MIN 16 byte data for AES-XTS (probably because the CTS block swap ;-)

	case SE_AES_KSLOT_SEL_SUBKEY:
		/* IV_SEL and IV_WORD are used in the XTS case to set the
		 * XTS mode subkey.
		 *
		 * This selects XTS subkey, which is actually placed in
		 * IV field (This is OK since the IV is not used for XTS)
		 *
		 * For some reason these do not seem to be defined in arse0.h =>
		 * add definition here for now (FIXME)
		 *
		 * The key word may be 0..3 for AES-XTS-128
		 * and 0..7 for the AES-XTS-256
		 */
/* XXXXXX FIXME: move these elsewhere! */
#ifndef SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SUBKEY
#define SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SUBKEY SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV
#endif

#ifndef SE_CRYPTO_KEYIV_PKT_SUBKEY_SEL_SHIFT
#define SE_CRYPTO_KEYIV_PKT_SUBKEY_SEL_SHIFT SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT
#endif

		keyiv_pkt = (
			(keyslot << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
			(SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SUBKEY << SE_CRYPTO_KEYIV_PKT_SUBKEY_SEL_SHIFT) |
			(word_sel << SE_CRYPTO_KEYIV_PKT_KEY_WORD_SHIFT)
			);
		break;
#endif /* HAVE_AES_XTS */

	case SE_AES_KSLOT_SEL_UPDATED_IV:
	case SE_AES_KSLOT_SEL_ORIGINAL_IV:
		if (SE_AES_KSLOT_SEL_UPDATED_IV == id_sel) {
			iv_sel = SE_CRYPTO_KEYIV_PKT_IV_SEL_UPDATED;
		}

		if (word_sel > 3U) {
			CCC_ERROR_MESSAGE("IV word too large: %u\n", word_sel);
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		keyiv_pkt = (
			(keyslot << SE_CRYPTO_KEYIV_PKT_KEY_INDEX_SHIFT) |
			(SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_IV << SE_CRYPTO_KEYIV_PKT_KEYIV_SEL_SHIFT) |
			(iv_sel << SE_CRYPTO_KEYIV_PKT_IV_SEL_SHIFT) |
			(word_sel << SE_CRYPTO_KEYIV_PKT_IV_WORD_SHIFT)
			);
		break;

	default:
		CCC_ERROR_MESSAGE("invalid aes slot id selected: %u\n", id_sel);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*se_keyiv_pkt = keyiv_pkt;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_set_op_mode_encrypt_crypto_config(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	uint32_t *se_config_reg_p)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;
	uint32_t counter_increment = 1U;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == se_config_reg_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Modify the CTR mode increment if allowed by counter mode.
	 * Used by AES counter variants in CTR_CNTN field.
	 *
	 * XXX * Note: Check that this works
	 *       correctly (see HW BUG 753854
	 *       => counter endianess value)
	 *       (May need to set SE_SPARE_0
	 *       bit 0 if increment endianess
	 *       is wrong)
	 */
#if HAVE_AES_CTR
	if ((TE_ALG_AES_CTR == algorithm) &&
	    (econtext->ctr.increment != 0U)) {
		counter_increment = econtext->ctr.increment;
	}
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
		/* CMAC-AES: enable HASH mode, otherwise HASH_ENB(DISABLE) (zero) by default.
		 */
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   HASH_ENB, ENABLE, se_config_reg);

		/* CMAC configureed otherwise like AES-CBC mode.
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
#endif /* HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM */

	default:
		/* [COVERITY] It is true that this is lwrrently deadcode, but if
		 * new algorithms are added to the enum then this would catch
		 * those before support is added.
		 */
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	/* common configs */
#if defined(SE0_AES0_CRYPTO_CONFIG_0_MEMIF_AHB)
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, MEMIF,
					   AHB, se_config_reg);
#endif

	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   CORE_SEL, ENCRYPT,
					   se_config_reg);

	*se_config_reg_p = se_config_reg;

fail:
	LTRACEF("exit: %d\n", ret);
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
#if defined(SE0_AES0_CRYPTO_CONFIG_0_MEMIF_AHB)
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, MEMIF,
					   AHB, se_config_reg);
#endif

	*se_config_reg_p = se_config_reg;

fail:
	LTRACEF("exit: %d\n", ret);
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
	 * XXX TODO
	 */
	if (SE0_AES0_CONFIG_0_DST_MEMORY != config_dst) {
		bool failed = true;

		/* AES-CMAC destination can be HASH_REG */
		if ((TE_ALG_CMAC_AES == algorithm) &&
		    (SE0_AES0_CONFIG_0_DST_HASH_REG == config_dst)) {
			failed = false;
		}

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
		if (SE0_AES0_CONFIG_0_DST_KEYTABLE == config_dst) {
			LTRACEF("AES destination is keytable\n");
			failed = false;
		}
#endif

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
	uint32_t keysize_pkt)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* HW may actually support this non-standard mode.
	 * Block it anyway for now.
	 */
	if (SE_MODE_PKT_AESMODE_KEY192 == keysize_pkt) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 LTRACEF("AES-XTS mode does not support 192 bit keys\n"));
	}

	if (BOOL_IS_TRUE(is_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   AES_ENC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
						   keysize_pkt, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   NOP, se_config_reg);
	} else {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   AES_DEC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DEC_MODE,
						   keysize_pkt, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   NOP, se_config_reg);
	}

	/* XTS only supports output to memory */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DST,
					   SE0_AES0_CONFIG_0_DST_MEMORY,
					   se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	se_config_reg = 0U;

#if defined(SE0_AES0_CRYPTO_CONFIG_0_MEMIF_AHB)
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, MEMIF,
					   AHB, se_config_reg);
#endif

	if (BOOL_IS_TRUE(is_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   CORE_SEL, ENCRYPT,
						   se_config_reg);
	}

	/* XTS is a two key tweakable AES mode, otherwise much
	 * like AES-CTR.  AES input is the value of the
	 * counter from SE_CRYPTO_LINEAR_CTR[0-3].
	 *
	 * Note: some field names are only defined with
	 * SE_AES1 prefix because the SE_AES0 engine does not
	 * support XTS mode.
	 *
	 * Because the CCC automatically adjusts register
	 * offsets for the used AES engine => any register
	 * reads and writes must anyway target SE_AES0 (only
	 * the register field bits need to be set with SE_AES1
	 * because they are not defined with the name SE_AES0
	 * prefix).
	 *
	 * Based on some texts AES-XTS was not designed to be
	 * "two key" cipher, it is made so by a mistake
	 * interpreting the paper defining it. But since all
	 * XTS implementations use two keys => this
	 * one also does. If you want the two keys to be
	 * identical (key and subkey) just use identical values.
	 */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES1, CRYPTO_CONFIG,
					   XOR_POS, BOTH,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES1, CRYPTO_CONFIG,
					   INPUT_SEL, MEMORY,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES1, CRYPTO_CONFIG,
					   VCTRAM_SEL, TWEAK,
					   se_config_reg);

	/* Set the keyslot used. */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES1, CRYPTO_CONFIG,
					   KEY_INDEX, econtext->se_ks.ks_slot,
					   se_config_reg);

#if HAVE_SE_SCC
	/* ENABLE SE SCC (disable SCC_DIS) */
	/* Jerry: AES CRYPTO CONFIG => SCC_DIS must be set to 0 to enable SCC (confirm please!) */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES1, CRYPTO_CONFIG,
					   SCC_DIS, DISABLE,
					   se_config_reg);
#else
	/* DISABLE SE SCC (enable SCC_DIS) */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES1, CRYPTO_CONFIG,
					   SCC_DIS, ENABLE,
					   se_config_reg);
#endif /* HAVE_SE_SCC */

	/* Fix as per Bug 200522735, recommended by HW */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   KEYSCH_BYPASS, ENABLE,
					   se_config_reg);

	/* Note: set reg must use AES0 offsets because they get mapped to selected AES1
	 * engine in the reg writer
	 */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

	/* From T194 IAS (page 158):
	 * HW TWEAK is derived once per START operation at the beginning of task.
	 * The implication is that SW must schedule one task per sector!
	 */
	ret = se_write_linear_counter(econtext->engine, &econtext->xts.tweak[0]);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_AES_XTS */

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF

status_t aes_keytable_dst_reg_setup(const struct se_engine_aes_context *econtext,
				    bool upper_quad)
{
	status_t ret = NO_ERROR;
	uint32_t ktable_reg = 0U;

	/* First QUAD bytes (keys_0_3 words), first 4 words */
	uint32_t word_quad = (BOOL_IS_TRUE(upper_quad) ? 1U : 0U);

	LTRACEF("entry\n");

	LTRACEF("Operation keytable target: %u, quad %u\n",
		econtext->se_ks.ks_target_keyslot,
		word_quad);

	/* Set the target keyslot of derivation/unwrap */
	ktable_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_DST,
					KEY_INDEX, econtext->se_ks.ks_target_keyslot,
					ktable_reg);

	ktable_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_DST,
					WORD_QUAD, word_quad,
					ktable_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_KEYTABLE_DST_0, ktable_reg);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_dst_keytable_setup(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	bool upper_quad = false;
	uint32_t dst_key_size =
		(econtext->aes_flags &
		 (AES_FLAGS_DERIVE_KEY_128 | AES_FLAGS_DERIVE_KEY_192 | AES_FLAGS_DERIVE_KEY_256));

	LTRACEF("entry\n");

	if ((econtext->aes_flags & AES_FLAGS_DERIVE_KEY_HIGH) != 0U) {
		switch (dst_key_size) {
		case AES_FLAGS_DERIVE_KEY_192:
		case AES_FLAGS_DERIVE_KEY_256:
			LTRACEF("AES target upper quad\n");
			upper_quad = true; /* Second QUAD (keys_4_7 words), upper 4 words */
			break;
		default:
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			LTRACEF("AES target upper quad with short key\n");
			break;
		}
		CCC_ERROR_CHECK(ret);
	}

	ret = aes_keytable_dst_reg_setup(econtext, upper_quad);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF */

static status_t engine_aes_op_mode_normal(const struct se_engine_aes_context *econtext,
					  te_crypto_algo_t algorithm,
					  bool do_encrypt,
					  uint32_t keysize_pkt,
					  bool use_orig_iv,
					  uint32_t config_dst)
{
	status_t ret = NO_ERROR;
	uint32_t se_config_reg = 0U;

	LTRACEF("entry\n");

	/* Set SE0_AES0_CONFIG_0 register fields */

	if (BOOL_IS_TRUE(do_encrypt)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   AES_ENC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, ENC_MODE,
						   keysize_pkt, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   NOP, se_config_reg);
	} else {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG,
						   AES_DEC, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DEC_MODE,
						   keysize_pkt, se_config_reg);
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG,
						   NOP, se_config_reg);
	}

	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CONFIG, DST, config_dst,
					   se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CONFIG_0, se_config_reg);

	/* Set SE0_AES0_CRYPTO_CONFIG_0 register fields */

	se_config_reg = 0U;
	if (BOOL_IS_TRUE(do_encrypt)) {
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

	if (BOOL_IS_TRUE(use_orig_iv)) {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   IV_SELECT, ORIGINAL,
						   se_config_reg);
	} else {
		se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
						   IV_SELECT, UPDATED,
						   se_config_reg);
	}

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

	/* Fix as per Bug 200522735, recommended by HW */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CRYPTO_CONFIG,
					   KEYSCH_BYPASS, ENABLE,
					   se_config_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_CONFIG_0, se_config_reg);

#if HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
	if (SE0_AES0_CONFIG_0_DST_KEYTABLE == config_dst) {
		ret = aes_dst_keytable_setup(econtext);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_SE_UNWRAP || HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF */

	ret = engine_aes_op_mode_write_linear_counter(econtext, algorithm);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_get_keysize_pkt(const struct se_engine_aes_context *econtext,
				    uint32_t *keysize_pkt_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (econtext->se_ks.ks_bitsize) {
	case 128U:
		*keysize_pkt_p = SE_MODE_PKT_AESMODE_KEY128;
		break;
	case 192U:
#if HAVE_AES_KEY192 == 0
		/* 192 bit AES keys are not supported by configuration */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#else
		*keysize_pkt_p = SE_MODE_PKT_AESMODE_KEY192;
#endif
		break;
	case 256U:
		*keysize_pkt_p = SE_MODE_PKT_AESMODE_KEY256;
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid AES keysize: %u\n",
				  econtext->se_ks.ks_bitsize);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Setup the operating mode registers for AES (modes)
 * encryption/decryption and CMAC Hash
 */
status_t setup_aes_op_mode_locked(
	const struct se_engine_aes_context *econtext, te_crypto_algo_t algorithm,
	bool is_encrypt, bool use_orig_iv, uint32_t config_dst)
{
	status_t ret = NO_ERROR;
	uint32_t keysize_pkt = 0U;

	LTRACEF("entry: algo 0x%x, is_encrypt=%u, orig_iv=%u\n",
		algorithm, is_encrypt, use_orig_iv);

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_aes_set_op_mode_check_args(econtext,
						algorithm,
						config_dst);
	CCC_ERROR_CHECK(ret);

	ret = aes_get_keysize_pkt(econtext, &keysize_pkt);
	CCC_ERROR_CHECK(ret);

	switch (algorithm) {
	case TE_ALG_AES_CTS:
		/* In our case CTS == NIST CBC-CS2. Could implement CS1 and CS3 as well,
		 * if required for some reason?
		 *
		 * Caller needs to deal with max last two data blocks => special IV handling,
		 * otherwise this is identical to CBC_NOPAD mode.
		 */
		CCC_ERROR_MESSAGE("XXXX AES-CTS mode not yet implemented: 0x%x\n", algorithm);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;

#if HAVE_CMAC_AES
	case TE_ALG_CMAC_AES: /* MAC */
		if (!BOOL_IS_TRUE(is_encrypt)) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
		ret = engine_aes_op_mode_normal(econtext,
						algorithm,
						CTRUE,
						keysize_pkt,
						use_orig_iv,
						config_dst);
		break;
#endif /* HAVE_CMAC_AES */

#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM || HAVE_AES_OFB
#if HAVE_AES_GCM
	case TE_ALG_AES_GCM: /* Like AES-CTR */
#endif
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
		 * for CMAC, CTR and OFB.
		 *
		 * These modes always use engine in ENCRYPT mode
		 */
		ret = engine_aes_op_mode_normal(econtext,
						algorithm,
						CTRUE,
						keysize_pkt,
						use_orig_iv,
						config_dst);
		break;
#endif /* HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM || HAVE_AES_OFB */

#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
#endif
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
		ret = engine_aes_op_mode_normal(econtext,
						algorithm,
						is_encrypt,
						keysize_pkt,
						use_orig_iv,
						config_dst);
		break;

#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
		ret = engine_aes_op_mode_xts(econtext, is_encrypt, keysize_pkt);
		if (NO_ERROR != ret) {
			break;
		}
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

/*
 * @brief Read an AES IV from AES key slot in the SE.
 *
 * @param keyslot SE key slot number.
 * @param id_sel Specify if this entity is the original IV or updated IV.
 *				Use id_sel to specify the type.
 *				*_ORIGINAL_IV for original IV,
 *				*_UPDATED_IV for updated IV.
 * @param keydata Pointer to key data. Must be valid memory location.
 *
 * @return NO_ERROR in case of no error.
 *	returns specific error in case of any error.
 */
status_t aes_read_iv_locked(
	const engine_t *engine,
	uint32_t keyslot,
	se_aes_keyslot_id_sel_t id_sel, uint32_t *keydata_param)
{
	status_t ret = NO_ERROR;
	uint32_t se_keytable_addr = 0U;
	uint32_t word_inx = 0U;
	uint32_t *keydata = keydata_param;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == keydata)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	DEBUG_ASSERT_HW_MUTEX;

	switch (id_sel) {
	case SE_AES_KSLOT_SEL_ORIGINAL_IV:
		break;
	case SE_AES_KSLOT_SEL_UPDATED_IV:
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("type id_sel=%u, slot %u\n", id_sel, keyslot);

	/**
	 * SE_CRYPTO_KEYTABLE_ADDR has to be written with the
	 * keyslot configuration for the particular KEY_WORD/IV_WORD
	 * you are reading from. The data to be read from the keyslot
	 * IV_WORD is then read from SE_CRYPTO_KEYTABLE_DATA.
	 */
	for (word_inx = 0U; word_inx < 4U; word_inx++) {
		ret = se_create_keyiv_pkt(keyslot, id_sel, word_inx, &se_keytable_addr);
		if (NO_ERROR != ret) {
			break;
		}

		tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);
		*keydata = tegra_engine_get_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_DATA_0);
		keydata++;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_set_aes_keytable_words(const engine_t *engine,
					  uint32_t keyslot,
					  se_aes_keyslot_id_sel_t id_sel,
					  uint32_t limit_index,
					  const uint8_t *kdata)
{
	status_t ret = NO_ERROR;
	uint32_t word_inx = 0U;
	uint32_t se_keytable_addr = 0U;
	const uint8_t *keydata = kdata;
	uint32_t kd = 0U;

	/**
	 * SE_CRYPTO_KEYTABLE_ADDR has to be written with the
	 * keyslot configuration for the particular KEY_WORD/IV_WORD
	 * you are writing to. The data to be written to the keyslot
	 * KEY_WORD/IV_WORD is then written to SE_CRYPTO_KEYTABLE_DATA.
	 *
	 * This is either IV or a key, depending on ID_SEL but they
	 * are written the same way.
	 */
	for (; word_inx < limit_index; word_inx++) {
		ret = se_create_keyiv_pkt(keyslot, id_sel, word_inx, &se_keytable_addr);
		if (NO_ERROR != ret) {
			break;
		}

		tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_ADDR_0, se_keytable_addr);

		if (NULL != keydata) {
			kd = se_util_buf_to_u32(&keydata[word_count_to_bytes(word_inx)],
						CFALSE);
		}
		tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_DATA_0, kd);
	}
	CCC_ERROR_CHECK(ret);

fail:
	return ret;
}

status_t aes_key_wordsize(uint32_t keysize_bits, uint32_t *key_wordsize_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

#if HAVE_AES_KEY192 == 0
	if (192U == keysize_bits) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}
#endif
	if (!IS_VALID_AES_KEYSIZE_BITS(keysize_bits)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != key_wordsize_p) {
		*key_wordsize_p = keysize_bits / BITS_IN_WORD;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t aes_write_key_iv_locked(
	const struct engine_s *engine,
	uint32_t keyslot, uint32_t keysize_bits_param,
	se_aes_keyslot_id_sel_t id_sel, const uint8_t *keydata)
{
	status_t ret = NO_ERROR;
	uint32_t keysize_bits = keysize_bits_param;
	uint32_t num_words = 4U; /* Default to 128 bits, 4 words */

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	AES_ERR_CHECK(engine, "AES key/iv write => START");

	switch (id_sel) {
	case SE_AES_KSLOT_SEL_KEY:

		/* Always clear the whole keyslot key */
		if (NULL == keydata) {
			keysize_bits = 256U;
		}

		ret = aes_key_wordsize(keysize_bits, &num_words);
		break;

	case SE_AES_KSLOT_SEL_ORIGINAL_IV:
	case SE_AES_KSLOT_SEL_UPDATED_IV:
		break;

#if HAVE_AES_XTS
	case SE_AES_KSLOT_SEL_SUBKEY:
		ret = aes_key_wordsize(keysize_bits, &num_words);
		break;
#endif /* HAVE_AES_XTS */

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("type id_sel=%u, slot %u [start]\n", id_sel, keyslot);

	ret = se_set_aes_keytable_words(engine, keyslot, id_sel, num_words, keydata);
	CCC_ERROR_CHECK(ret);

	LTRACEF("type id_sel=%u, slot %u [done]\n", id_sel, keyslot);

	AES_ERR_CHECK(engine, "AES key/iv write => END");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

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
 * excluded from the SE0_AES1_CRYPTO_LAST_BLOCK_0 setup.
 * [ AES-XTS is only supported with the AES1 engine ]
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

	if (0U != unaligned_bytes) {
		/* In case XTS/GCM has non-block aligned
		 * bytes => need to handle one partial
		 * AES block more
		 *
		 * last_block already is ok the HW used value
		 * is num_blocks - 1 after this increment.
		 */
		CCC_SAFE_ADD_U32(num_blocks, num_blocks, 1U);
	} else {
		if (0U == num_blocks) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_LEN,
						 LTRACEF("AES 0x%x zero block count\n",
							 econtext->algorithm));
		}

		/* LAST_BLOCK = number_of_blocks - 1 in general case */
		last_block--;
	}

	(void)residual_bits;

	switch (econtext->algorithm) {
#if HAVE_AES_XTS
	case TE_ALG_AES_XTS:
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

		/* HW uses cipher text stealing to manage the last two AES-XTS blocks
		 * (in case the length is not aligned). If it is aligned, no swap.
		 *
		 * Much like in NIST-CBC-CS2 mode for AES-CTS.
		 */
		break;
#endif /* HAVE_AES_XTS */

#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
#if HAVE_AES_CTS
	case TE_ALG_AES_CTS: /* From engine point of view CTS identical to CBC */
#endif
#if HAVE_AES_CCM
	case TE_ALG_AES_CCM:
#endif
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
#endif
	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_PADDING
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
#endif
#if HAVE_AES_GCM
	case TE_ALG_AES_GCM:
#endif
		if (unaligned_bytes != 0U) {
			LTRACEF("AES algo 0x%x last block not size aligned for HW\n",
				econtext->algorithm);
			ret = SE_ERROR(ERR_BAD_STATE);
		}
		break;

	default:
		/* All AES modes need to be handled above for last block settings.
		 * when supported.
		 */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

#if HAVE_AES_XTS
	if ((SE_AES_BLOCK_LENGTH * 8U) != residual_bits) {
		/* KW/KUW/KWUW/GCM/GMAC/XTS only supported on AES1 engine => use
		 * SE0_AES1 names here.
		 */
		last_block = LW_FLD_SET_DRF_NUM(SE0_AES1,
						CRYPTO_LAST_BLOCK,
						RESIDUAL_BITS,
						residual_bits,
						last_block);

		LTRACEF("AES mode unaligned last block contains %u bytes (%u bits)\n",
			unaligned_bytes, residual_bits);
	}
#endif /* HAVE_AES_XTS */

	tegra_engine_set_reg(econtext->engine,
			     SE0_AES0_CRYPTO_LAST_BLOCK_0, last_block);
	*num_blocks_p = num_blocks;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_encrypt_decrypt_arg_check(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	bool dst_keytable = false;

	LTRACEF("entry\n");

	if ((NULL == ac) || (NULL == ac->econtext) ||
	    (NULL ==  ac->input.src) ||
	    (0U == ac->input.input_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
	dst_keytable = ((ac->econtext->aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U);
#endif

	if (!BOOL_IS_TRUE(dst_keytable)) {
		if (NULL == ac->input.dst) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_encrypt_decrypt_algorithm_check(async_aes_ctx_t *ac,
						    uint32_t *config_dst_p)
{
	status_t ret = NO_ERROR;
	uint32_t config_dst = SE0_AES0_CONFIG_0_DST_MEMORY;

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

		if ((ac->econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U) {
			/* Tell HW the AES counter is in BIG ENDIAN byte order */
			ac->spare_reg_0 = tegra_engine_get_reg(ac->econtext->engine, SE0_AES0_SPARE_0);
			tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_SPARE_0, (ac->spare_reg_0 | 0x1U));
		}
#if HAVE_SE_AES_KDF
		if ((ac->econtext->aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
			config_dst = SE0_AES0_CONFIG_0_DST_KEYTABLE;
			LTRACEF("XXX AES output to keyslot\n");
		}
#endif
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
#if HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
		if ((ac->econtext->aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
			config_dst = SE0_AES0_CONFIG_0_DST_KEYTABLE;
			LTRACEF("XXX AES output to keyslot\n");
		}
#endif
		break;

	case TE_ALG_AES_CTS:
		ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*config_dst_p = config_dst;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_encrypt_decrypt_handle_block(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t config_dst = SE0_AES0_CONFIG_0_DST_MEMORY;

	LTRACEF("entry\n");

	/* Padding increasing output size to next block boundary
	 */
	if (BOOL_IS_TRUE(ac->econtext->is_encrypt) &&
	    TE_AES_MODE_PADDING(ac->econtext->algorithm)) {
		uint32_t remain = ac->input.input_size % SE_AES_BLOCK_LENGTH;
		uint32_t num_bytes = 0U;

		CCC_SAFE_SUB_U32(num_bytes, SE_AES_BLOCK_LENGTH, remain);
		CCC_SAFE_ADD_U32(ac->flush_dst_size, ac->flush_dst_size,
				 num_bytes);
	}
	/* Cert-C boundary check */
	if (ac->flush_dst_size > MAX_AES_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}
	ac->flush_dst_size = DMA_ALIGN_SIZE(ac->flush_dst_size);

	ret = aes_encrypt_decrypt_algorithm_check(ac, &config_dst);
	CCC_ERROR_CHECK(ret);

	/* Setup SE engine parameters for AES encrypt operation. */
	ret = setup_aes_op_mode_locked(
		ac->econtext, ac->econtext->algorithm,
		ac->econtext->is_encrypt, ac->econtext->is_first,
		config_dst);
	CCC_ERROR_CHECK(ret);

	/* Callwlate the number of blocks written to the engine and write the
	 * SE engine SE_CRYPTO_LAST_BLOCK_0
	 */
	ret = engine_aes_set_block_count(ac->econtext, ac->input.input_size,
					 &ac->num_blocks);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_handle_cache_line(const async_aes_ctx_t *ac,
				      PHYS_ADDR_T paddr_src_addr,
				      PHYS_ADDR_T *paddr_dst_addr_p)
{
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr_dst_addr = 0U;

	LTRACEF("entry\n");

	if (NULL == paddr_dst_addr_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (MAX_AES_ENGINE_CHUNK_SIZE < ac->input.input_size) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	if (NULL != ac->input.dst) {
		if (ac->input.src != ac->input.dst) {
			SE_CACHE_FLUSH((VIRT_ADDR_T)ac->input.src, DMA_ALIGN_SIZE(ac->input.input_size));

			paddr_dst_addr = DOMAIN_VADDR_TO_PADDR(ac->econtext->domain, ac->input.dst);
			SE_PHYS_RANGE_CHECK(paddr_dst_addr, ac->flush_dst_size);
		} else {
			paddr_dst_addr = paddr_src_addr;
		}

		/* Just flush for the DST here; it will get ilwalidated only once after the AES
		 * operation completes. This makes a difference in a write-through
		 * cache system.
		 */
		SE_CACHE_FLUSH((VIRT_ADDR_T)ac->input.dst, ac->flush_dst_size);
	}

	*paddr_dst_addr_p = paddr_dst_addr;
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
	uint32_t se_config_reg = 0U;
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr_src_addr = 0U;
	PHYS_ADDR_T paddr_dst_addr = 0U;

	LTRACEF("entry\n");

	ret = aes_encrypt_decrypt_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					 CCC_ERROR_MESSAGE("AES operation context already started\n"));
	}

	ac->update_ctr = false;

	ac->flush_dst_size = ac->input.input_size;

	ret = aes_encrypt_decrypt_handle_block(ac);
	CCC_ERROR_CHECK(ret);
#if SE_RD_DEBUG
	LTRACEF("[num_blocks %u (bytes %u) (src= %p, dst=%p), algo=0x%x]\n",
		ac->num_blocks, ac->input.input_size, ac->input.src,
		ac->input.dst, ac->econtext->algorithm);
#endif

#if HAVE_AES_COUNTER_OVERFLOW_CHECK
	ret = do_aes_counter_overflow_check(ac);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_AES_COUNTER_OVERFLOW_CHECK */

	paddr_src_addr = DOMAIN_VADDR_TO_PADDR(ac->econtext->domain, ac->input.src);
	SE_PHYS_RANGE_CHECK(paddr_src_addr, ac->input.input_size);

	// XXXX FIXME: the cache line handling with AES is still incomplete!
	//
	ret = aes_handle_cache_line(ac, paddr_src_addr, &paddr_dst_addr);
	CCC_ERROR_CHECK(ret);

	/* Program input message address. */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_src_addr));

	/* Program input message buffer size into SE0_AES0_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, SZ, ac->input.input_size,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_src_addr),
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_HI_0, se_config_reg);

	/* Program output address. */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_dst_addr));

	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, ac->input.input_size,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_dst_addr),
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

	AES_ERR_CHECK(ac->econtext->engine, "AES START");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	ac->gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(ac->econtext->engine, 0U);
	CCC_ERROR_CHECK(ret);

	ac->started = true;
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
#endif

	ac->started = false;

	ret = se_wait_engine_free(ac->econtext->engine);
	CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	if (0U != ac->gen_op_perf_usec) {
		show_se_gen_op_entry_perf("AES", get_elapsed_usec(ac->gen_op_perf_usec));
	}
#endif

	AES_ERR_CHECK(ac->econtext->engine, "AES DONE");

	if (NULL != ac->input.dst) {
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
	}

#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM
	/* In AES counter modes, update the counter after AES =>
	 * ctr.counter = num_blocks * ctr.increment
	 */
	if (BOOL_IS_TRUE(ac->update_ctr)) {
		ret = se_update_linear_counter(ac->econtext, ac->num_blocks);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
#if HAVE_AES_CCM || HAVE_AES_CTR || HAVE_AES_GCM
	if (BOOL_IS_TRUE(ac->update_ctr)) {
		/* Restore old content of the SPARE register, if saved in start */
		if ((ac->econtext->aes_flags & AES_FLAGS_CTR_MODE_BIG_ENDIAN) != 0U) {
			tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_SPARE_0, ac->spare_reg_0);
		}
	}
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_check_input_data(const async_aes_ctx_t *ac, bool dst_keytable)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Keytable src is checked later in the call chain
	 */
	if (!BOOL_IS_TRUE(dst_keytable)) {
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
	bool dst_keytable = false;

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

	if (TE_ALG_AES_XTS != ac->econtext->algorithm) {
		// XXXX (But AES-CBC-CS2 (AES-CTS) also
		// XXXX does not require this here,
		// XXXX process layer must manage
		// XXXX  the last block swaps!!!)
		if ((ac->input.input_size % SE_AES_BLOCK_LENGTH) != 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

#if HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
	dst_keytable = ((ac->econtext->aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U);
#endif

	ret = aes_check_input_data(ac, dst_keytable);
	CCC_ERROR_CHECK(ret);

	/* AES engine can only handle up to ((2^24 / 16) * 16) bytes
	 * in one call (Largest number of 16 byte blocks in (2^24-1)
	 * bytes).
	 */
	if (ac->input.input_size > MAX_AES_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
					 LTRACEF("AES input chunk size too big: 0x%x\n",
						 ac->input.input_size));
	}

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
 * For AES-XTS the subkey is catenated after the master AES pkey of se_ks.ks_bitsize bits.
 */
static status_t engine_aes_write_keyslot(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = aes_write_key_iv_locked(
		econtext->engine,
		econtext->se_ks.ks_slot, econtext->se_ks.ks_bitsize,
		SE_AES_KSLOT_SEL_KEY, econtext->se_ks.ks_keydata);
	CCC_ERROR_CHECK(ret);

#if HAVE_AES_XTS
	if (TE_ALG_AES_XTS == econtext->algorithm) {
		/* AES XTS mode subkey is catenated after the aes_key */
		ret = aes_write_key_iv_locked(
			econtext->engine,
			econtext->se_ks.ks_slot, econtext->se_ks.ks_bitsize,
			SE_AES_KSLOT_SEL_SUBKEY,
			&econtext->se_ks.ks_keydata[econtext->se_ks.ks_bitsize / 8U]);
		CCC_ERROR_CHECK(ret);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_write_iv(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(econtext->is_first)) {
		if ((econtext->aes_flags & AES_FLAGS_USE_KEYSLOT_IV) != 0U) {
			LTRACEF("Using pre-set IV value from keyslot %u\n",
				econtext->se_ks.ks_slot);
		} else {
			LOG_HEXBUF("Setting AES IV (initial)", &econtext->iv_stash[0], AES_IV_SIZE);

			ret = aes_write_key_iv_locked(
				econtext->engine,
				econtext->se_ks.ks_slot, 0U, SE_AES_KSLOT_SEL_ORIGINAL_IV,
				(const uint8_t *)&econtext->iv_stash[0]);
			CCC_ERROR_CHECK(ret);
		}
	} else {
		LOG_HEXBUF("Setting AES IV (update)", &econtext->iv_stash[0], 16U);
		ret = aes_write_key_iv_locked(
			econtext->engine,
			econtext->se_ks.ks_slot, 0U, SE_AES_KSLOT_SEL_UPDATED_IV,
			(const uint8_t *)&econtext->iv_stash[0]);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_async_start_set_key(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(ac->use_preset_key) ||
	    ((ac->econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U)) {
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

	/* If the loaded key must be left to keyslot after the operation.
	 * Unmodified in case the bit is not set!
	 */
	if ((ac->econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U) {
		ac->leave_key_to_keyslot = true;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_process_locked_async_start(async_aes_ctx_t *ac)
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

	ret = aes_async_start_set_key(ac);
	CCC_ERROR_CHECK(ret);

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		ret = engine_aes_write_iv(ac->econtext);
		CCC_ERROR_CHECK(ret);
	}

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

/* Clear keyslot when finalizing aes if slot modified and not preserved
 */
static status_t finalize_aes_async_finish(const async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	/* If key was committed and not flagged to leave in keyslot => clear slot
	 */
	if ((BOOL_IS_TRUE(ac->use_preset_key) || BOOL_IS_TRUE(ac->key_written)) &&
	    !BOOL_IS_TRUE(ac->leave_key_to_keyslot)) {
		ret = tegra_se_clear_aes_keyslot_locked(
			ac->econtext->engine, ac->econtext->se_ks.ks_slot, true);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to clear engine %u AES keyslot %u\n",
					ac->econtext->engine->e_id, ac->econtext->se_ks.ks_slot));
	}
fail:
	return ret;
}

static status_t aes_process_locked_async_finish(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* AC checked by this callee */
	ret = aes_encrypt_decrypt_locked_async_finish(ac);
	CCC_ERROR_CHECK(ret);

	ac->econtext->is_first = false;

	ac->input.output_size = ac->input.input_size;

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		// XXX Should this be copied out just before releasing lock, not here..
		// XXX  yes, but the code does not yet cope with this (same as setting
		// XXX  the key by caller in a locked econtext => consider doing this later)
		// XXXXX  TODO, FIXME!!! Well, yes, but if this is done => need new logic...
		//
		// XXX We do not need copy this value if we pass more data through engine
		// XXX before releasing the lock.
		//
		// The lock is now released after each call => not that optimal, but allows
		// others to use the engine in between...
		//
		ret = aes_read_iv_locked(
			ac->econtext->engine,
			ac->econtext->se_ks.ks_slot,
			SE_AES_KSLOT_SEL_UPDATED_IV,
			&ac->econtext->iv_stash[0]);
		CCC_ERROR_CHECK(ret);
	}

fail:
	{
		status_t finret = finalize_aes_async_finish(ac);
		if (NO_ERROR == ret) {
			ret = finret;
			/* FALLTHROUGH */
		}
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF

static status_t aes_derive_kslot_quad(async_aes_ctx_t *ac, bool leave_key_for_this_op)
{
	status_t ret = NO_ERROR;
	bool leave_key_save = false;

	LTRACEF("entry\n");

	LTRACEF("Deriving %s quad of keyslot\n",
		((ac->econtext->aes_flags & AES_FLAGS_DERIVE_KEY_HIGH) == 0U) ? "lower" : "upper");

	ret = aes_process_locked_async_start(ac);
	CCC_ERROR_CHECK(ret);

	/* The above call handles the persistent leave-key flag.
	 */
	leave_key_save = ac->leave_key_to_keyslot;

	/* Override in case key needs to be preserved for multi step
	 * operation or erased in last step of such operation.
	 */
	ac->leave_key_to_keyslot = (leave_key_for_this_op || ac->leave_key_to_keyslot);

	ret = aes_process_locked_async_finish(ac);
	CCC_ERROR_CHECK(ret);

	ac->leave_key_to_keyslot = leave_key_save;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_AES_KDF
static status_t aes_cipher_derive_kslot(async_aes_ctx_t *ac, uint32_t key_bsize)
{
	status_t ret = NO_ERROR;
	bool leave_key_for_this_op = false;

	/* For AES derivations the input size must match
	 * key_bsize.
	 */
	if (key_bsize != ac->input.input_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* 128 bit keys can be derived with normal operations.
	 * HW does not support writing more than 128 bit to keyslot
	 * with one operation, so need to split longer writes
	 * to two operations, each writing it's own quad of the keyslot.
	 */
	if (key_bsize > SE_AES_BLOCK_LENGTH) {
		ac->input.input_size = SE_AES_BLOCK_LENGTH;
		ac->econtext->aes_flags &= ~AES_FLAGS_DERIVE_KEY_HIGH;
		leave_key_for_this_op = true;
	}

	ret = aes_derive_kslot_quad(ac, leave_key_for_this_op);
	CCC_ERROR_CHECK(ret);

	if (key_bsize > SE_AES_BLOCK_LENGTH) {
		/* Key was set in the previous async start
		 * and left to the keyslot (mutex locked).
		 */
		ac->use_preset_key = true;

		ac->econtext->aes_flags |= AES_FLAGS_DERIVE_KEY_HIGH;
		ac->input.input_size = SE_AES_BLOCK_LENGTH;
		ac->input.src = &ac->input.src[SE_AES_BLOCK_LENGTH];

		ret = aes_derive_kslot_quad(ac, false);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES_KDF */

static status_t aes_process_kslot_dst_op(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t key_bsize = 0U;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	/* Check source data length when targetting to keyslot
	 *
	 * Keytable does not support padding operations.
	 *
	 * Here 192 bit keys can be derived with either 24 or
	 * 32 bytes of derivation data.
	 */
	switch (ac->econtext->aes_flags & (AES_FLAGS_DERIVE_KEY_128 |
					   AES_FLAGS_DERIVE_KEY_192 |
					   AES_FLAGS_DERIVE_KEY_256)) {
	case AES_FLAGS_DERIVE_KEY_128:
		key_bsize = 16U;
		break;
	case AES_FLAGS_DERIVE_KEY_192:
		if (32U == ac->input.input_size) {
			key_bsize = 32U;
		} else {
			key_bsize = 24U;
		}
		break;
	case AES_FLAGS_DERIVE_KEY_256:
		key_bsize = 32U;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret,
			LTRACEF("Invalid data length for selected kdf\n"));

	/* CMAC-AES derivation 128 bit slices of the key;
	 * to the quad specified by the client.
	 * CCC does not check consistency of sequential
	 * quad derivations for > 128 bit keys: client must do that
	 * (CCC keeps no state for cross-call consistency checks
	 *  and the requirement was that two SEPARATE CMAC-AES
	 *  calls will be used for 256 bit key derivation).
	 *
	 * ----
	 *
	 * For AES cipher derivations the data length decides
	 * how the key is derived, both quads are generated
	 * by the derive call below.
	 */
	(void)key_bsize;

	/* Note that this code is not compiled when both
	 * HAVE_CMAC_DST_KEYTABLE and HAVE_SE_AES_KDF
	 * are 0. In such case the loop below would never terminate
	 * if run.
	 */
	CCC_LOOP_BEGIN {
#if HAVE_CMAC_DST_KEYTABLE
		/* CMAC-AES keyslot derive uses AES-ECB-NOPAD algorithm, but sets
		 * this context flag to indicate that original derivation algorithm
		 * was CMAC-AES.
		 */
		if ((ac->econtext->aes_flags & AES_FLAGS_CMAC_DERIVE) != 0U) {
			LTRACEF("CMAC-AES deriving keyslot\n");

			/* The CMAC-AES ops are individual operations
			 * which need to take care of their own key
			 * setup.
			 *
			 * The first CMAC-AES can leav the key into
			 *  keyslot with options, but it is not up to
			 *  CCC to do so (since these are individual
			 *  CMAC-AES calls, as defined).
			 *
			 * So unless caller sets the leave-key flag,
			 * the CMAC-AES keyslot key will get erased
			 * before deriving the second half of the 256
			 * bit key.
			 */
			ret = aes_derive_kslot_quad(ac, false);
			CCC_LOOP_STOP;
		}
#endif /* HAVE_CMAC_DST_KEYTABLE */

#if HAVE_SE_AES_KDF
		LTRACEF("AES cipher deriving keyslot\n");
		ret = aes_cipher_derive_kslot(ac, key_bsize);
		CCC_LOOP_STOP;
#endif
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF */

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

	CCC_OBJECT_ASSIGN(ac.input, *input_params);
	ac.econtext = econtext;

	CCC_LOOP_BEGIN {
#if HAVE_CMAC_DST_KEYTABLE || HAVE_SE_AES_KDF
		if ((econtext->aes_flags & AES_FLAGS_DST_KEYSLOT) != 0U) {
			ret = aes_process_kslot_dst_op(&ac);
			CCC_LOOP_STOP;
		}
#endif
		ret = aes_process_locked_async_start(&ac);
		CCC_ERROR_END_LOOP(ret);

		ret = aes_process_locked_async_finish(&ac);
		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	se_util_mem_set((uint8_t *)&ac, 0U, sizeof_u32(ac));

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

#if HAVE_CLOSE_FUNCTIONS
void engine_aes_close_locked(void)
{
	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	return;
}
#endif

/* Set clear_ivs false only when there is no security concern with leaking
 * IV values.
 *
 * Note that AES_XTS mode sets the subkey to IV fields => you should
 * always clear the IVs when using XTS! (that is one such security
 * concern!) This call does not know the algorithm/mode => caller must
 * set clear_ivs true in that case.
 */
status_t tegra_se_clear_aes_keyslot_locked(const struct engine_s *engine, uint32_t keyslot, bool clear_ivs)
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
	ret = aes_write_key_iv_locked(engine, keyslot, 0U, SE_AES_KSLOT_SEL_KEY, NULL);
	CCC_ERROR_CHECK(ret);

	/* XXX Is it necessary ever to clear the IV's?
	 * XXX Would save some time not to do it.
	 * TODO: consider
	 */

	if (BOOL_IS_TRUE(clear_ivs)) {
		/* Clear OIV */
		ret = aes_write_key_iv_locked(engine, keyslot, 0U,
					      SE_AES_KSLOT_SEL_ORIGINAL_IV,
					      NULL);
		CCC_ERROR_CHECK(ret);

		/* Clear UIV */
		ret = aes_write_key_iv_locked(engine, keyslot, 0U,
					      SE_AES_KSLOT_SEL_UPDATED_IV,
					      NULL);
		CCC_ERROR_CHECK(ret);
	}

	AES_ERR_CHECK(engine, "after kslot clear");
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_AES */
