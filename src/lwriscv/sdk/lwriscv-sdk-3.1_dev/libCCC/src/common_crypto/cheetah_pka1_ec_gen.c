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

#if CCC_WITH_ECC

#include <crypto_ec.h>
#include <tegra_cdev.h>

#if HAVE_RNG1_DRNG
#include <tegra_rng1.h>
#endif

#if HAVE_NIST_LWRVES
#include <nist_lwrves.h>
#endif

#if HAVE_BRAINPOOL_LWRVES
#include <brainpool_lwrves.h>
#endif /* HAVE_BRAINPOOL_LWRVES */

#if HAVE_KOBLITZ_LWRVES
#include <koblitz_lwrves.h>
#endif

#if CCC_WITH_X25519
#include <c25519_lwrves.h>
#endif

#if CCC_WITH_ED25519
#include <ed25519_lwrves.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/**** NIST F(p) lwrves ****/

/* XXX Consider supporting this later */
static bool fips_enabled = false;

#if HAVE_NIST_LWRVES

/* The macro auto-sets the montgomery_ok flag if the m_prime value
 * is defined in the lwrve parameters. If m_prime is set but r2 is not =>
 * all lwrve operations will fail for that lwrve (so => don't do that).
 *
 * Added "X_" prefix for Cert-C scanners.
 *
 * NIST P521 lwrve handled separately.
 */
