/*
 * Copyright (c) 2017-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic engine : modular operations
 *
 * Implement the modular primitive operations with the PKA engine, i.e. =>
 *   multiply, add, subtract, reduce, divide, ilwert
 *
 * Modular exponentation (RSA operation) is in the tegra_lwpka_rsa.c file.
 */
#include <crypto_system_config.h>

#if HAVE_LWPKA_MODULAR

#include <crypto_ops.h>
#include <lwpka/tegra_lwpka_mod.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifndef MOD_REDUCE_MAX_OPERAND_BIT_SIZE
/* HW limit for reduction length.
 */
#define MOD_REDUCE_MAX_OPERAND_BIT_SIZE 4096U
#endif

/* This object only exists to reduce complexity.
 */
struct mfun_param_block {
	const engine_t *engine;
	const te_mod_params_t *params;
	const char *op_str;
	lwpka_reg_id_t result_reg;
	enum lwpka_primitive_e primitive;
	enum lwpka_engine_ops engine_op;
	uint32_t nwords;
};

// FIXME: input values are not properly verifed!!!

static status_t reg_write_xy(const struct mfun_param_block *marg,
			     bool x_BE, lwpka_reg_id_t Rx,
			     bool y_BE, lwpka_reg_id_t Ry)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' to %s\n",
		marg->op_str, x_BE, marg->nwords, lwpka_lor_name(Rx));

	ret = lwpka_register_write(marg->engine, marg->params->op_mode,
				   Rx,
				   marg->params->x,
				   marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x' reg %s\n",
				marg->op_str, lwpka_lor_name(Rx)));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'y' to %s\n",
		marg->op_str, y_BE, marg->nwords, lwpka_lor_name(Ry));

	ret = lwpka_register_write(marg->engine, marg->params->op_mode,
				   Ry,
				   marg->params->y,
				   marg->nwords, y_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'y' to %s\n",
				marg->op_str, lwpka_lor_name(Ry)));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t reg_write_x(const struct mfun_param_block *marg,
			    bool x_BE, lwpka_reg_id_t Rx)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' to %s\n",
		marg->op_str, x_BE, marg->nwords, lwpka_lor_name(Rx));

	ret = lwpka_register_write(marg->engine, marg->params->op_mode,
				   Rx,
				   marg->params->x,
				   marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x' to %s\n",
				marg->op_str, lwpka_lor_name(Rx)));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Map op_mode to dp mode ONLY for MODULAR OPERATIONS
 *
 * Mapping relate for Modular operations only, due to double length
 * of operands for MOD.
 *
 * For the mod reduction the type change does not matter when value not
 * used in functions that use the type.
 */
static status_t modular_reduction_dp_mode(enum pka1_op_mode op_mode,
					  enum pka1_op_mode *dp_op_mode)
{
	status_t ret = NO_ERROR;
	enum pka1_op_mode dp_mode = PKA1_OP_MODE_ILWALID;

	LTRACEF("entry\n");

	switch (op_mode) {
	case PKA1_OP_MODE_ECC256:  dp_mode = PKA1_OP_MODE_ECC512; break;

	case PKA1_OP_MODE_ECC448:  dp_mode = PKA1_OP_MODE_RSA1024; break; /* type change */
	case PKA1_OP_MODE_ECC320:  dp_mode = PKA1_OP_MODE_RSA1024; break; /* type change */
	case PKA1_OP_MODE_ECC384:  dp_mode = PKA1_OP_MODE_RSA1024; break; /* type change */
	case PKA1_OP_MODE_ECC512:  dp_mode = PKA1_OP_MODE_RSA1024; break; /* type change */
	case PKA1_OP_MODE_ECC521:  dp_mode = PKA1_OP_MODE_RSA1536; break; /* type change */

	case PKA1_OP_MODE_RSA512:  dp_mode = PKA1_OP_MODE_RSA1024; break;
	case PKA1_OP_MODE_RSA768:  dp_mode = PKA1_OP_MODE_RSA1536; break;
	case PKA1_OP_MODE_RSA1024: dp_mode = PKA1_OP_MODE_RSA2048; break;
	case PKA1_OP_MODE_RSA1536: dp_mode = PKA1_OP_MODE_RSA3072; break;
	case PKA1_OP_MODE_RSA2048: dp_mode = PKA1_OP_MODE_RSA4096; break;

	case PKA1_OP_MODE_RSA3072:
	case PKA1_OP_MODE_RSA4096:
		/* HW does not support DP size reductions for these lengths */
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	*dp_op_mode = dp_mode;
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set argument to LOR_S0, word length in x_nwords
 *
 * HW zero pads bits after "op radix words" when operation started later,
 * but for DP reduction the RADIX may be for larger size than marg->nwords.
 */
static status_t mod_reduct_args(const struct mfun_param_block *marg,
				bool x_BE, uint32_t x_nwords)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' to LOR_S0\n",
		marg->op_str, x_BE, x_nwords);

