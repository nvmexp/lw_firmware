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
 * Implement the modular primitive operations with the PKA1 engine, i.e. =>
 *   multiply, add, subtract, reduce, divide, ilwert
 *
 * Modular exponentation (RSA operation) is in the tegra_pka1_rsa.c file.
 */

#include <crypto_system_config.h>

#if HAVE_PKA1_MODULAR

#include <crypto_ops.h>
#include <tegra_pka1_mod.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* This object only exists to reduce complexity.
 */
struct mfun_param_block {
	const engine_t *engine;
	const te_mod_params_t *params;
	const char *op_str;
	pka1_bank_t result_bank;
	pka1_reg_id_t result_reg;
	pka1_entrypoint_t entrypoint;
	enum pka1_engine_ops engine_op;
	uint32_t nwords;
};

// FIXME: input values are not properly verifed!!!

static status_t bank_reg_write_xy(const struct mfun_param_block *marg,
				  bool x_BE, pka1_bank_t Bx, pka1_reg_id_t Rx,
				  bool y_BE, pka1_bank_t By, pka1_reg_id_t Ry)
{
	status_t ret = NO_ERROR;

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' => %c%u\n",
		marg->op_str, x_BE, marg->nwords, ((uint32_t)'A' + Bx), Rx);

	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       Bx, Rx,
				       marg->params->x,
				       marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x' PKA bank reg %c%u\n",
				marg->op_str,  ((uint32_t)'A' + Bx), Rx));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'y' => %c%u\n",
		marg->op_str, y_BE, marg->nwords, ((uint32_t)'A' + By), Ry);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       By, Ry,
				       marg->params->y,
				       marg->nwords, y_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'y' PKA bank reg %c%u\n",
				marg->op_str, ((uint32_t)'A' + By), Ry));
fail:
	return ret;
}

static status_t bank_reg_write_x(const struct mfun_param_block *marg,
				 bool x_BE, pka1_bank_t Bx, pka1_reg_id_t Rx)
{
	status_t ret = NO_ERROR;

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' => %c%u\n",
		marg->op_str, x_BE, marg->nwords, ((uint32_t)'A' + Bx), Rx);

	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       Bx, Rx,
				       marg->params->x,
				       marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x' PKA bank reg %c%u\n",
				marg->op_str,  ((uint32_t)'A' + Bx), Rx));
fail:
	return ret;
}

#if HAVE_ELLIPTIC_20
#if HAVE_P521

/* Montgomery value must be pre-computed and MUST EXIST OOB
 * to use this call !!!!
 *
 * FIXME: This is not yet supported => will return an error until done!
 */
static status_t mfun_m_521_montmult(const struct mfun_param_block *marg,
				    bool x_BE, bool y_BE)
{
	status_t ret = NO_ERROR;

	bool m_prime_BE = false;
	bool r2_BE = false;

	LTRACEF("entry\n");

	ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
	CCC_ERROR_CHECK(ret);

	/* PKA can not callwlate these if not provided (OOB) => trap error
	 */
	if ((NULL == marg->params->mont_m_prime) ||
	    (NULL == marg->params->mont_r2)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 CCC_ERROR_MESSAGE("P521 montgomery values not provided\n"));
	}

	m_prime_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_MONT_M_PRIME_LITTLE_ENDIAN) == 0U);
	r2_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_MONT_R2_LITTLE_ENDIAN) == 0U);

	// Write P521 montgomery values => These must be pre-computed!
	LTRACEF("%s: Reg (BE:%u) writing %u word 'mprime' => D1\n",
		marg->op_str, m_prime_BE, marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_D, R1,
				       marg->params->mont_m_prime,
				       marg->nwords, m_prime_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'm_prime' PKA bank reg D1\n",
				marg->op_str));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'r_sqr' => D3\n",
		marg->op_str, r2_BE, marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_D, R3,
				       marg->params->mont_r2,
				       marg->nwords, r2_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'r_sqr' PKA bank reg D3\n",
				marg->op_str));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_P521 */
#endif /* HAVE_ELLIPTIC_20 */

