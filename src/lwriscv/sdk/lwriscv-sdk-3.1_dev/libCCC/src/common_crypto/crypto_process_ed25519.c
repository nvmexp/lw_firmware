/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* process ED25519 signatures
 */
#include <crypto_system_config.h>

#if CCC_WITH_ECC && CCC_WITH_ED25519 && CCC_WITH_MODULAR

#include <crypto_derive_proc.h>
#include <crypto_md_proc.h>
#include <crypto_process_call.h>

#include <crypto_ec.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#define CCC_ED25519_LWRVE_BLEN 32U

#define CCC_SHA512_DIGEST_LEN 64U
#define CCC_ED25519_SIGNATURE_LEN 64U

/* Adds support for compressing points */
#define HAVE_POINT_COMPRESS 1


/* Little endian constants for ED25519 */

static const struct ed25519_const_s {
	const uint8_t LE_sqrt_minus_1[CCC_ED25519_LWRVE_BLEN];
	const uint8_t LE_p_minus2[CCC_ED25519_LWRVE_BLEN];
	const uint8_t LE_p_plus3_div8[CCC_ED25519_LWRVE_BLEN];
	const uint8_t LE_1[CCC_ED25519_LWRVE_BLEN];
#if VERIFY_GROUP_EQUATION_LONG_FORM
	const uint8_t LE_8[CCC_ED25519_LWRVE_BLEN];
#endif
	const uint8_t LE_d[CCC_ED25519_LWRVE_BLEN];
} ed25519_cvalue = {
	/* Square root of -1 (mod p) */
	.LE_sqrt_minus_1 = {
		0xB0, 0xA0, 0x0E, 0x4A, 0x27, 0x1B, 0xEE, 0xC4,
		0x78, 0xE4, 0x2F, 0xAD, 0x06, 0x18, 0x43, 0x2F,
		0xA7, 0xD7, 0xFB, 0x3D, 0x99, 0x00, 0x4D, 0x2B,
		0x0B, 0xDF, 0xC1, 0x4F, 0x80, 0x24, 0x83, 0x2B,
	},

	/* -2 (mod p), p-2 */
	.LE_p_minus2 = {
		0xEB, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
	},

	/* (p+3) IDIV 8 */
	.LE_p_plus3_div8 = {
		0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,
	},

	/* 1 */
	.LE_1 = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},

#if VERIFY_GROUP_EQUATION_LONG_FORM
	.LE_8 = {
		0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	},
#endif

	/* Little endian version of lwrve->d constant
	 * Value is (-121665/121666) mod p
	 */
	.LE_d = {
		0xA3, 0x78, 0x59, 0x13, 0xCA, 0x4D, 0xEB, 0x75,
		0xAB, 0xD8, 0x41, 0x41, 0x4D, 0x0A, 0x70, 0x00,
		0x98, 0xE8, 0x79, 0x77, 0x79, 0x40, 0xC7, 0x8C,
		0x73, 0xFE, 0x6F, 0x2B, 0xEE, 0x6C, 0x03, 0x52,
	},
};

/* Input for ED25519 signature validation:
 *
 *  signature <R || S>   : 64 byte signature, consists of <s[32], r[32]>.
 *
 *  compressed point A   : 32 byte compressed public key to verify signature.
 *                         (Set this to k_ec_public.ed25519_compressed_point
 *                          and set the point flag CCC_EC_POINT_FLAG_COMPRESSED_ED25519)
 *
 *  message M		 : Message or the SHA512 digest of a message.
 */

