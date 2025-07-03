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
 * Support for CheetAh Security Engine Elliptic crypto point operations.
 */
#include <crypto_system_config.h>

#if HAVE_PKA1_ECC

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

// XXXXX TODO: Should probably move gen functions to ec_gen
// XXXXX   but keep here until merged; then move to keep diffs?

enum point_id_t {
	POINT_ID_P = 0,
	POINT_ID_Q = 1,
};

/*****************************/

static uint32_t ec_get_pow2_aligned_word_size(uint32_t nbytes)
{
	return (CCC_EC_POW2_ALIGN(nbytes) / BYTES_IN_WORD);
}

static status_t pka1_ec_load_point_args_check(const struct se_engine_ec_context *econtext,
					      const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) || (NULL == econtext->pka1_data) ||
	    (NULL == point)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

fail:
	return ret;
}

static status_t load_point_operand_id_Q(const struct se_engine_ec_context *econtext,
					const te_ec_point_t *point,
					enum pka1_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	uint32_t field_word_size = 0U;
	uint32_t pow2_field_word_size = 0U;
	bool src_point_BE = false;

	LTRACEF("entry\n");

	ec_size_words        = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;
	pow2_field_word_size = ec_get_pow2_aligned_word_size(econtext->ec_lwrve->nbytes);

	src_point_BE = ((point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);

	if (PKA1_OP_MODE_ECC521 == econtext->ec_lwrve->mode) {
		field_word_size = pow2_field_word_size;
	}

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_SHAMIR_TRICK:
	case PKA1_OP_EC_P521_SHAMIR_TRICK:
	case PKA1_OP_EC_P521_POINT_ADD:
#endif
	case PKA1_OP_EC_POINT_ADD:
	case PKA1_OP_EC_SHAMIR_TRICK:
		LTRACEF("Reg writing %u word 'Qx'\n", ec_size_words);
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_A, R3,
					       point->x,
					       ec_size_words, src_point_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC 'Qx' PKA bank reg A3\n");
			break;
		}

		LTRACEF("Reg writing %u word 'Qy'\n", ec_size_words);
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_B, R3,
					       point->y,
					       ec_size_words, src_point_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC 'Qy' PKA bank reg B3\n");
			break;
		}
		break;

	default:
		LTRACEF("Point Q not used as input for EC op %u\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t load_point_operand_id_P(const struct se_engine_ec_context *econtext,
					const te_ec_point_t *point,
					enum pka1_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	uint32_t field_word_size = 0U;
	uint32_t pow2_field_word_size = 0U;
	bool src_point_BE = false;
	bool use_Py = true;

	/* Load Px and Py; load point to <A2,B2> except for
	 * POINT_DOUBLE for which point is loaded to <A3,B3>.
	 */
	pka1_reg_id_t reg = R2;

	LTRACEF("entry\n");

	ec_size_words        = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;
	pow2_field_word_size = ec_get_pow2_aligned_word_size(econtext->ec_lwrve->nbytes);

	src_point_BE = ((point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);

	if (PKA1_OP_MODE_ECC521 == econtext->ec_lwrve->mode) {
		field_word_size = pow2_field_word_size;
	}

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_DOUBLE:
#endif
	case PKA1_OP_EC_POINT_DOUBLE:
		reg = R3;
		break;

#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_ADD:
	case PKA1_OP_EC_P521_POINT_VERIFY:
	case PKA1_OP_C25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_POINT_DOUBLE: /* This point dbl loads to <A2,B2> !!! */
	case PKA1_OP_ED25519_POINT_VERIFY:
	case PKA1_OP_ED25519_SHAMIR_TRICK:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
	case PKA1_OP_EC_POINT_ADD:
	case PKA1_OP_EC_SHAMIR_TRICK:
	case PKA1_OP_EC_POINT_VERIFY:
		break;

	default:
		LTRACEF("Point P not used as input for EC op %u\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Reg writing %u word 'Px'\n", ec_size_words);
	ret = pka1_bank_register_write(econtext->engine,
				       econtext->pka1_data->op_mode,
				       BANK_A, reg,
				       point->x,
				       ec_size_words, src_point_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC 'Px' PKA bank reg A%u\n", reg));

#if HAVE_ELLIPTIC_20
	/* Only X-coordinate is loaded for Lwrve25519 point mult operation */
	if (PKA1_OP_C25519_POINT_MULTIPLY == eop) {
		use_Py = false;
	}
#endif

	if (BOOL_IS_TRUE(use_Py)) {
		LTRACEF("Reg writing %u word 'Py'\n", ec_size_words);
		ret = pka1_bank_register_write(econtext->engine,
					       econtext->pka1_data->op_mode,
					       BANK_B, reg,
					       point->y,
					       ec_size_words, src_point_BE,
					       field_word_size);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to write EC 'Py' PKA bank reg B%u\n", reg));
	}

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

/*
 * This is setting point_id point to operation specific PKA bank registers
 * (See document EDN_0577_PKA_FW_ECC_521_Lib_vp1_draft.pdf)
 *
 * If (POINT_ID == POINT_ID_P) => point is P and uses point P bank registers.
 * If (POINT_ID == POINT_ID_Q) => point is Q and uses point Q bank registers.
 *
 * Note that only POINT_ADD and SHAMIR'S TRICK use two (P and Q) point operands;
 * the other ops use just P (except PKA1_OP_EC_ISM3 which does not use points).
 */
static status_t pka1_ec_load_point_operand(const struct se_engine_ec_context *econtext,
					   enum point_id_t point_id,
					   const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;
	enum pka1_engine_ops eop;

	LTRACEF("entry\n");

	ret = pka1_ec_load_point_args_check(econtext, point);
	CCC_ERROR_CHECK(ret);

	eop = econtext->pka1_data->pka1_op;

#if HAVE_PKA1_ISM3
	if (PKA1_OP_EC_ISM3 == eop) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC 'a==-3' check does not use point operands\n"));
	}
#endif /* HAVE_PKA1_ISM3 */

	switch (point_id) {
	case POINT_ID_Q:
		ret = load_point_operand_id_Q(econtext, point, eop);
		break;

	case POINT_ID_P:
		ret = load_point_operand_id_P(econtext, point, eop);
		break;

	default:
		LTRACEF("Unsupported point id type %u\n", point_id);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t pka1_ec_get_boolean_result(const struct se_engine_ec_context *econtext,
					   bool *result_p)
{
	status_t ret = NO_ERROR;
	enum pka1_engine_ops eop;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->pka1_data) || (NULL == result_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*result_p = false;

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	eop = econtext->pka1_data->pka1_op;

	switch (eop) {
#if HAVE_PKA1_ISM3
	case PKA1_OP_EC_ISM3:
		if (PKA1_OP_MODE_ECC521 == econtext->ec_lwrve->mode) {
			LTRACEF("a==-3 operation does not support P-521 lwrves\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}

		/* ISM3 returns result in flag bit F3
		 */
		val = tegra_engine_get_reg(econtext->engine, SE_PKA1_FLAGS_OFFSET);

		*result_p = false;
		if ((val & SE_PKA1_FLAGS_FLAG_F3(SE_ELP_ENABLE)) != 0U) {
			*result_p = true;
		}
		break;
#endif /* HAVE_PKA1_ISM3 */

#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_VERIFY:
	case PKA1_OP_ED25519_POINT_VERIFY:
#endif
	case PKA1_OP_EC_POINT_VERIFY:
		/* RC_ZERO bit in RETURN_CODE register is a copy of the
		 * FLAGS register flag F0.
		 *
		 * XXXX TODO: maybe should anyway read Z value (RC_ZERO?)
		 */
		val = tegra_engine_get_reg(econtext->engine, SE_PKA1_FLAGS_OFFSET);

		*result_p = false;
		if ((val & SE_PKA1_FLAGS_FLAG_ZERO(SE_ELP_ENABLE)) != 0U) {
			*result_p = true;
		}
		break;

	default:
		CCC_ERROR_MESSAGE("operation %u does not return a boolean result\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t pka1_ec_select_point_register(pka1_reg_id_t *reg_x_p,
					      pka1_reg_id_t *reg_y_p,
					      bool *use_Py_p,
					      enum pka1_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == reg_x_p) || (NULL == reg_y_p) || (NULL == use_Py_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (eop) {
#if HAVE_PKA1_ISM3
	case PKA1_OP_EC_ISM3:
		LTRACEF("EC 'a==-3' check does not return point result\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
#endif /* HAVE_PKA1_ISM3 */

#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_VERIFY:
	case PKA1_OP_ED25519_POINT_VERIFY:
#endif
	case PKA1_OP_EC_POINT_VERIFY:
		LTRACEF("EC 'point verify' does not return point result\n");
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;

#if HAVE_ELLIPTIC_20
	case PKA1_OP_C25519_POINT_MULTIPLY:
		*use_Py_p = false;

		/* Returns point in <A2,B2> */
		*reg_x_p = R2;
		*reg_y_p = R2;
		break;

	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_POINT_DOUBLE:
	case PKA1_OP_ED25519_SHAMIR_TRICK:

	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_DOUBLE:
	case PKA1_OP_EC_P521_POINT_ADD:
	case PKA1_OP_EC_P521_SHAMIR_TRICK:
		/* FALLTHROUGH */
#endif /* HAVE_ELLIPTIC_20 */

	case PKA1_OP_EC_POINT_DOUBLE:
		/* Returns point in <A2,B2> */
		*reg_x_p = R2;
		*reg_y_p = R2;
		break;

	case PKA1_OP_EC_POINT_MULTIPLY:
	case PKA1_OP_EC_POINT_ADD:
	case PKA1_OP_EC_SHAMIR_TRICK:
		break;

	default:
		LTRACEF("EC operation %u not supported\n", eop);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

/* Reads a POINT RESULT from operations that return a point
 */
static status_t pka1_ec_get_point_result(const struct se_engine_ec_context *econtext,
					 te_ec_point_t *point,
					 bool result_BE)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	enum pka1_engine_ops eop;

	pka1_reg_id_t reg_x = R3;
	pka1_reg_id_t reg_y = R3;
	bool use_Py = true;

#if HAVE_ELLIPTIC_20 == 0
	/* New FW => all "point returning functions" return point in <A2,B2>
	 */
	reg_x = R2;
	reg_y = R2;
#endif

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->pka1_data) ||
	    (NULL == point)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	ec_size_words        = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;
	eop		     = econtext->pka1_data->pka1_op;

	point->point_flags = CCC_EC_POINT_FLAG_NONE;
	if (!BOOL_IS_TRUE(result_BE)) {
		point->point_flags |= CCC_EC_POINT_FLAG_LITTLE_ENDIAN;
	}

	ret = pka1_ec_select_point_register(&reg_x, &reg_y, &use_Py, eop);
	CCC_ERROR_CHECK(ret);

	LTRACEF("Reg reading %u word 'Rx (A%u)'\n", ec_size_words, reg_x);
	ret = pka1_bank_register_read(econtext->engine, econtext->pka1_data->op_mode,
				      BANK_A, reg_x,
				      point->x,
				      ec_size_words, result_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to read EC 'Rx' PKA bank reg A%u\n", reg_x));

	if (BOOL_IS_TRUE(use_Py)) {
		LTRACEF("Reg reading %u word 'Ry(B%u)'\n", ec_size_words, reg_y);
		ret = pka1_bank_register_read(econtext->engine, econtext->pka1_data->op_mode,
					      BANK_B, reg_y,
					      point->y,
					      ec_size_words, result_BE);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to read EC 'Ry' PKA bank reg B%u\n", reg_y));
	}

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t pka1_ec_run_program(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;
	pka1_entrypoint_t entrypoint = PKA1_ENTRY_ILWALID;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->pka1_data) ||
	    (NULL == econtext->ec_lwrve)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (econtext->pka1_data->pka1_op) {
	case PKA1_OP_EC_POINT_MULTIPLY:
		entrypoint = PKA1_ENTRY_EC_POINT_MULT;
		break;

	case PKA1_OP_EC_POINT_ADD:
		entrypoint = PKA1_ENTRY_EC_POINT_ADD;
		break;

	case PKA1_OP_EC_POINT_DOUBLE:
		entrypoint = PKA1_ENTRY_EC_POINT_DOUBLE;
		break;

	case PKA1_OP_EC_POINT_VERIFY:
		entrypoint = PKA1_ENTRY_EC_ECPV;
		break;

	case PKA1_OP_EC_SHAMIR_TRICK:
		entrypoint = PKA1_ENTRY_EC_SHAMIR_TRICK;
		break;

#if HAVE_PKA1_ISM3
	case PKA1_OP_EC_ISM3:
		entrypoint = PKA1_ENTRY_IS_LWRVE_M3;
		break;
#endif /* HAVE_PKA1_ISM3 */

#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
		entrypoint = PKA1_ENTRY_EC_521_POINT_MULT;
		break;
	case PKA1_OP_EC_P521_POINT_ADD:
		entrypoint = PKA1_ENTRY_EC_521_POINT_ADD;
		break;
	case PKA1_OP_EC_P521_POINT_DOUBLE:
		entrypoint = PKA1_ENTRY_EC_521_POINT_DOUBLE;
		break;
	case PKA1_OP_EC_P521_POINT_VERIFY:
		entrypoint = PKA1_ENTRY_EC_521_PV;
		break;
	case PKA1_OP_EC_P521_SHAMIR_TRICK:
		entrypoint = PKA1_ENTRY_EC_521_SHAMIR_TRICK;
		break;

	case PKA1_OP_GEN_MULTIPLY:
		/* non-modular multiplication: c=a*b
		 *
		 * XXX May be used with ED25519; but is generic DP multiply.
		 */
		entrypoint = PKA1_ENTRY_GEN_MULTIPLY;
		break;

#if HAVE_PKA1_X25519
	case PKA1_OP_C25519_POINT_MULTIPLY:
		entrypoint = PKA1_ENTRY_C25519_POINT_MULT;
		break;
	case PKA1_OP_C25519_MOD_EXP:
		entrypoint = PKA1_ENTRY_C25519_MOD_EXP;
		break;
	case PKA1_OP_C25519_MOD_MULT:
		entrypoint = PKA1_ENTRY_C25519_MOD_MULT;
		break;
	case PKA1_OP_C25519_MOD_SQUARE:
		entrypoint = PKA1_ENTRY_C25519_MOD_SQR;
		break;
#endif /* HAVE_PKA1_X25519 */

#if HAVE_PKA1_ED25519
	case PKA1_OP_ED25519_POINT_MULTIPLY:
		entrypoint = PKA1_ENTRY_ED25519_POINT_MULT;
		break;

	case PKA1_OP_ED25519_POINT_ADD:
		entrypoint = PKA1_ENTRY_ED25519_POINT_ADD;
		break;

	case PKA1_OP_ED25519_POINT_DOUBLE:
		entrypoint = PKA1_ENTRY_ED25519_POINT_DOUBLE;
		break;

	case PKA1_OP_ED25519_POINT_VERIFY:
		entrypoint = PKA1_ENTRY_ED25519_PV;
		break;

	case PKA1_OP_ED25519_SHAMIR_TRICK:
		entrypoint = PKA1_ENTRY_ED25519_SHAMIR_TRICK;
		break;
#endif /* HAVE_PKA1_ED25519 */
#endif /* HAVE_ELLIPTIC_20 */

	default:
		CCC_ERROR_MESSAGE("Unknown EC engine operation: %u", econtext->pka1_data->pka1_op);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	// XXXX If LOAD_KEY bit is set, should maybe wait until the status reg load key is clear?
	// XXXX But I do not know which reg write triggers the load key operation and when to wait!
	// XXX TODO support key loads from SE slots
	//
	ret = pka1_go_start(econtext->engine, econtext->pka1_data->op_mode,
			    econtext->ec_lwrve->nbytes, entrypoint);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC go start returned: %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
static status_t ec_clear_ec_keyslot_locked(const engine_t *engine,
					     uint32_t pka_keyslot,
					     uint32_t ec_byte_size)
{
	status_t ret = NO_ERROR;
	status_t tret = NO_ERROR;
	uint32_t ec_size_words = ec_get_pow2_aligned_word_size(ec_byte_size);
	const uint32_t zero = 0U;
	uint32_t field = KMAT_EC_KEY;

	LTRACEF("entry\n");

	if ((NULL == engine) || (pka_keyslot >= PKA1_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* On size error: clear HW max size EC operands (32 words, 128 bytes)
	 * i.e. just use value 0 to clear max size.
	 */
	if ((ec_size_words < (160U/BITS_IN_WORD)) ||
	    (ec_size_words > (512U/BITS_IN_WORD))) {
		/* HW supports max 32 words (128 bytes) per field */
		ec_size_words = 32U;
	}

	LTRACEF("Clearing PKA1 EC keyslot %u\n", pka_keyslot);

	FI_BARRIER(status, ret);
	/* Try to clear all fields in spite of errors
	 * KMAT_EC_KEY is the only field that is used => ignore the other
	 * fields until code uses them.
	 *
	 * LOOP present for future only => if need to clear other fields as well.
	 * This loops only once now.
	 */
	for (field = KMAT_EC_KEY; field <= KMAT_EC_KEY; field++) {
		LTRACEF("Clearing %u word EC: %u\n", ec_size_words, field);
		tret = pka1_set_keyslot_field(engine, field,
					      pka_keyslot, (const uint8_t *)&zero,
					      1U, CFALSE, ec_size_words);
		if (NO_ERROR != tret) {
			LTRACEF("Failed to clear EC kslot %u field %u\n",
				pka_keyslot, field);
			if (NO_ERROR == ret) {
				ret = tret;
				/* FALLTHROUGH */
			}
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Note: even though the PKA1 EC keyslot contains a lot of fields
 * only the field "k" is used lwrrently, because only POINT MULTIPLICATION
 * operations trigger LOAD_KEY in HW.
 *
 * ==> Do not spend time writing the other fields, they are unused.
 *
 * This always writes scalar to keyslot in LITTLE ENDIAN zero padding
 * to next pow(2) size
 */
static status_t pka1_write_scalar_to_keyslot_locked(const struct se_engine_ec_context *econtext,
						    const uint8_t *scalar,
						    uint32_t slen,
						    bool scalar_LE)
{
	status_t ret = NO_ERROR;
	uint32_t pow2_field_word_size = 0U;
	const pka1_ec_lwrve_t *lwrve = NULL;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == scalar)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	// XXXX private key (or scalar secret) is written here...
	// XXX FIXME: deal with non-word multiple scalars unless handled
	//     by caller already!!!
	//
	if ((0U == slen) || ((slen % BYTES_IN_WORD) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("XXXX scalar length not word aligned %u\n",
					     slen));
	}

	lwrve	      = econtext->ec_lwrve;
	pow2_field_word_size = ec_get_pow2_aligned_word_size(lwrve->nbytes);

	if (slen > lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("scalar longer than lwrve size\n"));
	}

	LTRACEF("Writing %u word EC scalar\n", lwrve->nbytes / BYTES_IN_WORD);
	ret = pka1_set_keyslot_field(econtext->engine, KMAT_EC_KEY,
				     econtext->ec_keyslot,
				     scalar,
				     (slen / BYTES_IN_WORD),
				     !scalar_LE,
				     pow2_field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC kslot %u field %u\n",
				econtext->ec_keyslot, KMAT_EC_KEY));

	LOG_HEXBUF("Written scalar (size actually pow2) to key slot",
		   scalar, slen);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_LOAD */

static status_t pka1_ec_set_keymat_check_args(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) || (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}
fail:
	return ret;
}

static status_t pka1_ec_set_keymat_weierstrass(const struct se_engine_ec_context *econtext,
					       const pka1_ec_lwrve_t *lwrve,
					       enum pka1_engine_ops eop,
					       uint32_t ec_size_words,
					       uint32_t field_word_size,
					       bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	// TODO: Support P521 lwrve => It needs specific values to be set up (and F1 bit).
	// Lwrrently the code does not support this ==> Add support TODO

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_MODMULT_521:
	case PKA1_OP_EC_M_521_MONTMULT:
	case PKA1_OP_EC_P521_POINT_ADD:
#endif
	case PKA1_OP_EC_POINT_ADD:
		break;

	default:
		/* Load A => A6 */
		LTRACEF("Reg writing %u word lwrve param 'a'\n", ec_size_words);
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_A, R6,
					       lwrve->a,
					       ec_size_words, lwrve_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC param 'a' PKA bank reg A6\n");
			break;
		}
		break;
	}
	CCC_ERROR_CHECK(ret);

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_VERIFY:
#endif
	case PKA1_OP_EC_POINT_VERIFY:
		/* Load B => A7 */
		LTRACEF("Reg writing %u word lwrve param 'b'\n", ec_size_words);
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_A, R7,
					       lwrve->b,
					       ec_size_words, lwrve_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC param 'b' PKA bank reg A7\n");
			break;
		}
		break;

	default:
		/* ok */
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_X25519
/* Lwrve25519 lwrrently only supports one operation:
 * Point multiplication.
 *
 * Lwrve25519: y^2 - x^2 = 1 + d*x^2*y^2 mod p
 * where:
 *  d = -121665/121666
 *  p = 2^255 - 19
 *
 * ECDH with lwrve25519 has been nicknamed
 *  to X25519 in Bernstein's paper.
 */
static status_t pka1_ec_set_keymat_lwrve25519(const struct se_engine_ec_context *econtext,
					      const pka1_ec_lwrve_t *lwrve,
					      enum pka1_engine_ops eop,
					      uint32_t ec_size_words,
					      uint32_t field_word_size,
					      bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	/* Elliptic requires loading the below value as lwrve param K
	 *  for all lwrrently supported Lwrve25519 operations.
	 *  (Note: this is not "key K" it is constant 121666 value K_param)
	 */
	uint32_t k_param = 121666U;

	(void)lwrve;
	(void)ec_size_words;
	(void)lwrve_BE;

	LTRACEF("entry\n");

	switch (eop) {
	case PKA1_OP_C25519_POINT_MULTIPLY:
		LTRACEF("Reg writing 1 word k_param-value\n");
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_D, R2,
					       (const uint8_t *)&k_param, 1U,
					       CFALSE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write k_param-value PKA bank reg D2\n");
			break;
		}
		break;

	default:
		CCC_ERROR_MESSAGE("Unsupported C25519 operation: %u\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_X25519 */

#if HAVE_PKA1_ED25519
static status_t pka1_ec_set_keymat_ed25519(const struct se_engine_ec_context *econtext,
					   const pka1_ec_lwrve_t *lwrve,
					   enum pka1_engine_ops eop,
					   uint32_t ec_size_words,
					   uint32_t field_word_size,
					   bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	(void)eop; /* XXX TODO: check eop? */

	LTRACEF("entry\n");

	/* Common for ED25519 => write d to C(5)
	 * This is a pre-callwlated constant value d = -121665/121666
	 */
	LTRACEF("Reg writing %u word d\n", ec_size_words);

	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_C, R5,
				       lwrve->d,
				       ec_size_words,
				       lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write d-value PKA bank reg C5\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ED25519 */

static status_t pka1_ec_set_keymat_write_montgomery(const struct se_engine_ec_context *econtext,
						    uint32_t ec_size_words,
						    uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	bool montg_BE = false;

	LTRACEF("entry\n");

	montg_BE = ((econtext->pka1_data->mont_flags & PKA1_MONT_FLAG_M_PRIME_LITTLE_ENDIAN) == 0U);

	LTRACEF("Reg writing %u word p'\n", ec_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R1,
				       econtext->pka1_data->m_prime,
				       ec_size_words, montg_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC p' PKA bank reg D1\n"));

	montg_BE = ((econtext->pka1_data->mont_flags & PKA1_MONT_FLAG_R2_LITTLE_ENDIAN) == 0U);

	LTRACEF("Reg writing %u word r^2 mod p\n", ec_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R3,
				       econtext->pka1_data->r2,
				       ec_size_words, montg_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC r^2 mod p PKA bank reg D3\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_update_shamir_trick_bank_select(enum pka1_engine_ops eop,
						   pka1_bank_t *st_bank_k_p,
						   pka1_reg_id_t *st_reg_k_p,
						   pka1_bank_t *st_bank_l_p,
						   pka1_reg_id_t *st_reg_l_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)st_bank_k_p;
	(void)st_reg_k_p;
	(void)st_bank_l_p;
	(void)st_reg_l_p;

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_ED25519_SHAMIR_TRICK:
		*st_bank_k_p = BANK_A;
		*st_reg_k_p  = R5;
		*st_bank_l_p = BANK_B;
		*st_reg_l_p  = R5;
		break;

	case PKA1_OP_EC_P521_SHAMIR_TRICK:
		break;
#endif
	case PKA1_OP_EC_SHAMIR_TRICK:
		break;

	default:
		LTRACEF("Not shamir trick compatible operation: %u\n", eop);
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * R = kP + lQ
 *
 * XXX FIXME: read 2.1.4 for operation size => TODO
 *
 * Normal:  Load k => A7, l => D7
 * ED25519: Load k => A5, l => B5 (blinding w == A7)
 *
 * Change: PAD both K and L values to next pow2 size.
 * XXX Not sure if required, but for other op key values it
 * XXX  is mandatory? ==> VERIFY => TODO
 *
 * FIXME: does not work if L or K size is not word multiple!
 * (XXX could make a local copy and pad if not word multiple...)
 */
static status_t pka1_ec_set_keymat_setup_shamir_trick(const struct se_engine_ec_context *econtext,
						      const pka1_ec_lwrve_t *lwrve,
						      enum pka1_engine_ops eop,
						      uint32_t pow2_field_word_size)
{
	status_t ret = NO_ERROR;

	/* load shamir-trick operation specific bank register values.
	 */
	pka1_bank_t st_bank_k  = BANK_A;
	pka1_reg_id_t st_reg_k = R7;
	pka1_bank_t st_bank_l  = BANK_D;
	pka1_reg_id_t st_reg_l = R7;

	bool scalar_l_BE = false;
	bool scalar_k_BE = false;

	LTRACEF("entry\n");

	if (econtext->ec_k_length > lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("scalar K too long\n"));
	}

	if (econtext->ec_l_length > lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("scalar L too long\n"));
	}

	/* field ec_l endian? */
	scalar_l_BE = ((econtext->ec_flags & CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN) == 0U);

	/* field ec_k endian? */
	scalar_k_BE = ((econtext->ec_flags & CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN) == 0U);

	ret = ec_update_shamir_trick_bank_select(eop, &st_bank_k, &st_reg_k,
						 &st_bank_l, &st_reg_l);
	CCC_ERROR_CHECK(ret);

	/* Load multiplier for point Q */
	LTRACEF("Reg writing %u word l\n", econtext->ec_l_length / BYTES_IN_WORD);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       st_bank_l, st_reg_l,
				       econtext->ec_l,
				       (econtext->ec_l_length / BYTES_IN_WORD),
				       scalar_l_BE, pow2_field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC l PKA bank reg\n"));

	// Load multiplier for point P
	LTRACEF("Reg writing %u word k\n", econtext->ec_k_length / BYTES_IN_WORD);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       st_bank_k, st_reg_k,
				       econtext->ec_k,
				       (econtext->ec_k_length / BYTES_IN_WORD),
				       scalar_k_BE, pow2_field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC k PKA bank reg\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* K (value must be 2 < k < order_of_base_point_of_lwrve)
 * XXXX TODO => need to verify this (by caller)
 *
 * If this is a partial radix value => must be zero extended
 * to pow2_field_word_size. This is done now below.
 *
 * Load k => D7, zero padding to pow2(nbytes)
 *
 * FIXME: does not work if K size is not word multiple!!!
 * (XXX make a local copy if not word multiple...)
 */
static status_t pka1_ec_set_keymat_setup_multiply(const struct se_engine_ec_context *econtext,
						  const pka1_ec_lwrve_t *lwrve,
						  enum pka1_engine_ops eop,
						  uint32_t pow2_field_word_size,
						  bool use_keyslot,
						  bool set_key_value)
{
	status_t ret = NO_ERROR;
	bool scalar_k_BE = false;

	(void)eop;
	(void)set_key_value; /* not used unless HAVE_PKA1_LOAD */

	LTRACEF("entry\n");

	/* field ec_k endian? */
	scalar_k_BE = ((econtext->ec_flags & CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN) == 0U);

	if (BOOL_IS_TRUE(use_keyslot)) {
#if HAVE_PKA1_LOAD
		if (BOOL_IS_TRUE(set_key_value)) {
			ret = pka1_write_scalar_to_keyslot_locked(econtext,
								  econtext->ec_k,
								  econtext->ec_k_length,
								  !scalar_k_BE);
			CCC_ERROR_CHECK(ret,
					LTRACEF("EC kslot k write failed %d\n", ret));
		}
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("PKA EC keyslot loading not supported\n"));
#endif /* HAVE_PKA1_LOAD */
	} else {
		if (econtext->ec_k_length > lwrve->nbytes) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("scalar K too long\n"));
		}

		LTRACEF("Reg writing %u word k\n", econtext->ec_k_length / BYTES_IN_WORD);
		ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
					       BANK_D, R7,
					       econtext->ec_k,
					       (econtext->ec_k_length / BYTES_IN_WORD),
					       scalar_k_BE, pow2_field_word_size);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to write EC k PKA bank reg D7\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_set_lwrve_keymat(const struct se_engine_ec_context *econtext,
				    const pka1_ec_lwrve_t *lwrve,
				    uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	enum pka1_engine_ops eop;
	uint32_t ec_size_words = lwrve->nbytes / BYTES_IN_WORD;
	bool lwrve_BE = ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) == 0U);

	LTRACEF("entry\n");

	eop = econtext->pka1_data->pka1_op;

	LTRACEF("Loading EC parameters to PKA bank registers\n");

	/* First set lwrve specific values to bank registers
	 */
	switch (lwrve->type) {
	case PKA1_LWRVE_TYPE_WEIERSTRASS:
		/* Lwrve: (x^3 + ax + b) mod p
		 * Point: Pxy
		 *
		 * All operations do not require all regs to be set =>
		 * write only used regs.
		 *
		 * Callee zero pads short fields.
		 */
		ret = pka1_ec_set_keymat_weierstrass(econtext,
						     lwrve,
						     eop,
						     ec_size_words,
						     field_word_size,
						     lwrve_BE);
		break;

#if HAVE_PKA1_X25519
	case PKA1_LWRVE_TYPE_LWRVE25519:
		ret = pka1_ec_set_keymat_lwrve25519(econtext,
						    lwrve,
						    eop,
						    ec_size_words,
						    field_word_size,
						    lwrve_BE);
		break;
#endif /* HAVE_PKA1_X25519 */

#if HAVE_PKA1_ED25519
	case PKA1_LWRVE_TYPE_ED25519:
		ret = pka1_ec_set_keymat_ed25519(econtext,
						 lwrve,
						 eop,
						 ec_size_words,
						 field_word_size,
						 lwrve_BE);
		break;
#endif /* HAVE_PKA1_ED25519 */

	default:
		CCC_ERROR_MESSAGE("Unsupported lwrve type: %u\n", lwrve->type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_setup_keymat_for_operation(const struct se_engine_ec_context *econtext,
					      const pka1_ec_lwrve_t *lwrve,
					      uint32_t pow2_field_word_size,
					      bool use_keyslot,
					      bool set_key_value,
					      enum pka1_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_ED25519_SHAMIR_TRICK:
	case PKA1_OP_EC_P521_SHAMIR_TRICK:
#endif
	case PKA1_OP_EC_SHAMIR_TRICK:
		ret = pka1_ec_set_keymat_setup_shamir_trick(econtext,
							    lwrve,
							    eop,
							    pow2_field_word_size);
		break;

#if HAVE_ELLIPTIC_20
	case PKA1_OP_C25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
		/*
		 * Set ec_k && ec_k_length correctly before calling point multiply locked!
		 * (ec_k is the scalar value to multiply the point with)
		 */
		ret = pka1_ec_set_keymat_setup_multiply(econtext,
							lwrve,
							eop,
							pow2_field_word_size,
							use_keyslot,
							set_key_value);
		break;

	default:
		/* ok */
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_ec_set_keymat(struct se_engine_ec_context *econtext,
				   bool use_keyslot, bool set_key_value)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	bool lwrve_BE = false;
	uint32_t field_word_size = 0U;
	uint32_t pow2_field_word_size = 0U;
	enum pka1_engine_ops eop;
	const pka1_ec_lwrve_t *lwrve = NULL;
	bool skip_montgomery = false;

	LTRACEF("entry\n");

	ret = pka1_ec_set_keymat_check_args(econtext);
	CCC_ERROR_CHECK(ret);

	lwrve = econtext->ec_lwrve;
	ec_size_words        = lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;
	pow2_field_word_size = ec_get_pow2_aligned_word_size(lwrve->nbytes);

	if (PKA1_OP_MODE_ECC521 == lwrve->mode) {
		field_word_size = pow2_field_word_size;
	}

	lwrve_BE = ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) == 0U);

	LTRACEF("entry (lwrve params(is BE: %u), use_keyslot: %u)\n",
		lwrve_BE, use_keyslot);

	eop = econtext->pka1_data->pka1_op;

	ret = ec_set_lwrve_keymat(econtext, lwrve, field_word_size);
	CCC_ERROR_CHECK(ret);

	ret = ec_setup_keymat_for_operation(econtext, lwrve,
					    pow2_field_word_size, use_keyslot,
					    set_key_value, eop);
	CCC_ERROR_CHECK(ret);

	/* Write p (modulus) to D0 for all operations with all lwrve types.
	 *
	 * TODO: Optimize... It might be there already if the Montgomery values were callwlated
	 * in this call earlier for this lwrve type.
	 *
	 * But just to be sure => write it anyway here for now. => TODO
	 */
	LTRACEF("Reg writing %u word modulus p\n", ec_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R0,
				       lwrve->p,
				       ec_size_words, lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC modulus p PKA bank reg D0\n"));

#if HAVE_PKA1_ISM3
	skip_montgomery = (PKA1_OP_EC_ISM3 == eop);
#endif

	if (!BOOL_IS_TRUE(skip_montgomery)) {
		/* If the current lwrve does not use montgomery => no operation with
		 *  such a lwrve uses these.
		 */
		if ((lwrve->flags & PKA1_LWRVE_FLAG_NO_MONTGOMERY) == 0U) {

			// NIST and BRAINPOOL lwrves use Montgomery colwersions.
			// (excluding Nist-P521, but that is also flagged as "non montgomery")

			/* Write Montgomery residues
			 *
			 * XXXX THESE SHOULD BE PRE_COMPUTED (they are lwrve specific constants)
			 *
			 * They are in LE order when callwlated by
			 * this SW, but if using public values they
			 * might be in same byte order as other
			 * lwrve params (which is most likely big
			 * endian)
			 */
			ret = pka1_ec_set_keymat_write_montgomery(econtext,
								  ec_size_words,
								  field_word_size);
			CCC_ERROR_CHECK(ret);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* The montgomery values are constants for EC lwrves => they should
 * be hard coded to the lwrve data, for each lwrve.
 *
 * They can also be callwlated in case they are not provided.
 *
 * This function sets up the montgomery constants to pka1data object,
 * either way.
 */
static status_t pka1_ec_setup_montgomery(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;
	bool lwrve_BE = false;
	struct pka1_engine_data *pka1data = NULL;

	LTRACEF("entry\n");

	pka1data = econtext->pka1_data;

	/* Set/Compute Montgomery constants for this EC lwrve */
	lwrve_BE = ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) == 0U);

	if ((pka1data->mont_flags & PKA1_MONT_FLAG_VALUES_OK) != 0U) {
		LTRACEF("Mont flags already OK\n");
	} else {
		/* Use the provided montgomery values in lwrve parameters?
		 */
		if ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_MONTGOMERY_OK) != 0U) {

			LTRACEF("Static EC montgomery constants for lwrve %s\n",
				econtext->ec_lwrve->name);

			ret = tegra_pka1_set_fixed_montgomery_values(pka1data,
								     econtext->ec_lwrve->mg.m_prime,
								     econtext->ec_lwrve->mg.r2,
								     lwrve_BE);
			CCC_ERROR_CHECK(ret,
					CCC_ERROR_MESSAGE("EC lwrve %s configured montgomery values error\n",
							  econtext->ec_lwrve->name));

			/* Flag also as constant values from lwrve parameters */
			pka1data->mont_flags |= PKA1_MONT_FLAG_CONST_VALUES;

		} else {
			LTRACEF("EC montgomery callwlated for lwrve %s\n",
				econtext->ec_lwrve->name);

			ret = pka1_montgomery_values_calc(econtext->cmem,
							  econtext->engine,
							  econtext->ec_lwrve->nbytes,
							  econtext->ec_lwrve->p,
							  pka1data,
							  lwrve_BE);
			CCC_ERROR_CHECK(ret,
					CCC_ERROR_MESSAGE("EC callwlating montgomery values failed: %d\n",
							  ret));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set the EC key either to SE keyslot or directly to PKA registers
 */
static status_t pka1_ec_setkey(struct se_engine_ec_context *econtext, bool use_keyslot, bool set_key_value)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_ec_set_keymat(econtext, use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to set EC key material: %d\n",
					  ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_ec_init(struct se_engine_ec_context *econtext,
			     enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->ec_lwrve)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != econtext->pka1_data) {
		econtext->pka1_data->pka1_op	= pka1_op;
		econtext->pka1_data->op_mode    = econtext->ec_lwrve->mode;
	} else {
		/* Binds a dynamic objects to econtext >= these must be
		 * release by EC operation reset later.
		 *
		 * Setup lwrve values to pka1_data constant values
		 * which are independent of the performed operation
		 * (e.g. montgomery values for the EC lwrve)
		 */

		// TODO: Maybe make this part of e.g. the CONTEXT instead and refer from there
		// (would add a few words for SIG and DERIVE contexts...)
		econtext->pka1_data =
			CMTAG_MEM_GET_OBJECT(econtext->cmem,
					     CMTAG_PKA_DATA,
					     0U,
					     struct pka1_engine_data,
					     struct pka1_engine_data *);
		if (NULL == econtext->pka1_data) {
			CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
		}

		econtext->pka1_data->pka1_op = pka1_op;
		econtext->pka1_data->op_mode = econtext->ec_lwrve->mode;

		if ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_NO_MONTGOMERY) == 0U) {
			/* mont values are constants for each lwrve */
			ret = pka1_ec_setup_montgomery(econtext);
			if (NO_ERROR != ret) {
				PKA_DATA_EC_RELEASE(econtext);
				CCC_ERROR_MESSAGE("EC montgomery residue constant callwlation failed: %d\n",
						  ret);
			}
			CCC_ERROR_CHECK(ret);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void pka1_data_ec_release(struct se_engine_ec_context *econtext)
{
	LTRACEF("entry\n");

	if (NULL != econtext) {
		if (NULL != econtext->pka1_data) {
			pka1_data_release(econtext->cmem, econtext->pka1_data);

			/* For EC this field is dynamic, because it persists
			 * across multiple EC primitive operations.
			 */
			CMTAG_MEM_RELEASE(econtext->cmem,
					  CMTAG_PKA_DATA,
					  econtext->pka1_data);
		}
	}

	LTRACEF("exit\n");
}

#if HAVE_PKA1_BLINDING
/* Setup blinding (set F0) and write the "w" value (random)
 * to A7 for point multiply to prevent various attacks on the private key.
 * Also sets the "Index L" register to ceiling(log2(OBP)).
 *
 * This is a SECURITY ISSUE which makes the operation slower
 * but must not be omitted.
 *
 * Uses PKA1 TRNG for random w value (0 < w < p)
 *
 * Caller needs to write the *FLAG_VAL_P to flag register, into which this
 * function XORs F0 bit (enable blinding).
 */
static status_t pka1_set_blinding_ec_reg(const struct se_engine_ec_context *econtext,
					 uint32_t *flag_val_p)
{
	status_t ret = NO_ERROR;
	uint32_t nbytes = 0U;
	uint8_t value_w[TE_MAX_ECC_BYTES];
	bool lwrve_LE = false;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->pka1_data) || (NULL == flag_val_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	nbytes = econtext->ec_lwrve->nbytes;

	lwrve_LE = ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U);

	ret = pka_ec_generate_smaller_vec_BE_locked(econtext->engine,
						    econtext->ec_lwrve->p,
						    value_w, nbytes,
						    lwrve_LE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to gen w\n"));

	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_A, R7,
				       value_w,
				       (nbytes / BYTES_IN_WORD),
				       CTRUE, (nbytes / BYTES_IN_WORD));
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to set w\n"));

	/* Should be the "number of bits present in the Order-of-the-Base-Point (OBP)"
	 * L_value = ceiling(log2(OBP))
	 *
	 * AFAIK: same as operation size for NIST-P lwrves and Brainpool lwrves.
	 * Elliptic doc says that it is one bit larger for some lwrves => TODO => VERIFY!!!
	 * (e.g. secp160r1 lwrve; but that is not supported by ccc (not requested...)).
	 *
	 * For ED25519 and C25519 the elliptic doc does not state that "index L" should
	 * be set => don't set it.
	 */
	if ((econtext->ec_lwrve->id != TE_LWRVE_C25519) &&
	    (econtext->ec_lwrve->id != TE_LWRVE_ED25519)) {
		uint32_t L_value = nbytes * 8U;
		tegra_engine_set_reg(econtext->engine, SE_PKA1_INDEX_L_OFFSET, L_value);
	}

	*flag_val_p |= SE_PKA1_FLAGS_FLAG_F0(SE_ELP_ENABLE);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_BLINDING */

static status_t pka1_ec_op_exit(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* No per operation secrets in e.g. econtext->pka1_data
	 * (no need to clear anything here)
	 *
	 * econtext->pka1_data lwrve specific data is released in EC operation
	 * reset code, since we may perform multiple operations
	 * with the same lwrve.
	 */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_ec_set_op_flags(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;
	enum pka1_engine_ops eop;
	uint32_t flag_val = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	eop = econtext->pka1_data->pka1_op;

	/* Set a==-3 optimization flag (F3) for supporting operations if
	 * lwrve parameter declares this
	 */
	switch(eop) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_POINT_DOUBLE:
	case PKA1_OP_ED25519_SHAMIR_TRICK:
	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_C25519_POINT_MULTIPLY:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
#if HAVE_PKA1_BLINDING
		ret = pka1_set_blinding_ec_reg(econtext, &flag_val);
		if (NO_ERROR != ret) {
			break;
		}
#endif
		break;

	case PKA1_OP_EC_SHAMIR_TRICK:
		if ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_A_IS_M3) != 0U) {
			flag_val |= SE_PKA1_FLAGS_FLAG_F3(SE_ELP_ENABLE);
		}
		break;

	default:
		/* No action required */
		break;
	}
	CCC_ERROR_CHECK(ret);

#if HAVE_ELLIPTIC_20 == 0U
	/* Set Enable 521 flag (F1) if using that lwrve (for supporting ops)
	 *
	 * Elliptic FW 2.0 has dedicated P521 operations, it does not
	 * use the F1 flag for this purpose.
	 */
	if (PKA1_OP_MODE_ECC521 == econtext->ec_lwrve->mode) {
		switch(eop) {
#if HAVE_PKA1_ISM3
		case PKA1_OP_EC_ISM3:
			break;
#endif /* HAVE_PKA1_ISM3 */
		case PKA1_OP_EC_POINT_MULTIPLY:
		case PKA1_OP_EC_POINT_ADD:
		case PKA1_OP_EC_POINT_DOUBLE:
		case PKA1_OP_EC_SHAMIR_TRICK:
		case PKA1_OP_EC_POINT_VERIFY:
			flag_val |= SE_PKA1_FLAGS_FLAG_F1(SE_ELP_ENABLE);
			break;

		default:
			/* No action required */
			break;
		}
	}
#endif

	tegra_engine_set_reg(econtext->engine, SE_PKA1_FLAGS_OFFSET, flag_val);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_process_boolean_result(const struct se_engine_ec_context *econtext,
					  enum pka1_engine_ops pka1_op,
					  status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	bool result = false;

	LTRACEF("entry\n");

	(void)pka1_op;

	if (NULL == rbad_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*rbad_p = ERR_BAD_STATE;

	ret = pka1_ec_get_boolean_result(econtext, &result);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to get boolean result for op %u (%d)\n",
				     pka1_op, ret));

	LTRACEF("XXX operation %u result: %s\n", pka1_op, (BOOL_IS_TRUE(result) ? "true" : "false"));
	if (!BOOL_IS_TRUE(result)) {
		ret = SE_ERROR(ERR_NOT_VALID);
		LTRACEF("Point not on lwrve\n");
		/* FALLTHROUGH */
	}

	*rbad_p = NO_ERROR;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_process_point_result(struct se_data_params *input_params,
					const struct se_engine_ec_context *econtext,
					enum pka1_engine_ops pka1_op,
					status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	bool result_BE = true;

	LTRACEF("entry\n");

	(void)pka1_op;

	if (NULL == rbad_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*rbad_p = ERR_BAD_STATE;

	/* The result point is of same byte order as source point; BE is default.
	 */
	if (NULL != input_params->point_P) {
		result_BE = ((input_params->point_P->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);
	}
	ret = pka1_ec_get_point_result(econtext, input_params->point_Q, result_BE);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to get boolean result for op %u (%d)\n",
				     pka1_op, ret));

	*rbad_p = NO_ERROR;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
static status_t pka1_ec_get_result(struct se_data_params *input_params,
				   struct se_engine_ec_context *econtext,
				   enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (pka1_op) {
#if HAVE_PKA1_ISM3
	case PKA1_OP_EC_ISM3:
#endif /* HAVE_PKA1_ISM3 */
	case PKA1_OP_EC_POINT_VERIFY:
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_VERIFY:
	case PKA1_OP_ED25519_POINT_VERIFY:
#endif
		ret = ec_process_boolean_result(econtext, pka1_op, &rbad);
		break;

#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_ADD:
	case PKA1_OP_EC_P521_POINT_DOUBLE:
	case PKA1_OP_EC_P521_SHAMIR_TRICK:

	case PKA1_OP_C25519_POINT_MULTIPLY:

	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_POINT_DOUBLE:
	case PKA1_OP_ED25519_SHAMIR_TRICK:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
	case PKA1_OP_EC_POINT_ADD:
	case PKA1_OP_EC_POINT_DOUBLE:
	case PKA1_OP_EC_SHAMIR_TRICK:
		ret = ec_process_point_result(input_params, econtext,
					      pka1_op, &rbad);
		break;

	default:
		LTRACEF("unsupported operation: %u\n", pka1_op);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_load_two_point_operands(const struct se_data_params *input_params,
					   struct se_engine_ec_context *econtext,
					   enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)pka1_op;

	if (NULL == input_params->point_Q) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_ec_load_point_operand(econtext,
					 POINT_ID_Q,
					 input_params->point_Q);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to load point Q for op %u (%d)\n",
				pka1_op, ret));

	ret = pka1_ec_load_point_operand(econtext,
					 POINT_ID_P,
					 input_params->point_P);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to load point P for op %u (%d)\n",
				     pka1_op, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_ec_set_point_operands(const struct se_data_params *input_params,
					   struct se_engine_ec_context *econtext,
					   enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params) ||
	    (NULL == input_params->point_P)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (pka1_op) {
#if HAVE_PKA1_ISM3
	case PKA1_OP_EC_ISM3:
		LTRACEF("IS_M3 does not use point operands\n");
		break;
#endif /* HAVE_PKA1_ISM3 */
		/* These use two points (P and Q) */
#if HAVE_ELLIPTIC_20
	case PKA1_OP_ED25519_POINT_ADD:
	case PKA1_OP_ED25519_SHAMIR_TRICK:

	case PKA1_OP_EC_P521_SHAMIR_TRICK:
	case PKA1_OP_EC_P521_POINT_ADD:
#endif
	case PKA1_OP_EC_POINT_ADD:
	case PKA1_OP_EC_SHAMIR_TRICK:
		ret = ec_load_two_point_operands(input_params, econtext,
						 pka1_op);
		break;

		/* Load point P for these ops */
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_VERIFY:

	case PKA1_OP_C25519_POINT_MULTIPLY:

	case PKA1_OP_ED25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_VERIFY:
	case PKA1_OP_ED25519_POINT_DOUBLE:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
	case PKA1_OP_EC_POINT_VERIFY:
	case PKA1_OP_EC_POINT_DOUBLE:
		ret = pka1_ec_load_point_operand(econtext,
						 POINT_ID_P,
						 input_params->point_P);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("Failed to load point P for op %u (%d)\n", pka1_op, ret);
			break;
		}
		break;

	default:
		LTRACEF("unsupported operation: %u\n", pka1_op);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Only EC point multiply operations can use the scalar
 * value in PKA HW keyslots.
 *
 * All other operations (and all other values for multiply)
 *  are loaded via PKA bank regs as before.
 */
static status_t pka1_ec_engine_op_get_keyslot_settings(
	const struct se_engine_ec_context *econtext,
	enum pka1_engine_ops pka1_op,
	bool *use_keyslot_p,
	bool *set_key_value_p)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false;
	bool set_key_value = true;

	LTRACEF("entry\n");

	switch (pka1_op) {
#if HAVE_ELLIPTIC_20
	case PKA1_OP_EC_P521_POINT_MULTIPLY:
	case PKA1_OP_EC_P521_POINT_VERIFY:
	case PKA1_OP_C25519_POINT_MULTIPLY:
	case PKA1_OP_ED25519_POINT_MULTIPLY:
#endif
	case PKA1_OP_EC_POINT_MULTIPLY:
#if HAVE_PKA1_LOAD

		if ((econtext->ec_flags & CCC_EC_FLAGS_PKEY_MULTIPLY) != 0U) {
			/* This multiply is using "private key" scalar value */

			if ((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSLOT) != 0U) {
				LTRACEF("PKA1 using EC keyslots for multiply\n");
				use_keyslot = true;
			}

			if ((econtext->ec_flags & CCC_EC_FLAGS_USE_PRESET_KEY) != 0U) {
				LTRACEF("PKA1 EC using existing keyslot key %u\n",
					econtext->ec_keyslot);
				set_key_value = false;
				use_keyslot = true;
			}

			if (BOOL_IS_TRUE(use_keyslot)) {
				if (econtext->ec_keyslot >= PKA1_MAX_KEYSLOTS) {
					LTRACEF("Invalid PKA1 keyslot number %u\n",
						econtext->ec_keyslot);
					ret = SE_ERROR(ERR_ILWALID_ARGS);
					break;
				}
			}
		}
#else
		if (((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSLOT) != 0U) ||
		    ((econtext->ec_flags & CCC_EC_FLAGS_USE_PRESET_KEY) != 0U)) {
			CCC_ERROR_MESSAGE("PKA EC HW keyslots not supported\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
#endif /* HAVE_PKA1_LOAD */
		break;

	default:
		/* No action required */
		break;
	}
	CCC_ERROR_CHECK(ret);

	*use_keyslot_p = use_keyslot;
	*set_key_value_p = set_key_value;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_ec_engine_op_check_args(
	const struct se_data_params *input_params,
	const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(econtext->ec_lwrve->nbytes))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

fail:
	return ret;
}

static status_t pka1_ec_engine_do_operation(
	struct se_data_params *input_params,
	struct se_engine_ec_context *econtext,
	enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = pka1_ec_run_program(econtext);
	CCC_ERROR_CHECK(ret);

	ret = pka1_wait_op_done(econtext->engine);
	CCC_ERROR_CHECK(ret);

	ret = pka1_ec_get_result(input_params, econtext, pka1_op);
	CCC_ERROR_CHECK(ret);

	ret = pka1_ec_op_exit(econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC exit failed %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
static void ec_clean_keyslot(const struct se_engine_ec_context *econtext,
			     bool use_keyslot)
{
	if (BOOL_IS_TRUE(use_keyslot)) {
		uint32_t ec_size = 0U;
		if (NULL != econtext->ec_lwrve) {
			ec_size = econtext->ec_lwrve->nbytes;
		}

		/* CD#5 update 1 behavior: clear key unless told otherwise */
		if ((econtext->ec_flags & CCC_EC_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U) {
			status_t fret = ec_clear_ec_keyslot_locked(econtext->engine,
								   econtext->ec_keyslot,
								   ec_size);
			if (NO_ERROR != fret) {
				LTRACEF("Failed to clear PKA1 EC keyslot %u\n",
					econtext->ec_keyslot);
				/* FALLTHROUGH */
			}
		}
	}
}
#endif /* HAVE_PKA1_LOAD */

static status_t pka1_ec_keyslot_load(const struct se_engine_ec_context *econtext,
				     bool use_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(use_keyslot)) {
#if HAVE_PKA1_LOAD
		ret = pka1_keyslot_load(econtext->engine, econtext->ec_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("EC keyslot load setup failed %d\n", ret));
#else
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA EC HW keyslots not supported\n"));
#endif /* HAVE_PKA1_LOAD */
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t pka1_ec_engine_op(struct se_data_params *input_params,
			   struct se_engine_ec_context *econtext,
			   enum pka1_engine_ops pka1_op)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false;
	bool set_key_value = true; /* ignored unless use_keyslot */

	LTRACEF("entry: opcode %u\n", pka1_op);

	ret = pka1_ec_engine_op_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	DUMP_DATA_PARAMS("EC input", 0x1U, input_params);

	/* Use PKA1 bank registers by default and with this option
	 * (CCC_EC_FLAGS_USE_KEYSLOT overrides this!)
	 */
	if ((econtext->ec_flags & CCC_EC_FLAGS_DYNAMIC_KEYSLOT) != 0U) {
		LTRACEF("PKA1 EC loading keys via bank registers\n");
		use_keyslot = false;
	}

	ret = pka1_ec_init(econtext, pka1_op);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC init failed %d\n", ret));

	/* Only EC point multiply operations can use the scalar
	 * in PKA HW keyslots.
	 *
	 * All other operations use PKA bank regs as before.
	 */
	ret = pka1_ec_engine_op_get_keyslot_settings(econtext,
						     pka1_op,
						     &use_keyslot,
						     &set_key_value);
	CCC_ERROR_CHECK(ret);

	/* Set PKA1 key unless using existing preset key already in SE HW keyslot
	 * Even when using keyslot for "key" in point mult ops, numerous other
	 * values need to be set to bank registers for EC operations (always).
	 */
	ret = pka1_ec_setkey(econtext, use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC setkey failed %d\n", ret));

	ret = pka1_ec_keyslot_load(econtext, use_keyslot);
	CCC_ERROR_CHECK(ret);

	ret = pka1_ec_set_op_flags(econtext);
	CCC_ERROR_CHECK(ret);

#if HAVE_PKA1_ISM3
	if (pka1_op != PKA1_OP_EC_ISM3) {
		ret = pka1_ec_set_point_operands(input_params, econtext, pka1_op);
		CCC_ERROR_CHECK(ret);
	}
#else
	ret = pka1_ec_set_point_operands(input_params, econtext, pka1_op);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_PKA1_ISM3 */

	ret = pka1_ec_engine_do_operation(input_params, econtext, pka1_op);
	CCC_ERROR_CHECK(ret);

fail:
#if HAVE_PKA1_LOAD
	ec_clean_keyslot(econtext, use_keyslot);
	/* Failure on final key cleanup does not trigger operation failure here. */
#endif /* HAVE_PKA1_LOAD */

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Here are the exported function from the PKA1 EC engine operations.
 * Everything in this file is called with the PKA mutex locked.
 *
 * These functions are for the Weierstrass EC lwrves (NIST P lwrves, Brainpool, etc).
 */

/* EC point modular multiplication with scalar K set by caller to EC_K (EC_K_LENGTH)
 *
 * Q = k x P
 */
status_t engine_pka1_ec_point_mult_locked(struct se_data_params *input_params,
					  struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_EC_POINT_MULTIPLY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_ECC_POINT_ADD
/* EC point modular addition: Q = P + Q */
status_t engine_pka1_ec_point_add_locked(struct se_data_params *input_params,
					 struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_EC_POINT_ADD);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ECC_POINT_ADD */

#if HAVE_PKA1_ECC_POINT_DOUBLE
/* EC point double: Q = P + P */
status_t engine_pka1_ec_point_double_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_EC_POINT_DOUBLE);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ECC_POINT_DOUBLE */

/* Verify that a point Pxy (X: src[0..ec_size-1] Y:src[ec_size..2*ec_size-1])
 * is on the lwrve.
 *
 * Is P on lwrve?
 */
status_t engine_pka1_ec_point_verify_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_EC_POINT_VERIFY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* EC_K (EC_K_LENGTH) and EC_L (EC_L_LENGTH) must be set by caller
 *
 * Q = (k x P) + (l x Q)
 */
status_t engine_pka1_ec_shamir_trick_locked(struct se_data_params *input_params,
					    struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = pka1_ec_engine_op(input_params, econtext, PKA1_OP_EC_SHAMIR_TRICK);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ECC */