static status_t mfun_set_montgomery_value_regs(const struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
	bool montg_BE = false;

	LTRACEF("entry\n");

	montg_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_MONT_M_PRIME_LITTLE_ENDIAN) == 0U);

	// XXXX TODO: If the field size and arg word size differ => add support (P521?)

	LTRACEF("Reg writing %u word p' (preset)\n", marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_D, R1,
				       marg->params->mont_m_prime,
				       marg->nwords, montg_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write p' PKA bank reg D1\n"));

	montg_BE = ((marg->params->mod_flags & PKA1_MONT_FLAG_R2_LITTLE_ENDIAN) == 0U);

	LTRACEF("Reg writing %u word r^2 mod p (preset)\n", marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_D, R3,
				       marg->params->mont_r2,
				       marg->nwords, montg_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write r^2 mod p PKA bank reg D3\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;

}

static status_t mfun_modular_multiplication(const struct mfun_param_block *marg,
					    bool x_BE, bool y_BE)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (marg->params->size > TE_MAX_ECC_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Modular %s operand size %u > max %u, not supported\n",
					     marg->op_str, marg->params->size, TE_MAX_ECC_BYTES));
	}

	if (marg->params->op_mode == PKA1_OP_MODE_ECC521) {
#if HAVE_ELLIPTIC_20
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("P521 lwrve modular multiplication with wrong function in new FW version\n"));
#else
		tegra_engine_set_reg(marg->engine, SE_PKA1_FLAGS_OFFSET,  SE_PKA1_FLAGS_FLAG_F1(SE_ELP_ENABLE));
