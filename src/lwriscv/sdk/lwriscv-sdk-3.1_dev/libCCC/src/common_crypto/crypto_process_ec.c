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

/* process ECDH shared secrets and ECDSA signatures */

#include <crypto_system_config.h>

#if CCC_WITH_ECC

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* If this is defined non-zero: point multiplication results of
 * zero will be treated as error in the following cases:
 *
 *  1) <Px> is zerovector on lwrve25519 => error
 * or
 *  2) <Px,Py> are both zerovector on any lwrve => error
 *
 * If defined zero: neither error is trapped and the zerovector is
 * just returned (default).
 */
#ifndef HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT
#define HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT 0
#endif

#if CCC_WITH_ECDH || CCC_WITH_ECDSA || CCC_WITH_ED25519
/* pmccabe reduction (loses the location where the error is set).
 */
static status_t ec_op_check_args_normal(const struct se_data_params *input_params,
					const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("Entry\n");

	if ((NULL == input_params) || (NULL == ec_context)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ec_context->ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve not defined\n"));
	}

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif

// XXXXXX These are now generic support functions => create process layer EC GEN file???

/* Verify that the point is on lwrve (called from a locked ec_context) */
status_t ec_check_point_on_lwrve_locked(se_engine_ec_context_t *econtext,
					const struct pka1_ec_lwrve_s *lwrve,
					const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == lwrve) || (NULL == point)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.point_P = point;
	input.dst     = NULL;
	input.input_size  = sizeof_u32(te_ec_point_t);
	input.output_size = 0U;

	ret = CCC_ENGINE_SWITCH_EC_VERIFY(econtext->engine, &input, econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC point on lwrve verify returns: %d\n", ret));

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

/* Verify that the point is on lwrve (called when no mutex held) */
status_t ec_check_point_on_lwrve(se_engine_ec_context_t *econtext,
				 const struct pka1_ec_lwrve_s *lwrve,
				 const te_ec_point_t *point)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	HW_MUTEX_TAKE_ENGINE(econtext->engine);
	ret = ec_check_point_on_lwrve_locked(econtext, lwrve, point);
	HW_MUTEX_RELEASE_ENGINE(econtext->engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

static status_t ec_pmult_scalar_check(const se_engine_ec_context_t *econtext,
				      const uint8_t *s, uint32_t slen, bool s_LE)
{
	status_t ret = ERR_BAD_STATE;
	uint32_t nbytes = econtext->ec_lwrve->nbytes;

	LTRACEF("entry\n");

	if (slen > nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Trap zero scalar multiplier
	 *
	 * If scalar is in keyslot, it is not checked.
	 */
	if (BOOL_IS_TRUE(se_util_vec_is_zero(s, slen, &ret))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("Point multiply with zero scalar: %d\n",
					     ret));
	}
	CCC_ERROR_CHECK(ret);

	if (slen == nbytes) {
		bool swap = false;

		ret = ERR_BAD_STATE;
		swap = (((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) != s_LE);

		/*
		 * Any lwrve point multiplied by lwrve order would
		 * result in identity element.
		 */
		if (BOOL_IS_TRUE(se_util_vec_match(s, econtext->ec_lwrve->n,
						   nbytes, swap, &ret))) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("Point multiply with lwrve order\n"));
		}
	}

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

static status_t ec_point_multiply_check_scalar_arg(const se_engine_ec_context_t *econtext,
						   const uint8_t *s, uint32_t slen,
						   bool s_LE)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Using a scalar in PKA1 keyslot? */
	if ((NULL == s) || (0U == slen)) {
		if ((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSLOT) == 0U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("no scalar in point multiply\n"));
		}

		/*
		 * Skip scalar value check if it is in keyslot. In this
		 * case <s, slen> are <NULL, 0U>
		 */
	} else {
		ret = ec_pmult_scalar_check(econtext, s, slen, s_LE);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

/* Does not verify scalar value and length thoroughly, either
 * callee or the entity setting it to PKA1 register needs to check it.
 */
static status_t ec_point_multiply_check_args(const se_engine_ec_context_t *econtext,
					     const te_ec_point_t *R,
					     const uint8_t *s, uint32_t slen,
					     bool s_LE)
{
	status_t ret = ERR_BAD_STATE;

	LTRACEF("entry\n");

	FI_BARRIER(status, ret);

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == R) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC result point R not set\n"));
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC lwrve not defined\n"));
	}

	ret = ec_point_multiply_check_scalar_arg(econtext, s, slen, s_LE);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT
static status_t pmult_trap_zero_result_locked(const se_engine_ec_context_t *econtext,
					      const te_ec_point_t *R,
					      uint32_t nbytes)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	bool Px_zero = false;

	LTRACEF("entry\n");

	/* Traps zero <Px and Py> or just zero Px for X25519 */
	if (BOOL_IS_TRUE(se_util_vec_is_zero(R->x, nbytes, &rbad))) {
		if (econtext->ec_lwrve->id == TE_LWRVE_C25519) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("Point multiply Px result is zero vector: %d\n",
						     ret));
		} else {
			Px_zero = true;
		}
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	if (econtext->ec_lwrve->id != TE_LWRVE_C25519) {
		rbad = ERR_BAD_STATE;
		if (BOOL_IS_TRUE(se_util_vec_is_zero(R->y, nbytes, &rbad))) {
			if (BOOL_IS_TRUE(Px_zero)) {
				CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
						     LTRACEF("point multiply results in zero: %d\n", ret));
			}
		}

		if (NO_ERROR != rbad) {
			CCC_ERROR_WITH_ECODE(rbad);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT */

static void ec_point_multiply_locked_cleanup(se_engine_ec_context_t *econtext,
					     te_ec_point_t **pbuf_p)
{
	te_ec_point_t *pbuf = *pbuf_p;

	if ((NULL != pbuf) && (NULL != econtext)) {
		CMTAG_MEM_RELEASE(econtext->cmem, CMTAG_BUFFER, pbuf);
		*pbuf_p = pbuf;
	}
}

/* Multiply point P with the scalar S[slen]: (R = s x P)
 * If P is not given (NULL), use the lwrve generator point G instead (P == G)
 * s_LE is ignored if using a keyslot key => they are always little endian.
 *
 * The caller can set the ec_flags CCC_EC_FLAGS_PKEY_MULTIPLY flag
 * when the operation is performed with the private key.
 * In this case the ec_pkey may also be NULL if the flags indicate
 * that the existing keyslot key is used, so let the callee verify
 * the scalar argument validity => not verified here.
 */
status_t ec_point_multiply_locked(se_engine_ec_context_t *econtext,
				  te_ec_point_t *R, const te_ec_point_t *P,
				  const uint8_t *s, uint32_t slen,
				  bool s_LE)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *P_tmp = NULL;
	struct se_data_params input;
	uint32_t nbytes = 0U;
	uint32_t saved_ec_flags = 0U;

	LTRACEF("entry\n");

	ret = ec_point_multiply_check_args(econtext, R, s, slen, s_LE);
	CCC_ERROR_CHECK(ret);

	nbytes = econtext->ec_lwrve->nbytes;

	P_tmp = CMTAG_MEM_GET_TYPED_BUFFER(econtext->cmem,
					   CMTAG_BUFFER,
					   BYTES_IN_WORD,
					   te_ec_point_t *,
					   sizeof_u32(te_ec_point_t));
	if (NULL == P_tmp) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	if (NULL != P) {
		input.point_P = P;
	} else {
		/* Use the lwrve generator point G if P not given
		 *
		 * Then this callwlates P = k*G, i.e. the public key corresponding
		 *  to the private key value K on the selected lwrve.
		 *
		 * For X25519 Py is not used, only the Px (value 0x9) is used
		 * for the ECDH secret value derivation.
		 * Lwrrently the code anyway sets the Py like for other
		 * ECDH's but the locked mult below just ignores Py.
		 */
		se_util_mem_move(P_tmp->x, econtext->ec_lwrve->g.gen_x, nbytes);
		se_util_mem_move(P_tmp->y, econtext->ec_lwrve->g.gen_y, nbytes);
		P_tmp->point_flags = CCC_EC_POINT_FLAG_NONE;

		if ((econtext->ec_lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
			P_tmp->point_flags |= CCC_EC_POINT_FLAG_LITTLE_ENDIAN;
		}
#if SE_RD_DEBUG
		LOG_HEXBUF("SELF => PUB(x):", P_tmp->x, nbytes);
		LOG_HEXBUF("SELF => PUB(y):", P_tmp->y, nbytes);
#endif
		input.point_P = P_tmp;
	}

	input.input_size = sizeof_u32(te_ec_point_t);
	input.point_Q = R;
	input.output_size = sizeof_u32(te_ec_point_t);

	// Set the P multiplier scalar value to EC_K.
	//
	// If EC_K (i.e. s) is NULL and SLEN == 0 => the EC point mult function
	// may still use the scalar lwrrently stored in EC_KEYSLOT, if
	// instructed by flags. Otherwise callee traps as error.
	//
	saved_ec_flags = econtext->ec_flags;

	/* If using a keyslot value it is always set in little endian */
	if ((NULL == s) || (0U == slen) || BOOL_IS_TRUE(s_LE)) {
		econtext->ec_flags |= CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN;
	}

	econtext->ec_k = s;
	econtext->ec_k_length = slen;

	ret = CCC_ENGINE_SWITCH_EC_MULT(econtext->engine, &input, econtext);

	// Unset the P multiplier scalar value
	econtext->ec_flags = saved_ec_flags;
	econtext->ec_k = NULL;
	econtext->ec_k_length = 0U;

	CCC_ERROR_CHECK(ret,
			LTRACEF("point multiply failed: %d\n", ret));

#if HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT
	ret = pmult_trap_zero_result_locked(econtext, R, nbytes);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_TRAP_EC_POINT_MULTIPLICATION_ZERO_RESULT */

#if SE_RD_DEBUG
	LTRACEF("Point R (big endian: %s)\n",
		(((R->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U) ? "yes" : "no"));
	LOG_HEXBUF("POINT MULT res(x):", R->x, nbytes);
	LOG_HEXBUF("POINT MULT res(y):", R->y, nbytes);
#endif

fail:
	ec_point_multiply_locked_cleanup(econtext, &P_tmp);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t ec_point_multiply(se_engine_ec_context_t *econtext,
			   te_ec_point_t *R, const te_ec_point_t *P,
			   const uint8_t *s, uint32_t slen,
			   bool s_LE)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(econtext->engine);
	ret = ec_point_multiply_locked(econtext, R, P, s, slen, s_LE);
	HW_MUTEX_RELEASE_ENGINE(econtext->engine);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_ECDH || CCC_WITH_X25519

#if CCC_WITH_X25519
static status_t x25519_setup_le_point(te_ec_point_t *Px,
				      const te_ec_point_t *src_point,
				      uint32_t nbytes)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (nbytes == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/*
	 * Copy the Px coordinate to a new point in LE and
	 *  adjust it for X25519: clear the msb of the U
	 *  coordinate (here Px bits[0..255]) bit 255
	 *  must be masked zero. Modifies the LE copy,
	 *  not the source.
	 *
	 * Py is not used by the algorithm (just clear it for now).
	 */
	se_util_mem_set(Px->y, 0U, nbytes);

	/* Move X-coordinate in LE to Px->x
	 */
	se_util_move_bytes(Px->x, src_point->x, nbytes,
			   ((src_point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) == 0U));

	Px->x[nbytes-1U] = BYTE_VALUE(Px->x[nbytes-1U] & 0x7FU);

	/* Coordinates in LE also cause the multiply result point to
	 * be little endian
	 */
	Px->point_flags = CCC_EC_POINT_FLAG_LITTLE_ENDIAN;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_X25519 */

static status_t ecdh_setup_mult_src_point(se_engine_ec_context_t *econtext,
					  struct se_data_params *input_params,
					  te_ec_point_t *tmp_point,
					  const te_ec_point_t **mult_point_p,
					  bool LE_pkey)
{
	status_t ret = NO_ERROR;
	uint32_t nbytes = econtext->ec_lwrve->nbytes;

	LTRACEF("entry\n");

	if (NULL == input_params->point_P) {
		te_ec_point_t *point = tmp_point;

		input_params->input_size = 0U;

		/* Pubkey = k*G (i.e. pubkey derived with private key from generator point)
		 *
		 * (if pubkey already exists and is verified, should use that... => TODO)
		 *
		 * Derive public key to tmp_point.
		 */
		econtext->ec_flags |= CCC_EC_FLAGS_PKEY_MULTIPLY;
		ret = ec_point_multiply(econtext, point, NULL,
					econtext->ec_pkey,
					nbytes,
					LE_pkey);
		econtext->ec_flags &= ~CCC_EC_FLAGS_PKEY_MULTIPLY;

		CCC_ERROR_CHECK(ret,
				LTRACEF("EC pubkey generation failed\n"));

		/* Generate ECDH from Pub x privkey (not very useful, though)
		 */
#if POINT_OP_RESULT_VERIFY
		/* Just for completeness, check that the generated PUBKEY
		 * is on lwrve.
		 */
		ret = ec_check_point_on_lwrve(econtext, econtext->ec_lwrve, point);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Point is not on lwrve: %d\n", ret));
#endif
		*mult_point_p = point;
	} else {
		/* Generate ECDH from given input point P after
		 * checking it is on lwrve.
		 */
		ret = ec_check_point_on_lwrve(econtext, econtext->ec_lwrve, input_params->point_P);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Point is not on lwrve: %d\n", ret));

		*mult_point_p = input_params->point_P;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define X_ECDH_CHUNK_SIZE (2U * sizeof_u32(te_ec_point_t))

/*
 * ECDH shared secret callwlation
 * ------------------------------
 *
 * To callwlate ECDH we need a scalar value S (in ec_pkey) and
 * some lwrve point P => ECDH(S,P) == S x P
 *
 * If SRC == NULL => callwlate ECDH value with the scalar Da (in
 *  ec_pkey) and my public key point Pmy (where Pmy = Da x G (G is
 *  lwrve generator point)) This case is not very useful, since the
 *  result can only be callwlated by knowing the private key. [ But
 *  this could also be used for deriving keys for symmetric ciphers,
 *  etc... ]
 *
 * If SRC != NULL => SRC is the peer public key point (in SRC_POINT)
 *  and scalar is my private key (Da in ec_pkey): ECDH(Da,SRC_POINT) =
 *  Da x SRC_POINT In this case SRC_POINT = peer_secret_key(PPk) x G
 *
 * In this case the ECDH generates a persistent shared secret between
 *  parties, if peer also (safely) knows my public key (Pmy):
 *
 *  ECDH(Da x SRC_POINT) = ECDH(PPk x Pmy) == persistent_shared_secret (pss)
 *
 * Set the CCC_EC_FLAGS_BIG_ENDIAN_KEY flag is ec_pkey is big endian,
 * LE by default.
 */
static status_t ecdh_process_input(struct se_data_params *input_params,
				   struct se_ec_context *ec_context,
				   te_ec_point_t *tpoint,
				   te_ec_point_t *R)
{
	status_t ret = NO_ERROR;
	const te_ec_point_t *mult_point = NULL;
	uint32_t nbytes = ec_context->ec.ec_lwrve->nbytes;
	bool reverse_result = true;
	bool LE_pkey = false;
	CCC_LOOP_VAR;

	LE_pkey = ((ec_context->ec.ec_flags & CCC_EC_FLAGS_BIG_ENDIAN_KEY) == 0U);

	/* Setup the point to multiply with scalar
	 */
	CCC_LOOP_BEGIN {
		if (TE_ALG_X25519 != ec_context->ec.algorithm) {
			ret = ecdh_setup_mult_src_point(&ec_context->ec, input_params,
							tpoint, &mult_point, LE_pkey);
			CCC_LOOP_STOP;
		}

#if CCC_WITH_X25519
		/* NOTE: Must not try to verify "point on
		 * lwrve with X25519", all values are valid to
		 * derive a X25519 shared secret.
		 */
		ret = x25519_setup_le_point(tpoint, input_params->point_P, nbytes);
		CCC_ERROR_END_LOOP(ret);

		reverse_result  = false;
		mult_point = tpoint;

		/* RFC-7748 states that the X25519 result MAY be checked for
		 * zero value and fail if so. The ec_point_multiply() already fails
		 * if Px is zero (or Py is zero for non X25519) =>
		 *
		 * TODO: verify if this zero trapping is always acceptable!!!
		 */
		CCC_LOOP_STOP;
#else
		CCC_ERROR_END_LOOP_WITH_ECODE(ERR_NOT_SUPPORTED,
					      LTRACEF("X25519 not supported\n"));
#endif /* CCC_WITH_X25519 */
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	ec_context->ec.ec_flags |= CCC_EC_FLAGS_PKEY_MULTIPLY;
	ret = ec_point_multiply(&ec_context->ec, R, mult_point,
				ec_context->ec.ec_pkey,
				nbytes,
				LE_pkey);
	ec_context->ec.ec_flags &= ~CCC_EC_FLAGS_PKEY_MULTIPLY;

	CCC_ERROR_CHECK(ret, LTRACEF("ECDH failed\n"));

	/* For ECDH the shared secret is the big endian X co-ordinate
	 *  value of point R (R = k*P)
	 *
	 * X25519 result is little endian value of the same and
	 * the only algorithm which sets reverse_result false.
	 */
	if ((R->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) != 0U) {
		ret = se_util_move_bytes(input_params->dst, R->x, nbytes, reverse_result);
		CCC_ERROR_CHECK(ret);
	} else {
		/* Px big endian */
		se_util_mem_move(input_params->dst, R->x, nbytes);
	}

	input_params->output_size = nbytes;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_ecdh_process_input(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *twopoints = NULL; /* optimize stack usage */

	LTRACEF("entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	if (NULL == input_params->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (input_params->output_size < ec_context->ec.ec_lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("ECDH shared secret does not fit in output buffer\n"));
	}

	/* No need to be contiguous but should be word aligned.
	 * Use this API to make that explicit
	 */
	twopoints = CMTAG_MEM_GET_TYPED_BUFFER(ec_context->ec.cmem,
					       CMTAG_ALIGNED_BUFFER,
					       BYTES_IN_WORD,
					       te_ec_point_t *,
					       X_ECDH_CHUNK_SIZE);
	if (NULL == twopoints) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = ecdh_process_input(input_params, ec_context, &twopoints[0], &twopoints[1]);
	CCC_ERROR_CHECK(ret);

fail:
	if (NULL != twopoints) {
		se_util_mem_set((uint8_t *)twopoints, 0U, X_ECDH_CHUNK_SIZE);
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_ALIGNED_BUFFER,
				  twopoints);
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDH */

#if HAVE_SHAMIR_TRICK == 0
/* All uint8_t buffers must be lwrve->nbytes bytes long.
 */

#define CHECK_X_COORDINATE 1U
#define CHECK_Y_COORDINATE 2U

static status_t reflected_coordinates_arg_check(const se_engine_ec_context_t *econtext,
						uint32_t cop,
						const uint8_t *c1, const uint8_t *c2,
						const pka1_ec_lwrve_t *lwrve, const uint8_t *scratch,
						const bool *is_reflected)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) ||
	    (NULL == c1) || (NULL == c2) || (NULL == lwrve) ||
	    (NULL == scratch) || (NULL == is_reflected)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((CHECK_X_COORDINATE != cop) &&
	    (CHECK_Y_COORDINATE != cop)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
fail:
	return ret;
}

/* Reflected points (X1 == X2 mod p, Y1 + Y2 == 0 mod p).
 * If yes, D == 0 and verification fail
 *
 * Called PKA1 mutex locked.
 */
static status_t check_reflected_coordinates_locked(const se_engine_ec_context_t *econtext,
						   uint32_t cop,
						   const uint8_t *c1, bool C1_LE,
						   const uint8_t *c2, bool C2_LE,
						   const pka1_ec_lwrve_t *lwrve, uint8_t *scratch,
						   bool *is_reflected)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

	ret = reflected_coordinates_arg_check(econtext, cop,
					      c1, c2,
					      lwrve, scratch,
					      is_reflected);
	CCC_ERROR_CHECK(ret);

	/* default fail */
	*is_reflected = true;

	/*
	 * for Y coordinate => c1 + c2 == 0 mod p
	 *  -> reflected
	 *
	 * for X coordinate =>
	 *
	 *  c1 == c2 mod p -> reflected
	 *    <==>
	 *  c1 - c2 == 0 mod p -> reflected
	 */

	/* The modular library calls do not modify the &modpar content
	 */
	modpar.cmem    = econtext->cmem;
	modpar.m       = lwrve->p;
	modpar.x       = c1;
	modpar.y       = c2;
	modpar.size    = lwrve->nbytes;
	modpar.op_mode = lwrve->mode;

	// Results are in BIG ENDIAN (not setting LE flag)
	modpar.r = scratch;

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	if (BOOL_IS_TRUE(C1_LE)) {
		modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	}

	if (BOOL_IS_TRUE(C2_LE)) {
		modpar.mod_flags |= PKA1_MOD_FLAG_Y_LITTLE_ENDIAN;
	}

	if (CHECK_X_COORDINATE == cop) {
		ret = CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(econtext->engine, &modpar);
	} else {
		ret = CCC_ENGINE_SWITCH_MODULAR_ADDITION(econtext->engine, &modpar);
	}

	CCC_ERROR_CHECK(ret);

	*is_reflected = se_util_vec_is_zero(scratch, lwrve->nbytes, &rbad);

	FI_BARRIER(status, rbad);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_check_reflected_locked(se_engine_ec_context_t *econtext,
					  const pka1_ec_lwrve_t *lwrve,
					  uint8_t *scratch,
					  const te_ec_point_t *TP,
					  const te_ec_point_t *R)
{
	status_t ret = NO_ERROR;
	bool TP_le = ((TP->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) != 0U);
	bool R_le = ((R->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) != 0U);
	bool ref_x = true;
	bool ref_y = true;

	LTRACEF("entry\n");

	ret = check_reflected_coordinates_locked(econtext,
						 CHECK_X_COORDINATE,
						 TP->x, TP_le,
						 R->x,  R_le,
						 lwrve, scratch, &ref_x);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC reflect check X error\n"));

	ret = check_reflected_coordinates_locked(econtext,
						 CHECK_Y_COORDINATE,
						 TP->y, TP_le,
						 R->y,  R_le,
						 lwrve, scratch, &ref_y);
	CCC_ERROR_CHECK(ret,
			LTRACEF("EC reflect check Y error\n"));

	if (BOOL_IS_TRUE(ref_x) && BOOL_IS_TRUE(ref_y)) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SHAMIR_TRICK == 0 */

/* pmccabe "comlexity reduction" */
static status_t shamir_trick_check_args(const se_engine_ec_context_t *econtext,
					const te_ec_point_t *R,
					const uint8_t *u1_k,
					const uint8_t *u2_l,
					const te_ec_point_t *P2)
{
	status_t ret = NO_ERROR;

	if ((NULL == econtext) ||
	    (NULL == R) || (NULL == P2) ||
	    (NULL == u1_k) || (NULL == u2_l)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == econtext->ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	// XXXX FIXME => Need to verify that both (1 < u1_k < OPB) and (1 < u2_l < OBP)

fail:
	return ret;
}

#define SHAMIR_CHUNK_SIZE (sizeof_u32(te_ec_point_t) + nbytes)

/*  R = u1_k x P1 + u2_l x P2
 *
 * If P1 is NULL, use lwrve generator point as P1.
 *
 * The scalars u1_k and u2_l endianess specified in ec_scalar_flags,
 * big endian by default.
 */
status_t callwlate_shamirs_value(se_engine_ec_context_t *econtext,
				 te_ec_point_t *R,
				 const uint8_t *u1_k,
				 const te_ec_point_t *P1,
				 const uint8_t *u2_l,
				 const te_ec_point_t *P2,
				 uint32_t ec_scalar_flags)
 {
	 status_t ret = NO_ERROR;
	 uint32_t nbytes = 0U;
	 const pka1_ec_lwrve_t *lwrve = NULL;
	 struct se_data_params input;

	 LTRACEF("entry\n");

	 /* P1 can be NULL */
	 ret = shamir_trick_check_args(econtext, R, u1_k, u2_l, P2);
	 CCC_ERROR_CHECK(ret);

	 lwrve  = econtext->ec_lwrve;
	 nbytes = lwrve->nbytes;

	 // XXXX FIXME => Need to verify that both (1 < u1_k < OPB) and (1 < u2_l < OBP)
	 // XXXX  FW20 should trap these if invalid; don't know what the old FW does....

#if HAVE_SHAMIR_TRICK
	 /* Use Shamir's Trick to callwlate:
	  *  R(x1,y1) = u1_k x P1 + u2_l x P2
	  *
	  * If P1 is NULL => use lwrve generator point as P1.
	  */

	 /* Point_Q will be overwritten by the result of Shamir's trick
	  * and only two points are passed.
	  *
	  * R = u2_l x P2 + u1_k x P1 = u2_l x point_P + u1_k x point_Q
	  */
	 if (NULL == P1) {
		 se_util_mem_move(R->x, lwrve->g.gen_x, nbytes);
		 se_util_mem_move(R->y, lwrve->g.gen_y, nbytes);
		 R->point_flags = CCC_EC_POINT_FLAG_NONE;

		 if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
			 R->point_flags |= CCC_EC_POINT_FLAG_LITTLE_ENDIAN;
		 }
	 } else {
		 if (R != P1) {
			 CCC_OBJECT_ASSIGN(*R, *P1); /* Copy the point */
		 }
	 }

	 {
		 uint32_t saved_ec_flags = econtext->ec_flags;

		 HW_MUTEX_TAKE_ENGINE(econtext->engine);

		 econtext->ec_flags |= (ec_scalar_flags & CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN);
		 econtext->ec_flags |= (ec_scalar_flags & CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN);

		 /* Set scalar multiplier k for => u2_l x P2 == k x P */
		 econtext->ec_k = u2_l;
		 econtext->ec_k_length = nbytes;

		 /* Set scalar multiplier l for => u1_k x P1 == l x Q */
		 econtext->ec_l = u1_k;
		 econtext->ec_l_length = nbytes;

		 input.point_P = P2;
		 input.point_Q = R; /* Resulting point written here */

		 input.input_size  = sizeof_u32(te_ec_point_t);
		 input.output_size = sizeof_u32(te_ec_point_t);

		 ret = CCC_ENGINE_SWITCH_EC_SHAMIR_TRICK(econtext->engine, &input, econtext);

		 econtext->ec_flags = saved_ec_flags;
		 econtext->ec_k = NULL;
		 econtext->ec_l = NULL;
		 econtext->ec_k_length = 0U;
		 econtext->ec_l_length = 0U;

		 HW_MUTEX_RELEASE_ENGINE(econtext->engine);

		 CCC_ERROR_CHECK(ret);
	 }
 #else
	 {
		 /* Do not use Shamir's trick for what ever reason.
		  * This is much slower than the "real thing" above.
		  */
		 te_ec_point_t *TP = NULL;
		 uint8_t *co_buf = NULL;
		 uint8_t *rbuf = NULL;
		 te_ec_point_t *pbuf = NULL;
		 CCC_LOOP_VAR;

		 LTRACEF("Not using Shamir's trick\n");

		 /* No need to be contiguous but should be word aligned.
		  * Use this API to make that explicit
		  */
		 pbuf = CMTAG_MEM_GET_TYPED_BUFFER(econtext->cmem,
						   CMTAG_ALIGNED_BUFFER,
						   BYTES_IN_WORD,
						   te_ec_point_t *,
						   SHAMIR_CHUNK_SIZE);
		 if (NULL == pbuf) {
			 CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
		 }

		 /* First point space is used as a point
		  */
		 TP = &pbuf[0];

		 /* Space after the first point is used
		  * as byte vector (with size nbytes).
		  */
		 rbuf = (uint8_t *)&pbuf[1U];

		 /* nbytes size coordinate scratch buffer */
		 co_buf = &rbuf[0];

		 /* Callwlate point R the long way:
		  *  R(x1,y1) = u1_k x P1 + u2_l x P2 = k x P1 + l x P2
		  *
		  * R  = u1_k x P1 = k x P1
		  * P  = u2_l x P2 = l x P2
		  *
		  * If at this point
		  *  (R.x == P.x mod p) and (R.y + P.y == 0 mod p)
		  * then verification fails, the points are reflected
		  * and the addition would produce point at infinity.
		  *
		  * If not, proceed with
		  * R  = P1 + R
		  */
		 HW_MUTEX_TAKE_ENGINE(econtext->engine);

		 CCC_LOOP_BEGIN {
			 /* R = u1_k x P1
			  *
			  * If point multiply is passed NULL as P1 => uses lwrve
			  * generator point as P1 point.
			  */
			 ret = ec_point_multiply_locked(econtext, R, P1, u1_k, nbytes,
						(ec_scalar_flags & CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN) != 0U);
			 CCC_ERROR_END_LOOP(ret,
					    LTRACEF("Failed point scalar multiplication u1_k\n"));

			 /* TP = u2_l x P2
			  */
			 ret = ec_point_multiply_locked(econtext, TP, P2, u2_l, nbytes,
						(ec_scalar_flags & CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN) != 0U);
			 CCC_ERROR_END_LOOP(ret,
					    LTRACEF("Failed point scalar multiplication u2_l\n"));

			 // XXXX is this required when not using Shamir's trick ==>
			 // TODO verify if useful also in other contexts
			 //
			 ret = ec_check_reflected_locked(econtext, lwrve, co_buf, TP, R);
			 CCC_ERROR_END_LOOP(ret);

			 /* R = TP + R
			  */
			 input.point_P = TP;	/* IN */
			 input.point_Q = R;	/* IN/OUT (Resulting point written here) */

			 input.input_size  = sizeof_u32(te_ec_point_t);
			 input.output_size = sizeof_u32(te_ec_point_t);

			 ret = CCC_ENGINE_SWITCH_EC_ADD(econtext->engine, &input, econtext);
			 CCC_ERROR_END_LOOP(ret);

			 CCC_LOOP_STOP;
		 } CCC_LOOP_END;

		 HW_MUTEX_RELEASE_ENGINE(econtext->engine);

		 CMTAG_MEM_RELEASE(econtext->cmem,
				   CMTAG_ALIGNED_BUFFER,
				   pbuf);

		 CCC_ERROR_CHECK(ret);
	 }
 #endif /* HAVE_SHAMIR_TRICK */
 fail:
	 LTRACEF("exit: %d\n", ret);
	 return ret;
 }

#if CCC_WITH_ECDSA

static status_t extract_coordinates_from_asn1_ec_signature(const uint8_t *p_param,
							   uint32_t bytes_left_param,
							   te_ec_sig_t *parsed_sig,
							   uint32_t nbytes)
{
	status_t ret = NO_ERROR;
	const uint8_t *p = p_param;
	uint32_t bytes_left = bytes_left_param;
	uint32_t used_bytes = 0U;

	LTRACEF("entry\n");

	ret = ecdsa_get_asn1_ec_signature_coordinate(p,
						     bytes_left,
						     parsed_sig->r,
						     nbytes,
						     &used_bytes);
	CCC_ERROR_CHECK(ret,
			LTRACEF("ECDSA ASN.1 parse failed for 'r': %d\n",
				ret));

	if (used_bytes > bytes_left) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	bytes_left -= used_bytes;

	p = &p[used_bytes];
	used_bytes = 0U;

	ret = ecdsa_get_asn1_ec_signature_coordinate(p,
						     bytes_left,
						     parsed_sig->s,
						     nbytes,
						     &used_bytes);
	CCC_ERROR_CHECK(ret,
			LTRACEF("ECDSA ASN.1 parse failed for 's': %d\n",
				ret));

	if (used_bytes != bytes_left) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("Junk at end of ASN.1 signature\n"));
	}

	parsed_sig->sig_flags = CCC_EC_SIG_FLAG_NONE; // Data is in BE

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t parse_asn1_sig_check_args(const te_ec_sig_t *parsed_sig,
					  const struct se_data_params *input_params,
					  uint32_t nbytes)
{
	status_t ret = NO_ERROR;

	if ((NULL == parsed_sig) || (NULL == input_params) ||
	    (NULL == input_params->src_asn1) ||
	    (input_params->src_signature_size > MAX_EC_ASN1_SIG_LEN) ||
	    (0U == nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
fail:
	return ret;
}

/**
 * @brief Parse an openssl generated ASN.1 ecdsa signature blob into parsed_sig
 *
 * @param parsed_sig parsed signature R and S fields on success
 * @param input_params ASN.1 signature bytes and size
 * @param nbytes length of R and S signature fields (equals lwrve prime length)
 *
 * @return NO_ERROR on success, error otherwise
 */
static status_t ecdsa_parse_asn1_signature(te_ec_sig_t *parsed_sig,
					   const struct se_data_params *input_params,
					   uint32_t nbytes)
{
	status_t ret = NO_ERROR;
	const uint8_t *p = NULL;
	uint32_t bytes_left = 0U;	/**< bytes left in ASN.1 blob */
	uint32_t used_bytes = 0U;
	uint32_t seq_len = 0U;

	LTRACEF("entry\n");

	ret = parse_asn1_sig_check_args(parsed_sig,
					input_params,
					nbytes);
	CCC_ERROR_CHECK(ret);

	/* Should be in above function; here for coverity cert-c.
	 */
	if (0U == input_params->src_signature_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	p = input_params->src_asn1;

	/* Signature is an ASN.1 sequence */
	if (ASN1_TAG_SEQUENCE != *p) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	bytes_left = input_params->src_signature_size - 1U;
	p++;

	/* Get the length of the sequence */
	ret = asn1_get_integer(p, &seq_len, &used_bytes, bytes_left);
	CCC_ERROR_CHECK(ret);

	if ((seq_len > bytes_left) ||
	    (used_bytes > bytes_left)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	p = &p[used_bytes];

	if (bytes_left != (used_bytes + seq_len)) {
		/* SIG sequence content does not fit or there is
		 * additional junk at end
		 */
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	bytes_left -= used_bytes;
	used_bytes = 0U;

	/* There must be room for at least two encoded integers (1+1) with at
	 * least one byte of data for each vector (1) to have even the components
	 * present.
	 */
	if (bytes_left < (2U * 3U)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	se_util_mem_set((uint8_t *)parsed_sig, 0U, sizeof_u32(te_ec_sig_t));

	ret = extract_coordinates_from_asn1_ec_signature(p, bytes_left, parsed_sig, nbytes);
	CCC_ERROR_CHECK(ret);

 fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_sig_field_value_check(const pka1_ec_lwrve_t *lwrve,
					    const uint8_t *field,
					    bool field_LE)
{
	status_t ret = NO_ERROR;
	status_t rbad1 = ERR_BAD_STATE;
	uint32_t nbytes = 0U;
	uint32_t eflags = 0U;

	LTRACEF("entry\n");

	nbytes = lwrve->nbytes;

	if (BOOL_IS_TRUE(se_util_vec_is_zero(field, nbytes, &rbad1))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("zero sig field\n"));
	}

	if (NO_ERROR != rbad1) {
		CCC_ERROR_WITH_ECODE(rbad1);
	}
	rbad1 = ERR_BAD_STATE;

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		eflags |= CMP_FLAGS_VEC1_LE;
	}

	if (BOOL_IS_TRUE(field_LE)) {
		eflags |= CMP_FLAGS_VEC2_LE;
	}

	if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(lwrve->n, field,
						 nbytes, eflags, &rbad1))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("sig field too large\n"));
	}

	if (NO_ERROR != rbad1) {
		CCC_ERROR_WITH_ECODE(rbad1);
	}

 fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t verify_ecdsa_signature_fields(const pka1_ec_lwrve_t *lwrve,
					      const te_ec_sig_t *ecdsa_sig)
{
	status_t ret = NO_ERROR;
	bool sig_LE = false;

	LTRACEF("entry\n");

	sig_LE = (ecdsa_sig->sig_flags & CCC_EC_SIG_FLAG_LITTLE_ENDIAN) != 0U;

	ret = ecdsa_sig_field_value_check(lwrve, ecdsa_sig->s, sig_LE);
	CCC_ERROR_CHECK(ret);

	ret = ecdsa_sig_field_value_check(lwrve, ecdsa_sig->r, sig_LE);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ec_context_pubkey_is_defined(const se_engine_ec_context_t *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext->ec_pubkey) ||
	    ((econtext->ec_pubkey->point_flags & CCC_EC_POINT_FLAG_UNDEFINED) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("EC pubkey not defined\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_mod_op_lwrve_init(te_mod_params_t *mp,
					const pka1_ec_lwrve_t *lwrve)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	mp->m       = lwrve->n;
	mp->size    = lwrve->nbytes;
	mp->op_mode = lwrve->mode;

	mp->mod_flags = PKA1_MOD_FLAG_NONE;

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		mp->mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_mod_mult_lwrve_order_locked(const engine_t *engine,
						  const pka1_ec_lwrve_t *lwrve,
						  te_mod_params_t *modpar,
						  uint8_t *result,
						  const uint8_t *x,
						  const uint8_t *y)
{
	status_t ret = NO_ERROR;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	(void)lwrve;

	modpar->x = x;
	modpar->y = y;
	modpar->r = result;

	CCC_LOOP_BEGIN {
#if HAVE_GEN_MULTIPLY
		/* Does not use montgomery values; instead does DP
		 *  multiply followed by DP bit serial reduction.
		 *
		 * This does not use montgomery values.
		 *
		 * This alternative SHOULD be faster than the previous
		 * case (single multiply should be faster than
		 * montgomery mult followed by reduction?).
		 *
		 * But if the order montgomery values are configured,
		 * use them above.
		 *
		 * GEN_MULTIPLY not supported in T18X.
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_GEN_MULTIPLICATION(engine, modpar);
		CCC_ERROR_END_LOOP(ret);
#else
		/* This uses Montgomery values but no preconfigured mont
		 * values for this lwrve exist.
		 *
		 * This is always the slowest version since it does
		 * runtime montgomery value colwersions for lwrve order value
		 * in modpar->m
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_MULTIPLICATION(engine, modpar);
		CCC_ERROR_END_LOOP(ret);
#endif
		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pmult_derive_scalars(const engine_t *engine,
				     const pka1_ec_lwrve_t *lwrve,
				     const te_ec_sig_t *ecdsa_sig,
				     te_mod_params_t *modpar,
				     uint32_t message_digest_size,
				     const uint8_t *message_digest,
				     uint8_t *e,
				     uint8_t *u1,
				     uint8_t *u2,
				     uint8_t *w)
{
	status_t ret = NO_ERROR;

	uint32_t digest_len = message_digest_size;
	uint32_t nbytes = lwrve->nbytes;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	/* Results in w are in BIG ENDIAN (not setting LE flag) */
	modpar->r = w;

	HW_MUTEX_TAKE_ENGINE(engine);

	/* Mutex locked single loop */
	CCC_LOOP_BEGIN {
		const uint8_t *z = NULL;

		if ((ecdsa_sig->sig_flags & CCC_EC_SIG_FLAG_LITTLE_ENDIAN) != 0U) {
			modpar->mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
		}
		modpar->x = ecdsa_sig->s;

		ret = CCC_ENGINE_SWITCH_MODULAR_ILWERSION(engine, modpar);
		CCC_ERROR_END_LOOP(ret);

		/* Remove X endian */
		modpar->mod_flags &= ~PKA1_MOD_FLAG_X_LITTLE_ENDIAN;

		/* XXX Assuming here that digest must be at most the same byte
		 * length as lwrve size => TODO => VERIFY THIS!
		 *
		 * XXX FIXME!!!! Can we cut the digest like this???
		 * XXX => caller should do it instead => Maybe should FAIL HERE!
		 */
		if (digest_len > nbytes) {
			digest_len = nbytes;
		}

		/* Swap the digest to LE order
		 */
		ret = se_util_reverse_list(e, message_digest, digest_len);
		CCC_ERROR_END_LOOP(ret);

		/* Digest is now zero padded little endian scalar */
		z = e;
		modpar->mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;

		/* u1 = z * w (mod n)
		 *
		 * z is LE
		 * w is BE
		 * u1 is BE
		 */
		ret = ecdsa_mod_mult_lwrve_order_locked(engine, lwrve, modpar, u1, z, w);
		CCC_ERROR_END_LOOP(ret);

		/* Remove X endian */
		modpar->mod_flags &= ~PKA1_MOD_FLAG_X_LITTLE_ENDIAN;

		if ((ecdsa_sig->sig_flags & CCC_EC_SIG_FLAG_LITTLE_ENDIAN) != 0U) {
			modpar->mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
		}

		/* u2 = r * w (mod n)
		 *
		 * ecdsa_sig->r is signature endian
		 * w is BE
		 * u2 is BE
		 */
		ret = ecdsa_mod_mult_lwrve_order_locked(engine, lwrve, modpar, u2,
							ecdsa_sig->r, w);
		CCC_ERROR_END_LOOP(ret);

		/* Remove X endian */
		modpar->mod_flags &= ~PKA1_MOD_FLAG_X_LITTLE_ENDIAN;

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t do_ecdsa_verify(const pka1_ec_lwrve_t *lwrve,
				se_engine_ec_context_t *econtext,
				const te_ec_sig_t *ecdsa_sig,
				const uint8_t *message_digest,
				uint32_t message_digest_size,
				status_t *ret2_p,
				te_ec_point_t *G,
				uint8_t *mem,
				uint32_t nbytes)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };

	/* Allocate these to reduce stack usage
	 * Allocated in one chunk to optimize.
	 */
	uint8_t *w = NULL;
	uint8_t *u1 = NULL;
	uint8_t *u2 = NULL;
	uint8_t *e = NULL;
	uint32_t offset = 0U;

	LTRACEF("entry\n");

	*ret2_p = ERR_BAD_STATE;

	if (nbytes > TE_MAX_ECC_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	w  = &mem[offset];
	offset += nbytes;

	u1 = &mem[offset];
	offset += nbytes;

	u2 = &mem[offset];
	offset += nbytes;

	e  = &mem[offset];

	/* Sig fields verified by caller */

	/* Set the mod op params */

	modpar.cmem = econtext->cmem;

	/* The modular library calls do not modify the &modpar content,
	 * do lwrve specific inits.
	 */
	ret = ecdsa_mod_op_lwrve_init(&modpar, lwrve);
	CCC_ERROR_CHECK(ret);

	ret = pmult_derive_scalars(econtext->engine,
				   lwrve, ecdsa_sig,
				   &modpar,
				   message_digest_size, message_digest,
				   e, u1, u2, w);
	CCC_ERROR_CHECK(ret);

	/* Callwlate R = u1 x P1 + u2 x P2 either with or
	 * without Shamir's trick (compile time selection).
	 *
	 * Here the u1 and u2 are big endian => no flags required.
	 */
	ret = callwlate_shamirs_value(econtext,
				      G,
				      u1, NULL,
				      u2, econtext->ec_pubkey,
				      CCC_EC_FLAGS_NONE);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to callwlate shamir's trick\n"));

	/* Signature is valid if => r == x1 (mod n)
	 * ==> first reduce x1 then compare.
	 */

	/* reduce x1 (mod n)
	 * Reuse W buffer for the reduction result.
	 */
	if ((G->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	}
	modpar.x = G->x;

	/* If the SIGNATURE fields are in little endian => swap the result
	 *  to little endian as well, easier to compare.
	 */
	if ((ecdsa_sig->sig_flags & CCC_EC_SIG_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;
	}
	modpar.r = w;

	/* Need to set modulus again */
	modpar.m = lwrve->n;

	HW_MUTEX_TAKE_ENGINE(econtext->engine);
	ret = CCC_ENGINE_SWITCH_MODULAR_REDUCTION(econtext->engine, &modpar);
	HW_MUTEX_RELEASE_ENGINE(econtext->engine);
	CCC_ERROR_CHECK(ret);

	CCC_RANDOMIZE_EXELWTION_TIME;

	ret = ERR_BAD_STATE;
	if (!BOOL_IS_TRUE(se_util_vec_match(w, ecdsa_sig->r, nbytes, CFALSE, &ret))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Signature validation failed\n"));
	}
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(status, ret);

	/* implies to caller that the validation funtion compared
	 * the signature successfully
	 */
	*ret2_p = ret;
fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#define X_ECDSA_VERIFY_CHUNK_SIZE (sizeof_u32(te_ec_point_t) + (4U * nbytes))

static status_t ecdsa_verify(const pka1_ec_lwrve_t *lwrve,
			     se_engine_ec_context_t *econtext,
			     const te_ec_sig_t *ecdsa_sig,
			     const uint8_t *message_digest,
			     uint32_t message_digest_size,
			     status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *pmem = NULL;
	te_ec_point_t *G = NULL;
	uint8_t *vmem = NULL;
	uint32_t nbytes = lwrve->nbytes;

	LTRACEF("entry\n");

	/* 1) verify that r and s are in [1..n-1] */

	ret = verify_ecdsa_signature_fields(lwrve, ecdsa_sig);
	CCC_ERROR_CHECK(ret);

	if (nbytes > TE_MAX_ECC_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	pmem = CMTAG_MEM_GET_TYPED_BUFFER(econtext->cmem,
					  CMTAG_ALIGNED_BUFFER,
					  BYTES_IN_WORD,
					  te_ec_point_t *,
					  X_ECDSA_VERIFY_CHUNK_SIZE);
	if (NULL == pmem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	G    = &pmem[0];
	vmem = (uint8_t *)&pmem[1];

	ret = do_ecdsa_verify(lwrve, econtext, ecdsa_sig, message_digest, message_digest_size,
			      ret2_p, G, vmem, nbytes);
	CCC_ERROR_CHECK(ret);

fail:
	if (NULL != pmem) {
		se_util_mem_set((uint8_t *)pmem, 0U, X_ECDSA_VERIFY_CHUNK_SIZE);
		CMTAG_MEM_RELEASE(econtext->cmem,
				  CMTAG_ALIGNED_BUFFER,
				  pmem);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

 /* For ECDSA signatures the signature contains two big number values:
  * ESIG=(r,s) both of which have the size of the EC lwrve in bytes.
  * To verify ESIG the public key Qa is required.
  *
  * The signature input parameter format is represented as a const te_ec_sig_t
  *
  * ECDSA sig verification:
  * -----------------------
  *
  * As in wikipedia: must first validate the pubkey and then verify the signature =>
  *
  * A) Pubkey validation
  *    1) check that Qa is not equal to the identity element (O_ie) and coordinates are valid
  *    2) check that Qa is on lwrve
  *    3) check that n x Qa == Oie
  *
  * B) Sig verification
  *    1) Verify that r and s are integers in [1,n-1] (inclusive).
  *    2) e = HASH(m) (=> in input_params->sig_digest)
  *    3) z = Ln bits of e (Ln is the bitlength of the group order n)
  *    4) w = s^-1 (mod n)
  *    5) u1 = z * w (mod n) and u2 = r * w (mod n)
 *    6) R(x1,y1) = u1 x G + u2 x Qa (use Shamir's trick (if possible, Verify if restrictions apply!!!))
 *    7) signature is valid if => r == x1 (mod n)
 */
static status_t ecdsa_verify_check_args(const struct se_data_params *input_params,
					const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	if ((NULL == input_params->src) ||
	    (NULL == input_params->src_ec_signature)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_verify_context_pubkey(se_engine_ec_context_t *econtext,
					    const pka1_ec_lwrve_t *lwrve)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)lwrve;

	// XXX Should support callwlating pubkey if only priv key is given => TODO (later)
	ret = ec_context_pubkey_is_defined(econtext);
	CCC_ERROR_CHECK(ret);

#if POINT_OP_RESULT_VERIFY
	/* Next time used on Shamir's which on T23X verifies input points
	 */
	ret = ec_check_point_on_lwrve(econtext, lwrve, econtext->ec_pubkey);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Point (public key) is not on lwrve: %d\n", ret));
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_digest_message(const struct se_data_params *input_params,
				     const struct se_ec_context *ec_context,
				     uint8_t *digest,
				     uint32_t *hlen_p)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t hlen = 0U;

	LTRACEF("entry\n");

	ret = get_message_digest_size(ec_context->md_algorithm, &hlen);
	CCC_ERROR_CHECK(ret);

	input.src	  = input_params->src;
	input.input_size  = input_params->input_size;
	input.dst	  = digest;
	input.output_size = hlen;

	ret = sha_digest(ec_context->ec.cmem, ec_context->ec.domain,
			 &input, ec_context->md_algorithm);
	CCC_ERROR_CHECK(ret);

	*hlen_p = hlen;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_message_digest_setup(struct se_data_params *input_params,
					   const struct se_ec_context *ec_context,
					   uint8_t *digest)
{
	status_t ret = NO_ERROR;
	uint32_t hlen = 0U;

	LTRACEF("entry\n");

	if ((ec_context->ec.ec_flags & CCC_EC_FLAGS_DIGEST_INPUT_FIRST) != 0U) {
		ret = ecdsa_digest_message(input_params, ec_context, digest, &hlen);
		CCC_ERROR_CHECK(ret);

		input_params->src_digest      = digest;
		input_params->src_digest_size = hlen;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_verify_sig_setup(const struct se_data_params *input_params,
				       const struct se_ec_context *ec_context,
				       const te_ec_sig_t **ecdsa_sig_p,
				       te_ec_sig_t *parsed_sig)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((ec_context->ec.ec_flags & CCC_EC_FLAGS_ASN1_SIGNATURE) != 0U) {

		ret = ecdsa_parse_asn1_signature(parsed_sig, input_params,
						 ec_context->ec.ec_lwrve->nbytes);
		CCC_ERROR_CHECK(ret);

		*ecdsa_sig_p = parsed_sig;
	} else {
		/* Else use the signature object format */
		*ecdsa_sig_p = input_params->src_ec_signature;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define X_ECDSA_VERIFY_SIZE (sizeof_u32(te_ec_sig_t) + MAX_DIGEST_LEN)

static void se_ecdsa_verify_cleanup(const struct se_ec_context *ec_context,
				    te_ec_sig_t **smem_p)
{
	te_ec_sig_t *smem = *smem_p;

	if (NULL != smem) {
		se_util_mem_set((uint8_t *)smem, 0U, X_ECDSA_VERIFY_SIZE);
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_BUFFER,
				  smem);

		*smem_p = smem;
	}
}

status_t se_ecdsa_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	const te_ec_sig_t *ecdsa_sig = NULL;
	const pka1_ec_lwrve_t *lwrve = NULL;

	te_ec_sig_t *smem = NULL;
	te_ec_sig_t *parsed_sig = NULL;
	uint8_t *digest = NULL;

	LTRACEF("entry\n");

	ret = ecdsa_verify_check_args(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	smem = CMTAG_MEM_GET_TYPED_BUFFER(ec_context->ec.cmem,
					  CMTAG_BUFFER,
					  0U,
					  te_ec_sig_t *,
					  X_ECDSA_VERIFY_SIZE);
	if (NULL == smem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	parsed_sig = &smem[0];
	digest = (uint8_t *)&smem[1];

	/* Pubkey for verifying the signature is in ec_public
	 */
	lwrve = ec_context->ec.ec_lwrve;

	ret = ecdsa_verify_context_pubkey(&ec_context->ec, lwrve);
	CCC_ERROR_CHECK(ret);

	/* Support parsing the openssl generated ASN.1 formatted
	 * ECDSA signature blob
	 */
	ret = ecdsa_verify_sig_setup(input_params, ec_context, &ecdsa_sig, parsed_sig);
	CCC_ERROR_CHECK(ret);

	/* Do the signature validation with the digest
	 * passed in or callwlated here.
	 */
	ret = ecdsa_message_digest_setup(input_params, ec_context, digest);
	CCC_ERROR_CHECK(ret);

	/* Verify the digest against the signature after args
	 * validated.
	 */
	ret = ecdsa_verify(lwrve, &ec_context->ec, ecdsa_sig,
			   input_params->src_digest,
			   input_params->src_digest_size, &rbad);
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(status, rbad);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	se_ecdsa_verify_cleanup(ec_context, &smem);

	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA */

/*
 * Note: For signing it is ESSENTIAL that the value k is unique per signature.
 * If not: ECDSA can be broken, key Da can be attacked.
 *
 * Can be achieved by both proper random number generator and by deterministic value generator,
 * (e.g. SHA2(concat(EC private key + message digest))).
 *
 * XXX I think I'll use the latter, or then make it configurable (or just XOR together the
 * XXX above SHA2 result and a DRNG generated by the SE random number generator).
 *
 * To generate a signature we need:
 *  Da			(private key)
 *  e			(message digest)
 *  lwrve		(lwrve parameters)
 *  k			(secure random number)
 *
 * ECDSA sig generation:
 * ---------------------
 *
 * 1) e = HASH(m) (=> in input_params->sig_digest)
 * 2) z = Ln bits of e (Ln is the bitlength of the group order n. Ln may be larger than n, but not longer)
 * 3) select value k from [1,n-1] (inclusive; this MUST BE UNIQUE and CRYPTOGRAPHICALLY SECURE)
 * 4) (x1,y1) = k x G
 * 5) r = x1 (mod n)			(if r == 0 => goto 3)
 * 6) s = k^-1 * (z + r * Da) (mod n)	(if s == 0 => goto 3)
 * 7) signature is (r,s)
 */

#if CCC_WITH_ECDSA

#if HAVE_ECDSA_SIGN

static status_t sign_callwlate_s_locked(se_engine_ec_context_t *econtext,
					const pka1_ec_lwrve_t *lwrve,
					te_mod_params_t *mpar,
					uint8_t *w,
					const uint8_t *sig_r,
					const uint8_t *k_ilw,
					const uint8_t *z)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* w = r*Da (mod n) */
	ret = ecdsa_mod_mult_lwrve_order_locked(econtext->engine, lwrve, mpar, w, sig_r, econtext->ec_pkey);
	CCC_ERROR_CHECK(ret);

	/* w = (z + r*Da) (mod n)
	 *
	 * z is LE
	 */
	mpar->x = z;
	mpar->y = w;
	mpar->r = w;

	mpar->mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	ret = CCC_ENGINE_SWITCH_MODULAR_ADDITION(econtext->engine, mpar);
	mpar->mod_flags &= ~PKA1_MOD_FLAG_X_LITTLE_ENDIAN;

	CCC_ERROR_CHECK(ret);

	/* sig_r = (k^-1 * (z + r*Da)) (mod n) */
	ret = ecdsa_mod_mult_lwrve_order_locked(econtext->engine, lwrve, mpar, w, k_ilw, w);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define X_ECDSA_SIGN_CHUNK_SIZE (sizeof_u32(te_ec_point_t) + (4U * nbytes))
#define MAX_ECDSA_SIGN_LOOP_COUNT 10U

static status_t ecdsa_sign(const pka1_ec_lwrve_t *lwrve,
			   se_engine_ec_context_t *econtext,
			   te_ec_sig_t *ecdsa_sig,
			   const uint8_t *message_digest,
			   uint32_t message_digest_size,
			   status_t *ret2_p)
{
	status_t ret = NO_ERROR;

	te_ec_point_t *pmem = NULL;
	uint8_t *mem = NULL;
	uint32_t digest_len = 0U;
	te_mod_params_t modpar = { .size = 0U };
	const uint8_t *z = NULL;
	uint32_t nbytes = 0U;
	uint32_t counter = 0U;

	/* Allocate these to reduce stack usage
	 * Allocated in one chunk to optimize.
	 */
	uint8_t *w = NULL;
	uint8_t *e = NULL;
	uint8_t *k = NULL;
	uint8_t *sig_r = NULL;
	te_ec_point_t *R = NULL;

	uint32_t offset = 0U;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	*ret2_p = ERR_BAD_STATE;
	nbytes = lwrve->nbytes;

	if (nbytes > TE_MAX_ECC_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	modpar.cmem = econtext->cmem;

	pmem = CMTAG_MEM_GET_TYPED_BUFFER(econtext->cmem,
					  CMTAG_BUFFER,
					  0U,
					  te_ec_point_t *,
					  X_ECDSA_SIGN_CHUNK_SIZE);
	if (NULL == pmem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* First chunk of memory is a point
	 */
	R = &pmem[0];

	/* Rest of the memory after the point is used for byte vectors,
	 * each nbytes in size.
	 */
	mem = (uint8_t *)&pmem[1];

	w = &mem[offset];
	offset += nbytes;

	e  = &mem[offset];
	offset += nbytes;

	k = &mem[offset];
	offset += nbytes;

	sig_r  = &mem[offset];
	offset += nbytes;

	if (offset != (X_ECDSA_SIGN_CHUNK_SIZE - sizeof_u32(te_ec_point_t))) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Set the mod op defaults
	 * The modular library calls do not modify the &modpar content,
	 * do lwrve specific inits.
	 */
	ret = ecdsa_mod_op_lwrve_init(&modpar, lwrve);
	CCC_ERROR_CHECK(ret);

	/* 1) Callwlate e = HASH(m)
	 *
	 * BE message digest given as parameter.
	 * Twist e to become z.
	 */

	/* e = message digest (digests are in BIG ENDIAN)
	 */
	digest_len = message_digest_size;

	/* XXX Assuming here that digest must be at most the same byte
	 * length as lwrve size => TODO => VERIFY THIS!
	 *
	 * XXX FIXME!!!! Can we cut the digest like this???
	 * XXX => caller should do it instead => Maybe should FAIL HERE!
	 */
	if (digest_len > nbytes) {
		digest_len = nbytes;
	}

	// Need to mangle the digest because it may be shorter than nbytes
	ret = se_util_reverse_list(e, message_digest, digest_len);
	CCC_ERROR_CHECK(ret);

	/* z (digest) is now zero padded little endian scalar not longer than bit length
	 * of the group order n
	 */
	z = e;

	HW_MUTEX_TAKE_ENGINE(econtext->engine);

	/* Mutex locked loop.
	 */
	CCC_LOOP_BEGIN {
		/* Step 3 begins */
		bool zero = false;

		modpar.mod_flags = PKA1_MOD_FLAG_NONE;

		counter++;

		/* If random number gen is not working properly,
		 * signing not possible.
		 */
		if (counter >= MAX_ECDSA_SIGN_LOOP_COUNT) {
			CCC_ERROR_END_LOOP_WITH_ECODE(ERR_NOT_VALID);
		}

		/* Step 3: select cryptographically secure random integer k from [ 1..n-1 ]
		 *
		 * k is BE
		 */
		ret = pka_ec_generate_smaller_vec_BE_locked(econtext->engine,
							    lwrve->n,
							    k, nbytes,
							    ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U));
		CCC_ERROR_END_LOOP(ret);

		/* Step 4: Callwlate lwrve point R(x,y) = k * G
		 * G is lwrve base point.
		 */
		ret = ec_point_multiply_locked(econtext, R, NULL, k, nbytes, false);
		CCC_ERROR_END_LOOP(ret);

		/* Step 5: Callwlate r = x1 mod n.
		 * If r == 0, go back to step 3.
		 *
		 * sig_r is BE
		 * R->x is BE
		 */
		modpar.x = R->x;
		modpar.r = sig_r;

		ret = CCC_ENGINE_SWITCH_MODULAR_REDUCTION(econtext->engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		zero = se_util_vec_is_zero(sig_r, nbytes, &ret);
		CCC_ERROR_END_LOOP(ret);

		/* If r == 0, go back to step 3.
		 */
		if (BOOL_IS_TRUE(zero)) {
			continue;
		}

		/* Overwrite k with k^-1 (mod n),
		 * k == k_ilw
		 */
		modpar.x = k;
		modpar.r = k;

		ret = CCC_ENGINE_SWITCH_MODULAR_ILWERSION(econtext->engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		/* Step 6: Callwlate s = k^-1 * (z + r*Da) (mod n).
		 * If s == 0, goback to step 3.
		 *
		 * s  is BE
		 * Da is BE
		 * z  is LE
		 * k  is BE
		 *
		 * On successfull return w contains s (BE)
		 */
		ret = sign_callwlate_s_locked(econtext, lwrve, &modpar, w, sig_r, k, z);
		CCC_ERROR_END_LOOP(ret);

		zero = se_util_vec_is_zero(w, nbytes, &ret);
		CCC_ERROR_END_LOOP(ret);

		/* If s == 0, go back to step 3.
		 */
		if (BOOL_IS_TRUE(zero)) {
			continue;
		}

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(econtext->engine);
	CCC_ERROR_CHECK(ret);

	*ret2_p = NO_ERROR;

	LOG_HEXBUF("ECDSA signature R",  sig_r, nbytes);
	LOG_HEXBUF("ECDSA signature S",  w, nbytes);

	/* Return big endian signature <r,s>+flags
	 *
	 * Caller may reformat it to client format.
	 */
	ecdsa_sig->sig_flags = CCC_EC_SIG_FLAG_NONE;
	se_util_mem_move(ecdsa_sig->r, sig_r, nbytes);
	se_util_mem_move(ecdsa_sig->s, w, nbytes);
fail:
	if (NULL != pmem) {
		se_util_mem_set((uint8_t *)pmem, 0U, X_ECDSA_SIGN_CHUNK_SIZE);
		CMTAG_MEM_RELEASE(econtext->cmem,
				  CMTAG_BUFFER,
				  pmem);
	}

	se_util_mem_set((uint8_t *)&modpar, 0U, sizeof_u32(modpar));

	LTRACEF("Exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_sign_input_check(const struct se_data_params *input_params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == input_params->src_digest) ||
	    (NULL == input_params->dst_ec_signature) ||
	    (input_params->output_size < sizeof_u32(te_ec_sig_t))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

static status_t ecdsa_signature_to_asn1(struct se_data_params *input_params,
					uint32_t nbytes,
					te_ec_sig_t *ecdsa_sig)
{
	status_t ret = NO_ERROR;
	uint8_t *asn1_sig = input_params->dst;
	uint32_t off = 2U;
	uint32_t rlen = 0U;
	uint32_t slen = 0U;
	uint32_t len = 0U;
	uint32_t lenlen = 1U;

	LTRACEF("entry\n");

	asn1_sig[0] = ASN1_TAG_SEQUENCE;

	/* asn1_sig[1] is ASN.1 sequence length, set below.
	 * In case nbytes >= 64 the sequence length encoding needs two
	 * bytes, asn1_sig[1] and asn1_sig[2].
	 *
	 * As a matter of corner case, also encoding two 62 byte vectors
	 * where both have high bit set will get two byte encoding.
	 */
	LOG_HEXBUF("ECDSA(R)", ecdsa_sig->r, nbytes);
	LOG_HEXBUF("ECDSA(S)", ecdsa_sig->s, nbytes);

	/* Encoding one ASN.1 integer of less than 128 bytes adds two
	 * meta bytes (<TAG><LEN>) to the vector byte length. Longer
	 * than 127 byte integers are not supported now, trapped later
	 * below. (127 because if msb set a zero byte prefix added).
	 *
	 * Since we encode two same length ASN.1 integers in a
	 * sequence, the sequence length is encoded as two bytes if
	 * nbytes >= 63 (2+63 == 65; 2*65 = 130, which is > 128).
	 * ASN1_INT_MAX_ONE_BYTE_VALUE == 128.
	 *
	 * If 62, length of sequence length encoding depends on the
	 * msb of the vectors. If either one or both have msb set, the
	 * length will be at least (62+2)+(63+2) == 129, which is >
	 * 128.
	 *
	 * With a 61 byte vectors, even when both have msb set and get
	 * padded, the sequence will be 2*(2+1+61) == 128 bytes, which
	 * encodes as one length byte (0x80) for the sequence.
	 *
	 * So the ASN.1 sequence length encoding fits in one byte
	 * (0x06..0x80) when <= 128 and it takes two bytes when in
	 * range 129..255 (0x81 0x<len>). And when DER encoding the
	 * minimal encoding must be used.
	 *
	 * Either way, sequence length fits in one or two bytes in all
	 * cases, when encoded integers take less than 126 bytes each.
	 *
	 * 0x06 because the integers must contain at least one byte
	 * and encoding two such ASN.1 integer takes at least 3 bytes
	 * each.
	 */
	if (nbytes >= 63U) {
		off++;
		lenlen++;
	} else if (nbytes == 62U) {
		if (((ecdsa_sig->r[0] & 0x80U) != 0U) ||
		    ((ecdsa_sig->s[0] & 0x80U) != 0U)) {
			off++;
			lenlen++;
		}
	}

	/* Encode nbytes long value into big endian ASN.1 integer as
	 * 0x02 <LEN> <ZPAD> <VALUE>
	 *
	 * Optional <ZPAD> is 0x00 or missing, depending on <VALUE> msb.
	 *
	 * Sets the encoding length.
	 */
	ret = encode_vector_as_asn1_integer(&asn1_sig[off],
					    &ecdsa_sig->r[0], nbytes, &rlen);
	CCC_ERROR_CHECK(ret, LTRACEF("ECDSA ASN.1 encoding R failed\n"));

	off = off + rlen;

	ret = encode_vector_as_asn1_integer(&asn1_sig[off],
					    &ecdsa_sig->s[0], nbytes, &slen);
	CCC_ERROR_CHECK(ret, LTRACEF("ECDSA ASN.1 encoding S failed\n"));

	len = rlen + slen;

	if (len > ASN1_INT_MAX_ONE_BYTE_VALUE) {
		if (2U != lenlen) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("Inconsistency in ASN.1 length encoder\n"));
		}

		if (len > 255U) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("Unsupported ECDSA sig SEQ length %u\n",
						     len));
		}

		/* Patch the ASN.1 sequence length in bytes 1 and 2.
		 *
		 * Byte 1 indicates (value 0x81) that the length encoding consists of
		 * one additional byte containing the value 0..255 in byte 2
		 * which is sufficient for all ECDSA signature lengths lwrrently
		 * supported (as a DER encoded ASN.1 length value).
		 */
		asn1_sig[1] = 0x81U;
		asn1_sig[2] = BYTE_VALUE(len);
	} else {
		if (1U != lenlen) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     LTRACEF("Inconsistency of ASN.1 length encoder\n"));
		}

		/* Patch the ASN.1 sequence length in the single byte length */
		asn1_sig[1] = BYTE_VALUE(len);
	}

	off = off + slen;

	input_params->output_size = off;

	LOG_HEXBUF("ECDSA(asn1)", asn1_sig, off);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA11_SUPPORT
static status_t ecdsa_keystore_sign(const pka1_ec_lwrve_t *lwrve,
				    se_engine_ec_context_t *econtext,
				    te_ec_sig_t *ecdsa_sig,
				    const uint8_t *message_digest,
				    uint32_t message_digest_size,
				    status_t *ret2_p)
{
	status_t ret = ERR_BAD_STATE;
	uint32_t digest_len = 0U;
	uint32_t nbytes = 0U;
	struct se_data_params input;

	LTRACEF("entry\n");

	*ret2_p = ERR_BAD_STATE;
	nbytes = lwrve->nbytes;

	if (nbytes > TE_MAX_ECC_BYTES) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	digest_len = message_digest_size;

	/* Digest must be at most the same byte
	 * length as lwrve size.
	 *
	 * Cut the digest to lwrve byte size.
	 */
	if (digest_len > nbytes) {
		digest_len = nbytes;
	}

	input.src_digest       = message_digest;
	input.input_size       = digest_len;
	input.dst_ec_signature = ecdsa_sig;
	input.output_size      = sizeof_u32(te_ec_sig_t);

	HW_MUTEX_TAKE_ENGINE(econtext->engine);

	/* Sign the digest with the ECC keystore key */
	*ret2_p = NO_ERROR;
	ret     = CCC_ENGINE_SWITCH_EC_ECDSA_SIGN(econtext->engine, &input, econtext);

	HW_MUTEX_RELEASE_ENGINE(econtext->engine);
	CCC_ERROR_CHECK(ret);

	/* Return big endian signature <r,s>+flags
	 *
	 * Caller may reformat it to client format.
	 */
	LOG_HEXBUF("ECDSA signature R",  ecdsa_sig->r, nbytes);
	LOG_HEXBUF("ECDSA signature S",  ecdsa_sig->s, nbytes);
fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA11_SUPPORT */

static status_t ecdsa_sign_method(const pka1_ec_lwrve_t *lwrve,
				  se_engine_ec_context_t *econtext,
				  te_ec_sig_t *ecdsa_sig,
				  struct se_data_params *input_params,
				  status_t *rbad_p)
{
	status_t ret = ERR_NOT_SUPPORTED;
	status_t rbad = ERR_BAD_STATE;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
#if HAVE_LWPKA11_SUPPORT

#if HAVE_LWPKA11_FORCE_ECDSA_SIGN_KEYSTORE
		/* If subsystem wants to *only* sign ECDSA by keystore keys.
		 */
		ret = ecdsa_keystore_sign(lwrve, econtext, ecdsa_sig,
					  input_params->src_digest,
					  input_params->src_digest_size, &rbad);
		CCC_LOOP_STOP;
#else
		/* If LWPKA11 is supported, allow runtime selection of ECDSA sign with
		 * EC KS keys or with CCC key object keys:
		 *
		 * If CCC_EC_FLAGS_USE_KEYSTORY is set => sign with EC KS key,
		 * otherwise sign with CCC key object key.
		 */
		if ((econtext->ec_flags & CCC_EC_FLAGS_USE_KEYSTORE) != 0U) {
			ret = ecdsa_keystore_sign(lwrve, econtext, ecdsa_sig,
						  input_params->src_digest,
						  input_params->src_digest_size, &rbad);
			CCC_LOOP_STOP;
		}

		/* If flags did not request EC KS sign => sign with
		 * CCC key object keys.
		 */
		ret = ecdsa_sign(lwrve, econtext, ecdsa_sig,
				 input_params->src_digest,
				 input_params->src_digest_size, &rbad);
		CCC_LOOP_STOP;
#endif /* HAVE_LWPKA11_FORCE_ECDSA_SIGN_KEYSTORE */

#else
		/* Without LWPKA11 support always sign with CCC key object keys.
		 */
		ret = ecdsa_sign(lwrve, econtext, ecdsa_sig,
				 input_params->src_digest,
				 input_params->src_digest_size, &rbad);
		CCC_LOOP_STOP;
#endif /* HAVE_LWPKA11_SUPPORT */
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	*rbad_p = rbad;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define X_ECDSA_SIGN_SIZE (sizeof_u32(te_ec_sig_t) + MAX_DIGEST_LEN)

status_t se_ecdsa_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	const pka1_ec_lwrve_t *lwrve = NULL;

	te_ec_sig_t *smem = NULL;
	te_ec_sig_t *ecdsa_sig = NULL;
	uint8_t *digest = NULL;

	LTRACEF("entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	smem = CMTAG_MEM_GET_TYPED_BUFFER(ec_context->ec.cmem,
					  CMTAG_BUFFER,
					  0U,
					  te_ec_sig_t *,
					  X_ECDSA_SIGN_SIZE);
	if (NULL == smem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ecdsa_sig = &smem[0];
	digest = (uint8_t *)&smem[1];

	/* Do the signing with the digest
	 * passed in or callwlated here.
	 */
	ret = ecdsa_message_digest_setup(input_params, ec_context, digest);
	CCC_ERROR_CHECK(ret);

	ret = ecdsa_sign_input_check(input_params);
	CCC_ERROR_CHECK(ret);

	/* Private for signing is in ec_pkey
	 */
	lwrve = ec_context->ec.ec_lwrve;

	/* Sign the digest after args validated.
	 */
	ret = ecdsa_sign_method(lwrve, &ec_context->ec, ecdsa_sig,
				input_params, &rbad);
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(status, rbad);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	if ((ec_context->ec.ec_flags & CCC_EC_FLAGS_ASN1_SIGNATURE) != 0U) {
		ret = ecdsa_signature_to_asn1(input_params,
					      lwrve->nbytes,
					      ecdsa_sig);
		CCC_ERROR_CHECK(ret);
	} else {
		/* Assign the object context to the dst signature object
		 */
		CCC_OBJECT_ASSIGN(*input_params->dst_ec_signature, *ecdsa_sig);
		input_params->output_size = sizeof_u32(te_ec_sig_t);
	}
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	if (NULL != smem) {
		se_util_mem_set((uint8_t *)smem, 0U, X_ECDSA_SIGN_SIZE);
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_BUFFER, smem);
	}

	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#else
status_t se_ecdsa_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	LTRACEF("Entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	ret = SE_ERROR(ERR_NOT_SUPPORTED);
fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_ECDSA_SIGN */
#endif /* CCC_WITH_ECDSA */

#if CCC_WITH_ECDSA || CCC_WITH_ED25519

status_t se_ecc_process_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	const pka1_ec_lwrve_t *lwrve = NULL;

	LTRACEF("Entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	/* Pubkey for verifying the signature is in k_ec_public
	 */
	lwrve = ec_context->ec.ec_lwrve;

	switch (lwrve->type) {
#if CCC_WITH_ECDSA
	case PKA1_LWRVE_TYPE_WEIERSTRASS:
		ret = se_ecdsa_verify(input_params, ec_context);
		break;
#endif
#if CCC_WITH_ED25519
	case PKA1_LWRVE_TYPE_ED25519:
		ret = se_ed25519_verify(input_params, ec_context);
		break;
#endif
	default:
		LTRACEF("Lwrve type 0x%x signature verifications not supported\n",
			lwrve->type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

status_t se_ecc_process_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	const pka1_ec_lwrve_t *lwrve = NULL;

	LTRACEF("Entry\n");

	ret = ec_op_check_args_normal(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	lwrve = ec_context->ec.ec_lwrve;

	switch (lwrve->type) {
#if CCC_WITH_ECDSA
	case PKA1_LWRVE_TYPE_WEIERSTRASS:
		ret = se_ecdsa_sign(input_params, ec_context);
		break;
#endif
#if CCC_WITH_ED25519
	case PKA1_LWRVE_TYPE_ED25519:
		ret = se_ed25519_sign(input_params, ec_context);
		break;
#endif
	default:
		LTRACEF("Lwrve type 0x%x for EC signing not supported\n",
			lwrve->type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECDSA || CCC_WITH_ED25519 */

#if USE_SMALLER_VEC_GEN

#ifndef SMALLER_BE_VALUE_RETRY_LIMIT
#define SMALLER_BE_VALUE_RETRY_LIMIT 10U
#endif

#define ZERO_BYTE_CHECK_LIMIT 8U

#if (HAVE_PKA1_TRNG || HAVE_RNG1_DRNG) == 0
//
// XXX Maybe could also use SE DRNG for generating randomness
// XXX  but must analyze for mutex deadlocks (both PKA1 mutex and SE mutex
// XXX  would need to be locked at the same time to do this).
//
// XXX  Lwrrently requires RNG1 or TRNG support.
//
#error "CCC needs a random number generator for pka1_ec_generate_smaller_vec_BE_locked()"
#endif

/* PKA mutex held by the caller
 */
static status_t get_rng_bytes_locked(const engine_t *engine,
				     uint8_t *buf, uint32_t blen)
{
	status_t ret  = ERR_BAD_STATE;
	uint32_t zlen = ZERO_BYTE_CHECK_LIMIT;

	(void)engine;

	LTRACEF("entry\n");

	FI_BARRIER(status, ret);

	/* Make sure the value is rejected by caller if HW generators
	 * do not work for any reason.
	 */
	if (blen < zlen) {
		zlen = blen;
	}
	se_util_mem_set(buf, 0U, zlen);

#if HAVE_RNG1_DRNG
	/* RNG1 uses it's own mutexes and engine managed by the callee =>
	 * state of PKA mutex does not matter here.
	 */
	ret = rng1_generate_drng(buf, blen);
	/* FALLTHROUGH */
#endif

#if HAVE_PKA1_TRNG
	if (NO_ERROR != ret) {
		/* This gets run in case RNG1 failed or does not exist.
		 */
		ret = pka1_trng_generate_locked(engine, buf, blen);
		/* FALLTHROUGH */
	}
#endif
	CCC_ERROR_CHECK(ret, LTRACEF("smaller vec gen: failed to generate random value\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t check_gen_BE_value(bool *ok, uint8_t *gen_value, uint32_t len,
				   const uint8_t *boundary, bool boundary_LE)
{
	status_t ret = ERR_BAD_STATE;
	uint32_t zlen = ZERO_BYTE_CHECK_LIMIT;

	LTRACEF("entry\n");

	*ok = false;

	if (len < zlen) {
		zlen = len;
	}

	if (BOOL_IS_TRUE(se_util_vec_is_zero(gen_value, zlen, &ret))) {
		/* Too many zeros in sequence are not acceptable.
		 * Unexpected to have HW generator output ZERO_BYTE_CHECK_LIMIT
		 * consequtive zeros, so reject the value is so.
		 */
		LTRACEF("Generated value is zero in first %u bytes\n", zlen);
	} else {
		uint32_t eflags = 0U;

		CCC_ERROR_CHECK(ret);

		/* gen_value contains a random number, treated as BE
		 */
		if (BOOL_IS_TRUE(boundary_LE)) {
			eflags |= CMP_FLAGS_VEC1_LE;
		}

		ret = ERR_BAD_STATE;

		/* ret ecode checked after loop
		 */
		if (BOOL_IS_TRUE(se_util_vec_cmp_endian(boundary, gen_value, len, eflags, &ret))) {
			*ok = true;
			LTRACEF("Generated value smaller than reference value\n");
		}
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define GEN_FIX_BYTES 8U

static status_t fix_gen_msb(const engine_t *engine,
			    bool *ok,
			    uint8_t *gen_value,
			    uint32_t bmsb)
{
	status_t ret  = ERR_BAD_STATE;
	uint8_t  fv[GEN_FIX_BYTES] = { 0x0U };
	uint32_t gmsb = gen_value[0];
	bool     done = false;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if (gmsb < bmsb) {
		/* Entered here because this was not true
		 */
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Generate more random bytes to select the msb from
	 */
	ret = get_rng_bytes_locked(engine, fv, GEN_FIX_BYTES);
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(se_util_vec_is_zero(fv, GEN_FIX_BYTES, &ret))) {
		CCC_ERROR_CHECK(ret);

		for (; inx < GEN_FIX_BYTES; inx++) {
			if (fv[inx] < BYTE_VALUE(bmsb)) {
				if (!BOOL_IS_TRUE(done)) {
					gen_value[0] = fv[inx];
					done = true;
				}
			}
		}

		FI_BARRIER(u32, inx);

		if (inx != GEN_FIX_BYTES) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}

		*ok = done;
	}
fail:
	se_util_mem_set(fv, 0U, GEN_FIX_BYTES);
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_smaller_BE_value_locked(const engine_t *engine,
					    const uint8_t *boundary,
					    bool boundary_LE,
					    uint8_t *gen_value,
					    uint32_t len,
					    status_t *rbad_p,
					    uint32_t bmsb)
{
	status_t ret   = ERR_BAD_STATE;
	status_t rbad  = ERR_BAD_STATE;
	uint32_t count = 0U;

	CCC_LOOP_VAR;

	*rbad_p = ret;

	CCC_LOOP_BEGIN {
		bool ok = false;

		count++;
		if (count > SMALLER_BE_VALUE_RETRY_LIMIT) {
			CCC_ERROR_END_LOOP_WITH_ECODE(ERR_TOO_BIG,
						      LTRACEF("Failed to generate value buffer content\n"));
		}

		/* PKA Mutex held, do not call CCC_GENERATE_RANDOM
		 */
		ret = get_rng_bytes_locked(engine, gen_value, len);
		CCC_ERROR_END_LOOP(ret);

		ret = check_gen_BE_value(&ok, gen_value, len, boundary, boundary_LE);
		CCC_ERROR_END_LOOP(ret);

		if (!BOOL_IS_TRUE(ok) && (0U != bmsb)) {
			/* Generated random value but value is too big.
			 * If there is a chance to fix it by making MSB smaller,
			 * try that first before generating the whole buffer value.
			 */
			ret = fix_gen_msb(engine, &ok, gen_value, bmsb);
			CCC_ERROR_END_LOOP(ret);
		}

		if (BOOL_IS_TRUE(ok)) {
			/* Value acceptable, terminate generation loop
			 */
			rbad = NO_ERROR;
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

	FI_BARRIER(status, rbad);
fail:
	*rbad_p = rbad;
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

/*
 * Generates a len byte BE value 0 < gen_value < cmp_value or fail.
 *
 * Note: hold PKA mutex when calling this (mandatory if TRNG gets used)
 */
status_t pka_ec_generate_smaller_vec_BE_locked(const engine_t *engine,
					       const uint8_t *boundary,
					       uint8_t *gen_value, uint32_t len,
					       bool boundary_LE)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint32_t bmsb = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == boundary) ||
	    (NULL == gen_value) || (0U == len)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Request to generate %u byte value limited BE byte vector\n", len);

	/* get vector msb */
	bmsb = boundary[0U];
	if (BOOL_IS_TRUE(boundary_LE)) {
		bmsb = boundary[len - 1U];
	}

	ret = get_smaller_BE_value_locked(engine, boundary, boundary_LE,
					  gen_value, len, &rbad, bmsb);
	CCC_ERROR_CHECK(ret);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#if HAVE_GENERATE_EC_PRIVATE_KEY
/* Generate a (valid) random pkey for the lwrve or fail.
 *
 * Key must be in range [ 1 .. n-1 ] (inclusive) where n is the lwrve order.
 *
 * Generated key is in BE.
 */
status_t pka_ec_generate_pkey_BE(const engine_t *engine,
				 const pka1_ec_lwrve_t *lwrve, uint8_t *key, uint32_t klen)
{
	status_t ret = NO_ERROR;
	bool is_LE = false;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == lwrve) ||
	    (NULL == key) || (lwrve->nbytes != klen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* random number, treated as requested endian */
	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		is_LE = true;
	}

	HW_MUTEX_TAKE_ENGINE(engine);
	ret = pka_ec_generate_smaller_vec_BE_locked(engine, lwrve->n, key, klen, is_LE);
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_GENERATE_EC_PRIVATE_KEY */
#endif /* USE_SMALLER_VEC_GEN */

#endif /* CCC_WITH_ECC */