	ret = lwpka_register_write(marg->engine, marg->params->op_mode,
				   LOR_S0,
				   &marg->params->x[0],
				   x_nwords, x_BE, x_nwords);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write modular %s 'x_lo' to LOR_S0\n",
					  marg->op_str));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mfun_mod_reduction_args(const struct mfun_param_block *marg,
					bool x_BE, bool double_precision)
{
	status_t ret = NO_ERROR;
	uint32_t effective_wsize = marg->nwords;
	uint32_t x_nwords = marg->nwords;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(double_precision)) {
		x_nwords = x_nwords << 1U;
	}

	if (x_nwords > (MOD_REDUCE_MAX_OPERAND_BIT_SIZE / BITS_IN_WORD)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("MOD_RED supports max %u bit operand\n",
					     MOD_REDUCE_MAX_OPERAND_BIT_SIZE));
	}

	if (BOOL_IS_TRUE(double_precision)) {
		enum pka1_op_mode dp_op_mode = PKA1_OP_MODE_ILWALID;

		/* Map "op_mode in double precision" to the first SIZE
		 * supported by HW for REDUCTIONS. The "type" may be
		 * mapped wrong, but it it not relevant for mod
		 * reductions.
		 */
		ret = modular_reduction_dp_mode(marg->params->op_mode, &dp_op_mode);
		CCC_ERROR_CHECK(ret);

		ret = lwpka_num_words(dp_op_mode, &effective_wsize, NULL);
		CCC_ERROR_CHECK(ret);
	}

	LTRACEF("Reducing %u word value (BE: %u, DOUBLE PREC: %u)\n",
		effective_wsize, x_BE, double_precision);

	LOG_HEXBUF("mod reduct X",
		   &marg->params->x[0], (effective_wsize * BYTES_IN_WORD));

	ret = mod_reduct_args(marg, x_BE, effective_wsize);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mparams_write_modulus(const engine_t *engine,
				      const te_mod_params_t *params,
				      uint32_t nwords,
				      uint32_t zero_pad_nwords)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	bool m_BE = false;

	LTRACEF("entry\n");

	m_BE = ((params->mod_flags & PKA1_MOD_FLAG_M_LITTLE_ENDIAN) == 0U);

	if (BOOL_IS_TRUE(se_util_vec_is_zero(params->m, params->size, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Zero modulus for mod operations\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	LTRACEF("Reg (BE:%u) writing %u word 'm' => LOR_C0\n", m_BE, nwords);

	ret = lwpka_register_write(engine, params->op_mode,
				   LOR_C0,
				   params->m,
				   nwords, m_BE, zero_pad_nwords);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write 'm' to LOR_C0\n"));

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static bool double_precision_operands(enum lwpka_engine_ops engine_op)
{
	return (LWPKA_OP_MODULAR_REDUCTION_DP == engine_op) ||
		(LWPKA_OP_GEN_MULTIPLY_MOD == engine_op);
}

static status_t mfun_run_operation(const struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
	bool r_BE = false;
	enum pka1_op_mode run_op_mode = marg->params->op_mode;

	LTRACEF("entry\n");

	r_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);

	if (double_precision_operands(marg->engine_op)) {
		/* These operations must be run with double precision
		 * input argument sizes (all parameters must have been
		 * be zero padded to run_op_mode sizes).
		 */
		ret = modular_reduction_dp_mode(marg->params->op_mode, &run_op_mode);
		CCC_ERROR_CHECK(ret);
	}

	ret = lwpka_go_start(marg->engine, run_op_mode, marg->primitive, CCC_SC_ENABLE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular %s go start returned: %d\n", marg->op_str, ret));

	ret = lwpka_wait_op_done(marg->engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Modular %s %u returned %d\n",
					  marg->op_str, marg->params->op_mode, ret));

	LTRACEF("%s: Reg (BE:%u) reading %u words from %s\n",
		marg->op_str, r_BE, marg->nwords, lwpka_lor_name(marg->result_reg));

	ret = lwpka_register_read(marg->engine, marg->params->op_mode,
				  marg->result_reg,
				  marg->params->r,
				  marg->nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular %s read (BE:%u) from %s failed: %d\n",
				marg->op_str, r_BE,
				lwpka_lor_name(marg->result_reg), ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_MODULAR_DIVISION
static mod_division_arg_check(const struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
	status_t rbad1 = ERR_BAD_STATE;

	if (BOOL_IS_TRUE(se_util_vec_is_zero(marg->params->x, marg->params->size, &ret)) ||
	    (BOOL_IS_TRUE(se_util_vec_is_zero(marg->params->y, marg->params->size, &rbad1)))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_CHECK_ERROR(ret);
	CCC_CHECK_ERROR(rbad1);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_MODULAR_DIVISION */

static status_t mfun_set_params_switch(struct mfun_param_block *marg,
				       enum lwpka_primitive_e *ep_p)
{
	/* Avoid FI skipping calls in switch */
	status_t ret = ERR_BAD_STATE;
	bool x_BE = false;
	bool y_BE = false;
	enum lwpka_primitive_e ep = LWPKA_PRIMITIVE_ILWALID;

	LTRACEF("entry\n");

	/* Common for all modular operations: modulus to LOR_C0
	 *
	 * If marg->params->m == NULL => assumes the modulus has already been
	 * loaded by caller (does not report an error).
	 */

	/* Set X and Y cooridinate endian flags
	 */
	x_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_X_LITTLE_ENDIAN) == 0U);
	y_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_Y_LITTLE_ENDIAN) == 0U);

	switch(marg->engine_op) {
	case LWPKA_OP_MODULAR_MULTIPLICATION:
		marg->op_str = "multiplication";

		ret = reg_write_xy(marg, x_BE, LOR_S0, y_BE, LOR_S1);
		ep = LWPKA_PRIMITIVE_MOD_MULTIPLICATION;
		break;

	case LWPKA_OP_MODULAR_ADDITION:
		//
		// XXX provided (0 <= x < 2m)
		// XXX and
		// XXX provided (0 <= y < 2m)
		//
		marg->op_str = "addition";

		ret = reg_write_xy(marg, x_BE, LOR_S0, y_BE, LOR_S1);
		ep = LWPKA_PRIMITIVE_MOD_ADDITION;
		break;

	case LWPKA_OP_MODULAR_SUBTRACTION:
		//
		// XXX provided (0 <= x < 2m)
		// XXX and
		// XXX provided (0 <= y < 2m)
		//
		marg->op_str = "subtraction";

		ret = reg_write_xy(marg, x_BE, LOR_S0, y_BE, LOR_S1);
		ep = LWPKA_PRIMITIVE_MOD_SUBTRACTION;
		break;

	case LWPKA_OP_MODULAR_ILWERSION:
		marg->op_str = "ilwersion";

		ret = reg_write_x(marg, x_BE, LOR_S1);
		ep  = LWPKA_PRIMITIVE_MOD_ILWERSION;
		break;

#if CCC_WITH_MODULAR_DIVISION
	case LWPKA_OP_MODULAR_DIVISION:
		marg->op_str = "division";

		ret = mod_division_arg_check(marg);
		if (NO_ERROR != ret) {
			break;
		}

		ret = reg_write_xy(marg, x_BE, LOR_S1, y_BE, LOR_S0); /* Regs swapped */
		ep  = NPKA_PRIMITIVE_MOD_DIVISION;
		break;
#endif /* CCC_WITH_MODULAR_DIVISION */

	case LWPKA_OP_MODULAR_EXPONENTIATION:
		marg->op_str      = "mod exp";

		ret = reg_write_xy(marg, x_BE, LOR_S0, y_BE, LOR_K0);
		ep  = LWPKA_PRIMITIVE_MOD_EXP;
		break;

	case LWPKA_OP_MODULAR_REDUCTION_DP:
		marg->op_str      = "dp reduction";

		ret = mfun_mod_reduction_args(marg, x_BE, CTRUE);
		ep  = LWPKA_PRIMITIVE_MOD_REDUCTION;
		break;

	case LWPKA_OP_MODULAR_REDUCTION:
		marg->op_str = "reduction";

		ret = mfun_mod_reduction_args(marg, x_BE, CFALSE);
		ep  = LWPKA_PRIMITIVE_MOD_REDUCTION;
		break;

	case LWPKA_OP_MODULAR_SQUARE:
		marg->op_str = "square";

		/* square(x) = x*x */
		ret = reg_write_x(marg, x_BE, LOR_S0);
		if (NO_ERROR != ret) {
			break;
		}
		ret = reg_write_x(marg, x_BE, LOR_S1);
		ep  = LWPKA_PRIMITIVE_MOD_MULTIPLICATION;
		break;

	default:
		LTRACEF("%s: Unsupported primitive modular operation: %u\n",
			marg->op_str, marg->params->op_mode);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	*ep_p = ep;

	/* Caller checks the value. CCM reduction by one */
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mfun_set_params(struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
	enum lwpka_primitive_e ep = LWPKA_PRIMITIVE_ILWALID;

	LTRACEF("entry\n");

	ret = mfun_set_params_switch(marg, &ep);
	CCC_ERROR_CHECK(ret);

	/* LWPKA HW function primitive ID,
	 */
	marg->primitive = ep;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mod_check_args_normal(const engine_t *engine, const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == params) ||
	    (NULL == params->r)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((params->size == 0U) || ((params->size % BYTES_IN_WORD) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("operand size invalid\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Do primitive modular operations with the LWPKA unit
 */
static status_t modular_operation(
	const engine_t  *engine,
	const te_mod_params_t *params,
	enum lwpka_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	struct mfun_param_block mfun_args = { .nwords = 0U };

	LTRACEF("entry\n");

	ret = mod_check_args_normal(engine, params);
	CCC_ERROR_CHECK(ret);

	/* Default: result in LOR_D1; all LWPKA modular
	 * ops return value in that register.
	 */
	mfun_args.op_str       = "unknown mod op";
	mfun_args.result_reg   = LOR_D1;
	mfun_args.primitive    = LWPKA_PRIMITIVE_ILWALID;
	mfun_args.engine       = engine;
	mfun_args.params       = params;
	mfun_args.engine_op    = engine_op;
	mfun_args.nwords       = params->size / BYTES_IN_WORD;

	/* All mod operations have modulus in LOR_C0.
	 * If the operations do not modify that register then this
	 * can be set 0 in a locked sequence and write the modulus
	 * only once.
	 *
	 * TODO: But it is NOT VERIFIED if LWPKA modified LOR_C0 in mod ops!!!
	 */
	if (NULL != params->m) {
		uint32_t mod_pad_nwords = mfun_args.nwords;

		if (double_precision_operands(engine_op)) {
			/* Double length zero fill */
			mod_pad_nwords = mod_pad_nwords << 1U;
		}
		ret = mparams_write_modulus(engine, params,
					    mfun_args.nwords, mod_pad_nwords);
		CCC_ERROR_CHECK(ret);
	}

	ret = mfun_set_params(&mfun_args);
	CCC_ERROR_CHECK(ret);

	ret = mfun_run_operation(&mfun_args);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*************************************/

/* x*y mod m */
status_t engine_lwpka_modular_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_MULTIPLICATION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x+y mod m */
status_t engine_lwpka_modular_addition_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_ADDITION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x-y mod m */
status_t engine_lwpka_modular_subtraction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_SUBTRACTION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* 1/x mod m */
status_t engine_lwpka_modular_ilwersion_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_ILWERSION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_MODULAR_DIVISION
/* y/x mod m */
status_t engine_lwpka_modular_division_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_DIVISION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_MODULAR_DIVISION */

/* x ^ y mod m */
status_t engine_lwpka_modular_exponentiation_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_EXPONENTIATION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}


/* x mod m (single precision) */
status_t engine_lwpka_modular_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_REDUCTION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x mod m (double precision) */
status_t engine_lwpka_modular_dp_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_REDUCTION_DP);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x^2 mod m == x*x mod m */
status_t engine_lwpka_modular_square_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, LWPKA_OP_MODULAR_SQUARE);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_GEN_MULTIPLY

static status_t gen_multiply_set_regs(const engine_t  *engine,
				      const te_mod_params_t *params,
				      uint32_t nwords,
				      enum lwpka_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	bool x_BE = ((params->mod_flags & PKA1_MOD_FLAG_X_LITTLE_ENDIAN) == 0U);
	bool y_BE = ((params->mod_flags & PKA1_MOD_FLAG_Y_LITTLE_ENDIAN) == 0U);
	const char *op_str = "gen_multiply (dp)";

	(void)op_str;

	LTRACEF("entry\n");

	if (LWPKA_OP_GEN_MULTIPLY_MOD == engine_op) {
		op_str = "gen multiply mod";
	}

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' to LOR_S0\n",
		op_str, x_BE, nwords);

	ret = lwpka_register_write(engine, params->op_mode,
				   LOR_S0,
				   params->x,
				   nwords, x_BE, nwords);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write %s 'x' to LOR_S0\n",
					  op_str));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'y' to LOR_S1\n",
		op_str, y_BE, nwords);
	ret = lwpka_register_write(engine, params->op_mode,
				   LOR_S1,
				   params->y,
				   nwords, y_BE, nwords);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write %s 'y' to LOR_S1\n",
					  op_str));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* read DP result for GEN_MULT from LOR_D1 register.
 *
 * LOR_D1 is a double length register for holding GEN_MUL result.
 */
static status_t gen_multiply_read_dp_result(const engine_t  *engine,
					    const te_mod_params_t *params,
					    uint32_t nwords)
{
	status_t ret = NO_ERROR;
	bool r_BE = ((params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);
	uint32_t dsize = nwords * 2U;

	LTRACEF("entry\n");

	if (nwords > MAX_LWPKA_OPERAND_WORD_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	/* LWPKA result always in LE */
	LTRACEF("GEN_MULT: Reg (BE:%u) reading dp result %u word 'r' from LOR_D1'\n",
		r_BE, dsize);

	ret = lwpka_register_read(engine, params->op_mode,
				  LOR_D1,
				  params->r, dsize, r_BE);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("GEN_MULT read (BE:%u) from LOR_D1 failed: %d\n",
					  r_BE, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * Move 2*nword (2*radix) register value D1 directly to S0 register, as
 * caller does not provide a double length temp buffer to store the value.
 *
 * Max length of value that MOD_REDUCTION can handle is 2048 bits (HW limit).
 *
 * SRC reg: LOR_D1 (double length result)
 * DST reg: LOR_S0 (double length source value, max 2048 bits, 256 bytes).
 *
 * HW zero extends S0 bits larger than 2*radix.
 */
static status_t copy_gen_mult_result_for_reduction(const engine_t *engine,
						   uint32_t dp_nwords)
{
	status_t ret = NO_ERROR;
	uint32_t wcount = 0U;
	uint32_t soffset = 0U;
	uint32_t val32 = 0U;

	LTRACEF("entry\n");

	if (dp_nwords > (MOD_REDUCE_MAX_OPERAND_BIT_SIZE / BITS_IN_WORD)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("MOD_RED supports max %u bit operand\n",
					     MOD_REDUCE_MAX_OPERAND_BIT_SIZE));
	}

	/* D1 => S0
	 */
	for (wcount = 0U; wcount < dp_nwords; wcount++) {
		val32 = tegra_engine_get_reg(engine, (LOR_D1 + soffset));
		LTRACEF("D1->S0 [%u] <= 0x%x\n", wcount, val32);
		tegra_engine_set_reg(engine, (LOR_S0 + soffset), val32);
		soffset = soffset + BYTES_IN_WORD;
	}

	FI_BARRIER(u32, wcount);

	if (wcount != dp_nwords) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	LTRACEF("%u words copied from LOR_D1 to LOR_S0 in LE\n",
		dp_nwords);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * D1 contains the DP gen mult result; set up the parameters to reduction
 * and redue that with modulus. The reduction written to D1.
 *
 * The operation mode is mapped to double-length size operation mode supported
 * by the HW. The "type" of the mapped mode may get mapped from ECC to RSA
 * but it is irrelevant for modular operations and do not affect the callees.
 *
 * This mapping is required since the reduction radix and modulus zero padding need
 * to MATCH the double-precision gen multiply RESULT SIZE, not the SINGLE PRECISION
 * GEN_MULT size with a double precision result.
 *
 * => the single precision MODULUS must be zero padded to DP RADIX SIZE and
 * the MOD_REDUCTION operation itself must be started in the mapped DP OP MODE.
 * The argument is also copied in DP RADIX SIZE from D1 to S0.
 */
static status_t gen_multiply_dp_result_reduction(const engine_t *engine,
						 const te_mod_params_t *params,
						 uint32_t nwords)
{
	status_t ret = NO_ERROR;
	uint8_t *r = &params->r[0];
	enum pka1_op_mode dp_op_mode = PKA1_OP_MODE_ILWALID;
	uint32_t dp_nwords = 0U;
	bool r_BE = ((params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);

	LTRACEF("entry\n");

	ret = modular_reduction_dp_mode(params->op_mode, &dp_op_mode);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_num_words(dp_op_mode, &dp_nwords, NULL);
	CCC_ERROR_CHECK(ret);

	/* If modulus given, write to LOR_C0 register.
	 * For DP reduction, zero pad the modulus to DP precision.
	 * (DP reduction does not work for 4096 bit modulus, since
	 *  8K modulus is not support)
	 */
	if (NULL != params->m) {
		ret = mparams_write_modulus(engine, params, nwords,
					    dp_nwords);
		CCC_ERROR_CHECK(ret);
	}

	ret = copy_gen_mult_result_for_reduction(engine, dp_nwords);
	CCC_ERROR_CHECK(ret);

	/* Do DP modular reduction in the mapped "double precision"
	 * reduction mode supported by HW.
	 */
	ret = lwpka_go_start(engine, dp_op_mode,
			     LWPKA_PRIMITIVE_MOD_REDUCTION, CCC_SC_ENABLE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Gen mult mod: DP reduction go start returned: %d\n",
				ret));

	ret = lwpka_wait_op_done(engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Gen mult mod: DP reduction returned %d\n",
					  ret));

	/* HW single precision result in D1 little endian
	 */
	LTRACEF("Gen mult mod: DP reduct: (BE:%u) read %u word 'r' from LOR_D1\n",
		r_BE, nwords);

	ret = lwpka_register_read(engine, params->op_mode,
				  LOR_D1,
				  r, nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Gen mult mod: DP reduct read (BE:%u) fail LOR_D1: %d\n",
					  r_BE, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t gen_mult_handle_result(const engine_t *engine,
				       const te_mod_params_t *params,
				       uint32_t nwords,
				       enum lwpka_engine_ops engine_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (LWPKA_OP_GEN_MULTIPLY == engine_op) {
		ret = gen_multiply_read_dp_result(engine, params, nwords);
		CCC_ERROR_CHECK(ret);
	} else if (LWPKA_OP_GEN_MULTIPLY_MOD == engine_op) {
		ret = gen_multiply_dp_result_reduction(engine, params, nwords);
		CCC_ERROR_CHECK(ret);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* non-modular operation (GEN_MULTIPLY)
 * and a combined gen mult with double precision reduction (GEN_MULT_MOD)
 */

/* GEN_MULT uses the modular param structure, but does not use the
 * modulus (m) field since these ops are not modular.
 *
 * Note:
 * This function will be used if/when the group equation of the EDDSA
 * gets verified in the long form (all terms multiplied by 8).
 * (This also has other uses not yet supported by CCC).
 *
 * Result in LOR_D1 for double precision result (double length register).
 */
static status_t gen_mult_operation(
	const engine_t  *engine,
	const te_mod_params_t *params,
	enum lwpka_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	uint32_t nwords = 0U;
	enum lwpka_primitive_e ep = LWPKA_PRIMITIVE_ILWALID;

	LTRACEF("entry\n");

	ret = mod_check_args_normal(engine, params);
	CCC_ERROR_CHECK(ret);

	if ((NULL == params->x) || (NULL == params->y)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	nwords = params->size / BYTES_IN_WORD;

	switch(engine_op) {
	case LWPKA_OP_GEN_MULTIPLY_MOD:
	case LWPKA_OP_GEN_MULTIPLY:
		ep = LWPKA_PRIMITIVE_GEN_MULTIPLICATION;
		ret = gen_multiply_set_regs(engine, params, nwords, engine_op);
		break;

	default:
		LTRACEF("Unsupported gen-mult operation: %u\n", engine_op);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = lwpka_go_start(engine, params->op_mode, ep, CCC_SC_ENABLE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("GEN_MULT go start returned: %d\n", ret));

	ret = lwpka_wait_op_done(engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Gen mult %u returned %d\n",
					  params->op_mode, ret));

	ret = gen_mult_handle_result(engine, params, nwords, engine_op);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x*y (double precision output)
 */
status_t engine_lwpka_gen_mult_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = gen_mult_operation(engine, params, LWPKA_OP_GEN_MULTIPLY);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* DP(x*y) mod m (double precision multiply  followed
 *  by DP reduction)
 */
status_t engine_lwpka_gen_mult_mod_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = gen_mult_operation(engine, params, LWPKA_OP_GEN_MULTIPLY_MOD);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_GEN_MULTIPLY */

#endif /* HAVE_LWPKA_MODULAR */