#endif
	}

	/* Modulus already loaded to D0 in LE at this point.
	 *
	 * Next: deal with montgomery values.
	 */
	if ((NULL == marg->params->mont_m_prime) ||
	    (NULL == marg->params->mont_r2)) {
		struct pka1_engine_data pka1_data = { .op_mode = PKA1_OP_MODE_ILWALID };

		/* Need mont values for MOD MULT => twice per ECDSA
		 * if these do not exist.
		 */
		if (marg->params->op_mode == PKA1_OP_MODE_ECC521) {
			/* P521 montgomery values can not be callwlated by PKA1
			 * they must be provided by the caller.
			 */
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("P521 lwrve montgomery values not provided\n"));
		}

		pka1_data.op_mode = marg->params->op_mode;
		pka1_data.pka1_op = PKA1_OP_MODULAR_MULTIPLICATION;

		ret = pka1_montgomery_values_calc(marg->params->cmem,
						  marg->engine, marg->params->size, NULL,
						  &pka1_data, false);

		pka1_data_release(marg->params->cmem, &pka1_data);

		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Montgomery value callwlation failed: %d\n",
						  ret));
	} else {
		/* Pre-set montgomery values are known, program those to PKA1 registers.
		 */
		ret = mfun_set_montgomery_value_regs(marg);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Pre-configured montgomery values set regs failed\n"));
	}

	/* At this point:
	 *  D1 is the M'    (not touched by R_SQR value calc)
	 *  D3 is the R_SQR (output of the last montgomery op (R_SQR))
	 *  D0 is the modulus (preserved)
	 */
	ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mfun_bit_serial_reduction_dp_args(const struct mfun_param_block *marg,
						  bool x_BE)
{
	status_t ret = NO_ERROR;

	const uint8_t *x_lo = NULL;
	const uint8_t *x_hi = NULL;

	LTRACEF("entry\n");

	if (marg->nwords > MAX_PKA1_OPERAND_WORD_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	x_lo = &marg->params->x[0];
	x_hi = &marg->params->x[marg->nwords * BYTES_IN_WORD];

	if (BOOL_IS_TRUE(x_BE)) {
		x_lo = x_hi;
		x_hi = &marg->params->x[0];
	}
	LTRACEF("%s: Reg (BE:%u) writing %u word 'x_lo' to C0\n",
		marg->op_str, x_BE, marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_C, R0,
				       x_lo,
				       marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x_lo' PKA bank reg C0\n",
				marg->op_str));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x_hi' to C1\n",
		marg->op_str, x_BE, marg->nwords);
	ret = pka1_bank_register_write(marg->engine, marg->params->op_mode,
				       BANK_C, R1,
				       x_hi,
				       marg->nwords, x_BE, marg->nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write modular %s 'x_hi' PKA bank reg C1\n",
				marg->op_str));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mparams_write_modulus(const engine_t *engine, const te_mod_params_t *params, uint32_t nwords)
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

	LTRACEF("Reg (BE:%u) writing %u word 'm' => D0\n",
		m_BE, nwords);

	ret = pka1_bank_register_write(engine, params->op_mode,
				       BANK_D, R0,
				       params->m,
				       nwords, m_BE, nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write 'm' PKA bank reg D0\n"));

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mfun_run_operation(const struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
	bool r_BE = false;

	LTRACEF("entry\n");

	r_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);

	ret = pka1_go_start(marg->engine, marg->params->op_mode, marg->params->size, marg->entrypoint);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular %s go start returned: %d\n", marg->op_str, ret));

	ret = pka1_wait_op_done(marg->engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular %s %u returned %d\n",
				marg->op_str, marg->params->op_mode, ret));

	LTRACEF("%s: Reg (BE:%u) reading %u word %c%u\n", marg->op_str, r_BE, marg->nwords,
		((uint32_t)'A' + marg->result_bank), marg->result_reg);

	ret = pka1_bank_register_read(marg->engine, marg->params->op_mode,
				      marg->result_bank, marg->result_reg,
				      marg->params->r,
				      marg->nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Modular %s read (BE:%u) from reg bank (%c%u) failed: %d\n",
				marg->op_str, r_BE, ((uint32_t)'A' + marg->result_bank),
				marg->result_reg, ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mfun_set_params(struct mfun_param_block *marg)
{
	status_t ret = NO_ERROR;
#if HAVE_PKA1_MODULAR_DIVISION
	status_t rbad1 = ERR_BAD_STATE;
	status_t rbad2 = ERR_BAD_STATE;
#endif /* HAVE_PKA1_MODULAR_DIVISION */
	pka1_entrypoint_t epoint = PKA1_ENTRY_ILWALID;

	bool x_BE = false;
	bool y_BE = false;

	LTRACEF("entry\n");

	/* Common for all modular operations: modulus to D0
	 *
	 * If marg->params->m == NULL => assumes the modulus has already been
	 * loaded by caller (does not report an error).
	 */

	/* Set X and Y cooridinate endian flags
	 */
	x_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_X_LITTLE_ENDIAN) == 0U);
	y_BE = ((marg->params->mod_flags & PKA1_MOD_FLAG_Y_LITTLE_ENDIAN) == 0U);

	/* Avoid FI skipping calls in switch */
	ret = ERR_BAD_STATE;

	switch(marg->engine_op) {
#if HAVE_ELLIPTIC_20

#if HAVE_P521
	case PKA1_OP_EC_MODMULT_521:
		// Mersenne-521 modular multiplication (FASTER)
		// x*y mod m (provided (0 <= x <= m) and (0 <= y <= m), where m = 2^521-1
		marg->op_str = "Mersenne-521 modular multiply";

		/* Caller must make sure that modulus M value is set to 2^251-1 for
		 * this operation (i.e. marg->params->m)
		 */
		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
		epoint = PKA1_ENTRY_MOD_MERSENNE_521_MULT;
		break;

	case PKA1_OP_EC_M_521_MONTMULT:
		// General purpose 521 bit modular multiplication(GENERIC, requires montgomery values)
		// x*y mod m (provided (0 <= x <= m) and (0 <= y <= m)
		marg->op_str = "General purpose 521-bit montgomery multiply";

		ret = mfun_m_521_montmult(marg, x_BE, y_BE);
		epoint = PKA1_ENTRY_MOD_521_MONTMULT;
		break;
#endif /* HAVE_P521 */

#if HAVE_PKA1_ED25519
	case PKA1_OP_C25519_MOD_MULT:
		/* x*y mod p (where p = 2^255-19) */
		marg->op_str = "C25519 multiply";

		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
		epoint = PKA1_ENTRY_C25519_MOD_MULT;
		break;

	case PKA1_OP_C25519_MOD_SQUARE:
		/* x^2 mod p (where p = 2^255-19) */
		marg->op_str = "C25519 square";

		ret = bank_reg_write_x(marg, x_BE, BANK_A, R0);
		epoint = PKA1_ENTRY_C25519_MOD_SQR;
		break;

	case PKA1_OP_C25519_MOD_EXP:
		/* x^y mod p (where p = 2^255-19) */
		marg->op_str = "C25519 exponentiation";

		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_D, R2);

		/* Force F0 flag zero */
		tegra_engine_set_reg(marg->engine, SE_PKA1_FLAGS_OFFSET, 0U);

		epoint = PKA1_ENTRY_C25519_MOD_EXP;
		break;