static status_t ed25519_first_root_candidate(const engine_t   *engine,
					     uint32_t	       nbytes,
					     te_mod_params_t  *modpar,
					     uint8_t	      *x_root,
					     uint8_t	      *tmp,
					     uint8_t	      *x2,
					     const uint8_t    *Py,
					     uint8_t	      *xx,
					     uint8_t	      *u,
					     uint8_t	      *v)
{
	status_t ret = NO_ERROR;

	/* u=y^2 mod p */
	modpar->x = Py;
	modpar->r = u;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_SQR(engine, modpar);
	CCC_ERROR_CHECK(ret);

#if PKA_MODULAR_OPERATIONS_PRESERVE_MODULUS
	/* The above op sets the modulus. It stays valid in this sequence
	 * because the device mutex is locked and mod operations do not modify it.
	 *
	 * No need to rewrite the modulus for the subsequent ops.
	 */
	modpar->m = NULL;
#endif

	/* Save (y^2) to tmp (used also for value v callwlation) */
	se_util_mem_move(tmp, u, nbytes);

	/* u=y^2-1 mod p (TODO: Possibly optimize) */
	modpar->x = u;
	modpar->y = ed25519_cvalue.LE_1;
	modpar->r = u;

	ret = CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* v=d*y^2 mod p */
	modpar->x = ed25519_cvalue.LE_d;
	modpar->y = tmp;
	modpar->r = v;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_MULT(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* v=v+1 mod p (TODO: Possibly optimize) */
	modpar->x = v;
	modpar->y = ed25519_cvalue.LE_1;
	modpar->r = v;

	ret = CCC_ENGINE_SWITCH_MODULAR_ADDITION(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Now u = (y^2-1) (mod p) and v = (d*y^2+1) (mod p) */

	/* tmp=1/v=v^(p-2) mod p */
	modpar->x = v;
	modpar->y = ed25519_cvalue.LE_p_minus2;
	modpar->r = tmp;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_EXP(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Here tmp = (v_ilw) =  1/v = 1/(d*y^2+1) (mod p) */

	/* x2 = u*(1/v) mod p */
	modpar->x = u;
	modpar->y = tmp;
	modpar->r = x2;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_MULT(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Here x2 =  u/v = (y^2-1)/(d*y^2+1) */

	/* x_root(1) = x2^((p+3)/8) mod p */
	modpar->x = x2;
	modpar->y = ed25519_cvalue.LE_p_plus3_div8;
	modpar->r = x_root;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_EXP(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Here x_root(1) is the first candidate value for "square
	 * root of x2" (mod p)
	 */

	/* Callwlate some more values to check if this is the case */

	/* xx=x_root^2 mod p */
	modpar->x = x_root;
	modpar->y = NULL;
	modpar->r = xx;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_SQR(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Here xx == x_root*x_root mod p and x2 == x^2 */

	modpar->x = xx;
	modpar->y = x2;
	modpar->r = tmp;
	ret = CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Now tmp is a zero vector if x_root is square root
	 * Let the caller verify that.
	 */
fail:
	return ret;
}

static status_t ed25519_second_root_candidate(const engine_t  *engine,
					      te_mod_params_t *modpar,
					      uint8_t	      *x_root,
					      uint8_t	      *xx,
					      const uint8_t   *x2,
					      uint8_t	      *tmp)
{
	status_t ret = NO_ERROR;

	/* x_root(2) = square_root_of_minus_1 * x_root mod p
	 *
	 * -1 is a square in Fq. sqrt(-1) value is 2^((p-1)/4)
	 */
	modpar->x = x_root;
	modpar->y = ed25519_cvalue.LE_sqrt_minus_1;
	modpar->r = x_root;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_MULT(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Here x_root(2) is the "other candidate" value for
	 * "square root of x2" (mod p)
	 *
	 * Check if the second candidate is the square root of
	 * x2 (mod p)
	 */

	/* Check if the second root candidate now in x_root actually
	 * is the square root.
	 */

	/* xx = x_root^2 mod p */
	modpar->x = x_root;
	modpar->y = NULL;
	modpar->r = xx;

	ret = CCC_ENGINE_SWITCH_MODULAR_C25519_SQR(engine, modpar);
	CCC_ERROR_CHECK(ret);

	modpar->x = xx;
	modpar->y = x2;
	modpar->r = tmp;

	ret = CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(engine, modpar);
	CCC_ERROR_CHECK(ret);

	/* Now tmp is a zero vector is x_root is square root
	 * Let the caller verify that.
	 */
fail:
	return ret;
}

static status_t ed25519_select_root_candidate(const engine_t *engine,
					      uint32_t nbytes,
					      te_mod_params_t *modpar,
					      uint8_t *x_root,
					      uint8_t *tmp,
					      uint8_t *x2,
					      const uint8_t *Py,
					      uint8_t *xx,
					      uint8_t *u,
					      uint8_t *v,
					      status_t *rbad_p)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;

	LTRACEF("entry\n");

	*rbad_p = ERR_BAD_STATE;

	ret = ed25519_first_root_candidate(engine, nbytes, modpar,
					   x_root, tmp, x2,
					   Py, xx, u, v);
	CCC_ERROR_CHECK(ret);

	/* Check if the first root candidate now in x_root actually
	 * is the square root.
	 */
	if (!BOOL_IS_TRUE(se_util_vec_is_zero(tmp, nbytes, &rbad))) {

		if (NO_ERROR != rbad) {
			CCC_ERROR_WITH_ECODE(rbad);
		}
		rbad = ERR_BAD_STATE;

		LTRACEF("First candidate is not the square root\n");

		ret = ed25519_second_root_candidate(engine, modpar, x_root, xx, x2, tmp);
		CCC_ERROR_CHECK(ret);

		if (!BOOL_IS_TRUE(se_util_vec_is_zero(tmp, nbytes, &rbad))) {
			/* No square root exists (mod p) and
			 * decoding fails.
			 */
			CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
					     LTRACEF("Second candidate is not the square root\n"));
		}
	}

	/* Caller checks for FI */
	*rbad_p = rbad;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_select_root(const engine_t *engine,
				    const pka1_ec_lwrve_t *lwrve,
				    te_mod_params_t *modpar,
				    uint8_t *x_root,
				    uint32_t x_0)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* When the least significant bit in the little endian x_root
	 * does not match x_0 value => negate it (mod p).
	 *
	 * if (x_root (mod 2) != x_0) then
	 *    x_root = p - x_root (mod p)
	 * fi
	 */
	if (((uint32_t)x_root[0U] & 0x1U) != x_0) {

		/* x_root = p - x_root mod p */
		modpar->x = lwrve->p;
		modpar->y = x_root;
		modpar->r = x_root;

		modpar->mod_flags &= ~PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
		if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
			modpar->mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
		}

		ret = CCC_ENGINE_SWITCH_MODULAR_SUBTRACTION(engine, modpar);
		CCC_ERROR_CHECK(ret,
				LTRACEF("XXX => x = p - x mod p failed\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_root(struct context_mem_s	*cmem,
			     const engine_t		*engine,
			     const pka1_ec_lwrve_t	*lwrve,
			     uint8_t			*x_root,
			     const uint8_t		*Py,
			     uint8_t			*mem,
			     uint32_t			x_0,
			     status_t			*rbad_p)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	te_mod_params_t modpar = { .size = 0U };
	uint32_t nbytes = lwrve->nbytes;

	/* Space in mem for these */
	uint8_t *u = NULL;
	uint8_t *v = NULL;
	uint8_t *tmp = NULL;
	uint8_t *x2 = NULL;
	uint8_t *xx = NULL;

	uint32_t mem_off = 0U;

	LTRACEF("entry\n");

	*rbad_p = ERR_BAD_STATE;

	if (CCC_ED25519_LWRVE_BLEN != nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	u = &mem[mem_off];
	mem_off += nbytes;

	v = &mem[mem_off];
	mem_off += nbytes;

	tmp = &mem[mem_off];
	mem_off += nbytes;

	x2 = &mem[mem_off];
	mem_off += nbytes;

	xx = &mem[mem_off];
	mem_off += nbytes;

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	/* All other values are little endian in ED25519 */
	modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_Y_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;

	modpar.cmem = cmem;
	modpar.m = lwrve->p;
	modpar.size = nbytes;
	modpar.op_mode = lwrve->mode;

	ret = ed25519_select_root_candidate(engine, nbytes, &modpar,
					    x_root, tmp, x2,
					    Py, xx, u, v,
					    &rbad);
	CCC_ERROR_CHECK(ret);

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}
	rbad = ERR_BAD_STATE;

	/* Here x_root is the square root of x2, but need to select
	 * the right square root with x_0 (sign).
	 */

	/* If x_root is zero but x_0 is not => fail
	 */
	if (BOOL_IS_TRUE(se_util_vec_is_zero(x_root, nbytes, &rbad)) &&
	    (0U != x_0)) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID);
	}

	/* rbad == NO_ERROR on success, passed to caller for FI checks
	 */
	*rbad_p = rbad;

	ret = ed25519_select_root(engine, lwrve, &modpar, x_root, x_0);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_find_root(struct se_engine_ec_context *econtext,
				  const pka1_ec_lwrve_t *lwrve,
				  uint8_t		*x_root,
				  const uint8_t		*Py,
				  uint8_t		 x_0)
{
	const engine_t *engine = econtext->engine;
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint8_t *mem = NULL;

	LTRACEF("entry\n");

#define X_ED25519_XRECOVER_CHUNK_SIZE (5U * CCC_ED25519_LWRVE_BLEN)

	mem = CMTAG_MEM_GET_BUFFER(econtext->cmem,
				   CMTAG_BUFFER,
				   0U,
				   X_ED25519_XRECOVER_CHUNK_SIZE);
	if (NULL == mem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = ed25519_root(econtext->cmem,
			   engine, lwrve, x_root, Py, mem, (uint32_t)x_0, &rbad);
	CCC_ERROR_CHECK(ret);

fail:
	if (NULL != mem) {
		/* No secrets in this, no need to clear. */
		CMTAG_MEM_RELEASE(econtext->cmem,
				  CMTAG_BUFFER,
				  mem);
	}

	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Solve X from the Twisted Edwards lwrve equation:
 * a*x^2 + y^2 = 1 + d*x^2*y^2
 *
 * The Twisted Edwards lwrve is isomorphic to Edwards25519 lwrve
 *  -x^2 + y^2 = 1 - (121665/121666)*x^2*y^2
 * since -1 is a square in Fq and lwrve parameters are:
 *
 * Let a = -1
 * Let d = -121665/121666
 *
 * Solving x^2:
 *
 * x^2 = (y^2 - 1) / (d*y^2 + 1) (mod p)
 * Note that denominator is always non-zero mod p.
 *
 * Let x2 = x^2
 * Let u = (y^2-1)
 * Let v = (d*y^2+1)
 *
 * So:
 *
 * x2 = u/v (mod p)
 *
 * To compute the square root of u/v, first callwlate the candidate
 *  root with the Tonelli-Shanks algorithm or the special case for
 *  p = 5 (mod 8), using a single modular powering for both the
 *  ilwersion and the square root:
 *
 * x = (u * (1/v))^((p+3)/8) (mod p)
 *
 * Using HW supported C25519_MOD_EXP Eulers algorithm to maintain
 * time-ilwariance requirements of the EDDSA, callwlate 1/v with:
 *
 * 1/v = mod_ilw(v, p) = v^(p-2) (mod p)
 *
 * Then:
 *
 * if (x*x == x2 (mod p)) then
 *  x is square root
 * elif (x*x == -x2 (mod p)) then
 *  x = x * 2^((p-1)/4)
 *  x is square root
 * else
 *  (u/v) is not a square (mod p) => failure
 * fi
 *
 * Then using the sign x_0 select the right square root:
 *
 * if ((x == 0) && (x_0 == 1)) then
 *   decoding failure => failure
 * else if (x_0 != (x & 0x1)) then
 *   x = p - x (mod p)
 * fi
 *
 * unless failure: Px = x_root
 *
 *----------------------------
 *
 * Above, if => (x*x - x2 == 0 (mod p)) does not hold, the code
 * then callwlates the second alternative
 * for the square root and checks if that satisfies the
 * equation (x*x - x2 == 0 (mod p)). If it does it is the square root,
 * otherwise decoding fails.
 */
static status_t ed25519_xrecover_locked(struct se_engine_ec_context *econtext,
					const pka1_ec_lwrve_t *lwrve,
					uint8_t *Px,
					const uint8_t *Py,
					uint8_t x_0)
{
	status_t ret = NO_ERROR;
	uint8_t x_root[CCC_ED25519_LWRVE_BLEN]; /* Fixed for ed25519; make nbytes long if other lwrves added */

	LTRACEF("entry\n");

	/* Lwrrently only supports ED25519 lwrve */
	if ((TE_LWRVE_ED25519 != lwrve->id) || (CCC_ED25519_LWRVE_BLEN != lwrve->nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ed25519_find_root(econtext, lwrve, x_root, Py, x_0);
	if (NO_ERROR != ret) {
		/* Need to return a fixed error code in these cases */
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Can't find root\n"));
	}

	/* Here x_root is the x coordinate */
	se_util_mem_move(Px, x_root, lwrve->nbytes);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_point_decompress_locked(struct se_engine_ec_context *econtext,
						te_ec_point_t		     *P,
						const uint8_t		     *cpoint)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	const pka1_ec_lwrve_t *lwrve = NULL;
	uint32_t eflags = 0U;
	uint8_t x_0 = 0U;
	uint32_t nbytes = 0U;
	uint32_t tmp = 0U;

	LTRACEF("entry\n");

	lwrve = econtext->ec_lwrve;
	nbytes = lwrve->nbytes;

	/* Decompress into little endian point */
	P->point_flags = CCC_EC_POINT_FLAG_LITTLE_ENDIAN;

	/* Check added due to a false positive in coverity; so add the
	 * check to silence it. nbytes is from const lwrve->nbytes and
	 * for the ed52219 lwrve this value is always 32U.
	 *
	 * (Since ed448 may be supported later when HW supports it
	 * => code does not use constants here).
	 */
	if (CCC_ED25519_LWRVE_BLEN != nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Decode the little endian coordinate Y from
	 * little endian "compressed point value cpoint"
	 */
	se_util_mem_move(P->y, cpoint, nbytes);

	/* Save the X coordinate lsb bit before clearing it
	 * from Py bit 255
	 * (as it was encoded in signing)
	 */
	if (((uint32_t)P->y[nbytes-1U] & 0x80U) !=  0U) {
		x_0 = 1U;
	}

	/* Clear sign bit, Py is now ready */
	tmp = P->y[nbytes-1U];
	P->y[nbytes-1U] = BYTE_VALUE(tmp & 0x7FU);

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		eflags |= CMP_FLAGS_VEC1_LE;
	}

	/* ED25519 signature values and derived values are
	 * kept in LITTLE ENDIAN
	 */
	eflags |= CMP_FLAGS_VEC2_LE;

	LOG_HEXBUF("cpoint:", cpoint, CCC_ED25519_LWRVE_BLEN);
	LOG_HEXBUF("lwrve L:", lwrve->n, CCC_ED25519_LWRVE_BLEN);
	LOG_HEXBUF("lwrve p:", lwrve->p, CCC_ED25519_LWRVE_BLEN);
	LOG_HEXBUF("Py:", P->y, CCC_ED25519_LWRVE_BLEN);
	LTRACEF("x_0 (sign)=%u\n", x_0);

	/* Decoded value must be < p */
	if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(lwrve->p,
						 P->y, nbytes, eflags, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Py larger than p\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	// rbad == NO_ERROR

	/* Solve Px from Py with the lwrve equation,
	 * pass Px sign (selects root sign)
	 */
	ret = ed25519_xrecover_locked(econtext, lwrve, P->x, P->y, x_0);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Recovering X failed\n"));

#if POINT_OP_RESULT_VERIFY
	/* T23X HW checks input points as well on Shamir's */
	ret = ec_check_point_on_lwrve_locked(econtext, lwrve, P);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Decompressed point not on Ed25519\n"));
#endif

	LTRACEF("Point decompressed and verified on Ed25519\n");
	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_POINT_COMPRESS
/* Point compress:
 *
 * The Py co-ordinate in colwerted to little endian (Py_LE) and the
 * least significant bit of Px is copied to the most significant bit
 * of Py_LE (bit 255 in Ed25519).
 *
 */
static status_t ed25519_point_compress(const struct se_engine_ec_context *econtext,
				       uint8_t				 *cpoint,
				       const te_ec_point_t		 *P)
{
	status_t ret = NO_ERROR;
	uint32_t nbytes = 0U;
	uint32_t x_0 = 0U;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == cpoint) ||
	    (NULL == P) || (NULL == econtext->ec_lwrve)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	nbytes = econtext->ec_lwrve->nbytes;

	if (CCC_ED25519_LWRVE_BLEN != nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((P->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) != 0U) {
		se_util_mem_move(cpoint, P->y, nbytes);
		x_0 = ((uint32_t)P->x[0U] & 0x1U);
	} else {
		ret = se_util_reverse_list(cpoint, P->y, nbytes);
		x_0 = ((uint32_t)P->x[nbytes-1U] & 0x1U);
	}

	/* x_0 is the lsb of Px (== Px & 0x1) */
	if (0U != x_0) {
		uint32_t tmp = cpoint[nbytes-1U];
		cpoint[nbytes-1U] = BYTE_VALUE(tmp | 0x80U);
	}

	LOG_HEXBUF("point compression result", cpoint, nbytes);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_POINT_COMPRESS */

/* TODO: generic EC function => should move out of here */
/* FI enhanced */
static status_t pka1_lwrve_point_compare(const pka1_ec_lwrve_t *lwrve,
					 const te_ec_point_t *LHS_point,
					 const te_ec_point_t *RHS_point,
					 bool *match,
					 status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	uint32_t nbytes = 0U;
	bool swap = false;
	bool x = false;
	bool y = false;

	LTRACEF("entry\n");

	if ((NULL == lwrve) || (NULL == LHS_point) ||
	    (NULL == RHS_point) || (NULL == match) ||
	    (NULL == ret2_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*ret2_p = ERR_BAD_STATE;

	*match = false;
	nbytes = lwrve->nbytes;
	swap = ((LHS_point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN) !=
		(RHS_point->point_flags & CCC_EC_POINT_FLAG_LITTLE_ENDIAN));

	CCC_RANDOMIZE_EXELWTION_TIME;

	x = se_util_vec_match(LHS_point->x, RHS_point->x, nbytes,
			      swap, ret2_p);

	if (NO_ERROR != *ret2_p) {
		CCC_ERROR_WITH_ECODE(*ret2_p);
	}

	*ret2_p = ERR_BAD_STATE;

	CCC_RANDOMIZE_EXELWTION_TIME;

	y = se_util_vec_match(LHS_point->y, RHS_point->y, nbytes,
			      swap, ret2_p);

	if (NO_ERROR != *ret2_p) {
		CCC_ERROR_WITH_ECODE(*ret2_p);
	}

	*match = (x && y);

#if SE_RD_DEBUG
	if (BOOL_IS_TRUE(*match)) {
		LTRACEF("RHS matches LHS\n");
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_ED25519_SIGN
/* Used by crypto_asig.c when setting ED25519 private key scalar value */
status_t ed25519_encode_scalar(const se_engine_ec_context_t *econtext,
			       uint8_t *a, uint8_t *h, const uint8_t *pkey,
			       uint32_t nbytes)
{
	status_t ret = NO_ERROR;

	uint8_t sha512_digest[CCC_SHA512_DIGEST_LEN];
	struct se_data_params input;

	LTRACEF("entry\n");

	if (NULL == pkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	input.src	  = pkey;
	input.input_size  = nbytes;
	input.dst	  = sha512_digest;
	input.output_size = sizeof_u32(sha512_digest);

	ret = sha_digest(econtext->cmem,
			 TE_CRYPTO_DOMAIN_KERNEL,
			 &input,
			 TE_ALG_SHA512);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Digesting scalar failed: %d\n", ret));

	if (NULL != a) {
		sha512_digest[0U]  = BYTE_VALUE((uint32_t)sha512_digest[0U] & 0xF8U);
		sha512_digest[31U] = BYTE_VALUE((uint32_t)sha512_digest[31U] & 0x7FU);
		sha512_digest[31U] = BYTE_VALUE((uint32_t)sha512_digest[31U] | 0x40U);

		/* Lower half of SHA-512 digest */
		se_util_mem_move(a, &sha512_digest[0U], (CCC_SHA512_DIGEST_LEN / 2U));
	}

	if (NULL != h) {
		/* Upper half of SHA-512 digest */
		se_util_mem_move(h, &sha512_digest[(CCC_SHA512_DIGEST_LEN / 2U)],
				 (CCC_SHA512_DIGEST_LEN / 2U));
	}

fail:
	/* Don't leave secrets in stack */
	se_util_mem_set((uint8_t *)sha512_digest, 0U, sizeof_u32(sha512_digest));

	LTRACEF("exit: %d\n", ret);

	return ret;
}
#endif /* HAVE_ED25519_SIGN */

static status_t ed25519_verify_params_context(const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ec_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ec_context->ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve not defined\n"));
	}

	if (TE_LWRVE_ED25519 != ec_context->ec.ec_lwrve->id) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Wrong lwrve for ED25519\n"));
	}

	/* Always true for ED25519 */
	if (CCC_ED25519_LWRVE_BLEN != ec_context->ec.ec_lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}


fail:
	LTRACEF("exit: %d\n", ret);

	return ret;
}

static status_t ed25519_verify_params(struct se_data_params *input_params,
				      const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = ed25519_verify_params_context(ec_context);
	CCC_ERROR_CHECK(ret);

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	// XXX For TE_ALG_ED25519PH the input size must be 64B
	// But for PureEDDSA all message lengths are allowed, including empty
	// messages...
	//
	if (NULL == input_params->src_signature) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* NULL input allowed for ED25519 */
	if (NULL == input_params->src) {
		input_params->input_size = 0U;
	}

	/* Prehashed input is a SHA-512 digest which is 64 bytes long */
	if (TE_ALG_ED25519PH == ec_context->ec.algorithm) {
		if ((NULL == input_params->src) ||
		    (input_params->input_size != CCC_SHA512_DIGEST_LEN)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("ED25519PH input is not a SHA-512 digest\n"));
		}
	}

	if (input_params->src_signature_size != CCC_ED25519_SIGNATURE_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Wrong signature size %u for ED25519\n",
					     input_params->src_signature_size));
	}

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#if HAVE_POINT_COMPRESS

/* Derive public key from private key; this may be useful in other contexts
 * as well, but lwrrently only used in EDDSA sig validation when no pubkey
 * provided. (This has reduced to a generic EC pubkey calculator now).
 */
static status_t ed25519_callwlate_public_key(struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	uint32_t saved_ec_flags = ec_context->ec.ec_flags;
	bool scalar_is_LE = ((ec_context->ec.ec_flags & CCC_EC_FLAGS_BIG_ENDIAN_KEY) == 0U);

	LTRACEF("entry\n");

	/* If we do not have private key => can't generate public key */
	if (ec_context->ec.ec_keytype != KEY_TYPE_EC_PRIVATE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("No EC private key, can't callwlate pubkey\n"));
	}

	/* We have an already encoded private key scalar for ED25519
	 * private key value => just callwlate the public key.
	 *
	 * The generated pubkey will be in same byte order as
	 * lwrve parameters (This is lwrrently big endian)
	 *
	 * pubkey = privkey * <lwrve generator point>
	 */
	ec_context->ec.ec_flags |= CCC_EC_FLAGS_PKEY_MULTIPLY;
	ret = ec_point_multiply(&ec_context->ec,
				ec_context->ec.ec_pubkey,
				NULL, ec_context->ec.ec_pkey, ec_context->ec.ec_lwrve->nbytes,
				scalar_is_LE);

	ec_context->ec.ec_flags = saved_ec_flags;

	CCC_ERROR_CHECK(ret);

	/* Point is no longer undefined nor compressed */
	ec_context->ec.ec_pubkey->point_flags = CCC_EC_POINT_FLAG_NONE;

	LTRACEF("EC public key derived\n");

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_POINT_COMPRESS */

/* dom2(x, y) =>
 * The blank octet string when signing or verifying Ed25519.
 *
 * Otherwise, the octet string:
 *   "SigEd25519 no Ed25519 collisions" ||
 *    octet(x) || octet(OLEN(y)) || y
 *
 * where x is in range 0-255 and y is an octet string
 * of at most 255 octets.
 *
 * "SigEd25519 no Ed25519 collisions" is in ASCII (32 octets).
 *
 * x == phflag
 * y == ec_context->dom_context
 *  Implementation restricts dom_context.value to MAX_ECC_DOM_CONTEXT_SIZE
 *
 * Due to SHA update reblocking by CCC to sha context buf until the
 * input data length matches SHA-512 block size (128 bytes) => SHA
 * engine will not get called with the stack arrays. Their content
 * gets copied to sha_context->buf (which is DMA compatible) which
 * get passed to HW later.
 */
static status_t ed25519_dom2(const struct se_ec_context *ec_context,
			     struct se_sha_context *sha_context,
			     struct se_data_params *input,
			     uint32_t phflag)
{
	status_t ret = NO_ERROR;

	/* OCTET STRING: "SigEd25519 no Ed25519 collisions"
	 * in 32 bytes of ASCII
	 */
	static const uint8_t d2_prefix[32U] = {
		0x53, 0x69, 0x67, 0x45, 0x64, 0x32, 0x35, 0x35,
		0x31, 0x39, 0x20, 0x6e, 0x6f, 0x20, 0x45, 0x64,
		0x32, 0x35, 0x35, 0x31, 0x39, 0x20, 0x63, 0x6f,
		0x6c, 0x6c, 0x69, 0x73, 0x69, 0x6f, 0x6e, 0x73,
	};

	uint8_t ctx_tmp[ 2U ];

	LTRACEF("entry\n");

	input->src	  = &d2_prefix[0];
	input->input_size = sizeof_u32(d2_prefix);
	input->dst	  = NULL;
	input->output_size= 0U;

	ret = PROCESS_SHA(input, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("dom prefix failed to digest\n"));

	/* OCTET STRING: OCTET(x) || OCTET(OLEN(Y))
	 */
	ctx_tmp[0] = BYTE_VALUE(phflag);
	ctx_tmp[1] = BYTE_VALUE(ec_context->dom_context.length);

	input->src	  = &ctx_tmp[0];
	input->input_size = sizeof_u32(ctx_tmp);

	ret = PROCESS_SHA(input, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("dom octets failed to digest\n"));

	if (ec_context->dom_context.length > 0U) {
		/* OCTET STRING: context value in Y octets (0..255)
		 */
		input->src	  = &ec_context->dom_context.value[0];
		input->input_size = ec_context->dom_context.length;

		ret = PROCESS_SHA(input, sha_context);
		CCC_ERROR_CHECK(ret,
				LTRACEF("dom context failed to digest\n"));
	}

fail:
	input->src = NULL; /* Cert-C cleanup */

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static void ed25519_digest_RAM_cleanup(const struct se_ec_context *ec_context,
				       struct se_sha_context **sha_context_p)
{
	struct se_sha_context *sha_context = *sha_context_p;

	if (NULL != sha_context) {
		/* Contains no secrets, no need to clear */
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		sha_context_dma_buf_release(sha_context);
#endif
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_ALIGNED_CONTEXT,
				  sha_context);

		*sha_context_p = sha_context;
	}
}

/* SHA-512(dom2(F,C), R,A,M)
 *
 * ED25519: dom2(F,C)   == empty octet stream
 * ED25519PH: dom2(F,C) == octet stream
 * ED25519CTX: dom2(F,C) == octet stream
 *
 * DST must be at least 64 bytes long.
 */
static status_t ed25519_digest_RAM(const struct se_ec_context *ec_context,
				   const pka1_ec_lwrve_t *lwrve,
				   uint8_t *dst,
				   const uint8_t *enc_R, const uint8_t *enc_A,
				   const struct se_data_params *input_params)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t nbytes = 0U;

	/* Context for SHA-512 calls.
	 */
	struct se_sha_context *sha_context = NULL;

	LTRACEF("entry\n");

	nbytes = lwrve->nbytes;

	sha_context = CMTAG_MEM_GET_OBJECT(ec_context->ec.cmem,
					   CMTAG_ALIGNED_CONTEXT,
					   CCC_DMA_MEM_ALIGN,
					   struct se_sha_context,
					   struct se_sha_context *);
	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* Next, need to compute h = H(ENC(R) || ENC(A) || PH(M))
	 * where H == SHA-512  (as in ED25519 specs).
	 *
	 * The created SHA context shares the EC cmem context.
	 */
	ret = sha_context_static_init(TE_CRYPTO_DOMAIN_KERNEL,
				      CCC_ENGINE_ANY,
				      TE_ALG_SHA512,
				      sha_context,
				      ec_context->ec.cmem);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Digesting scalar init failed: %d\n", ret));

	sha_context->ec.is_last = false;

	/* dom2(x, y) => The blank octet string when
	 * signing or verifying Ed25519.
	 */
	if (TE_ALG_ED25519PH == ec_context->ec.algorithm) {
		ret = ed25519_dom2(ec_context, sha_context, &input, 1U);
		CCC_ERROR_CHECK(ret);
	}

	if (TE_ALG_ED25519CTX == ec_context->ec.algorithm) {
		ret = ed25519_dom2(ec_context, sha_context, &input, 0U);
		CCC_ERROR_CHECK(ret);
	}

	/* Perform a partial digest operation in the specified sequence, first
	 * digest_update(enc_R)
	 */
	input.src	  = enc_R;
	input.input_size  = nbytes;
	input.dst	  = NULL;
	input.output_size = 0U;

	/* enc_R lives in kernel space */
	ret = PROCESS_SHA(&input, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Digest enc_R failed\n"));

	/*
	 * digest_update with enc_A (compressed pubkey)
	 */
	input.src	  = enc_A;
	input.input_size  = nbytes;
	input.dst	  = NULL;
	input.output_size = 0U;

	/* enc_A lives in kernel space */
	ret = PROCESS_SHA(&input, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Digest enc_A failed\n"));

	/*
	 * And finally for the last data pushed to the digest:
	 * k = digest_dofinal(PH(M))
	 *
	 * The resulting SHA512 digest value k is a 64 byte
	 * little-endian multiplier for the group equation
	 * public key (point A')
	 *
	 * input_params->src data is in the caller domain econtext->domain
	 */
	sha_context->ec.is_last = true;

	input.src	  = input_params->src;
	input.input_size  = input_params->input_size;
	input.dst	  = dst;
	input.output_size = CCC_SHA512_DIGEST_LEN;

	/* input_params->src (message M) data lives in caller domain */
	sha_context->ec.domain = ec_context->ec.domain;
	ret = PROCESS_SHA(&input, sha_context);
	sha_context->ec.domain = TE_CRYPTO_DOMAIN_KERNEL;

	CCC_ERROR_CHECK(ret,
			LTRACEF("Digest M failed\n"));

	LOG_HEXBUF("SHA_RAM", dst, CCC_SHA512_DIGEST_LEN);
fail:
	ed25519_digest_RAM_cleanup(ec_context, &sha_context);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if VERIFY_GROUP_EQUATION_LONG_FORM || HAVE_ED25519_SIGN
static status_t ed_mod_multiply(const engine_t  *engine,
				const te_mod_params_t *modpar)
{
	status_t ret = NO_ERROR;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
#if HAVE_GEN_MULTIPLY
		/* Does not use montgomery values; instead does DP
		 *  multiply followed by DP bit serial reduction.
		 */
		ret = CCC_ENGINE_SWITCH_MODULAR_GEN_MULTIPLICATION(engine, modpar);
		CCC_ERROR_END_LOOP(ret, LTRACEF("gen_multiply mod m failed\n"));
#else
		/* This uses Montgomery values in PKA1 but no preconfigured mont
		 * values for this lwrve exist: For PKA1 use HAVE_GEN_MULTIPLY
		 * code above.
		 *
		 * With LWPKA this is faster than the gen mult version
		 * due to engine differences: In this platrform either alternative
		 * can be used, but this is faster.
		 */
#if CCC_SOC_WITH_PKA1
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		LTRACEF("multiply mod m failed\n");
#else
		ret = CCC_ENGINE_SWITCH_MODULAR_MULTIPLICATION(engine, modpar);
		CCC_ERROR_END_LOOP(ret, LTRACEF("multiply mod m failed\n"));
#endif
#endif
		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* VERIFY_GROUP_EQUATION_LONG_FORM || HAVE_ED25519_SIGN */

/* Verify that the group equation holds:
 *
 * [8][S]B = [8]R + [8][k]A' (mod L)
 *
 * To avoid the multiply by 8 of all terms,
 * verify this instead:
 *
 * [S]B = R + [k]A'
 *
 * S was decoded (and verified) above, R and A' are decompressed
 * (and verified) lwrve points. B is the base point of the
 * Ed25519 lwrve.
 *
 * Because Edwards25519 is a complete lwrve => there are no
 * invalid values which would prevent using the ed25519
 * shamir's trick with this.
 *
 * Gee Whiz... Shamir's trick does not allow multiplier 1 => can't use it ;-)
 * => callwlate RHS without it...
 *
 * NOTE: I do not know if this is faster or slower than using
 * shamir's trick and two more mod multiply ops
 * (i.e. verify the mult all by 8 version instead... TODO: consider later)
 */
#if VERIFY_GROUP_EQUATION_LONG_FORM

static status_t long_form_group_eq_lhs_locked(struct se_engine_ec_context *econtext,
					      const engine_t		  *engine,
					      const pka1_ec_lwrve_t	  *lwrve,
					      const te_mod_params_t	  *modpar,
					      te_ec_point_t		  *tpoint, /* Modified */
					      const uint8_t		  *res,
					      uint32_t			   nbytes)
{
	status_t ret = NO_ERROR;

	(void)lwrve;

	LTRACEF("entry\n");

	ret = ed_mod_multiply(engine, modpar);
	CCC_ERROR_CHECK(ret, LTRACEF("multiply + reduction failed\n"));

	LOG_HEXBUF("reduced s (res)' = [8]S", res, lwrve->nbytes);

	ret = ec_point_multiply_locked(econtext, tpoint, NULL, res, nbytes, true);
	CCC_ERROR_CHECK(ret, LTRACEF("Digest dofinal failed\n"));

#if POINT_OP_RESULT_VERIFY
	ret = ec_check_point_on_lwrve_locked(econtext, lwrve, tpoint);
	CCC_ERROR_CHECK(ret, LTRACEF("LHS on E failed\n"));
#endif

	LTRACEF("LHS verified on lwrve\n");
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_long_form_group_equation_point(struct se_engine_ec_context *econtext,
						       te_ec_point_t	      *tpoint, /* Modified */
						       const pka1_ec_lwrve_t  *lwrve,
						       const uint8_t	      *S,
						       te_ec_point_t	      *R, /* Modified */
						       uint8_t		      *k_dp, /* Modified */
						       const te_ec_point_t    *A)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };
	uint8_t res[CCC_ED25519_LWRVE_BLEN];
	uint32_t nbytes = lwrve->nbytes;
	bool is_locked = false;
	const engine_t *engine = econtext->engine;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	/* The modular operations below
	 * are done modulo lwrve order (n)
	 */
	modpar.m = lwrve->n;
	modpar.size = nbytes;
	modpar.op_mode = lwrve->mode;
	modpar.cmem = econtext->cmem;

	/*
	 * Let's first callwlate the LHS point value
	 *
	 * As a convention, if the ec_point_multiply point_P parameter
	 * is NULL it multiplies instead the lwrve base point with the
	 * provided scalar.
	 *
	 * tpoint = LHS_point = [8][S]B
	 *
	 * All scalars are little endian in this function.
	 */
	modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_Y_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;

	LOG_HEXBUF("scalar [S]", S, CCC_ED25519_LWRVE_BLEN);

	/* res = [8]S
	 */
	modpar.x = ed25519_cvalue.LE_8;
	modpar.y = S;
	modpar.r = res;

	HW_MUTEX_TAKE_ENGINE(engine);
	is_locked = true;

	CCC_LOOP_BEGIN {
		ret = long_form_group_eq_lhs_locked(econtext, engine, lwrve, &modpar, tpoint, res, nbytes);
		CCC_ERROR_END_LOOP(ret);

		/* Callwlate RHS_point: [8]R + [8][k]A'
		 *
		 * The value k_dp (double precision) is a 64 byte value, so we need to do a bit
		 * serial double precision reduction with lwrve order first to
		 * colwert it to a 32 byte multiplier which is then
		 * multiplied by 8 (result is double precision) and again reduce
		 * it to single precision (with lwrve order).
		 */

		/* First reduce double precision value k to single precision
		 * to avoid overflow when callwlating [8][k]
		 */
		LOG_HEXBUF("DOUBLE PRECISION K (sha digest)", k_dp, 64U);

		/* k = k mod L (double precision k); overwrites k (with single precision)
		 *
		 * The double-length (64 bytes) value generated by SHA512
		 * is treated as little-endian value.
		 *
		 * For this operation modular lib expects buffer k_dp size to be
		 * 2*modpar.size in input
		 */
		modpar.x = k_dp;
		modpar.r = k_dp;

		ret = CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION_DP(engine, &modpar);
		CCC_ERROR_END_LOOP(ret, LTRACEF("DP bit serial reduction failed\n"));

		LOG_HEXBUF("DP_REDUCED K", k_dp, CCC_ED25519_LWRVE_BLEN);

		/* res (reduced)) = [8]k
		 */
		modpar.x = ed25519_cvalue.LE_8;
		modpar.y = k_dp;
		modpar.r = res;

		ret = ed_mod_multiply(engine, &modpar);
		CCC_ERROR_END_LOOP(ret, LTRACEF("multiply + reduction failed\n"));

		LOG_HEXBUF("(res, reduced) z' = [8][reduced k]", res, 64U);

		HW_MUTEX_RELEASE_ENGINE(engine);

		is_locked = false;

		/* Callwlate => R = [8]R + [z']A */
		ret = callwlate_shamirs_value(econtext, R,
					      ed25519_cvalue.LE_8, R, res, A,
					      (CCC_EC_FLAGS_SCALAR_K_LITTLE_ENDIAN |
					       CCC_EC_FLAGS_SCALAR_L_LITTLE_ENDIAN));

		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("RHS point value callwlation failed\n"));

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	if (BOOL_IS_TRUE(is_locked)) {
		HW_MUTEX_RELEASE_ENGINE(engine);
	}
	CCC_ERROR_CHECK(ret);

	/* point R is now the RHS (shamir's trick result point)
	 */
	LOG_HEXBUF("RHS->Px", R->x, nbytes);
	LOG_HEXBUF("RHS->Py", R->y, nbytes);
	LOG_HEXBUF("LHS->Px", tpoint->x, nbytes);
	LOG_HEXBUF("LHS->Py", tpoint->y, nbytes);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_verify_long_form_group_equation(struct se_ec_context  *ec_context,
							te_ec_point_t	      *tpoint,
							const pka1_ec_lwrve_t *lwrve,
							const uint8_t	      *S,
							te_ec_point_t	      *R, /* Modified */
							uint8_t	      	      *k_dp, /* Modified */
							const te_ec_point_t   *A)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	bool points_match = false;
	uint32_t nbytes = 0U;

	LTRACEF("entry\n");

	se_util_mem_set((uint8_t *)tpoint, 0U, sizeof_u32(te_ec_point_t));

	nbytes = lwrve->nbytes;

	if (nbytes != CCC_ED25519_LWRVE_BLEN) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	ret = ed25519_long_form_group_equation_point(&ec_context->ec, tpoint,
						     lwrve, S, R, k_dp, A);
	CCC_ERROR_CHECK(ret);

	/* If LHS == RHS (group equation match) =>
	 * ED25519 signature verified
	 */
	ret = pka1_lwrve_point_compare(lwrve, tpoint, R, &points_match, &rbad);
	CCC_ERROR_CHECK(ret, LTRACEF("Group equation compare failed\n"));

	if (!BOOL_IS_TRUE(points_match)) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("Group equation does not hold\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	FI_BARRIER(status, rbad);

fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#else

static status_t ed25519_verify_setup_short_form_rhs(struct se_ec_context  *ec_context,
						    te_ec_point_t	    *tpoint,
						    const pka1_ec_lwrve_t *lwrve,
						    te_ec_point_t	    *R, /* Modified */
						    uint8_t	            *k_dp, /* Modified */
						    const te_ec_point_t   *A)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };
	struct se_data_params input;
	uint32_t nbytes = lwrve->nbytes;
	const engine_t *engine = ec_context->ec.engine;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;

	/* These operations are mod lwrve order (L, OBP, lwrve->n)
	 */
	modpar.m = lwrve->n;
	modpar.size = nbytes;
	modpar.op_mode = lwrve->mode;
	modpar.cmem = ec_context->ec.cmem;

	LOG_HEXBUF("DOUBLE PRECISION K (sha digest)", k_dp, 64U);

	/* k = k mod L (double precision k); overwrites k
	 *
	 * The double-length (64 bytes) value generated by SHA512
	 * is treated as little-endian value.
	 *
	 * For this operation modular lib expects buffer k size to be
	 * 2*modpar.size
	 */
	modpar.x = k_dp;
	modpar.r = k_dp;

	HW_MUTEX_TAKE_ENGINE(engine);

	CCC_LOOP_BEGIN {
		ret = CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION_DP(engine, &modpar);
		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("DP bit serial reduction failed\n"));

		LOG_HEXBUF("DP_REDUCED K", k_dp, CCC_ED25519_LWRVE_BLEN);

		/* Do not use Ed25519 Shamir's Trick to callwlate RHS = 1*R + k*A
		 *
		 * It is not defined when multiplier value is 1....
		 */
		ret = ec_point_multiply_locked(&ec_context->ec, tpoint, A, k_dp, nbytes, true);
		CCC_ERROR_CHECK(ret,
				LTRACEF("RHS point multiply failed\n"));

		input.point_P = tpoint;
		input.point_Q = R; /* Result point overwrites R (<= RHS_point) */

		input.input_size  = sizeof_u32(te_ec_point_t);
		input.output_size = sizeof_u32(te_ec_point_t);

		ret = CCC_ENGINE_SWITCH_EC_ADD(engine, &input, &ec_context->ec);
		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("RHS point addition failed\n"));

#if POINT_OP_RESULT_VERIFY
		/* point R is now the RHS_point
		 */
		ret = ec_check_point_on_lwrve_locked(&ec_context->ec, lwrve, R);
		CCC_ERROR_END_LOOP(ret,
				   LTRACEF("RHS on E failed\n"));
#endif

		LTRACEF("RHS verified on lwrve\n");
	        CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_verify_short_form_group_equation(struct se_ec_context  *ec_context,
							 te_ec_point_t	       *tpoint,
							 const pka1_ec_lwrve_t *lwrve,
							 const uint8_t	       *S,
							 te_ec_point_t	       *R, /* Modified */
							 uint8_t	       *k_dp, /* Modified */
							 const te_ec_point_t   *A)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	bool points_match = false;
	uint32_t nbytes = 0U;

	LTRACEF("entry\n");

	nbytes = lwrve->nbytes;

	se_util_mem_set((uint8_t *)tpoint, 0U, sizeof_u32(te_ec_point_t));

	/* Callwlate RHS_point: R + [k]A'
	 *
	 * The value k_dp is a 64 byte value, so we need to do a bit
	 * serial double precision reduction with lwrve order first to
	 * colwert it to a 32 byte multiplier for the point multipy.
	 */
	ret = ed25519_verify_setup_short_form_rhs(ec_context, tpoint, lwrve,
						  R, k_dp, A);
	CCC_ERROR_CHECK(ret);

	/*
	 * Then callwlate the LHS point
	 *
	 * As a convention, if the ec_point_multiply point_P parameter
	 * is NULL it multiplies instead the lwrve base point with the
	 * provided scalar.
	 *
	 * LHS_point = [S]B
	 *
	 * S is a little endian scalar here.
	 * B is the lwrve base point.
	 */
	se_util_mem_set((uint8_t *)tpoint, 0U, sizeof_u32(te_ec_point_t));

	ret = ec_point_multiply(&ec_context->ec, tpoint, NULL, S, nbytes, true);
	CCC_ERROR_CHECK(ret,
			    LTRACEF("Digest dofinal failed\n"));

#if POINT_OP_RESULT_VERIFY
	ret = ec_check_point_on_lwrve(&ec_context->ec, lwrve, tpoint);
	CCC_ERROR_CHECK(ret,
			LTRACEF("LHS on E failed\n"));
#endif
	LTRACEF("LHS verified on lwrve\n");

	LOG_HEXBUF("RHS->Px", R->x, nbytes);
	LOG_HEXBUF("RHS->Py", R->y, nbytes);
	LOG_HEXBUF("LHS->Px", tpoint->x, nbytes);
	LOG_HEXBUF("LHS->Py", tpoint->y, nbytes);

	/* If LHS == RHS (group equation match) =>
	 * ED25519 signature verified
	 */
	ret = pka1_lwrve_point_compare(lwrve, tpoint, R, &points_match, &rbad);
	CCC_ERROR_CHECK(ret,
			    LTRACEF("Group equation compare failed\n"));

	if (!BOOL_IS_TRUE(points_match)) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
					 LTRACEF("Group equation does not hold\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		LTRACEF("Inconsistent error state\n");
		ret = rbad;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* VERIFY_GROUP_EQUATION_LONG_FORM */

static status_t ed25519_verify_setup_pubkey(struct se_ec_context *ec_context,
					    const pka1_ec_lwrve_t *lwrve,
					    te_ec_point_t	**A_p,
					    uint8_t		**enc_A_p)
{
	status_t       ret   = NO_ERROR;
	uint8_t       *enc_A = *enc_A_p;
	te_ec_point_t *A     = *A_p;
	const engine_t *engine = ec_context->ec.engine;

	LTRACEF("entry\n");

	// XXXX This returns ERROR if ec_pubkey is NULL
	if ((NULL == ec_context->ec.ec_pubkey) ||
	    ((ec_context->ec.ec_pubkey->point_flags & CCC_EC_POINT_FLAG_UNDEFINED) != 0U)) {
#if HAVE_POINT_COMPRESS
		ret = ed25519_callwlate_public_key(ec_context);
		CCC_ERROR_CHECK(ret);
#else
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Pubkey not defined\n"));
#endif
	}

	/* For ED25519 the public key is/can be specified in compressed form.
	 * If so => decompress it first (both A and ENC(A) are needed
	 * in the verification process).
	 *
	 * If not compressed => allow using the pubkey point which is either
	 * given or callwlated with the private key (if specified).
	 * For now, assume the public key exists in point form.
	 *
	 * What ever points are used, verify that all are on ED25519 lwrve!
	 */
	if ((ec_context->ec.ec_pubkey->point_flags & CCC_EC_POINT_FLAG_COMPRESSED_ED25519) != 0U) {

		LTRACEF("Compressed ED25519 pubkey => decompressing to A\n");

		/* space reserved for enc_A (compressed pubkey) by caller is not used
		 * since the compressed pubkey exists in key object.
		 */
		enc_A = ec_context->ec.ec_pubkey->ed25519_compressed_point;

		LTRACEF("Decompressing A\n");

		/* Also verifies point is on lwrve */
		HW_MUTEX_TAKE_ENGINE(engine);
		ret = ed25519_point_decompress_locked(&ec_context->ec, A, enc_A);
		HW_MUTEX_RELEASE_ENGINE(engine);

		CCC_ERROR_CHECK(ret,
				LTRACEF("Decompressing public key A' failed\n"));

		*enc_A_p = enc_A;

		LTRACEF("Decompressing A complete\n");
	} else {
#if HAVE_POINT_COMPRESS
		LTRACEF("ED25519 point pubkey used (A=pubkey)\n");

		/* MUTEX not held here => call the unlocked version */
		ret = ec_check_point_on_lwrve(&ec_context->ec, lwrve, ec_context->ec.ec_pubkey);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Provided pubkey not on ED25519\n"));

		/* work space for deriving A (pubkey) is not used since it
		 * already exists in key arguments.
		 */
		A    = ec_context->ec.ec_pubkey;
		*A_p = A;

		LTRACEF("Compressing lwrve public key\n");

		/* Compress the pubkey to enc_A */
		ret = ed25519_point_compress(&ec_context->ec,
					     enc_A,
					     ec_context->ec.ec_pubkey);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to encode public key\n"));
#else
		/* Only supports compressed pubkeys in this case
		 */
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("point compression not supported, must provide compressed pubkey ENC(A)\n"));
#endif
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* About 324 bytes */
#define X_ED25519_VERIFY_CHUNK_SIZE					\
	((3U * sizeof_u32(te_ec_point_t)) + (3U * CCC_ED25519_LWRVE_BLEN) + CCC_SHA512_DIGEST_LEN)

static void ed2559_verify_cleanup(struct se_ec_context *ec_context,
				  te_ec_point_t *pmem_arg)
{
	te_ec_point_t *pmem = pmem_arg;

	if (NULL != pmem) {
		se_util_mem_set((uint8_t *)pmem, 0U, X_ED25519_VERIFY_CHUNK_SIZE);
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_BUFFER,
				  pmem);
	}
}

static status_t ed25519_verify_setup(
	struct se_data_params *input_params,
	const struct se_ec_context *ec_context,
	te_ec_point_t **pmem_p)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *pmem = NULL;

	LTRACEF("entry\n");

	*pmem_p = NULL;

	ret = ed25519_verify_params(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	pmem = CMTAG_MEM_GET_TYPED_BUFFER(ec_context->ec.cmem,
					  CMTAG_BUFFER,
					  0U,
					  te_ec_point_t *,
					  X_ED25519_VERIFY_CHUNK_SIZE);
	if (NULL == pmem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	*pmem_p = pmem;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_ed25519_verify(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	te_ec_point_t *pmem = NULL;
	uint8_t *mem = NULL;
	uint32_t pmem_off = 0U;
	uint32_t mem_off = 0U;
	uint32_t nbytes = 0U;
	uint32_t eflags = 0U;

	const pka1_ec_lwrve_t *lwrve = NULL;
	const engine_t *engine = NULL;

	/* Allocate these to reduce stack usage
	 * Allocated in one chunk to optimize.
	 */

	/* These scalars are 32 bytes each */
	const uint8_t *enc_R = NULL; /* sig field enc_R */
	const uint8_t *S = NULL; /* sig field enc_S == S */
	uint8_t *enc_A = NULL;	/* compressed pubkey */

	/* This is 64 byte SHA512 digest (reserving CCC_SHA512_DIGEST_LEN)
	 * treated as a little endian scalar value.
	 */
	uint8_t *k_dp	 = NULL;

	te_ec_point_t *R = NULL; /* decompressed enc_R => point R */
	te_ec_point_t *A = NULL; /* decompressed enc_A => point public key */
	te_ec_point_t *tmp_point = NULL; /* point scratch space */

	LTRACEF("entry\n");

	ret = ed25519_verify_setup(input_params, ec_context, &pmem);
	CCC_ERROR_CHECK(ret);

	/* Pubkey for verifying the signature is in ec_pubkey.
	 * But with ED25519 it may (and usually is) be in compressed form
	 * (i.e. not a valid point before the data is decompressed).
	 *
	 * If CCC_EC_POINT_FLAG_COMPRESSED_ED25519 is set =>
	 * the value in ec_pubkey->ed25519_copressed_point is little endian
	 * compressed ED25519 public key (encoded as 32 byte little endian
	 * value) and must first be decompressed.
	 *
	 * Decompress it to a local object; does not modify the compressed
	 * ec_pubkey data.
	 */
	lwrve  = ec_context->ec.ec_lwrve;
	nbytes = lwrve->nbytes;
	engine = ec_context->ec.engine;

	(void)engine;	/* engine not used if subsystem does not use mutexes via CCC */

	if (CCC_ED25519_LWRVE_BLEN != nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Reserve work variables */

	/* Space for decompressed point R */
	pmem_off = 0U;

	R  = &pmem[pmem_off];
	pmem_off++;

	/* Space for decompressed point A */
	A  = &pmem[pmem_off];
	pmem_off++;

	/* Scratch space for group equation LHS (point) */
	tmp_point  = &pmem[pmem_off];
	pmem_off++;

	/* Rest of the mem pool memory is accessed as a byte buffer
	 */
	mem = (uint8_t *)&pmem[pmem_off];

	/* Copy signature to work space memory
	 * (gets split to enc_R and S objects) below
	 */
	se_util_mem_move(mem, input_params->src_signature, input_params->src_signature_size);

	/* Split signature <ENC(R) || ENC(S)> to the components */
	mem_off = 0U;

	enc_R  = &mem[mem_off];
	mem_off += nbytes;

	/* ENC(S) is S encoded as little endian scalar. Since we use
	 * little endian scalars here, decoding is just verifying
	 * that S is in the allowed set of values.
	 *
	 * This is verified below. Variable S here is used to hold
	 * also "ulwerified" enc_S.
	 */
	S  = &mem[mem_off];
	mem_off += nbytes;

	enc_A  = &mem[mem_off];
	mem_off += nbytes;

	k_dp = &mem[mem_off];
	mem_off += CCC_SHA512_DIGEST_LEN;

	(void)mem_off;

	/* End working variables */

	eflags = 0U;
	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		eflags |= CMP_FLAGS_VEC1_LE;
	}

	/* ED25519 values are in LITTLE ENDIAN (defined in the spec) */
	eflags |= CMP_FLAGS_VEC2_LE;

	/* ENC(S) value must be in set { 0, 1, ..., L-1 }
	 * If so: S == ENC(S)
	 *
	 * L = lwrve order = lwrve->n
	 */
	if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(lwrve->n, S, nbytes, eflags, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_SIGNATURE_ILWALID,
				     LTRACEF("EDDSA scalar S not less than lwrve order\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

// rbad now NO_ERROR

	/* "A and R are elements of E" => they must be points on ED25519 lwrve
	 * The input values are little endian, keep also the points that way.
	 */

	LTRACEF("Decompressing R\n");

	/* Decompress point R, verify on lwrve */
	HW_MUTEX_TAKE_ENGINE(engine);
	ret = ed25519_point_decompress_locked(&ec_context->ec, R, enc_R);
	HW_MUTEX_RELEASE_ENGINE(engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Decompressing enc_R failed\n"));

	LTRACEF("Decompressing R complete\n");

	ret = ed25519_verify_setup_pubkey(ec_context, lwrve, &A, &enc_A);
	CCC_ERROR_CHECK(ret);

	/* Here: R and A have been decompressed to lwrve points on E
	 * and S is a verified member of set { 0, 1, 2, ... L-1 }.
	 *
	 * Also we have:
	 *
	 * enc_A = ENC(A)
	 * enc_R = ENC(R)
	 * S     = ENC(S)
	 * PH(M) = input_params->input_size bytes in input_params->src
	 *
	 * PH may be the identity function: then PH(M) == M == message.
	 *
	 * For TE_ALG_ED25519PH the PH function is SHA512 and
	 * input_params->src contains a 64 byte SHA512 digest.
	 *
	 * Callwlate: k = SHA512(R,A,M)
	 */
	ret = ed25519_digest_RAM(ec_context, lwrve, k_dp, enc_R, enc_A, input_params);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("K (sha digest 64 bytes)", k_dp, CCC_SHA512_DIGEST_LEN);

	/* R is overwritten in the call (used as RHS_point)
	 * It is not used after this call.
	 */
#if VERIFY_GROUP_EQUATION_LONG_FORM
	ret = ed25519_verify_long_form_group_equation(ec_context,
						      tmp_point,
						      lwrve, S, R, k_dp, A);
#else
	ret = ed25519_verify_short_form_group_equation(ec_context,
						       tmp_point,
						       lwrve, S, R, k_dp, A);
#endif /* VERIFY_GROUP_EQUATION_LONG_FORM */

	CCC_ERROR_CHECK(ret);

fail:
	ed2559_verify_cleanup(ec_context, pmem);

	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#if HAVE_ED25519_SIGN == 0

status_t se_ed25519_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	LTRACEF("Entry\n");
	ret = SE_ERROR(ERR_NOT_SUPPORTED);

	(void)ec_context;
	(void)input_params;

	LTRACEF("Exit: %d\n", ret);
	return ret;
}

#else

#if HAVE_POINT_COMPRESS == 0
#error "ED25519 sign needs point compress"
#endif

#if HAVE_GEN_MULTIPLY == 0
#if CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X)
/* In T23X this is no longer required.
 */
#error "ED25519 sign needs gen multiply"
#endif
#endif

/* Derive r = SHA512(dom2(F,C) || prefix || PH(M))
 *
 * ED25519: dom2(F,C)   == empty octet stream
 * ED25519PH: dom2(F,C) == octet stream
 * ED25519CTX: dom2(F,C) == octet stream
 *
 * prefix = h[32]..h[63], i.e. the upper half of
 * value of SHA512(private_key)
 *
 * DST must be at least 64 bytes long.
 */
static status_t ed25519_derive_r(const struct se_ec_context *ec_context,
				 const pka1_ec_lwrve_t *lwrve,
				 uint8_t *dst,
				 const struct se_data_params *input_params)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;
	uint32_t nbytes = 0U;

	/* Context for SHA-512 calls.
	 */
	struct se_sha_context *sha_context = NULL;

	LTRACEF("entry\n");

	nbytes = lwrve->nbytes;

	sha_context = CMTAG_MEM_GET_OBJECT(ec_context->ec.cmem,
					   CMTAG_ALIGNED_CONTEXT,
					   CCC_DMA_MEM_ALIGN,
					   struct se_sha_context,
					   struct se_sha_context *);
	if (NULL == sha_context) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	/* need to compute r  = H(dom2(F,C) || prefix || PH(M))
	 * where H == SHA-512  (as in ED25519 specs).
	 *
	 * dom2() is NULL input for current version;
	 * prefix = upper half (32 bytes) of ec_key
	 * PH(M) is input_params->src
	 *
	 * The created SHA context shares the EC cmem context.
	 */
	ret = sha_context_static_init(TE_CRYPTO_DOMAIN_KERNEL,
				      CCC_ENGINE_ANY,
				      TE_ALG_SHA512,
				      sha_context,
				      ec_context->ec.cmem);
	CCC_ERROR_CHECK(ret,
			LTRACEF("SHA512 context init failed: %d\n", ret));

	sha_context->ec.is_last = false;

	/* dom2(x, y) => The blank octet string when
	 * signing or verifying Ed25519.
	 */
	if (TE_ALG_ED25519PH == ec_context->ec.algorithm) {
		ret = ed25519_dom2(ec_context, sha_context, &input, 1U);
		CCC_ERROR_CHECK(ret);
	}

	if (TE_ALG_ED25519CTX == ec_context->ec.algorithm) {
		ret = ed25519_dom2(ec_context, sha_context, &input, 0U);
		CCC_ERROR_CHECK(ret);
	}

	/* Perform a partial digest operation in the specified sequence, first
	 * digest_update(h[32]..h[63])
	 */
	input.src	  = &ec_context->ec.ec_pkey[nbytes];
	input.input_size  = nbytes;
	input.dst	  = NULL;
	input.output_size = 0U;

	LOG_HEXBUF("r SHA input h[32..63]", input.src, nbytes);

	ret = PROCESS_SHA(&input, sha_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Digest h[upper] failed\n"));

	/*
	 * And then for the last data pushed to the digest:
	 * k = digest_dofinal(PH(M))
	 *
	 * The resulting SHA512 digest value r is a 64 byte
	 * little-endian integer for deriving point R
	 *
	 * input_params->src data is in the caller domain econtext->domain
	 */
	sha_context->ec.is_last = true;

	input.src	  = input_params->src;
	input.input_size  = input_params->input_size;
	input.dst	  = dst;
	input.output_size = CCC_SHA512_DIGEST_LEN;

	LOG_HEXBUF("r SHA input PH(M)", input.src, nbytes);

	/* input_params->src (message M) data lives in caller domain */
	sha_context->ec.domain = ec_context->ec.domain;
	ret = PROCESS_SHA(&input, sha_context);
	sha_context->ec.domain = TE_CRYPTO_DOMAIN_KERNEL;

	CCC_ERROR_CHECK(ret,
			LTRACEF("Digest PH(M) failed\n"));

fail:
	if (NULL != sha_context) {
		/* Contains no secrets, no need to clear */
#if CCC_USE_DMA_MEM_BUFFER_ALLOC
		sha_context_dma_buf_release(sha_context);
#endif
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_ALIGNED_CONTEXT,
				  sha_context);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_le_reduce_dp(const struct se_ec_context *ec_context,
				     const pka1_ec_lwrve_t *lwrve,
				     uint8_t *val_dp)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };
	const engine_t *engine = NULL;

	LTRACEF("entry\n");

	engine = ec_context->ec.engine;

	LOG_HEXBUF("val_dp to be dp reduced mod L (i.e. lwrve order n)", val_dp, 64U);

	HW_MUTEX_TAKE_ENGINE(engine);

	modpar.size = lwrve->nbytes;
	modpar.op_mode = lwrve->mode;
	modpar.m = lwrve->n;
	modpar.cmem = ec_context->ec.cmem;

	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
	}

	modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
	modpar.mod_flags |= PKA1_MOD_FLAG_Y_LITTLE_ENDIAN; // ??? y not used
	modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;

	/* val_dp = val_dp mod L (double precision value, 64 bytes for ed25519)
	 * overwrites val_dp to 32 bytes.
	 *
	 * The double-length (64 bytes) value, e.g. generated by SHA512,
	 * is treated as little-endian value.
	 *
	 * For this operation modular lib expects buffer size to be
	 * 2*modpar.size
	 */
	modpar.x = val_dp;
	modpar.r = val_dp;

	ret = CCC_ENGINE_SWITCH_MODULAR_BIT_REDUCTION_DP(engine, &modpar);
	HW_MUTEX_RELEASE_ENGINE(engine);

	LOG_HEXBUF("DP_REDUCED val_dp", val_dp, CCC_ED25519_LWRVE_BLEN);

	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * S = (r + k * a) mod L
 *
 * All vector values are 32 byte little endian vectors.
 */
static status_t ed25519_compute_S(const struct se_ec_context *ec_context,
				  const pka1_ec_lwrve_t *lwrve,
				  uint8_t	       *S,
				  const uint8_t	       *r,
				  const uint8_t	       *k_dp,
				  const uint8_t	       *a)
{
	status_t ret = NO_ERROR;
	te_mod_params_t modpar = { .size = 0U };
	const engine_t *engine = NULL;
	uint8_t tmp[CCC_ED25519_LWRVE_BLEN];
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	engine = ec_context->ec.engine;

	HW_MUTEX_TAKE_ENGINE(engine);

	CCC_LOOP_BEGIN {
		modpar.size = lwrve->nbytes;
		modpar.op_mode = lwrve->mode;
		modpar.m = lwrve->n;
		modpar.cmem = ec_context->ec.cmem;

		if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
			modpar.mod_flags |= PKA1_MOD_FLAG_M_LITTLE_ENDIAN;
		}

		/* All values little endian */
		modpar.mod_flags |= PKA1_MOD_FLAG_X_LITTLE_ENDIAN;
		modpar.mod_flags |= PKA1_MOD_FLAG_Y_LITTLE_ENDIAN;
		modpar.mod_flags |= PKA1_MOD_FLAG_R_LITTLE_ENDIAN;

		modpar.x = k_dp;
		modpar.y = a;
		modpar.r = &tmp[0];

		ret = ed_mod_multiply(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		modpar.x = r;
		modpar.y = &tmp[0];
		modpar.r = S;

		ret = CCC_ENGINE_SWITCH_MODULAR_ADDITION(engine, &modpar);
		CCC_ERROR_END_LOOP(ret);

		CCC_LOOP_STOP;
	} CCC_LOOP_END;

	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("Resulting S", S, CCC_ED25519_LWRVE_BLEN);
fail:
	se_util_mem_set(tmp, 0, sizeof_u32(tmp));

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_sign_param_check_input(const struct se_data_params *input_params,
					       const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == input_params->dst) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (input_params->output_size < CCC_ED25519_SIGNATURE_LEN) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Prehashed input is a SHA-512 digest which is 64 bytes long */
	if (TE_ALG_ED25519PH == ec_context->ec.algorithm) {
		if ((NULL == input_params->src) ||
		    (input_params->input_size != CCC_SHA512_DIGEST_LEN)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("ED25519PH input is not a SHA-512 digest\n"));
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t ed25519_sign_param_check(const struct se_data_params *input_params,
					 const struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == ec_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ec_context->ec.ec_lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve not defined\n"));
	}

	if (TE_LWRVE_ED25519 != ec_context->ec.ec_lwrve->id) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Wrong lwrve for ED25519\n"));
	}

	/* Always true for ED25519 */
	if (CCC_ED25519_LWRVE_BLEN != ec_context->ec.ec_lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* EDDSA signing needs an EC private key
	 */
	if (ec_context->ec.ec_keytype != KEY_TYPE_EC_PRIVATE) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == ec_context->ec.ec_pkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ed25519_sign_param_check_input(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

/* ED25519 signing: steps 2..3
 */
static status_t ed25519_handle_r(const struct se_data_params *input_params,
				 struct se_ec_context *ec_context,
				 const pka1_ec_lwrve_t *lwrve,
				 uint8_t *r,
				 te_ec_point_t *R,
				 uint8_t *enc_R)
{
	status_t ret = NO_ERROR;
	uint32_t nbytes = lwrve->nbytes;

	LTRACEF("entry\n");

	/* Step 2: r = SHA512(dom2(F,C) || prefix || PH(M))\n")
	 */
	ret = ed25519_derive_r(ec_context, lwrve, r, input_params);
	CCC_ERROR_CHECK(ret, LTRACEF("ED25519 deriving r failed\n"));

	/* reduce 64 byte LE r modulo L to 32 byte LE value
	 */
	ret = ed25519_le_reduce_dp(ec_context, lwrve, r);
	CCC_ERROR_CHECK(ret,
			LTRACEF("ED25519 DP reduction of r failed\n"));

	/* Step 3: R = r * B
	 * where B = lwrve base point (generator G)
	 * where r was DP reduced above to 32 bytes
	 *
	 * r is in little endian byte order.
	 */
	ret = ec_point_multiply(&ec_context->ec, R, NULL, r, nbytes, true);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to derive R\n"));

#if POINT_OP_RESULT_VERIFY
	ret = ec_check_point_on_lwrve(&ec_context->ec, lwrve, R);
	CCC_ERROR_CHECK(ret, LTRACEF("R on ED25519 failed\n"));
#endif

	/* Let string enc_R be encoding of point R (i.e. compress point R)
	 */
	ret = ed25519_point_compress(&ec_context->ec, enc_R, R);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to compress R\n"));

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

/* ED25519 signing: pubkey compressing and steps 4..end.
 */
static status_t ed25519_handle_k(const struct se_data_params *input_params,
				 const struct se_ec_context *ec_context,
				 const pka1_ec_lwrve_t *lwrve,
				 uint8_t *k_dp,
				 const uint8_t *enc_R,
				 uint8_t *enc_A,
				 uint8_t *enc_S,
				 const uint8_t *r)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Let string enc_A be encoding of public key (i.e. compress public key)
	 */
	ret = ed25519_point_compress(&ec_context->ec,
				     enc_A,
				     ec_context->ec.ec_pubkey);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to compress public key\n"));

	/* Step 4:
	 * Callwlate: k = SHA512(R,A,M)
	 * where k_dp is 64 byte little endian integer value.
	 */
	ret = ed25519_digest_RAM(ec_context, lwrve, k_dp, enc_R, enc_A, input_params);
	CCC_ERROR_CHECK(ret);

	/* reduce 64 byte LE k modulo L to 32 byte LE value
	 */
	ret = ed25519_le_reduce_dp(ec_context, lwrve, k_dp);
	CCC_ERROR_CHECK(ret,
			LTRACEF("ED25519 DP reduction of k failed\n"));

	ret = ed25519_compute_S(ec_context, lwrve, enc_S, r, k_dp, ec_context->ec.ec_pkey);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

static status_t ed25519_handle_pubkey_for_sign(struct se_ec_context *ec_context,
					       const pka1_ec_lwrve_t *lwrve)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Step 1. hash private key with h=SHA-512(pkey).
	 * s = encode(h[0..31])
	 *
	 * This is already done in ed25519_scalar_expand() in crypto_asig.c
	 *
	 * Construct public key A from the pre-encoded private key if
	 * pubkey not provided.
	 *
	 * The public key is in ec_context->ec.ec_pubkey after this call.
	 */
	if ((NULL == ec_context->ec.ec_pubkey) ||
	    ((ec_context->ec.ec_pubkey->point_flags & CCC_EC_POINT_FLAG_UNDEFINED) != 0U)) {
		ret = ed25519_callwlate_public_key(ec_context);
		CCC_ERROR_CHECK(ret,
				LTRACEF("ED25519 pubkey derivation failed\n"));
	}

	ret = ec_check_point_on_lwrve(&ec_context->ec, lwrve, ec_context->ec.ec_pubkey);
	CCC_ERROR_CHECK(ret, LTRACEF("Pubkey on ED25519 failed\n"));

fail:
	LTRACEF("Exit: %d\n", ret);
	return ret;
}

status_t se_ed25519_sign(
	struct se_data_params *input_params,
	struct se_ec_context *ec_context)
{
	status_t ret = NO_ERROR;
	te_ec_point_t *pmem = NULL;
	uint8_t *mem = NULL;
	uint32_t mem_off = 0U;
	uint32_t nbytes = 0U;

	const pka1_ec_lwrve_t *lwrve = NULL;

	/* Mempool objects follow */

	te_ec_point_t *R = NULL; /* Point R = r * B */

	/* These little endian scalars are 32 bytes each
	 */
	uint8_t *enc_S = NULL; /* sig field enc_S == S */
	uint8_t *enc_R = NULL; /* sig field enc_R, 32 bytes (compressed point R) */
	uint8_t *enc_A = NULL; /* compressed pubkey (32 bytes) */

	/* 64 byte SHA512 digest.
	 * Treated as a little endian scalar value.
	 */
	uint8_t *k_dp	 = NULL;

	/* 64 byte SHA512 value.
	 * Treated as a little endian scalar value.
	 *
	 * r = SHA512(dom2(F,C) || prefix || PH(M))
	 */
	uint8_t *r	 = NULL;

	/* Use mempool for the above to reduce stack usage.
	 */
#define X_ED25519_SIGN_CHUNK_SIZE					\
	(sizeof_u32(te_ec_point_t) + (3U * nbytes) + (2U * CCC_SHA512_DIGEST_LEN))

	LTRACEF("entry\n");

	ret = ed25519_sign_param_check(input_params, ec_context);
	CCC_ERROR_CHECK(ret);

	/* NULL input allowed for ED25519, set length 0 if so
	 */
	if (NULL == input_params->src) {
		input_params->input_size = 0U;
	}

	/* Private key to sign the message ec_pkey[0..63]
	 *
	 * Signing also needs the public key (A, ec_pubkey) which is derived if not
	 * provided. In addition to the public key also the compressed
	 * pubkey (enc_A) is callwlated (a little_endian 32 byte value).
	 *
	 * First 32 bytes of ec_pkey are used for public key derivation,
	 * latter 32 bytes used for deriving value r.
	 */
	lwrve  = ec_context->ec.ec_lwrve;
	nbytes = lwrve->nbytes;

	if (nbytes != CCC_ED25519_LWRVE_BLEN) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	pmem = CMTAG_MEM_GET_TYPED_BUFFER(ec_context->ec.cmem,
					  CMTAG_BUFFER,
					  0U,
					  te_ec_point_t *,
					  X_ED25519_SIGN_CHUNK_SIZE);
	if (NULL == pmem) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	R = &pmem[0];

	mem = (uint8_t *)&pmem[1];
	mem_off = 0U;

	enc_R = &mem[mem_off];
	mem_off += nbytes;

	enc_S = &mem[mem_off];
	mem_off += nbytes;

	enc_A = &mem[mem_off];
	mem_off += nbytes;

	k_dp = &mem[mem_off];
	mem_off += CCC_SHA512_DIGEST_LEN;

	r = &mem[mem_off];
	mem_off += CCC_SHA512_DIGEST_LEN;

	(void)mem_off;

	ret = ed25519_handle_pubkey_for_sign(ec_context, lwrve);
	CCC_ERROR_CHECK(ret);

	ret = ed25519_handle_r(input_params, ec_context, lwrve, r, R, enc_R);
	CCC_ERROR_CHECK(ret);

	ret = ed25519_handle_k(input_params, ec_context, lwrve, k_dp, enc_R, enc_A, enc_S, r);
	CCC_ERROR_CHECK(ret);

	/* ED25519 signature is tupple <enc_R,enc_S>
	 * where both enc_R and S are 32 byte scalar values and the three most significant
	 * bits of the final octet are always zero.
	 *
	 * Mask off the lsb from S.
	 */
	enc_S[31] = BYTE_VALUE(enc_S[31] & 0x1FU);

	se_util_mem_move(&input_params->dst[0], enc_R, CCC_ED25519_LWRVE_BLEN);
	se_util_mem_move(&input_params->dst[32U], enc_S, CCC_ED25519_LWRVE_BLEN);
	input_params->output_size = CCC_ED25519_SIGNATURE_LEN;

	LOG_HEXBUF("ED25519 signature <enc_R || enc_S>", input_params->dst, CCC_ED25519_SIGNATURE_LEN);

fail:
	if (NULL != pmem) {
		se_util_mem_set((uint8_t *)pmem, 0U, X_ED25519_SIGN_CHUNK_SIZE);
		CMTAG_MEM_RELEASE(ec_context->ec.cmem,
				  CMTAG_BUFFER,
				  pmem);
	}

	LTRACEF("Exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_ED25519_SIGN */
#endif /* CCC_WITH_ECC && CCC_WITH_ED25519 && CCC_WITH_MODULAR */
