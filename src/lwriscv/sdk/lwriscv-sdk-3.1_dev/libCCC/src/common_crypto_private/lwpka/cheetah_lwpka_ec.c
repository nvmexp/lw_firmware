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

#if HAVE_LWPKA_ECC

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

static status_t lwpka_ec_load_point_args_check(const struct se_engine_ec_context *econtext,
					      const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) ||
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
					enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	uint32_t field_word_size = 0U;
	bool src_point_BE = false;

	LTRACEF("entry\n");

	ec_size_words        = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;

	src_point_BE = ((point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);

	switch (eop) {
	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
	case LWPKA_OP_EC_SHAMIR_TRICK:
		LTRACEF("Reg writing %u word 'Qx'\n", ec_size_words);
		ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
					   LOR_S2,
					   point->x,
					   ec_size_words, src_point_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC 'Qx' reg LOR_S2\n");
			break;
		}

		LTRACEF("Reg writing %u word 'Qy'\n", ec_size_words);
		ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
					   LOR_S3,
					   point->y,
					   ec_size_words, src_point_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC 'Qy' LOR_S3\n");
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

/* Load Px and Py; load point to <LOR_S0,LOR_S1>
 */
static status_t load_point_operand_id_P(const struct se_engine_ec_context *econtext,
					const te_ec_point_t *point,
					enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	uint32_t field_word_size = 0U;
	bool src_point_BE = false;

	LTRACEF("entry\n");

	ec_size_words        = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;

	src_point_BE = ((point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);

	switch (eop) {
	case LWPKA_OP_EC_POINT_VERIFY:
	case LWPKA_OP_EC_POINT_MULTIPLY:
	case LWPKA_OP_EC_SHAMIR_TRICK:

	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:

	case LWPKA_OP_EDWARDS_POINT_VERIFY:
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
		break;

	default:
		LTRACEF("Point P not used as input for EC op %u\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	LTRACEF("Reg writing %u word 'Px'\n", ec_size_words);
	ret = lwpka_register_write(econtext->engine,
				   econtext->pka_data.op_mode,
				   LOR_S0,
				   point->x,
				   ec_size_words, src_point_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC 'Px' to LOR_S0\n"));

	/* Only X-coordinate is loaded for montgomery lwrve point
	 * mult operation
	 */
	if (LWPKA_OP_MONTGOMERY_POINT_MULTIPLY != eop) {
		LTRACEF("Reg writing %u word 'Py'\n", ec_size_words);
		ret = lwpka_register_write(econtext->engine,
					   econtext->pka_data.op_mode,
					   LOR_S1,
					   point->y,
					   ec_size_words, src_point_BE,
					   field_word_size);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to write EC 'Py' to LOR_S1\n"));
	}

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

/*
 * This is setting point_id point to operation specific registers
 *
 * If (POINT_ID == POINT_ID_P) => point is P and uses LOR_S<0,1>
 * If (POINT_ID == POINT_ID_Q) => point is Q and uses LOR_S<2,3>
 *
 * Note that only SHAMIR'S TRICK use two (P and Q) point operands;
 * the other ops use just P
 */
static status_t lwpka_ec_load_point_operand(const struct se_engine_ec_context *econtext,
					    enum point_id_t point_id,
					    const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;

	LTRACEF("entry\n");

	ret = lwpka_ec_load_point_args_check(econtext, point);
	CCC_ERROR_CHECK(ret);

	eop = econtext->pka_data.lwpka_op;

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

static status_t lwpka_ec_get_boolean_pv_result(const struct se_engine_ec_context *econtext,
					       bool *result_p)
{
	status_t ret = NO_ERROR;
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == result_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*result_p = false;

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	eop = econtext->pka_data.lwpka_op;

	switch (eop) {
	case LWPKA_OP_EDWARDS_POINT_VERIFY:
	case LWPKA_OP_EC_POINT_VERIFY:
		/* CORE_STATUS register contains the IS_VALID_POINT bit
		 * result for point on lwrve checks.
		 */
		val = tegra_engine_get_reg(econtext->engine, LW_SE_LWPKA_CORE_STATUS_0);

		LTRACEF("BOOLEAN result masked from core status: 0x%x\n", val);

		*result_p = (LW_DRF_VAL(LW_SE_LWPKA, CORE_STATUS, IS_VALID_POINT, val) ==
			     LW_SE_LWPKA_CORE_STATUS_0_IS_VALID_POINT_TRUE);
		break;

	default:
		LTRACEF("operation %u does not return a boolean result\n", eop);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

/* Reads a POINT RESULT from operations that return a point
 */
static status_t lwpka_ec_get_point_result(const struct se_engine_ec_context *econtext,
					  te_ec_point_t *point,
					  bool result_BE,
					  enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == point)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}

	ec_size_words = econtext->ec_lwrve->nbytes / BYTES_IN_WORD;

	point->point_flags = CCC_EC_POINT_FLAG_NONE;
	if (!BOOL_IS_TRUE(result_BE)) {
		point->point_flags |= CCC_EC_POINT_FLAG_LITTLE_ENDIAN;
	}

	LTRACEF("Reg reading %u word 'Rx (LOR_D0)'\n", ec_size_words);
	ret = lwpka_register_read(econtext->engine, econtext->pka_data.op_mode,
				  LOR_D0,
				  point->x,
				  ec_size_words, result_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to read EC 'Rx' from LOR_D0\n"));

	if (LWPKA_OP_MONTGOMERY_POINT_MULTIPLY != eop) {
		LTRACEF("Reg reading %u word 'Ry (LOR_D1)'\n", ec_size_words);
		ret = lwpka_register_read(econtext->engine, econtext->pka_data.op_mode,
					  LOR_D1,
					  point->y,
					  ec_size_words, result_BE);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to read EC 'Ry' from LOR_D1\n"));
	}

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static inline uint32_t lwpka_ec_scena(const struct se_engine_ec_context *econtext)
{
	return (((econtext->ec_flags & (CCC_EC_FLAGS_PKEY_MULTIPLY | CCC_EC_FLAGS_USE_PRIVATE_KEY)) == 0U) ?
		CCC_SC_DISABLE : CCC_SC_ENABLE);
}

static status_t lwpka_ec_map_primitive(const struct se_engine_ec_context *econtext,
				       enum lwpka_primitive_e *primitive_p,
				       uint32_t *scena_p)
{
	status_t ret = NO_ERROR;
	enum lwpka_primitive_e primitive = LWPKA_PRIMITIVE_ILWALID;
	uint32_t scena = CCC_SC_ENABLE;

	LTRACEF("entry\n");

	switch (econtext->pka_data.lwpka_op) {
	case LWPKA_OP_EC_POINT_MULTIPLY:
		primitive = LWPKA_PRIMITIVE_EC_POINT_MULT;
		scena = lwpka_ec_scena(econtext);
		break;

	case LWPKA_OP_EC_POINT_VERIFY:
		primitive = LWPKA_PRIMITIVE_EC_ECPV;
		scena = CCC_SC_DISABLE;
		break;

	case LWPKA_OP_EC_SHAMIR_TRICK:
		primitive = LWPKA_PRIMITIVE_EC_SHAMIR_TRICK;
		scena = CCC_SC_DISABLE;
		break;

	case LWPKA_OP_GEN_MULTIPLY:
		/* non-modular multiplication: c=a*b
		 * with double precision result.
		 *
		 * Expectation is terms could be up to
		 * 2048 bit and result 4096 bit
		 * (HW improved).
		 */
		primitive = LWPKA_PRIMITIVE_GEN_MULTIPLICATION;
		scena = CCC_SC_DISABLE;
		break;

#if HAVE_LWPKA_X25519
	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:
		primitive = LWPKA_PRIMITIVE_MONTGOMERY_POINT_MULT;
		scena = lwpka_ec_scena(econtext);
		break;
#endif /* HAVE_LWPKA_X25519 */

#if CCC_WITH_EDWARDS
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
		primitive = LWPKA_PRIMITIVE_EDWARDS_POINT_MULT;
		scena = lwpka_ec_scena(econtext);
		break;

	case LWPKA_OP_EDWARDS_POINT_VERIFY:
		primitive = LWPKA_PRIMITIVE_EDWARDS_ECPV;
		scena = CCC_SC_DISABLE;
		break;

	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
		primitive = LWPKA_PRIMITIVE_EDWARDS_SHAMIR_TRICK;
		scena = CCC_SC_DISABLE;
		break;
#endif /* CCC_WITH_EDWARDS */

	default:
		CCC_ERROR_MESSAGE("Unknown EC engine operation: %u",
				  econtext->pka_data.lwpka_op);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*primitive_p = primitive;
	*scena_p     = scena;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_run_primitive(const struct se_engine_ec_context *econtext,
				       bool use_keyslot)
{
	status_t ret = NO_ERROR;
	enum lwpka_primitive_e primitive = LWPKA_PRIMITIVE_ILWALID;
	uint32_t keyslot = LWPKA_MAX_KEYSLOTS;
	uint32_t scena = CCC_SC_ENABLE;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == econtext->ec_lwrve)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(use_keyslot)) {
		keyslot = econtext->ec_keyslot;
	}

	ret = lwpka_ec_map_primitive(econtext, &primitive, &scena);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_go_start_keyslot(econtext->engine, econtext->pka_data.op_mode,
				     primitive, use_keyslot, keyslot,
				     scena);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC go start returned: %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA_LOAD
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

	if ((NULL == engine) || (pka_keyslot >= LWPKA_MAX_KEYSLOTS)) {
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

	LTRACEF("Clearing EC keyslot %u\n", pka_keyslot);

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
		tret = lwpka_set_keyslot_field(engine, field,
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

/* Note: even though the EC keyslot contains a lot of fields
 * only the field "k" is used lwrrently, because only POINT MULTIPLICATION
 * operations trigger LOAD_KEY in HW.
 *
 * ==> Do not spend time writing the other fields, they are unused.
 *
 * This always writes scalar to keyslot in LITTLE ENDIAN zero padding
 * to next pow(2) size
 */
static status_t lwpka_write_scalar_to_keyslot_locked(const struct se_engine_ec_context *econtext,
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
	ret = lwpka_set_keyslot_field(econtext->engine, KMAT_EC_KEY,
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
#endif /* HAVE_LWPKA_LOAD */

static status_t lwpka_ec_set_keymat_check_args(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve undefined\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_set_keymat_weierstrass(const struct se_engine_ec_context *econtext,
						const pka1_ec_lwrve_t *lwrve,
						enum lwpka_engine_ops eop,
						uint32_t ec_size_words,
						uint32_t field_word_size,
						bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Load A => LOR_C1 */
	LTRACEF("Reg writing %u word lwrve param 'a' to LOR_C1\n", ec_size_words);
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_C1,
				   lwrve->a,
				   ec_size_words, lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC param 'a' to LOR_C1\n"));

	switch (eop) {
	case LWPKA_OP_EC_SHAMIR_TRICK:
	case LWPKA_OP_EC_POINT_MULTIPLY:
	case LWPKA_OP_EC_POINT_VERIFY:
		/* Load B => LOR_C2 */
		LTRACEF("Reg writing %u word lwrve param 'b' to LOR_C2\n", ec_size_words);
		ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
					   LOR_C2,
					   lwrve->b,
					   ec_size_words, lwrve_BE, field_word_size);
		if (NO_ERROR != ret) {
			LTRACEF("Failed to write EC param 'b' to LOR_C2\n");
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

#if HAVE_LWPKA_X25519
/* Montgomery lwrve lwrve25519 lwrrently only supports one operation:
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
static status_t lwpka_ec_set_keymat_montgomery(const struct se_engine_ec_context *econtext,
					       const pka1_ec_lwrve_t *lwrve,
					       enum lwpka_engine_ops eop,
					       uint32_t ec_size_words,
					       uint32_t field_word_size,
					       bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	/* This is montgomery lwrve25519 value, lwrve448 has 156326
	 *
	 * FIXME: Support both!!!!
	 */
	uint32_t a = 486662U;

	(void)lwrve;
	(void)ec_size_words;
	(void)lwrve_BE;

	LTRACEF("entry\n");

	if (LWPKA_OP_MONTGOMERY_POINT_MULTIPLY == eop) {
		LTRACEF("Reg writing 1 word k_param-value\n");

		ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
					   LOR_C1,
					   (const uint8_t *)&a, 1U,
					   CFALSE, field_word_size);
		CCC_ERROR_CHECK(ret, LTRACEF("Failed to write k_param-value reg LOR_C1\n"));
	} else {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS, LTRACEF("Unsupported C25519 operation: %u\n", eop));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_X25519 */

#if CCC_WITH_EDWARDS
static status_t lwpka_ec_set_keymat_edwards(const struct se_engine_ec_context *econtext,
					    const pka1_ec_lwrve_t *lwrve,
					    enum lwpka_engine_ops eop,
					    uint32_t ec_size_words,
					    uint32_t field_word_size,
					    bool lwrve_BE)
{
	status_t ret = NO_ERROR;

	// TODO:_ Support ed448 goldilocks

	(void)eop; /* XXX TODO: check eop? */

	LTRACEF("entry\n");

	LTRACEF("Reg writing %u words of A to LOR_C1\n", ec_size_words);

	// Note: lwrve->a now contains value A (-1 mod p for ED25519)
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_C1,
				   lwrve->a,
				   ec_size_words,
				   lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write A to LOR_C1\n"));

	/* write d to LOR_C2 from field lwrve->d
	 */
	LTRACEF("Reg writing %u value d to LOR_C2\n", ec_size_words);

	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_C2,
				   lwrve->d,
				   ec_size_words,
				   lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write d-value PKA reg LOR_C2\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_EDWARDS */

/*
 * R = kP + lQ
 *
 * Change: PAD both K and L values to next pow2 size.
 * XXX Not sure if required, but for other op key values it
 * XXX  is mandatory? ==> VERIFY => TODO
 *
 * FIXME: does not work if L or K size is not word multiple!
 * (XXX could make a local copy and pad if not word multiple...)
 */
static status_t lwpka_ec_set_keymat_setup_shamir_trick(const struct se_engine_ec_context *econtext,
						       const pka1_ec_lwrve_t *lwrve,
						       enum lwpka_engine_ops eop,
						       uint32_t pow2_field_word_size)
{
	status_t ret = NO_ERROR;
	bool scalar_l_BE = false;
	bool scalar_k_BE = false;

	(void)eop;

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

	/* Load multiplier for point Q
	 */
	LTRACEF("Reg writing %u word l to LOR_K1\n", econtext->ec_l_length / BYTES_IN_WORD);
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_K1,
				   econtext->ec_l,
				   (econtext->ec_l_length / BYTES_IN_WORD),
				   scalar_l_BE, pow2_field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC l to LOR_K1\n"));

	/* Load multiplier for point P
	 */
	LTRACEF("Reg writing %u word k to LOR_K0\n", econtext->ec_k_length / BYTES_IN_WORD);
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_K0,
				   econtext->ec_k,
				   (econtext->ec_k_length / BYTES_IN_WORD),
				   scalar_k_BE, pow2_field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC k PKA reg LOR_K0\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* K (value must be 0 < k < order_of_base_point_of_lwrve)
 * XXXX TODO => need to verify this (by caller)
 *
 * Load k => LOR_K0, zero padding.
 *
 * FIXME: does not work if K size is not word multiple!!!
 * (XXX make a local copy if not word multiple...)
 */
static status_t lwpka_ec_set_keymat_setup_multiply(const struct se_engine_ec_context *econtext,
						   const pka1_ec_lwrve_t *lwrve,
						   enum lwpka_engine_ops eop,
						   uint32_t pow2_field_word_size,
						   bool use_keyslot,
						   bool set_key_value)
{
	status_t ret = NO_ERROR;
	bool scalar_k_BE = false;

	(void)eop;
	(void)set_key_value; /* not used unless HAVE_LWPKA_LOAD */

	LTRACEF("entry\n");

	/* field ec_k endian? */
	scalar_k_BE = ((econtext->ec_flags & CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN) == 0U);

	if (BOOL_IS_TRUE(use_keyslot)) {
#if HAVE_LWPKA_LOAD
		if (BOOL_IS_TRUE(set_key_value)) {
			ret = lwpka_write_scalar_to_keyslot_locked(econtext,
								   econtext->ec_k,
								   econtext->ec_k_length,
								   !scalar_k_BE);
			CCC_ERROR_CHECK(ret,
					LTRACEF("EC kslot k write failed %d\n", ret));
		}
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("EC keyslot loading not supported\n"));
#endif /* HAVE_LWPKA_LOAD */
	} else {
		if (econtext->ec_k_length > lwrve->nbytes) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("scalar K too long\n"));
		}

		// XXX FIXME: Check key range: in LWPKA must be 0 < k < OBP

		LTRACEF("Reg writing %u word k\n", econtext->ec_k_length / BYTES_IN_WORD);
		ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
					   LOR_K0,
					   econtext->ec_k,
					   (econtext->ec_k_length / BYTES_IN_WORD),
					   scalar_k_BE, pow2_field_word_size);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to write EC k PKA reg LOR_K0\n"));
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
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;
	uint32_t ec_size_words = lwrve->nbytes / BYTES_IN_WORD;
	bool lwrve_BE = ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) == 0U);

	LTRACEF("entry\n");

	eop = econtext->pka_data.lwpka_op;

	LTRACEF("Loading EC parameters to PKA LOR\n");

	/* First set lwrve specific values to LOR registers
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
		ret = lwpka_ec_set_keymat_weierstrass(econtext,
						      lwrve,
						      eop,
						      ec_size_words,
						      field_word_size,
						      lwrve_BE);
		break;

#if HAVE_LWPKA_X25519
	case PKA1_LWRVE_TYPE_LWRVE25519:
		ret = lwpka_ec_set_keymat_montgomery(econtext,
						     lwrve,
						     eop,
						     ec_size_words,
						     field_word_size,
						     lwrve_BE);
		break;
#endif /* HAVE_LWPKA_X25519 */

#if CCC_WITH_EDWARDS
	case PKA1_LWRVE_TYPE_ED25519:
		ret = lwpka_ec_set_keymat_edwards(econtext,
						  lwrve,
						  eop,
						  ec_size_words,
						  field_word_size,
						  lwrve_BE);
		break;
#endif /* CCC_WITH_EDWARDS */

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
					      enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (eop) {
	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
	case LWPKA_OP_EC_SHAMIR_TRICK:
		ret = lwpka_ec_set_keymat_setup_shamir_trick(econtext,
							     lwrve,
							     eop,
							     pow2_field_word_size);
		break;

	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
	case LWPKA_OP_EC_POINT_MULTIPLY:
		/*
		 * Set ec_k && ec_k_length correctly before calling point multiply locked!
		 * (ec_k is the scalar value to multiply the point with)
		 */
		ret = lwpka_ec_set_keymat_setup_multiply(econtext,
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

static status_t lwpka_ec_set_keymat(const struct se_engine_ec_context *econtext,
				    bool use_keyslot, bool set_key_value)
{
	status_t ret = NO_ERROR;
	uint32_t ec_size_words = 0U;
	bool lwrve_BE = false;
	uint32_t field_word_size = 0U;
	uint32_t pow2_field_word_size = 0U;
	enum lwpka_engine_ops eop = LWPKA_OP_ILWALID;
	const pka1_ec_lwrve_t *lwrve = NULL;

	LTRACEF("entry\n");

	ret = lwpka_ec_set_keymat_check_args(econtext);
	CCC_ERROR_CHECK(ret);

	lwrve = econtext->ec_lwrve;
	ec_size_words        = lwrve->nbytes / BYTES_IN_WORD;
	field_word_size      = ec_size_words;
	pow2_field_word_size = ec_get_pow2_aligned_word_size(lwrve->nbytes);

	lwrve_BE = ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) == 0U);

	eop = econtext->pka_data.lwpka_op;

	LTRACEF("%s => (lwrve params(is BE: %u), use_keyslot: %u)\n",
		lwpka_get_engine_op_name(eop),
		lwrve_BE, use_keyslot);

	ret = ec_set_lwrve_keymat(econtext, lwrve, field_word_size);
	CCC_ERROR_CHECK(ret);

	ret = ec_setup_keymat_for_operation(econtext, lwrve,
					    pow2_field_word_size, use_keyslot,
					    set_key_value, eop);
	CCC_ERROR_CHECK(ret);

	/* Write p (modulus) to LOR_C0 for all operations with all lwrve types.
	 */
	LTRACEF("Reg writing %u word modulus p to LOR_C0\n", ec_size_words);
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_C0,
				   lwrve->p,
				   ec_size_words, lwrve_BE, field_word_size);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write EC modulus p PKA reg LOR_C0\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set the EC key either to SE keyslot or directly to PKA registers
 */
static status_t lwpka_ec_setkey(const struct se_engine_ec_context *econtext,
				bool use_keyslot, bool set_key_value)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwpka_ec_set_keymat(econtext, use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to set EC key material: %d\n",
					  ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_ec_init(struct se_engine_ec_context *econtext,
		       enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == econtext->ec_lwrve)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	econtext->pka_data.lwpka_op = eop;
	econtext->pka_data.op_mode = econtext->ec_lwrve->mode;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void lwpka_data_ec_release(struct se_engine_ec_context *econtext)
{
	LTRACEF("entry\n");

	(void)econtext;

	// XXX No longer needed, remove this function

	LTRACEF("exit\n");
}

static status_t lwpka_ec_op_exit(const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* No per operation secrets in e.g. econtext->pka_data
	 * (no need to clear anything here)
	 */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_process_point_verify_result(const struct se_engine_ec_context *econtext,
					       enum lwpka_engine_ops eop,
					       status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	bool result = false;

	LTRACEF("entry\n");

	(void)eop;

	if (NULL == rbad_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*rbad_p = ERR_BAD_STATE;

	ret = lwpka_ec_get_boolean_pv_result(econtext, &result);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to get boolean result for op %u (%d)\n",
				     eop, ret));

	LTRACEF("XXX operation %s result: %s\n",
		lwpka_get_engine_op_name(eop),
		(BOOL_IS_TRUE(result) ? "true" : "false"));
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
					enum lwpka_engine_ops eop,
					status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	bool result_BE = true;

	LTRACEF("entry\n");

	(void)eop;

	if (NULL == rbad_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*rbad_p = ERR_BAD_STATE;

	/* The result point is of same byte order as source point; BE is default.
	 */
	if (NULL != input_params->point_P) {
		result_BE = ((input_params->point_P->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U);
	}
	ret = lwpka_ec_get_point_result(econtext, input_params->point_Q, result_BE, eop);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to get point result for op %u (%d)\n",
				     eop, ret));

	*rbad_p = NO_ERROR;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_get_result(struct se_data_params *input_params,
				    const struct se_engine_ec_context *econtext,
				    enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (eop) {
	case LWPKA_OP_EC_POINT_VERIFY:
	case LWPKA_OP_EDWARDS_POINT_VERIFY:
		ret = ec_process_point_verify_result(econtext, eop, &rbad);
		break;

	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
	case LWPKA_OP_EC_POINT_MULTIPLY:
	case LWPKA_OP_EC_SHAMIR_TRICK:
		ret = ec_process_point_result(input_params, econtext,
					      eop, &rbad);
		break;

	default:
		LTRACEF("unsupported operation: %u\n", eop);
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
					   const struct se_engine_ec_context *econtext,
					   enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)eop;

	if (NULL == input_params->point_Q) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwpka_ec_load_point_operand(econtext,
					  POINT_ID_Q,
					  input_params->point_Q);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to load point Q for op %u (%d)\n",
				eop, ret));

	ret = lwpka_ec_load_point_operand(econtext,
					  POINT_ID_P,
					  input_params->point_P);
	CCC_ERROR_CHECK(ret, LTRACEF("Failed to load point P for op %u (%d)\n",
				     eop, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_set_point_operands(const struct se_data_params *input_params,
					    const struct se_engine_ec_context *econtext,
					    enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params) ||
	    (NULL == input_params->point_P)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (eop) {
		/* These use two points (P and Q) */
	case LWPKA_OP_EDWARDS_SHAMIR_TRICK:
	case LWPKA_OP_EC_SHAMIR_TRICK:
		ret = ec_load_two_point_operands(input_params, econtext,
						 eop);
		break;

		/* Load point P for these ops */
	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
	case LWPKA_OP_EC_POINT_MULTIPLY:

	case LWPKA_OP_EDWARDS_POINT_VERIFY:
	case LWPKA_OP_EC_POINT_VERIFY:
		ret = lwpka_ec_load_point_operand(econtext,
						  POINT_ID_P,
						  input_params->point_P);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("Failed to load point P for op %u (%d)\n", eop, ret);
			break;
		}
		break;

	default:
		LTRACEF("unsupported primitive: %u\n", eop);
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
 *  are loaded via PKA LOR regs as before.
 */
static status_t lwpka_ec_engine_op_get_keyslot_settings(
	const struct se_engine_ec_context *econtext,
	enum lwpka_engine_ops eop,
	bool *use_keyslot_p,
	bool *set_key_value_p)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false;
	bool set_key_value = true;

	LTRACEF("entry\n");

	switch (eop) {
	case LWPKA_OP_MONTGOMERY_POINT_MULTIPLY:
	case LWPKA_OP_EDWARDS_POINT_MULTIPLY:
	case LWPKA_OP_EC_POINT_MULTIPLY:
#if HAVE_LWPKA_LOAD

		if ((econtext->ec_flags & CCC_EC_FLAGS_PKEY_MULTIPLY) != 0U) {
			/* This multiply is using "private key" scalar value */

			if ((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSLOT) != 0U) {
				LTRACEF("using EC keyslots for multiply\n");
				use_keyslot = true;
			}

			if ((econtext->ec_flags & CCC_EC_FLAGS_USE_PRESET_KEY) != 0U) {
				LTRACEF("EC using existing keyslot key %u\n",
					econtext->ec_keyslot);
				set_key_value = false;
				use_keyslot = true;
			}

			if (BOOL_IS_TRUE(use_keyslot)) {
				if (econtext->ec_keyslot >= LWPKA_MAX_KEYSLOTS) {
					LTRACEF("Invalid keyslot number %u\n",
						econtext->ec_keyslot);
					ret = SE_ERROR(ERR_ILWALID_ARGS);
					break;
				}
			}
		}
#else
		if (((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSLOT) != 0U) ||
		    ((econtext->ec_flags & CCC_EC_FLAGS_USE_PRESET_KEY) != 0U)) {
			CCC_ERROR_MESSAGE("EC HW keyslots not supported\n");
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		}
#endif /* HAVE_LWPKA_LOAD */
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

static status_t lwpka_ec_engine_op_check_args(
	const struct se_data_params *input_params,
	const struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(econtext->ec_lwrve->nbytes))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Unsupported EC lwrve byte size: %u\n",
					     econtext->ec_lwrve->nbytes));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_ec_engine_do_operation(
	struct se_data_params *input_params,
	const struct se_engine_ec_context *econtext,
	enum lwpka_engine_ops eop,
	bool use_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = lwpka_ec_run_primitive(econtext, use_keyslot);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_wait_op_done(econtext->engine);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_get_result(input_params, econtext, eop);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_op_exit(econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC exit failed %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA_LOAD
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
#endif /* HAVE_LWPKA_LOAD */

static status_t lwpka_ec_keyslot_load(const struct se_engine_ec_context *econtext,
				      bool use_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(use_keyslot)) {
#if HAVE_LWPKA_LOAD
		ret = lwpka_keyslot_load(econtext->engine, econtext->ec_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("EC keyslot load setup failed %d\n", ret));
#else
		(void)econtext;

		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA EC HW keyslots not supported\n"));
#endif /* HAVE_LWPKA_LOAD */
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_ec_engine_op(struct se_data_params *input_params,
			   struct se_engine_ec_context *econtext,
			   enum lwpka_engine_ops eop)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false;
	bool set_key_value = true; /* ignored unless use_keyslot */

	LTRACEF("entry: opcode %u %s\n", eop, lwpka_get_engine_op_name(eop));

	ret = lwpka_ec_engine_op_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	DUMP_DATA_PARAMS("EC input", 0x1U, input_params);

	/* Use PKA1 LOR registers by default and with this option
	 * (CCC_EC_FLAGS_USE_KEYSLOT overrides this!)
	 */
	if ((econtext->ec_flags & CCC_EC_FLAGS_DYNAMIC_KEYSLOT) != 0U) {
		LTRACEF("EC loading keys via LOR registers\n");
		use_keyslot = false;
	}

	ret = lwpka_ec_init(econtext, eop);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC init failed %d\n", ret));

	/* Only EC point multiply operations can use the scalar
	 * in PKA HW keyslots.
	 *
	 * All other operations use LOR regs as before.
	 */
	ret = lwpka_ec_engine_op_get_keyslot_settings(econtext,
						      eop,
						      &use_keyslot,
						      &set_key_value);
	CCC_ERROR_CHECK(ret);

	/* Set key unless using existing preset key already in SE HW keyslot
	 * Even when using keyslot for "key" in point mult ops, numerous other
	 * values need to be set to LOR registers for EC operations (always).
	 */
	ret = lwpka_ec_setkey(econtext, use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC setkey failed %d\n", ret));

	ret = lwpka_ec_keyslot_load(econtext, use_keyslot);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_set_point_operands(input_params, econtext, eop);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_ec_engine_do_operation(input_params, econtext, eop, use_keyslot);
	CCC_ERROR_CHECK(ret);

	LTRACEF("done => opcode %u %s\n", eop, lwpka_get_engine_op_name(eop));
fail:
#if HAVE_LWPKA_LOAD
	ec_clean_keyslot(econtext, use_keyslot);
	/* Failure on final key cleanup does not trigger operation failure here. */
#endif /* HAVE_LWPKA_LOAD */

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
status_t engine_lwpka_ec_point_mult_locked(struct se_data_params *input_params,
					  struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EC_POINT_MULTIPLY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Verify that a point Pxy (X: src[0..ec_size-1] Y:src[ec_size..2*ec_size-1])
 * is on the lwrve.
 *
 * Is P on lwrve?
 */
status_t engine_lwpka_ec_point_verify_locked(struct se_data_params *input_params,
					     struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EC_POINT_VERIFY);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* EC_K (EC_K_LENGTH) and EC_L (EC_L_LENGTH) must be set by caller
 *
 * Q = (k x P) + (l x Q)
 */
status_t engine_lwpka_ec_shamir_trick_locked(struct se_data_params *input_params,
					     struct se_engine_ec_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = lwpka_ec_engine_op(input_params, econtext, LWPKA_OP_EC_SHAMIR_TRICK);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ECC */