#endif /* HAVE_PKA1_ED25519 */

#endif /* HAVE_ELLIPTIC_20 */

	case PKA1_OP_MODULAR_MULTIPLICATION:
		marg->op_str = "multiplication";

		ret = mfun_modular_multiplication(marg, x_BE, y_BE);
		epoint = PKA1_ENTRY_MOD_MULTIPLICATION;
		break;

	case PKA1_OP_MODULAR_SUBTRACTION:
		//
		// XXX provided (0 <= x < 2m) or (0 < x < 2^ceil(log2(m))
		// XXX and
		// XXX provided (0 <= y < 2m) or (0 < y < 2^ceil(log2(m))
		//
		marg->op_str = "subtraction";

		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
		epoint = PKA1_ENTRY_MOD_SUBTRACTION;
		break;

	case PKA1_OP_MODULAR_ADDITION:
		//
		// XXX provided (0 <= x < 2m) or (0 < x < 2^ceil(log2(m))
		// XXX and
		// XXX provided (0 <= y < 2m) or (0 < y < 2^ceil(log2(m))
		//
		marg->op_str = "addition";

		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_B, R0);
		epoint = PKA1_ENTRY_MOD_ADDITION;
		break;

	case PKA1_OP_BIT_SERIAL_REDUCTION_DP:
		marg->op_str      = "double precision bit serial reduction";

		ret = mfun_bit_serial_reduction_dp_args(marg, x_BE);
		epoint  = PKA1_ENTRY_BIT_SERIAL_REDUCTION_DP;
		marg->result_bank = BANK_C;
		break;

#if HAVE_PKA1_BIT_SERIAL_REDUCTION
	case PKA1_OP_BIT_SERIAL_REDUCTION:
		marg->op_str = "bit serial reduction";

		ret = bank_reg_write_x(marg, x_BE, BANK_C, R0);
		epoint  = PKA1_ENTRY_BIT_SERIAL_REDUCTION;
		marg->result_bank = BANK_C;
		break;
#endif /* HAVE_PKA1_BIT_SERIAL_REDUCTION */

	case PKA1_OP_MODULAR_REDUCTION:
		marg->op_str = "reduction";

		ret = bank_reg_write_x(marg, x_BE, BANK_C, R0);
		epoint = PKA1_ENTRY_MOD_REDUCTION;
		break;

#if HAVE_PKA1_MODULAR_DIVISION
	case PKA1_OP_MODULAR_DIVISION:
		marg->op_str = "division";

		if (BOOL_IS_TRUE(se_util_vec_is_zero(marg->params->x, marg->params->size, &rbad1)) ||
		    (BOOL_IS_TRUE(se_util_vec_is_zero(marg->params->y, marg->params->size, &rbad2)))) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
		if (NO_ERROR != rbad1) {
			CCC_ERROR_WITH_ECODE(rbad1);
		}
		if (NO_ERROR != rbad2) {
			CCC_ERROR_WITH_ECODE(rbad2);
		}

		ret = bank_reg_write_xy(marg, x_BE, BANK_A, R0, y_BE, BANK_C, R0);
		epoint  = PKA1_ENTRY_MOD_DIVISION;
		marg->result_bank = BANK_C;
		break;