#define CCC_EC_ADD_NIST_LWRVE(size, flag)				\
	{								\
		.id = TE_LWRVE_NIST_P_ ## size,				\
		.flags = PKA1_LWRVE_FLAG_A_IS_M3 | (flag) | NIST_MONT_FLAG(size), \
		.name = "NIST_P-" #size ,				\
		.g = {							\
			.gen_x = ec_nist_p ## size .x,			\
			.gen_y = ec_nist_p ## size .y,			\
		},							\
		.mg = {							\
			.m_prime = NIST_MONT_VALUE_OR_NULL(size, m_prime), \
			.r2 = NIST_MONT_VALUE_OR_NULL(size, r2),	\
		},							\
		.p = ec_nist_p ## size .p,				\
		.n = ec_nist_p ## size .n,				\
		.a = ec_nist_p ## size .a,				\
		.b = ec_nist_p ## size .b,				\
		.nbytes = (size ## U / 8U),				\
		.mode = PKA1_OP_MODE_ECC ## size ,			\
		.type = PKA1_LWRVE_TYPE_WEIERSTRASS,			\
	 }

#define ADD_NIST_LWRVE_FIPS(size) CCC_EC_ADD_NIST_LWRVE(size, PKA1_LWRVE_FLAG_FIPS)
#define ADD_NIST_LWRVE(size)	  CCC_EC_ADD_NIST_LWRVE(size, PKA1_LWRVE_FLAG_NONE)
#endif /* HAVE_NIST_LWRVES */

#if HAVE_BRAINPOOL_LWRVES

#define ADD_BRAINPOOL_LWRVE_R1(size)				\
	{							\
		.id = TE_LWRVE_BRAINPOOL_P ## size ## r1,	\
		.flags = BP_R1_MONT_FLAG(size),			\
		.name = "brainpoolP" #size "r1",		\
		.g = {						\
			.gen_x = ec_bp_p ## size ## r1.x,	\
			.gen_y = ec_bp_p ## size ## r1.y,	\
		},						\
		.mg = {							\
			.m_prime = BP_R1_MONT_VALUE_OR_NULL(size, m_prime), \
			.r2 = BP_R1_MONT_VALUE_OR_NULL(size, r2),	\
		},							\
		.p = ec_bp_p ## size ## r1.p,			\
		.n = ec_bp_p ## size ## r1.n,			\
		.a = ec_bp_p ## size ## r1.a,			\
		.b = ec_bp_p ## size ## r1.b,			\
		.nbytes = (size ## U / 8U),			\
		.mode = PKA1_OP_MODE_ECC ## size,		\
		.type = PKA1_LWRVE_TYPE_WEIERSTRASS,		\
	}

#if HAVE_BRAINPOOL_TWISTED_LWRVES
#define ADD_BRAINPOOL_TWISTED_LWRVE_T1(size)			\
	{							\
		.id = TE_LWRVE_BRAINPOOL_P ## size ## t1,	\
		.flags = BP_T1_MONT_FLAG(size),			\
		.name = "brainpoolP" #size "t1 (twisted lwrve)",\
		.g = {						\
			.gen_x = ec_bp_p ## size ## t1.x,	\
			.gen_y = ec_bp_p ## size ## t1.y,	\
		},						\
		.mg = {						\
			.m_prime = BP_T1_MONT_VALUE_OR_NULL(size, m_prime), \
			.r2 = BP_T1_MONT_VALUE_OR_NULL(size, r2),	\
		},						\
		.p = ec_bp_p ## size ## t1.p,			\
		.n = ec_bp_p ## size ## t1.n,			\
		.a = ec_bp_p ## size ## t1.a,			\
		.b = ec_bp_p ## size ## t1.b,			\
		.nbytes = (size ## U / 8U),			\
		.mode = PKA1_OP_MODE_ECC ## size ,		\
		.type = PKA1_LWRVE_TYPE_WEIERSTRASS,		\
	}
#endif /* HAVE_BRAINPOOL_TWISTED_LWRVES*/
#endif /* HAVE_BRAINPOOL_LWRVES */

#if HAVE_KOBLITZ_LWRVES
#define ADD_KOBLITZ_LWRVE(size)						\
	{								\
		.id = TE_LWRVE_KOBLITZ_P_ ## size ## k1,		\
		.flags = KOBLITZ_MONT_FLAG(size),			\
		.name = "secp" #size "k1", \
		.g = {						\
			.gen_x = ec_koblitz_p ## size ## k1.x,	\
			.gen_y = ec_koblitz_p ## size ## k1.y,	\
		},							 \
		.mg = {							 \
			.m_prime = KOBLITZ_MONT_VALUE_OR_NULL(size, m_prime), \
			.r2 = KOBLITZ_MONT_VALUE_OR_NULL(size, r2),	\
		},							\
		.p = ec_koblitz_p ## size ## k1.p,		\
		.n = ec_koblitz_p ## size ## k1.n,		\
		.a = ec_koblitz_p ## size ## k1.a,		\
		.b = ec_koblitz_p ## size ## k1.b,		\
		.nbytes = (size ## U / 8U),			\
		.mode = PKA1_OP_MODE_ECC ## size ,		\
		.type = PKA1_LWRVE_TYPE_WEIERSTRASS,		\
	 }
#endif /* HAVE_KOBLITZ_LWRVES */

/* Use the above macros to define the supported lwrves
 *
 * This table is only used in ccc_ec_get_lwrve() to lookup
 * lwrve values, so it could be moved in that function.
 */
static const pka1_ec_lwrve_t pka1_lwrves[] =
{
#if HAVE_NIST_LWRVES

	/* NIST lwrves */

#if CCC_SOC_WITH_PKA1
#if HAVE_NIST_P192
	ADD_NIST_LWRVE(192),
#endif
#if HAVE_NIST_P224
	ADD_NIST_LWRVE(224),
#endif
#endif /* #if CCC_SOC_WITH_PKA1 */

#if HAVE_NIST_P256
	ADD_NIST_LWRVE_FIPS(256),
#endif
#if HAVE_NIST_P384
	ADD_NIST_LWRVE_FIPS(384),
#endif

#if HAVE_P521 && HAVE_NIST_P521
	{
		.id = TE_LWRVE_NIST_P_521,
		.flags = PKA1_LWRVE_FLAG_A_IS_M3 |
			 PKA1_LWRVE_FLAG_SPECIAL | PKA1_LWRVE_FLAG_NO_MONTGOMERY | NIST_MONT_FLAG(521),
		.name = "NIST_P-521",
		.g = {
			.gen_x = ec_nist_p521.x,
			.gen_y = ec_nist_p521.y,
		},
		.mg = {
			.m_prime = NIST_MONT_VALUE_OR_NULL(521, m_prime),
			.r2 = NIST_MONT_VALUE_OR_NULL(521, r2),
		},
		.p = ec_nist_p521.p,
		.n = ec_nist_p521.n,
		.a = ec_nist_p521.a,
		.b = ec_nist_p521.b,
		.nbytes = CCC_NIST_P521_BYTE_LENGTH,
		.mode = PKA1_OP_MODE_ECC521,
		.type = PKA1_LWRVE_TYPE_WEIERSTRASS,
	},
#endif /* HAVE_P521 && HAVE_NIST_P521 */

#endif /* HAVE_NIST_LWRVES */

#if HAVE_BRAINPOOL_LWRVES

	/* Brainpool lwrves */

#if HAVE_BP_160
	ADD_BRAINPOOL_LWRVE_R1(160),
#endif
#if HAVE_BP_192
	ADD_BRAINPOOL_LWRVE_R1(192),
#endif
#if HAVE_BP_224
	ADD_BRAINPOOL_LWRVE_R1(224),
#endif
#if HAVE_BP_256
	ADD_BRAINPOOL_LWRVE_R1(256),
#endif
#if HAVE_BP_320
	ADD_BRAINPOOL_LWRVE_R1(320),
#endif
#if HAVE_BP_384
	ADD_BRAINPOOL_LWRVE_R1(384),
#endif
#if HAVE_BP_512
	ADD_BRAINPOOL_LWRVE_R1(512),
#endif

#if HAVE_BRAINPOOL_TWISTED_LWRVES

	/* Brainpool twisted lwrves */

#if HAVE_BPT_160
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(160),
#endif
#if HAVE_BPT_192
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(192),
#endif
#if HAVE_BPT_224
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(224),
#endif
#if HAVE_BPT_256
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(256),
#endif
#if HAVE_BPT_320
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(320),
#endif
#if HAVE_BPT_384
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(384),
#endif
#if HAVE_BPT_512
	ADD_BRAINPOOL_TWISTED_LWRVE_T1(512),
#endif
#endif /* HAVE_BRAINPOOL_TWISTED_LWRVES */

#endif /* HAVE_BRAINPOOL_LWRVES */

#if HAVE_KOBLITZ_LWRVES

	/* KOBLITZ lwrves */

#if HAVE_KOBLITZ_P160K1
	ADD_KOBLITZ_LWRVE(160),
#endif
#if HAVE_KOBLITZ_P192K1
	ADD_KOBLITZ_LWRVE(192),
#endif
#if HAVE_KOBLITZ_P224K1
	ADD_KOBLITZ_LWRVE(224),
#endif
#if HAVE_KOBLITZ_P256K1
	ADD_KOBLITZ_LWRVE(256),
#endif

#endif /* HAVE_KOBLITZ_LWRVES */

#if CCC_WITH_X25519 /* Single instance lwrve Lwrve25519 */
	/* b coefficient not defined for this lwrve, left NULL,
	 * not using montgomery values => NULL
	 */
	{
		.id = TE_LWRVE_C25519,
		.flags = PKA1_LWRVE_FLAG_SPECIAL | PKA1_LWRVE_FLAG_NO_MONTGOMERY,
		.name = "Lwrve25519",
		.g = {
			.gen_x = ec_c25519.x,
			.gen_y = ec_c25519.y,
		},
		.mg = {
			.m_prime = NULL,
			.r2 = NULL,
		},
		.p = ec_c25519.p,
		.n = ec_c25519.n,
		.a = ec_c25519.a,
		.b = NULL,
		.nbytes = 32U,
		.mode = PKA1_OP_MODE_ECC256,
		.type = PKA1_LWRVE_TYPE_LWRVE25519,
	},
#endif /* CCC_WITH_X25519 */

#if CCC_WITH_ED25519 /* Single instance lwrve ED25519 */
	/* a && b coefficient not defined for this lwrve
	 * Uses coefficient d (constant value -121665/121666),
	 * not using montgomery values => NULL
	 */
	{
		.id = TE_LWRVE_ED25519,
		.flags = PKA1_LWRVE_FLAG_SPECIAL | PKA1_LWRVE_FLAG_NO_MONTGOMERY,
		.name = "ED25519",
		.g = {
			.gen_x = ec_ed25519.x,
			.gen_y = ec_ed25519.y,
		},
		.mg = {
			.m_prime = NULL,
			.r2 = NULL,
		},
		.p = ec_ed25519.p,
		.n = ec_ed25519.n,
		.a = ec_ed25519.a,

		/* The D value was initialized to field B in ed25519_lwrve.h
		 * Actual field B values are not used for this lwrve, so
		 * it is used as storage for value D.
		 *
		 * PKA1 does not use A or B values, so it can be now set to
		 * expected value (-1 mod p) for other engines.
		 */
		.d = ec_ed25519.b,
		.b = NULL,
		.nbytes = 32U,
		.mode = PKA1_OP_MODE_ECC256,
		.type = PKA1_LWRVE_TYPE_ED25519,
	},
#endif /* CCC_WITH_ED25519 */
};

const struct pka1_ec_lwrve_s *ccc_ec_get_lwrve(te_ec_lwrve_id_t lwrve_id)
{
	const pka1_ec_lwrve_t *selected_lwrve = NULL;
	uint32_t cindex = 0U;
	const uint32_t asize = sizeof_u32(pka1_lwrves);
	const uint32_t lwrve_count = asize / sizeof_u32(pka1_ec_lwrve_t);

	for (; cindex < lwrve_count; cindex++) {
		if (pka1_lwrves[cindex].id == lwrve_id) {
			if (!BOOL_IS_TRUE(fips_enabled) ||
			    ((pka1_lwrves[cindex].flags & PKA1_LWRVE_FLAG_FIPS) != 0U)) {
				selected_lwrve = &pka1_lwrves[cindex];
				break;
			}
		}
	}

	if (NULL == selected_lwrve) {
		LTRACEF("Lwrve %u not supported\n", lwrve_id);
	}
	return selected_lwrve;
}

#if HAVE_PKA1_SCALAR_MULTIPLIER_CHECK
/* Returns true if vec1 > 2
 */
static bool pka1_ec_vec_gt_two(const uint8_t *vec1, uint32_t nbytes, bool vec1_BE)
{
	uint32_t i = 0U;
	bool rbool = false;

	do {
		if (0U == nbytes) {
			rbool = false;
			break;
		}

		if (1U == nbytes) {
			rbool = vec1[0] > 2U;
			break;
		}

		if (BOOL_IS_TRUE(vec1_BE)) {
			i = nbytes-1U;
		} else {
			i = 0U;
		}

		if (vec1[i] > 2U) {
			rbool = true;
			break;
		}

		if (BOOL_IS_TRUE(vec1_BE)) {
			for (i = 0U; i < nbytes - 2U; i++) {
				if (vec1[i] != 0U) {
					rbool = true;
					break;
				}
			}
		} else {
			for (i = 1U; i < nbytes - 1U; i++) {
				if (vec1[i] != 0U) {
					rbool = true;
					break;
				}
			}
		}
	} while (false);

	return rbool;
}

static status_t ec_scalar_multiplier_valid_check(se_engine_ec_context_t *econtext,
						 const uint8_t *scalar,
						 uint32_t scalar_len,
						 bool *valid_p)
{
	status_t ret = NO_ERROR;
	const pka1_ec_lwrve_t *lwrve = NULL;

	if ((NULL == econtext) || (NULL == scalar) || (NULL == valid_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	lwrve = econtext->ec_lwrve;

	if (NULL == lwrve) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC lwrve unset in econtext\n"));
	}

	if ((NULL == lwrve->n) || (0U == lwrve->nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("EC %u lwrve parameters invalid\n",
					     lwrve->id));
	}

	if (scalar_len != lwrve->nbytes) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Scalar length does not match EC lwrve byte size\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t pka1_ec_scalar_multiplier_valid(se_engine_ec_context_t *econtext,
					 const uint8_t *scalar,
					 uint32_t scalar_len,
					 bool scalar_BE,
					 bool *valid_p)
{
	status_t ret = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	const pka1_ec_lwrve_t *lwrve = NULL;
	const uint8_t *order = NULL;
	uint32_t nbytes = 0U;
	uint32_t eflags = 0U;

	ret = ec_scalar_multiplier_valid_check(econtext,
					       scalar, scalar_len,
					       valid_p);
	CCC_ERROR_CHECK(ret);

	*valid_p = false;

	lwrve  = econtext->ec_lwrve;
	order  = lwrve->n;
	nbytes = lwrve->nbytes;

	/* Scalar multiplier must be in range (2 < k < order)
	 */
	if (!BOOL_IS_TRUE(pka1_ec_vec_gt_two(scalar, nbytes, scalar_BE))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Scalar value <= 2\n"));
	}

	eflags = 0U;
	if ((lwrve->flags & PKA1_LWRVE_FLAG_LITTLE_ENDIAN) != 0U) {
		eflags |= CMP_FLAGS_VEC1_LE;
	}
	if (!BOOL_IS_TRUE(scalar_BE)) {
		eflags |= CMP_FLAGS_VEC2_LE;
	}
	if (!BOOL_IS_TRUE(se_util_vec_cmp_endian(order, scalar, nbytes, eflags, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Scalar value must be less than lwrve order\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	*valid_p = true;

	FI_BARRIER(status, rbad);
fail:
	if ((NO_ERROR == ret) && (NO_ERROR != rbad)) {
		ret = rbad;
	}
	return ret;
}
#endif /* HAVE_PKA1_SCALAR_MULTIPLIER_CHECK */

#if HAVE_ECDSA_SIGN
/* SIMPLE asn.1 vector to int value encoder for COMPATIBLE vectors.
 *
 * Encode vlen long vector in vect as big endian
 * ASN.1 integer: 0x02 <LEN> <ZPAD> <VALUE>
 * <ZPAD> is 0x00 or missing, depending on <VALUE>.
 *
 * Sets the encoding length.
 *
 * Restrictions: Vector must be longer at least 1 byte long and
 * shorter than 128 bytes. Vector values are not inspected for
 * minimal encodings, zero paddings on input vectors are not dropped.
 *
 * This is just a simple encoder for ECDSA signatures and similar
 * simple things.
 *
 * This implementation does not support encoding input vectors
 * with 0 or >= 128 value bytes, returning error if used.
 *
 * This means the the INTEGER LENGTH encoding fits in a single byte,
 * <0x02><LENGTH><VALUE> even when the VALUE has msb set and gets padded.
 *
 * <LENGTH> 0x80 means that there are 128 bytes in the value.
 *
 * If ever needed, just improve this later.
 */
status_t encode_vector_as_asn1_integer(uint8_t *obuf,
				       const uint8_t *vect, uint32_t vlen,
				       uint32_t *elen_p)
{
	status_t ret = NO_ERROR;
	uint32_t off = 0U;
	uint32_t len = 0U;

	LTRACEF("entry\n");

	if ((0U == vlen) || (vlen >= 128U)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_IMPLEMENTED,
				     LTRACEF("Unsupported vector length (%u) for ASN.1 int encoding\n",
					     vlen));
	}

	obuf[0] = ASN1_TAG_INTEGER;
	off = off + 2U; /* Skip the length byte in obuf[1] for now */

	if ((vect[0] & 0x80) != 0U) {
		/* Zero padding to keep number positive, increases length */
		obuf[off] = 0x00U;
		off++;
		len++;
	}

	se_util_mem_move(&obuf[off], &vect[0], vlen);
	len = len + vlen;

	if (len >= 128U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_IMPLEMENTED,
				     LTRACEF("Byte vector too long (%u) for ASN.1 int encoding\n",
					     vlen));
	}

	/* Vector length in bytes always fits in a single byte (msb not set)
	 * for compatible vectors. This is NOT a full ASN.1 integer encoder!!!
	 */
	obuf[1] = BYTE_VALUE(len);

	if (NULL != elen_p) {
		/* Sum of TAG(1) + LEN(1) + ZPAD(optional, 1) + vector(vlen) */
		*elen_p = (len + 2U);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_ECDSA_SIGN */

status_t asn1_get_integer(const uint8_t *p_a, uint32_t *val_p, uint32_t *len_p,
			  uint32_t maxlen)
{
	status_t ret = NO_ERROR;
	const uint8_t *p = p_a;
	uint32_t val = 0U;
	uint8_t bv = *p;

	LTRACEF("entry\n");

	if (0U == maxlen) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	LOG_HEXBUF("get_integer", p, maxlen);

	if (bv <= ASN1_INT_MAX_ONE_BYTE_VALUE) {
		/* bv is the int value */
		*val_p = bv;
		*len_p = 1;
	} else {
		/* (bv - 0x80) is the number of bytes in the int value */
		uint32_t len = ((uint32_t)bv - ASN1_INT_MAX_ONE_BYTE_VALUE) & 0xffU;
		uint32_t inx = 0U;
		CCC_LOOP_VAR;

		if (maxlen < (len + 1U)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
		}

		p++;

		CCC_LOOP_BEGIN {
			if (inx >= len) {
				break;
			}

			if ((val & 0xFF000000U) != 0U) {
				CCC_ERROR_END_LOOP_WITH_ECODE(ERR_NOT_VALID);
			}
			val = val << 8U;

			ret = CCC_ADD_U32(val, val, ((uint32_t)*p));
			CCC_ERROR_END_LOOP(ret);

			p++;
			inx++;
		} CCC_LOOP_END;
		CCC_ERROR_CHECK(ret);

		*len_p = len + 1U;
		*val_p = val;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t asn1_skip_zeropad(const uint8_t *p_a,
				  uint32_t bval_arg,
				  uint32_t clen,
				  uint32_t skip_a,
				  bool    *zero_padded_p,
				  uint32_t *skip_bytes,
				  uint32_t *vlen_p)
{
	status_t ret     = NO_ERROR;
	const uint8_t *p = p_a;
	bool zero_padded = false;
	uint32_t skip    = skip_a;
	uint32_t bval    = bval_arg;

	LTRACEF("entry\n");

	if (bval > clen) {
		uint32_t zpad = bval - clen;

		/* May be zero padded e.g. to make positive.
		 */
		for (; zpad > 0U; zpad--) {
			if (*p != 0x00U) {
				ret = SE_ERROR(ERR_NOT_VALID);
				break;
			}
			p++;
		}
		CCC_ERROR_CHECK(ret);

		zero_padded = true;
		CCC_SAFE_ADD_U32(skip, skip, (bval - clen));
		bval = clen;
	}

	/* p[skip] now moves to start of clen vector bytes
	 */
	*skip_bytes = skip;
	*zero_padded_p = zero_padded;
	*vlen_p = bval;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}


static status_t asn1_move_to_start_of_vector(const uint8_t *p_a,
					     uint32_t left_a,
					     uint32_t clen,
					     bool *zero_padded_p,
					     uint32_t *skip_bytes,
					     uint32_t *vlen_p)
{
	status_t ret = NO_ERROR;
	uint32_t left = left_a;
	const uint8_t *p = p_a;

	uint32_t bval = 0U;
	uint32_t skip = 0U;

	LTRACEF("entry\n");

	*vlen_p = 0U;

	/* encoded as <int><byte vector>
	 *
	 * <int> starts with the integer tag 0x02U
	 */
	if (*p != ASN1_TAG_INTEGER) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	/* skip one byte, added below to skip */
	p++;
	CCC_SAFE_SUB_U32(left, left, 1U);

	ret = asn1_get_integer(p, &bval, &skip, left);
	CCC_ERROR_CHECK(ret);

	p = &p[skip];

	CCC_SAFE_SUB_U32(left, left, skip);
	CCC_SAFE_ADD_U32(skip, skip, 1U);

	LTRACEF("parsed integer bval %u, skip %u, clen %u\n",
		bval, skip, clen);

	if (left < bval) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

#ifdef H_CLEN
	if (bval < clen) {
		// XXX TODO: maybe support short vectors, just align to right
		// XXX but openssl does not do such signatures...
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
#endif

	ret = asn1_skip_zeropad(p, bval, clen, skip,
				zero_padded_p, skip_bytes, vlen_p);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_asn1_ec_signature_coordinate_arg_check(const uint8_t *asn1_data,
							   uint32_t	asn1_bytes,
							   const uint8_t *cbuf,
							   uint32_t clen,
							   const uint32_t *used_bytes)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == asn1_data) ||
	    (NULL == cbuf) ||
	    (0U == clen) || (CCC_UINT_MAX == clen) ||
	    (NULL == used_bytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#ifdef H_CLEN
	if (asn1_bytes < (clen + 1U)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
#else
	/* Must have at least 0x02 <tag> and length byte */
	if (asn1_bytes < 2U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Simple ASN1 field extractor for the fixed format binary ECDSA
 * signature fields generated by openssl 1.*.
 *
 * Caller must make sure that CLEN bytes fits to cbuf.
 * CLEN is the co-ordinate length in the EC lwrve (lwrve size in bytes).
 */
status_t ecdsa_get_asn1_ec_signature_coordinate(const uint8_t *asn1_data,
						uint32_t	asn1_bytes,
						uint8_t *cbuf,
						uint32_t clen,
						uint32_t *used_bytes)
{
	status_t ret = NO_ERROR;
	const uint8_t *p = asn1_data;
	uint32_t left = asn1_bytes;
	bool zero_padded = false;
	uint32_t skip = 0U;
	uint32_t vector_len = 0U;
	uint32_t val32 = 0U;

	LTRACEF("entry\n");

	ret = get_asn1_ec_signature_coordinate_arg_check(asn1_data,
							 asn1_bytes,
							 cbuf,
							 clen,
							 used_bytes);
	CCC_ERROR_CHECK(ret);

	LOG_HEXBUF("ASN.1", asn1_data, asn1_bytes);
	LOG_HEXBUF("p ", p, asn1_bytes);
	LTRACEF("ZERO=left %u clen %u zero_padded %u, siglen %u \n",
		left, clen, zero_padded, asn1_bytes);

	ret = asn1_move_to_start_of_vector(p, left, clen, &zero_padded, &skip, &vector_len);
	CCC_ERROR_CHECK(ret);

	CCC_SAFE_SUB_U32(left, left, skip);

	if ((left < vector_len) || (0U == vector_len)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID);
	}

	p = &p[skip];

	LOG_HEXBUF("p start of vector bytes", p, clen);

	if ((p[0] & 0x80U) != 0U) {
		if (!BOOL_IS_TRUE(zero_padded)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("Negative ASN.1 Integer was not zero padded\n"));
		}
	}

	if (vector_len > clen) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("invalid state -- NOT A VALID CASE\n"));
	} else {
		/* Copy the requested signature field */
		uint32_t offset = clen - vector_len;

		/* prefix cleared by the caller */
		se_util_mem_move(&cbuf[offset], p, vector_len);
	}

	CCC_SAFE_ADD_U32(val32, skip, vector_len);
	*used_bytes = val32;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_ECC */
