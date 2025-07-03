/*
 * Copyright (c) 2015-2021, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#include <crypto_system_config.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if defined(NEED_UTIL_ARITH_EMSG)
void se_util_arith_emsg(const char *msg)
{
	if (NULL != msg) {
		CCC_ERROR_MESSAGE("%s", msg);
	}
}
#endif /* defined(NEED_UTIL_ARITH_EMSG) */

void se_util_mem_set(uint8_t *dst, uint8_t ch, uint32_t len)
{
	if ((NULL != dst) && (0U != len)) {
		uint32_t inx;
		for (inx = 0U; inx < len; inx++) {
			dst[inx] = ch;
		}
	}
}

#if defined(CCC_MEMMOVE_FUNCTION)
/* Subsystems can provide a library function that implements
 * a compatible memory copy.
 *
 * A valid prototype needs to be imported by system config
 * file.
 *
 * Note: memmove() implementations need to support overlapping
 * memory regions, i.e. copy forwards or backwards when
 * overlapping.
 */
void se_util_mem_move(uint8_t *dst, const uint8_t *src, uint32_t len)
{
	(void)CCC_MEMMOVE_FUNCTION(dst,src,len);
}

#else

static void mem_move(uint8_t *dst, const uint8_t *src, uint32_t len,
		     bool backwards)
{
	volatile uint8_t *d = dst;
	volatile const uint8_t *s = src;
	uint32_t inx;

	if (BOOL_IS_TRUE(backwards)) {
		inx = len;
		while (inx > 0U) {
			inx--;
			d[inx] = s[inx];
		}
	} else {
		inx = 0U;
		while (inx < len) {
			d[inx] = s[inx];
			inx++;
		}
	}
}

void se_util_mem_move(uint8_t *dst, const uint8_t *src, uint32_t len)
{
	if (0U != len) {
		if (src != dst) {
			bool copy_backwards =
				(((const uint8_t *)dst > src) &&
				((const uint8_t *)dst <= &src[len - 1U]));

			mem_move(dst, src, len, copy_backwards);
		}
	}
}
#endif /* !defined(CCC_MEMMOVE_FUNCTION) */

/* reverse buffer byte order.
 * Buffers can be fully overlapping (src == dst) or distinct.
 *
 * XXX Partially overlapping buffers are not supported!!!
 * XXX FIXME: Add detection of this condition or support these.
 *
 * FIXME: add overlapping buffer reverse support!!!
 */