#endif /* HAVE_PKA1_MODULAR_DIVISION */

	case PKA1_OP_MODULAR_ILWERSION:
		marg->op_str = "ilwersion";

		ret = bank_reg_write_x(marg, x_BE, BANK_A, R0);
		epoint  = PKA1_ENTRY_MOD_ILWERSION;
		marg->result_bank = BANK_C;
		break;

	default:
		LTRACEF("%s: Unsupported primitive modular operation: %u\n",
			marg->op_str, marg->params->op_mode);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}

	/* All switch cases set the ret if exelwted correctly.
	 */
	CCC_ERROR_CHECK(ret);

	/* PKA1 FW function entrypoint */
	marg->entrypoint = epoint;

fail:
	/* No check for rbad1 && rbad2; checked in switch */
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* pmccabe reduction (this is inferior, because it loses the location where the
 * error is set).
 */
static status_t mod_check_args_normal(const engine_t *engine, const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	if ((NULL == engine) || (NULL == params) ||
	    (NULL == params->r)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((params->size == 0U) || ((params->size % BYTES_IN_WORD) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 LTRACEF("operand size invalid\n"));
	}

fail:
	return ret;
}

/* Do primitive modular operations with the PKA1 unit
 */
static status_t modular_operation(
	const engine_t  *engine,
	const te_mod_params_t *params,
	enum pka1_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	struct mfun_param_block mfun_args = { .nwords = 0U };

	LTRACEF("entry\n");

	ret = mod_check_args_normal(engine, params);
	CCC_ERROR_CHECK(ret);

	/* Default: result in A0;
	 * Division and ilwersion return value in C0, as set by callee.
	 */
	mfun_args.op_str       = "unknown mod op";
	mfun_args.result_bank  = BANK_A;
	mfun_args.result_reg   = R0;
	mfun_args.entrypoint   = PKA1_ENTRY_ILWALID;
	mfun_args.engine       = engine;
	mfun_args.params       = params;
	mfun_args.engine_op    = engine_op;
	mfun_args.nwords       = params->size / BYTES_IN_WORD;

	/* All mod operations have modulus in D0 and none of them modify
	 * the D0 register where it is written => in subsequent operations the params->m
	 * can be NULL, only the first op in sequence needs to set D0.
	 */
	if (NULL != params->m) {
		ret = mparams_write_modulus(engine, params, mfun_args.nwords);
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
status_t engine_pka1_modular_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_MULTIPLICATION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x+y mod m */
status_t engine_pka1_modular_addition_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_ADDITION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x-y mod m */
status_t engine_pka1_modular_subtraction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_SUBTRACTION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x mod m */
status_t engine_pka1_modular_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_REDUCTION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_MODULAR_DIVISION
/* y/x mod m */
status_t engine_pka1_modular_division_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_DIVISION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_MODULAR_DIVISION */

/* 1/x mod m */
status_t engine_pka1_modular_ilwersion_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_MODULAR_ILWERSION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_BIT_SERIAL_REDUCTION
status_t engine_pka1_bit_serial_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_BIT_SERIAL_REDUCTION);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_BIT_SERIAL_REDUCTION */

status_t engine_pka1_bit_serial_double_precision_reduction_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_BIT_SERIAL_REDUCTION_DP);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_P521 && HAVE_ELLIPTIC_20
status_t engine_pka1_modular_m521_multiplication_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
#if 1
	(void)engine;
	(void)params;
	ret = SE_ERROR(ERR_NOT_SUPPORTED);
#else
	ret = modular_operation(engine, params, PKA1_OP_EC_MODMULT_521);
#endif
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_pka1_modular_multiplication521_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
#if 1
	(void)engine;
	(void)params;
	ret = SE_ERROR(ERR_NOT_SUPPORTED);
#else
	ret = modular_operation(engine, params, PKA1_OP_EC_M_521_MONTMULT);
#endif
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_P521 && HAVE_ELLIPTIC_20 */

#if HAVE_PKA1_ED25519

/* These are used for ED25519 (Ed25519 lwrve EDDSA) in PKA
 *
 * Not lwrrently for Lwrve25519 operations (X25519) since that
 * only uses Lwrve25519 point multiply.
 *
 * The following modular operations use slower Euler's algortihm, not
 * Euclideian because Euler's algorithm is time ilwariant. This
 * property is required in Bernstein's paper for attack resistance.
 */

/* Lwrve25519 modular exponentiation */
status_t engine_pka1_c25519_mod_exp_locked(const engine_t *engine,
					   const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_C25519_MOD_EXP);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Lwrve25519 modular multiply */
status_t engine_pka1_c25519_mod_mult_locked(const engine_t *engine,
					    const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_C25519_MOD_MULT);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Lwrve25519 modular square */
status_t engine_pka1_c25519_mod_square_locked(const engine_t *engine,
					      const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = modular_operation(engine, params, PKA1_OP_C25519_MOD_SQUARE);
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_ED25519 */

#if HAVE_GEN_MULTIPLY

static status_t gen_multiply_set_regs(const engine_t  *engine,
				      const te_mod_params_t *params,
				      uint32_t nwords,
				      enum pka1_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	bool x_BE = ((params->mod_flags & PKA1_MOD_FLAG_X_LITTLE_ENDIAN) == 0U);
	bool y_BE = ((params->mod_flags & PKA1_MOD_FLAG_Y_LITTLE_ENDIAN) == 0U);
	const char *op_str = "gen_multiply (dp)";

	(void)op_str;

	LTRACEF("entry\n");

	if (PKA1_OP_GEN_MULTIPLY_MOD == engine_op) {
		op_str = "gen multiply mod";
	}

	LTRACEF("%s: Reg (BE:%u) writing %u word 'x' => A0\n",
		op_str, x_BE, nwords);
	ret = pka1_bank_register_write(engine, params->op_mode,
				       BANK_A, R0,
				       params->x,
				       nwords, x_BE, nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write %s 'x' PKA bank reg A0\n",
				op_str));

	LTRACEF("%s: Reg (BE:%u) writing %u word 'y' => B0\n",
		op_str, y_BE, nwords);
	ret = pka1_bank_register_write(engine, params->op_mode,
				       BANK_B, R0,
				       params->y,
				       nwords, y_BE, nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to write %s 'y' PKA bank reg B0\n",
				op_str));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* read DP result for GEN_MULT from C0 and C1 registers.
 */
static status_t gen_multiply_read_dp_result(const engine_t  *engine,
					    const te_mod_params_t *params,
					    uint32_t nwords)
{
	status_t ret = NO_ERROR;
	uint8_t *r_lo = NULL;
	uint8_t *r_hi = NULL;
	bool r_BE = ((params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);

	LTRACEF("entry\n");

	if (nwords > MAX_PKA1_OPERAND_WORD_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	r_lo = &params->r[0];
	r_hi = &params->r[nwords * BYTES_IN_WORD];

	if (BOOL_IS_TRUE(r_BE)) {
		r_lo = r_hi;
		r_hi = &params->r[0];
	}

	/* PKA1 result always in LE */
	LTRACEF("GEN_MULT: Reg (BE:%u) reading %u word 'r (low half) from C0'\n",
		r_BE, nwords);

	ret = pka1_bank_register_read(engine, params->op_mode,
				      BANK_C, R0,
				      r_lo, nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("GEN_MULT read (BE:%u) from reg bank C0 failed: %d\n",
				r_BE, ret));

	LTRACEF("GEN_MULT: Reg (BE:%u) reading %u word 'r (upper half) from C1'\n",
		r_BE, nwords);

	ret = pka1_bank_register_read(engine, params->op_mode,
				      BANK_C, R1,
				      r_hi, nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("GEN_MULT read (BE:%u) from reg bank C1 failed: %d\n",
				r_BE, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Result bank of preceding gen_mult and input args of the DP serial
 * reduction is the reg tuple <C0, C1>.
 *
 * C0 is also the location of the reduced result (C0 gets overwritten
 * by the reduction).
 */
static status_t gen_multiply_dp_reduction(const engine_t *engine,
					  const te_mod_params_t *params,
					  uint32_t nwords)
{
	status_t ret = NO_ERROR;
	uint8_t *r = &params->r[0];
	bool r_BE = ((params->mod_flags & PKA1_MOD_FLAG_R_LITTLE_ENDIAN) == 0U);
	pka1_entrypoint_t entrypoint = PKA1_ENTRY_BIT_SERIAL_REDUCTION_DP;

	LTRACEF("entry\n");

	/* If modulus given, write to D0 register
	 */
	if (NULL != params->m) {
		ret = mparams_write_modulus(engine, params, nwords);
		CCC_ERROR_CHECK(ret);
	}

	ret = pka1_go_start(engine, params->op_mode, params->size, entrypoint);
	CCC_ERROR_CHECK(ret,
			LTRACEF("MULT+DP serial reduction go start returned: %d\n",
				ret));

	ret = pka1_wait_op_done(engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("MULT+DP serial reduction %u returned %d\n",
				params->op_mode, ret));

	// PKA1 result always in LE
	LTRACEF("MULT+DP reduction: Reg (BE:%u) reading %u word 'r' (SP reduction value in C0)\n",
		r_BE, nwords);

	ret = pka1_bank_register_read(engine, params->op_mode,
				      BANK_C, R0,
				      r, nwords, r_BE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("MULT+DP serial reduction read (BE:%u) from reg bank C0 failed: %d\n",
				r_BE, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t gen_mult_handle_result(const engine_t *engine,
				       const te_mod_params_t *params,
				       uint32_t nwords,
				       enum pka1_engine_ops engine_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (PKA1_OP_GEN_MULTIPLY == engine_op) {
		ret = gen_multiply_read_dp_result(engine, params, nwords);
		CCC_ERROR_CHECK(ret);
	} else if (PKA1_OP_GEN_MULTIPLY_MOD == engine_op) {
		ret = gen_multiply_dp_reduction(engine, params, nwords);
		CCC_ERROR_CHECK(ret);
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* non-modular operation support follows (GEN_MULTIPLY) */

/* Uses the modular param structure, but does not use the modulus
 * (m) field since these ops are not modular.
 *
 * Note:
 * This function will be used if/when the group equation of the EDDSA
 * gets verified in the long form (all terms multiplied by 8).
 * (This also has other uses not yet supported by CCC).
 *
 * Result in C0 && C1 for double precision ops.
 */
static status_t gen_mult_operation(
	const engine_t  *engine,
	const te_mod_params_t *params,
	enum pka1_engine_ops engine_op)
{
	status_t ret = NO_ERROR;
	uint32_t nwords = 0U;
	pka1_entrypoint_t entrypoint = PKA1_ENTRY_ILWALID;

	LTRACEF("entry\n");

	ret = mod_check_args_normal(engine, params);
	CCC_ERROR_CHECK(ret);

	if ((NULL == params->x) || (NULL == params->y)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	nwords = params->size / BYTES_IN_WORD;

	switch(engine_op) {
	case PKA1_OP_GEN_MULTIPLY_MOD:
	case PKA1_OP_GEN_MULTIPLY:
		entrypoint = PKA1_ENTRY_GEN_MULTIPLY;
		ret = gen_multiply_set_regs(engine, params, nwords, engine_op);
		break;

	default:
		LTRACEF("Unsupported gen-mult operation: %u\n",
			params->op_mode);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = pka1_go_start(engine, params->op_mode, params->size, entrypoint);
	CCC_ERROR_CHECK(ret,
			LTRACEF("GEN_MULT go start returned: %d\n", ret));

	ret = pka1_wait_op_done(engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("GEN_MULT %u returned %d\n",
				params->op_mode, ret));

	ret = gen_mult_handle_result(engine, params, nwords, engine_op);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* x*y (double precision output)
 */
status_t engine_pka1_gen_mult_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = gen_mult_operation(engine, params, PKA1_OP_GEN_MULTIPLY);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* DP(x*y) mod m (double precision multiply  followed
 *  by bit serial DP reduction)
 */
status_t engine_pka1_gen_mult_mod_locked(
	const engine_t  *engine,
	const te_mod_params_t *params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	ret = gen_mult_operation(engine, params, PKA1_OP_GEN_MULTIPLY_MOD);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_GEN_MULTIPLY */

#endif /* HAVE_PKA1_MODULAR */
