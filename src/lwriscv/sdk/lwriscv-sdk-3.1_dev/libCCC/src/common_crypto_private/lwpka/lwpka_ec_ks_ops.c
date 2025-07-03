/*
 * Copyright (c) 2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* LWPKA 1.1 EC keystore related operations
 */
#include <crypto_system_config.h>

#if HAVE_LWPKA11_SUPPORT

#include <dev_se_lwpka.h>
#include <dev_se_lwpka11.h>

#include <tegra_cdev.h>
#include <tegra_se.h>
#include <lwpka/tegra_lwpka_gen.h>
#include <lwpka/lwpka_ec_ks_ops.h>
#include <lwpka/lwpka_ec_ks_kac.h>
#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#define CCC_EC_KS_DUMP_MANIFEST
#else
#define LOCAL_TRACE 0
#endif

/* CORE_OPERATION_MASK will get handled for EC KS if the
 * LW_SE_LWPKA_CORE_OPERATION_MASK_0 register is defined.
 *
 * Disable in config file if required.
 */
#ifndef HAVE_LWPKA11_CORE_OPERATION_MASK
#define HAVE_LWPKA11_CORE_OPERATION_MASK defined(LW_SE_LWPKA_CORE_OPERATION_MASK_0)
#endif /* HAVE_LWPKA11_CORE_OPERATION_MASK */