status_t se_util_reverse_list(
	uint8_t *dst, const uint8_t *src, uint32_t list_size)
{
	status_t ret = NO_ERROR;
	uint32_t i = 0U;
	const uint8_t *s = src;
	uint8_t       *d = dst;

	LTRACEF("entry\n");

	if ((NULL == dst) || (NULL == src) ||
	    (0U == list_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	FI_BARRIER_CONST(u8_ptr, s);
	FI_BARRIER(u8_ptr, d);

	if (s == d) {
		uint8_t tmp;

		for (i = 0U; i < (list_size / 2U); i++) {
			tmp = s[i];
			d[i] = s[list_size - i - 1U];
			d[list_size - i - 1U] = tmp;
		}

		FI_BARRIER(u32, i);
		if (i != (list_size / 2U)) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (i = 0U; i < list_size; i++) {
			d[i] = s[list_size - 1U - i];
		}

		FI_BARRIER(u32, i);
		if (i != list_size) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CMAC_AES
status_t se_util_left_shift_one_bit(uint8_t *in_buf, uint32_t size)
{
	status_t ret = NO_ERROR;
	uint32_t i = 0U;
	uint32_t val = 0U;

	// LTRACEF "entry\n"

	if ((NULL == in_buf) || (0U == size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* left shift one bit. Constant time for given bytecount */
	if (size > 1U) {
		for (i = 0U; i <= (size - 2U); i++) {
			val = in_buf[i];
			in_buf[i] = BYTE_VALUE(val << 1U);

			val = ((uint32_t)in_buf[i + 1U]) >> 7U;
			val = val & 0x1U;
			in_buf[i] = BYTE_VALUE(in_buf[i] | val);
		}
	}
	val = in_buf[i];
	in_buf[i] = BYTE_VALUE(val << 1U);
	i++;

	FI_BARRIER(u32, i);
	if (i != size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	// LTRACEF "exit\n"
	return ret;
}
#endif /* HAVE_CMAC_AES */

static inline void ret2_err(status_t *ret2_p, status_t ret)
{
	if ((NULL != ret2_p) && (NO_ERROR != ret)) {
		*ret2_p = ret;
	}
}

/* Returns true if vec is zero */
bool se_util_vec_is_zero(const uint8_t *vec, uint32_t nbytes, status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	bool rval = false;
	uint32_t i = 0U;
	uint8_t val = 0U;

	LTRACEF("entry\n");

	if (NULL == ret2_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	*ret2_p = ERR_BAD_STATE;

	if ((NULL == vec) || (0U == nbytes)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	for (i = 0U; i < nbytes; i++) {
		val |= BYTE_VALUE(vec[i]);
	}

	rval = (0U == val);

	FI_BARRIER(u32, i);
	if (i == nbytes) {
		*ret2_p = NO_ERROR;
	}

fail:
	ret2_err(ret2_p, ret);

	LTRACEF("exit: rval %u\n", rval);
	return rval;
}

/**
 * @brief Return true if the given N byte vector has odd value
 *
 * @param vec vector bytes
 * @param nbytes vector byte length
 * @param big_endian true if byte vector is big_endian
 *
 * @return true if LSB is odd
 */
bool se_util_vec_is_odd(const uint8_t *vec, uint32_t nbytes, bool big_endian)
{
	uint32_t n;
	bool rbool = false;

	if (BOOL_IS_TRUE(big_endian)) {
		n = 1U;
	} else {
		n = nbytes;
	}

	/* coverity incorrectly does not allow using (0U != nbytes) here.
	 */
	if (nbytes >= n) {
		rbool = (vec[nbytes - n] & 0x1U) != 0U;
	}

	return rbool;
}

/* Returns true if vec1 > vec2
 *
 * Flags specify the endianess of vectors:
 * if CMP_FLAGS_VEC1_LE is set => vec1 is little endian
 * if CMP_FLAGS_VEC2_LE is set => vec2 is little endian
 *
 * Constant time value loop.
 */
bool se_util_vec_cmp_endian(const uint8_t *vec1, const uint8_t *vec2, uint32_t nbytes,
			    uint32_t endian_flags, status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	uint32_t is_lt = 0U;
	uint32_t is_gt = 0U;

	uint32_t j = 0U;
	bool vec1_LE = (endian_flags & CMP_FLAGS_VEC1_LE) != 0U;
	bool vec2_LE = (endian_flags & CMP_FLAGS_VEC2_LE) != 0U;
	bool arg_error = ((NULL == vec1) || (NULL == vec2) || (0U == nbytes));

	LTRACEF("entry\n");

	if (NULL == ret2_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	*ret2_p = ERR_BAD_STATE;

	if (BOOL_IS_TRUE(arg_error)) {
		*ret2_p = ERR_ILWALID_ARGS;
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	for (j = 0U; j < nbytes; j++) {
		uint32_t i  = nbytes - 1U - j;
		uint32_t v1 = (vec1_LE ? (uint32_t)vec1[i] : (uint32_t)vec1[j]);
		uint32_t v2 = (vec2_LE ? (uint32_t)vec2[i] : (uint32_t)vec2[j]);

		/* Will be set non-zero when subtraction results in
		 * an underflow.
		 */
		uint32_t gt_diff = (v2 - v1);
		uint32_t lt_diff = (v1 - v2);
		uint32_t gt = ((gt_diff >> (uint32_t)8U) & 0x1U);
		uint32_t lt = ((lt_diff >> (uint32_t)8U) & 0x1U);

		is_lt = (lt & ~is_gt) | is_lt;
		is_gt = (gt & ~is_lt) | is_gt;
	}

	FI_BARRIER(u32, j);

	if (j == nbytes) {
		*ret2_p = NO_ERROR;
	}

fail:
	ret2_err(ret2_p, ret);

	LTRACEF("exit: rval %u\n", (is_gt != 0U));
	return (is_gt != 0U);
}

/*
 * true if matching, false otherwise.
 */
#define MATCH_PREFIX 0x13E500U

/* complexity reduction */
static inline status_t vec_match_check_args(const uint8_t *v1, const uint8_t *v2,
					    uint32_t vlen, const status_t *ret2_p)
{
	status_t ret = ERR_BAD_STATE;

	if (NULL == ret2_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	if ((NULL == v1) || (NULL == v2) || (0U == vlen)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid arguments\n"));
	}

	ret = NO_ERROR;
fail:
	return ret;
}

bool se_util_vec_match(const uint8_t *v1, const uint8_t *v2,
		       uint32_t vlen, bool swap, status_t *ret2_p)
{
	status_t ret = ERR_BAD_STATE;
	bool rval = false;
	uint32_t inx = 0U;
	uint32_t cmp = MATCH_PREFIX;
	uint32_t tmp = 0U;

	if (NO_ERROR != vec_match_check_args(v1, v2, vlen, ret2_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*ret2_p = ret;

	if (BOOL_IS_TRUE(swap)) {
		for (; inx < vlen; inx++) {
			tmp = v2[vlen - inx - 1U];

			tmp = (uint32_t)v1[inx] ^ tmp;
			cmp = (cmp | tmp);
		}
	} else {
		for (; inx < vlen; inx++) {
			tmp = v2[inx];

			tmp = (uint32_t)v1[inx] ^ tmp;
			cmp = (cmp | tmp);
		}
	}
	cmp = cmp ^ MATCH_PREFIX;

	FI_BARRIER(u32, inx);
	if (inx == vlen) {
		rval = (cmp == 0U);
		ret = NO_ERROR;
	}

fail:
	if (NULL != ret2_p) {
		*ret2_p = ret;
	}
	return rval;
}

#if HAVE_CCC_64_BITS_DATA_COLWERSION
/* Compiler must not optimize this into any larger scalar moves than
 * what we have here. In case the byte array is in device memory it
 * only supports naturally aligned accesses to such.
 *
 * Use barriers to prevent that.
 */
uint64_t se_util_buf_to_u64(const uint8_t *buf, bool buf_BE)
{
	uint64_t val64 = 0ULL;
	uint32_t val32 = 0U;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(buf_BE)) {
		val32 = se_util_buf_to_u32(&buf[0], true);
		FI_BARRIER(u32, val32);

		val64 = (uint64_t)val32 << 32U;
		val32 = se_util_buf_to_u32(&buf[4], true);
	} else {
		val32  = se_util_buf_to_u32(&buf[4], false);
		FI_BARRIER(u32, val32);
		val64 = (uint64_t)val32 << 32U;
		val32 = se_util_buf_to_u32(&buf[0], false);
	}
	FI_BARRIER(u32, val32);

	val64 = val64 | (uint64_t)val32;
	LTRACEF("exit => 0x%llx\n", (unsigned long long)val64);

	return val64;
}

void se_util_u64_to_buf(uint64_t val64, uint8_t *buf, bool buf_BE)
{
	if (BOOL_IS_TRUE(buf_BE)) {
		se_util_u32_to_buf(se_util_msw_u64(val64), &buf[0], true);
		se_util_u32_to_buf(se_util_lsw_u64(val64), &buf[4], true);
	} else {
		se_util_u32_to_buf(se_util_msw_u64(val64), &buf[4], false);
		se_util_u32_to_buf(se_util_lsw_u64(val64), &buf[0], false);
	}
}
#endif

uint32_t se_util_mem_cmp(const uint8_t *a, const uint8_t *b, uint32_t len)
{
	status_t ret = NO_ERROR;
	bool res = false;

	if (0U == len) {
		/* Zero byte compare on non-NULL vectors is OK */
		res = ((NULL != a) && (NULL != b));
	} else {
		res = se_util_vec_match(a, b, len, CFALSE, &ret);
	}

	(void)ret;

	return (res ? 0U : 1U);
}