#ifdef CCC_EC_KS_DUMP_MANIFEST
static void rd_ec_ks_dump_keymanifest(const char *info, uint32_t val)
{
	uint32_t field = 0U;
	const char *msg = info;

	if (NULL == msg) {
		msg = "";
	}

	/* HW and SW manifest regs: identical format */
	LOG_ALWAYS("EC keymanifest register: 0x%x (%s)\n", val, msg);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, TYPE, val);
	LOG_ALWAYS(" type: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, DP_IDX, val);
	LOG_ALWAYS(" dp_idx: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, SW, val);
	LOG_ALWAYS(" sw: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, SIZE, val);
	LOG_ALWAYS(" size: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, EX, val);
	LOG_ALWAYS(" ex: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, PURPOSE, val);
	LOG_ALWAYS(" purpose: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, USER, val);
	LOG_ALWAYS(" user: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, ORIGIN, val);
	LOG_ALWAYS(" origin: 0x%x\n", field);
}

static void rd_ec_ks_dump_keystore_ctrl(const char *info, uint32_t val)
{
	uint32_t field = 0U;
	const char *msg = info;

	if (NULL == msg) {
		msg = "";
	}

	/* HW and SW manifest regs: identical format */
	LOG_ALWAYS("EC keystore ctrl register: 0x%x (%s)\n", val, msg);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, OWNER, val);
	LOG_ALWAYS(" owner: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, LOCK, val);
	LOG_ALWAYS(" lock: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, VLD_PUB, val);
	LOG_ALWAYS(" vld_pub: 0x%x\n", field);

	field = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, VLD_PRIV, val);
	LOG_ALWAYS(" vld_priv: 0x%x\n", field);
}
#endif /* CCC_EC_KS_DUMP_MANIFEST */

static inline bool lwpka_ec_ks_pri_is_valid(uint32_t eks_ctrl_val)
{
	return (LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, VLD_PRIV, eks_ctrl_val) ==
		LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0_VLD_PRIV_USABLE);
}

static status_t lwpka_ec_ks_map_primitive(enum lwpka_engine_ops eop,
					  enum lwpka_primitive_e *primitive_p,
					  uint32_t *scena_p)
{
	status_t ret = NO_ERROR;
	enum lwpka_primitive_e primitive = LWPKA_PRIMITIVE_ILWALID;

	LTRACEF("entry\n");

	/* IAS => SCC setting does not matter for EC KS ops */
	*scena_p = CCC_SC_DISABLE;
	*primitive_p = primitive;

	switch (eop) {
#if HAVE_LWPKA11_ECKS_KEYOPS
	case LWPKA_OP_EC_KS_KEYINS_PRI:
		primitive = LWPKA_PRIMITIVE_ECC_KEYINS_PRI;
		break;
	case LWPKA_OP_EC_KS_KEYGEN_PRI:
		primitive = LWPKA_PRIMITIVE_ECC_KEYGEN_PRI;
		break;
	case LWPKA_OP_EC_KS_KEYGEN_PUB:
		primitive = LWPKA_PRIMITIVE_ECC_KEYGEN_PUB;
		break;
	case LWPKA_OP_EC_KS_KEYDISP:
		primitive = LWPKA_PRIMITIVE_ECC_KEYDISP;
		break;
#endif /* HAVE_LWPKA11_ECKS_KEYOPS */

	case LWPKA_OP_EC_KS_ECDSA_SIGN:
		primitive = LWPKA_PRIMITIVE_ECDSA_SIGN;
		break;
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		LTRACEF("EC KS does not support ec operation eop %u\n",
			eop);
		break;
	}
	CCC_ERROR_CHECK(ret);
	*primitive_p = primitive;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA11_CORE_OPERATION_MASK
static inline status_t lwpka_ec_ks_enable_operation_mask(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	// XXX Change 1U to something defined later
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_OPERATION_MASK_0, 1U);

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_OPERATION_MASK_0);
	if ((val & 1U) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_ALLOWED,
				     LTRACEF("EC KS operation not allowed for subsystem\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static inline void lwpka_ec_ks_disable_operation_mask(const engine_t *engine)
{
	LTRACEF("entry\n");

	// XXX Change 1U to something defined later
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_OPERATION_MASK_0, 0U);

	LTRACEF("exit\n");
}
#endif /* HAVE_LWPKA11_CORE_OPERATION_MASK */

static status_t lwpka_ec_ks_run_engine(const engine_t *engine,
				       enum lwpka_engine_ops eop,
				       enum pka1_op_mode op_mode)
{
	status_t ret = NO_ERROR;
	enum lwpka_primitive_e primitive = LWPKA_PRIMITIVE_ILWALID;
	uint32_t scena = CCC_SC_DISABLE;

	LTRACEF("entry\n");

#if HAVE_LWPKA11_CORE_OPERATION_MASK
	ret = lwpka_ec_ks_enable_operation_mask(engine);
	CCC_ERROR_CHECK(ret);
#endif

	ret = lwpka_ec_ks_map_primitive(eop, &primitive, &scena);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_go_start(engine, op_mode, primitive, scena);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_wait_op_done(engine);
	CCC_ERROR_CHECK(ret);
fail:
#if HAVE_LWPKA11_CORE_OPERATION_MASK
	lwpka_ec_ks_disable_operation_mask(engine);
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_ks_op_check_ectx_arg(
	const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(econtext->ec_lwrve->nbytes))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Unsupported EC lwrve byte size: %u\n",
					     econtext->ec_lwrve->nbytes));
	}

	if (!BOOL_IS_TRUE(CCC_EC_KS_LWRVE_SUPPORTED(econtext->ec_lwrve->id))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("EC KS does not support lwrve %u\n",
					     econtext->ec_lwrve->id));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA11_ECKS_KEYOPS

static inline bool lwpka_ec_ks_is_locked(uint32_t eks_ctrl_val)
{
	return (LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, LOCK, eks_ctrl_val) ==
		LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0_LOCK_LOCKED);
}

static inline bool lwpka_ec_ks_pub_is_valid(uint32_t eks_ctrl_val)
{
	return (LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, VLD_PUB, eks_ctrl_val) ==
		LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0_VLD_PUB_USABLE);
}

static status_t lwpka_ec_ks_get_hw_manifest_op_mode(const engine_t *engine,
						    enum pka1_op_mode *op_mode_p)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t key_size = 0U;

	LTRACEF("entry\n");

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keymanifest("get HW manifest op_mode", val);
#endif

	if (LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, TYPE, val) !=
	    LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0_TYPE_ECC) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LOG_ERROR("CORE_ECC_KEYMANIFEST_HW register key is not for EC: 0x%x\n",
					       val));
	}

	key_size = LW_DRF_VAL(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_HW, SIZE, val);

	switch (key_size) {
	case LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0_SIZE_KEY256:
		*op_mode_p = PKA1_OP_MODE_ECC256;
		break;
	case LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0_SIZE_KEY384:
		*op_mode_p = PKA1_OP_MODE_ECC384;
		break;
	case LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0_SIZE_KEY521:
		*op_mode_p = PKA1_OP_MODE_ECC521;
		break;
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		LOG_ERROR("CORE_ECC_KEYMANIFEST_HW size field value unsupported in 0x%x\n", val);
		break;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* All engines have same manifest fields, uses AES0 for all.
 */
static status_t lwpka_ec_ks_sw_manifest_encode(uint32_t *manifest_p, enum pka1_op_mode op_mode,
					       uint32_t ec_ks_sw, uint32_t ec_ks_user,
					       uint32_t ec_ks_ex)
{
	status_t ret = NO_ERROR;
	uint32_t m = 0U;
	uint32_t mf_size = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_SIZE_RESERVED;
	uint32_t mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_NONE;
	uint32_t mf_sw = ec_ks_sw;

	LTRACEF("entry\n");

	/* This HW does not allow to set the EX manifest bit for key export
	 * from keystore.
	 */
	if (0U != ec_ks_ex) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	if (NULL == manifest_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (((mf_sw & ~LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_SW_MASK)) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC KS SW does not fit in field: 0x%x\n", mf_sw));
	}

	switch (op_mode) {
	case PKA1_OP_MODE_ECC256:
		mf_size = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_SIZE_KEY256;
		break;
	case PKA1_OP_MODE_ECC384:
		mf_size = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_SIZE_KEY384;
		break;
	case PKA1_OP_MODE_ECC521:
		mf_size = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_SIZE_KEY521;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	switch (ec_ks_user) {
	case CCC_MANIFEST_EC_KS_USER_NONE:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_NONE;
		break;
	case CCC_MANIFEST_EC_KS_USER_PWR:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_PWR;
		break;
	case CCC_MANIFEST_EC_KS_USER_LWDEC:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_LWDEC;
		break;
	case CCC_MANIFEST_EC_KS_USER_SEC:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_SEC;
		break;
	case CCC_MANIFEST_EC_KS_USER_GSP:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_GSP;
		break;
	case CCC_MANIFEST_EC_KS_USER_PRIV:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_PRIV;
		break;
	case CCC_MANIFEST_EC_KS_USER_FSP:
		mf_user = LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0_USER_FSP;
		break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	m = LW_FLD_SET_DRF_NUM(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_SW, SW, mf_sw, m);

	m = LW_FLD_SET_DRF_NUM(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_SW, SIZE, mf_size, m);

	m = LW_FLD_SET_DRF_NUM(LW_SE, LWPKA_CORE_ECC_KEYMANIFEST_SW, USER, mf_user, m);

	/* Other fields are R/O, leave zero. */
	*manifest_p = m;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/**** EC keystore operations ****/

status_t lwpka_ec_ks_insert_key_locked(const engine_t *engine,
				       enum pka1_op_mode op_mode,
				       uint32_t keystore,
				       const uint8_t *keydata,
				       bool key_big_endian,
				       uint32_t ec_ks_user,
				       uint32_t ec_ks_ex,
				       uint32_t ec_ks_sw)
{
	status_t ret = NO_ERROR;
	uint32_t mfest = 0U;
	uint32_t hw_nwords = 0U;
	uint32_t val = 0U;
	uint32_t algo_nwords = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == keydata)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* There is only one keystore in LWPKA1.1, so it is implicit. */
	if (keystore > LWPKA_MAX_EC_KEYSTORES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid keystore id %u\n", keystore));
	}

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("before ECC_KEYINS_PRI", val);
#endif

	if (BOOL_IS_TRUE(lwpka_ec_ks_is_locked(val))) {
		/* XXX Could fail this now, we know it will not succeed.
		 *
		 * OTOH, need to check if HW detects this and fails the operation
		 */
		LOG_ERROR("EC keystore %u locked before ECC_KEYINS_PRI\n", keystore);
	}

	/* setup CORE_ECC_KEYMANIFEST_SW */
	ret = lwpka_ec_ks_sw_manifest_encode(&mfest, op_mode,
					     ec_ks_sw,
					     ec_ks_user,
					     ec_ks_ex);
	CCC_ERROR_CHECK(ret, LTRACEF("ECC_KEYINS_PRI manifest encoding failed\n"));
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keymanifest("KEYINS_PRI sw mfest", mfest);
#endif
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0, mfest);

	ret = lwpka_num_words(op_mode, &hw_nwords, &algo_nwords);
	CCC_ERROR_CHECK(ret);

	/* setup LOR_K0 */
	ret = lwpka_register_write(engine, op_mode, LOR_K0,
				   keydata, algo_nwords, key_big_endian,
				   hw_nwords);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_ks_run_engine(engine, LWPKA_OP_EC_KS_KEYINS_PRI, op_mode);
	CCC_ERROR_CHECK(ret, LTRACEF("KEYINS_PRI failed\n"));

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("after ECC_KEYINS_PRI", val);
#endif

#ifdef CCC_EC_KS_DUMP_MANIFEST
	mfest = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0);
	rd_ec_ks_dump_keymanifest("After KEYINS_PRI HW mfest", mfest);
#endif

	/* Check that the key is now set */
	if (! BOOL_IS_TRUE(lwpka_ec_ks_pri_is_valid(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC keystore %u key not valid after KEYINS_PRI\n", keystore));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_ec_ks_gen_key_locked(const engine_t *engine,
				    enum pka1_op_mode op_mode,
				    uint32_t keystore,
				    uint32_t ec_ks_user,
				    uint32_t ec_ks_ex,
				    uint32_t ec_ks_sw)
{
	status_t ret = NO_ERROR;
	uint32_t mfest = 0U;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	/* There is only one keystore in LWPKA1.1, so it is implicit. */
	if (keystore > LWPKA_MAX_EC_KEYSTORES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid keystore id %u\n", keystore));
	}

	if (BOOL_IS_TRUE(lwpka_ec_ks_is_locked(val))) {
		/* XXX Could fail this now, we know it will not succeed.
		 *
		 * OTOH, need to check if HW detects this and fails the operation
		 */
		LOG_ERROR("EC keystore %u locked before ECC_KEYGEN_PRI\n", keystore);
	}

	/* CORE_CONFIG: random source set to HW DRBG (only in debug mode it
	 * can be something else)
	 */
	val = LW_DRF_DEF(LW_SE, LWPKA_CORE_CONFIG, INPUT_SEL, DRBG);
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_CONFIG_0, val);

	/* setup CORE_ECC_KEYMANIFEST_SW */
	ret = lwpka_ec_ks_sw_manifest_encode(&mfest, op_mode,
					     ec_ks_sw,
					     ec_ks_user,
					     ec_ks_ex);
	CCC_ERROR_CHECK(ret, LTRACEF("ECC_KEYGEN_PRI manifest encoding failed\n"));
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keymanifest("Before KEYGEN_PRI", mfest);
#endif
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_SW_0, mfest);

	ret = lwpka_ec_ks_run_engine(engine, LWPKA_OP_EC_KS_KEYGEN_PRI, op_mode);
	CCC_ERROR_CHECK(ret, LTRACEF("KEYGEN_PRI failed\n"));

	/* Verify KEYGEN_PRI success */

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("after ECC_KEYGEN_PRI", val);
#endif

	/* Check that the key is now set */
	if (! BOOL_IS_TRUE(lwpka_ec_ks_pri_is_valid(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC keystore %u key not valid after KEYGEN_PRI\n",
					       keystore));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Read result of EC KS ECDSA sign operation from LOR_D0/D1
 */
static status_t lwpka_ec_ks_genpub_get_pubkey(const engine_t *engine,
					      enum pka1_op_mode op_mode,
					      struct te_ec_point_s *pubkey)
{
	status_t ret = NO_ERROR;
	uint32_t algo_nwords = 0U;

	if (NULL == pubkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwpka_num_words(op_mode, NULL, &algo_nwords);
	CCC_ERROR_CHECK(ret);

	/* Colwert HW little endian LOR_D0 to big endian in the pubkey */
	ret = lwpka_register_read(engine, op_mode, LOR_D0, &pubkey->x[0], algo_nwords, true);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_register_read(engine, op_mode, LOR_D1, &pubkey->y[0], algo_nwords, true);
	CCC_ERROR_CHECK(ret);

	pubkey->point_flags = CCC_EC_POINT_FLAG_NONE;

	LOG_HEXBUF("GENPUB(Qx)", &pubkey->x[0], algo_nwords*BYTES_IN_WORD);
	LOG_HEXBUF("GENPUB(Qy)", &pubkey->y[0], algo_nwords*BYTES_IN_WORD);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_ec_ks_gen_pub_locked(const engine_t *engine,
				    uint32_t keystore,
				    struct te_ec_point_s *pubkey)
{
	status_t ret = NO_ERROR;
	enum pka1_op_mode op_mode = PKA1_OP_MODE_ILWALID;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	/* There is only one keystore in LWPKA1.1, so it is implicit. */
	if (keystore > LWPKA_MAX_EC_KEYSTORES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid keystore id %u\n", keystore));
	}

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("before ECC_GENPUB", val);
#endif

	if (! BOOL_IS_TRUE(lwpka_ec_ks_pri_is_valid(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC keystore %u private key not valid\n", keystore));
	}

	if (BOOL_IS_TRUE(lwpka_ec_ks_pub_is_valid(val))) {
		LOG_INFO("EC keystore %u public key already valid before GENPUB\n", keystore);
	}

	if (BOOL_IS_TRUE(lwpka_ec_ks_is_locked(val))) {
		/* XXX Could fail this now, we know it will not succeed.
		 *
		 * OTOH, need to check if HW detects this and fails the operation
		 */
		LOG_ERROR("EC keystore %u locked before GENPUB\n", keystore);
	}

	ret = lwpka_ec_ks_get_hw_manifest_op_mode(engine, &op_mode);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_ks_run_engine(engine, LWPKA_OP_EC_KS_KEYGEN_PUB, op_mode);
	CCC_ERROR_CHECK(ret, LTRACEF("GENPUB failed\n"));

	/* Verify GENPUB success */

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("after GENPUB", val);
#endif

	if (! BOOL_IS_TRUE(lwpka_ec_ks_pub_is_valid(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("EC keystore %u pubkey not valid after GENPUB\n", keystore));
	}

	if (NULL != pubkey) {
		ret = lwpka_ec_ks_genpub_get_pubkey(engine, op_mode, pubkey);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_EC_KS_DISPOSE_SUPPORTED
status_t lwpka_ec_ks_dispose_locked(const engine_t *engine,
				    uint32_t keystore)
{
	status_t ret = NO_ERROR;
	/* Largest supported; ignored by HW op */
	enum pka1_op_mode op_mode = PKA1_OP_MODE_ECC521;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	/* There is only one keystore in LWPKA1.1, so it is implicit. */
	if (keystore > LWPKA_MAX_EC_KEYSTORES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid keystore id %u\n", keystore));
	}

#if SE_RD_DEBUG
	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("Before ECC_KEYDISP", val);
#endif

	if (! lwpka_ec_ks_pri_is_valid(val) &&
	    ! lwpka_ec_ks_pub_is_valid(val)) {
		LOG_ERROR("EC keystore %u is not valid before dispose\n", keystore);
		/* But do it anyway */
	}
#endif

	ret = lwpka_ec_ks_run_engine(engine, LWPKA_OP_EC_KS_KEYDISP, op_mode);
	CCC_ERROR_CHECK(ret, LTRACEF("KEYDISP failed\n"));

	/* Verify KEY DISPOSE success
	 */
	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("After ECC_KEYDISP", val);
#endif

	if (lwpka_ec_ks_pri_is_valid(val) ||
	    lwpka_ec_ks_pub_is_valid(val)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("EC keystore %u valid after dispose\n",
						       keystore));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_EC_KS_DISPOSE_SUPPORTED */

/* Set the sticky-to-reset LOCK bit for the EC keystore
 */
status_t lwpka_ec_ks_lock_locked(const engine_t *engine,
				 uint32_t keystore)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	/* There is only one keystore in LWPKA1.1, so it is implicit. */
	if (keystore > LWPKA_MAX_EC_KEYSTORES) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid keystore id %u\n", keystore));
	}

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("before lock", val);
#endif

	if (!BOOL_IS_TRUE(lwpka_ec_ks_pub_is_valid(val))) {
		LOG_INFO("Locking EC keystore %u when public key is not valid\n", keystore);
	}

	if (!BOOL_IS_TRUE(lwpka_ec_ks_pri_is_valid(val))) {
		LOG_INFO("Locking EC keystore %u when private key is not valid\n", keystore);
	}

	if (BOOL_IS_TRUE(lwpka_ec_ks_is_locked(val))) {
		LOG_INFO("EC keystore %u already locked\n", keystore);
	}

	val = LW_FLD_SET_DRF_DEF(LW_SE, LWPKA_CORE_ECC_KEYSTORE_CTRL, LOCK, LOCKED, val);
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0, val);

#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("after lock", val);
#endif

	if (! BOOL_IS_TRUE(lwpka_ec_ks_is_locked(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC keystore %u lock failed\n", keystore));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* input_params can be NULL here.
 *
 * It is optionally used when client requests to return the EC public
 * key generated by the KEYGEN_PUB operation. In that case (when not
 * NULL), the input_params->dst_ec_pubkey must point to
 * sizeof(te_ec_point_t) object.
 *
 * The other EC KS key operations do not access input_params at this
 * point of time.
 */
static status_t lwpka_ec_ks_keyop_locked(struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext,
					 enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *pubkey = NULL;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((econtext->ec_flags & CCC_EC_FLAGS_KEYSTORE) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC KS key eop %u not set to use keystore\n",
					     eop));
	}

	switch (eop) {
	case LWPKA_OP_EC_KS_KEYINS_PRI:
		if (KEY_TYPE_EC_PRIVATE != econtext->ec_keytype) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		ret = lwpka_ec_ks_insert_key_locked(econtext->engine,
						    econtext->pka_data.op_mode,
						    econtext->ec_ks.eks_keystore,
						    econtext->ec_pkey,
						    true,
						    econtext->ec_ks.eks_mf.emf_user,
						    econtext->ec_ks.eks_mf.emf_ex,
						    econtext->ec_ks.eks_mf.emf_sw);
		break;
	case LWPKA_OP_EC_KS_KEYGEN_PRI:
		ret = lwpka_ec_ks_gen_key_locked(econtext->engine,
						 econtext->pka_data.op_mode,
						 econtext->ec_ks.eks_keystore,
						 econtext->ec_ks.eks_mf.emf_user,
						 econtext->ec_ks.eks_mf.emf_ex,
						 econtext->ec_ks.eks_mf.emf_sw);
		break;
	case LWPKA_OP_EC_KS_KEYGEN_PUB:
		/* Input_params can be NULL in case caller does not want to read
		 * the public key from HW. Otherwise public key placed
		 * to input_params->dst_ec_pubkey (which needs to be set by caller).
		 */
		if (NULL != input_params) {
			if ((NULL == input_params->dst_ec_pubkey) ||
			    (input_params->output_size != sizeof_u32(te_ec_point_t))) {
				ret = SE_ERROR(ERR_ILWALID_ARGS);
				break;
			}
			pubkey = input_params->dst_ec_pubkey;
		}

		if (KEY_TYPE_EC_PRIVATE != econtext->ec_keytype) {
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		ret = lwpka_ec_ks_gen_pub_locked(econtext->engine,
						 econtext->ec_ks.eks_keystore,
						 pubkey);
		break;
#if CCC_EC_KS_DISPOSE_SUPPORTED
	case LWPKA_OP_EC_KS_KEYDISP:
		ret = lwpka_ec_ks_dispose_locked(econtext->engine,
						 econtext->ec_ks.eks_keystore);
		break;
#endif
	case LWPKA_OP_EC_KS_KEYLOCK:
		ret = lwpka_ec_ks_lock_locked(econtext->engine,
					      econtext->ec_ks.eks_keystore);
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

status_t lwpka_ec_ks_map_algo_to_operation(te_crypto_algo_t algo,
					   enum lwpka_engine_ops *eop_p)
{
	status_t ret = NO_ERROR;
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;

	LTRACEF("entry\n");

	switch (algo) {
	case TE_ALG_KEYOP_INS:
		eop = LWPKA_OP_EC_KS_KEYINS_PRI;
		break;
	case TE_ALG_KEYOP_GENKEY:
		eop = LWPKA_OP_EC_KS_KEYGEN_PRI;
		break;
	case TE_ALG_KEYOP_GENPUB:
		eop = LWPKA_OP_EC_KS_KEYGEN_PUB;
		break;
	case TE_ALG_KEYOP_DISPOSE:
		eop = LWPKA_OP_EC_KS_KEYDISP;
		break;
	case TE_ALG_KEYOP_LOCK:
		eop = LWPKA_OP_EC_KS_KEYLOCK;
		break;
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*eop_p = eop;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA11_ECKS_KEYOPS */

/* Read result of EC KS ECDSA sign operation from LOR_D0/D1*/
static status_t lwpka_ecdsa_sign_get_result(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext,
					    uint32_t nwords)
{
	status_t ret = NO_ERROR;
	struct te_ec_sig_s *dsig = NULL;

	if ((NULL == input_params->dst_ec_signature) ||
	    (input_params->output_size != sizeof_u32(te_ec_sig_t))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	dsig = input_params->dst_ec_signature;

	/* get sig from D0/D1 (r/s)
	 *
	 * Colwert HW little endian LOR_D0 to big endian S in the signature
	 */
	ret = lwpka_register_read(econtext->engine, econtext->pka_data.op_mode,
				  LOR_D0, &dsig->r[0], nwords, true);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_register_read(econtext->engine, econtext->pka_data.op_mode,
				  LOR_D1, &dsig->s[0], nwords, true);
	CCC_ERROR_CHECK(ret);

	/* Generating big endian signatures */
	dsig->sig_flags = CCC_EC_SIG_FLAG_NONE;

	LOG_HEXBUF("Sig(r)", &dsig->r[0], nwords*BYTES_IN_WORD);
	LOG_HEXBUF("Sig(s)", &dsig->s[0], nwords*BYTES_IN_WORD);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_ks_sign_arg_check(const struct se_data_params *input_params,
					   const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* ECDSA sign needs input_params */
	if ((NULL == input_params) ||
	    (NULL == input_params->src_digest)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((0U == input_params->input_size) ||
	    ((input_params->input_size % BYTES_IN_WORD) != 0U)) {
		/* Not a word size multiple input digest */
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((KEY_TYPE_EC_PRIVATE != econtext->ec_keytype) ||
	    (TE_ALG_ECDSA != econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((econtext->ec_flags & CCC_EC_FLAGS_KEYSTORE) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC KS ECDSA sign not set to use keystore\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_P521
/*
 * NOTE: p521 length is 16 words and 9 bits, rounded up to 17 words
 * (68 bytes). When it contains 17 words, check if the first 23 bits
 * are zero.
 *
 * If yes => code supports signing 521 bit digests.
 * If no => code does not lwrrently support truncating digest values
 * longer than 521 bits to 521 bits.
 */
static status_t ec_ks_p521_value_check(const uint8_t *dgst)
{
	status_t ret = NO_ERROR;

	/* Not supported now to have any of these 23 bits nonzero,
	 * they should be pruned zero.
	 *
	 * To add support: Need to force these bits 0, which means the
	 * digest needs to get copied to a temp buffer and then
	 * passing that zero pruned temp digest to the below call
	 * instead of the original call.
	 *
	 * As we do not support digests longer than 64 bytes
	 * (e.g. SHA-512) so it is not really required now.
	 * Add a requirement if you need such.
	 */
	if ((0U != dgst[0]) ||
	    (0U != dgst[1]) ||
	    (0U != (dgst[2] & 0xFEU))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

	/* First word contains at least 23 leading zero bits =>
	 * just pass such 544 bit digest to P521 sign.
	 */
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_P521 */

// NOTE: KAC_ERROR does NOT SCRUB MEMORY!!!!
// NOTE: may check LFSR status/seeded for detecting hang operation
//  if DRNG is not working (how)

/* Does NOT support any HW debug features (e.g. DRBG from K0)
 *
 * NOTE: Matching LWRNG must be set up before entering this function is called.
 *
 * Uses implicitly the EC keystore 0 (only one exists in HW for now).
 *
 * Now the econtext contains "struct ec_ks_slot ec_ks" field which is
 * the EC keystore for the keystore management functions above.
 * ECDSA sign does not really need that sincxe the keystore contains
 * all information needed. But it also contains ec_ks.eks_keystore which
 * can be used as the keystore id for this call, if required anytime later.
 */
static status_t lwpka_ec_ks_ecdsa_sign_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext,
					      enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t algo_nwords = 0U;
	uint32_t hw_nwords = 0U;
	uint32_t hash_words = 0U;
	uint32_t val = 0U;
	enum pka1_op_mode op_mode = PKA1_OP_MODE_ILWALID;

	LTRACEF("entry\n");

	ret = lwpka_ec_ks_op_check_ectx_arg(econtext);
	CCC_ERROR_CHECK(ret);

	op_mode = econtext->pka_data.op_mode;

	ret = lwpka_ec_ks_sign_arg_check(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	DUMP_DATA_PARAMS("EC KS op input", 0x1U, input_params);

	ret = lwpka_ec_init(econtext, eop);
	CCC_ERROR_CHECK(ret);

	val = tegra_engine_get_reg(econtext->engine, LW_SE_LWPKA_CORE_ECC_KEYSTORE_CTRL_0);
#ifdef CCC_EC_KS_DUMP_MANIFEST
	rd_ec_ks_dump_keystore_ctrl("ECDSA sign", val);
#endif

	if (!BOOL_IS_TRUE(lwpka_ec_ks_pri_is_valid(val))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("ECDSA with EC keystore when private key is not valid\n"));
	}

#ifdef CCC_EC_KS_DUMP_MANIFEST
	val = tegra_engine_get_reg(econtext->engine, LW_SE_LWPKA_CORE_ECC_KEYMANIFEST_HW_0);
	rd_ec_ks_dump_keymanifest("ECDSA sign", val);
#endif

	/* CORE_CONFIG: random source set to HW DRBG (only in debug mode it
	 * can be something else)
	 */
	val = LW_DRF_DEF(LW_SE, LWPKA_CORE_CONFIG, INPUT_SEL, DRBG);
	tegra_engine_set_reg(econtext->engine, LW_SE_LWPKA_CORE_CONFIG_0, val);

	/* Digest to LOR_S2 */
	ret = lwpka_num_words(op_mode, &hw_nwords, &algo_nwords);
	CCC_ERROR_CHECK(ret);

	hash_words = (input_params->input_size / BYTES_IN_WORD);

	/* These lwrve lengths support word size multiple hash length.
	 */
	if (hash_words > algo_nwords) {
		LOG_INFO("Truncating %u word digest to %u words\n", hash_words, algo_nwords);
		hash_words = algo_nwords;
	}

#if HAVE_P521
	if ((PKA1_OP_MODE_ECC521 == op_mode) && (hash_words == algo_nwords)) {
		/* 17 word (544 bit) digest passed in, check if leading
		 * 23 bits in that are zero.
		 * If yes, pass the 521 bit signature for signing, otherwise
		 * throw error ERR_NOT_SUPPORTED.
		 */
		ret = ec_ks_p521_value_check(input_params->src_digest);
		CCC_ERROR_CHECK(ret);
	}
#endif

	/* message hash to LOR_S2 (possibly truncated),
	 * swap byte order to little endian
	 */
	ret = lwpka_register_write(econtext->engine, op_mode, LOR_S2,
				   input_params->src_digest, hash_words, true,
				   hw_nwords);
	CCC_ERROR_CHECK(ret);

	/* Ensure that LWRNG module communicates with LWPKA (how?).
	 *
	 * The operation will stall until it gets the LWRGN DRNG
	 * values it needs.
	 *
	 * Not aware of available SW mechanism to check that the HW
	 * internal LWRNG communication is operational to LWPKA, so
	 * this is not checked by SW.
	 */
	ret = lwpka_ec_ks_run_engine(econtext->engine, eop, op_mode);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ecdsa_sign_get_result(input_params, econtext, algo_nwords);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA11_ECKS_KEYOPS
status_t engine_lwpka_ec_ks_keyop_locked(struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;

	LTRACEF("entry\n");

	ret = lwpka_ec_ks_op_check_ectx_arg(econtext);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_ks_map_algo_to_operation(econtext->algorithm, &eop);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_init(econtext, eop);
	CCC_ERROR_CHECK(ret);

	/* input_params can be NULL, check/used only by KEYGEN_PUB */
	ret = lwpka_ec_ks_keyop_locked(input_params, econtext, eop);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA11_ECKS_KEYOPS */

status_t engine_lwpka_ec_ks_ecdsa_sign_locked(struct se_data_params *input_params,
					      struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_ks_ecdsa_sign_locked(input_params, econtext, LWPKA_OP_EC_KS_ECDSA_SIGN);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Save some space by reading lwrve parameters from HW
 * for the following lwrves in LWPKA11 (HW stores parameters
 * in little endian):
 *
 * Offsets:
 *
 * value: range          : reserved    : name
 * -----------------------------------------------
 * p	: 0x0000 .. 0x001F (.. 0x007F) : Field prime
 * h	: 0x0080 .. 0x009F (.. 0x00FF) : Lwrve co-factor
 * n    : 0x0100 .. 0x011F (.. 0x017F) : Order of base point
 * a    : 0x0180 .. 0x019F (.. 0x01FF) : Coefficient a
 * b    : 0x0200 .. 0x021F (.. 0x027F) : Coefficient b
 * Gx   : 0x0280 .. 0x029F (.. 0x02FF) : Base point X
 * Gy   : 0x0300 .. 0x031F (.. 0x037F) : Base point Y
 * -----------------------------------------------
 * Rsvd : ----             (0x0380..)  : Reserved
 *
 * Supported lwrves:
 *
 * NIST-P256
 * NIST-P384
 * NIST-P521
 */
#if 0 // optimization to use ROM lwrve values instead of CCC defined (support later)
status_t lwpka_copy_hw_lwrve_parameters(const engine_t *engine,
					lwrve_id_t lwrve_id)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (lwrve_id) {
	case TE_LWRVE_ID_NIST_P_256:
	case TE_LWRVE_ID_NIST_P_384:
	case TE_LWRVE_ID_NIST_P_521:
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif // 0

#endif /* HAVE_LWPKA11_SUPPORT */
