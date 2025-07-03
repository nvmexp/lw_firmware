/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Rewritten and stripped from lwgpu safe arith functions.
 */
#ifndef INCLUDED_CCC_SAFE_ARITH_H
#define INCLUDED_CCC_SAFE_ARITH_H

/* See: gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
 */
#ifndef HAVE_BUILTIN_OVERFLOW
#define HAVE_BUILTIN_OVERFLOW 0
#endif

#ifndef HAVE_EMSG_FROM_ARITH_ERROR
#define HAVE_EMSG_FROM_ARITH_ERROR 1
#endif

#ifndef CCC_UINT8_MAX
#ifdef UINT8_MAX
#define CCC_UINT8_MAX ((uint8_t)UINT8_MAX)
#else
# ifdef UCHAR_MAX
# define CCC_UINT8_MAX ((uint8_t)UCHAR_MAX)
# else
# define CCC_UINT8_MAX ((uint8_t)255U)
# endif
#endif
#endif /* CCC_UINT8_MAX */

/* Defined for backwards compatibility */
#define CCC_UCHAR_MAX CCC_UINT8_MAX

#ifndef CCC_UINT_MAX
#ifdef UINT32_MAX
#define CCC_UINT_MAX UINT32_MAX
#else
# ifdef UINT_MAX
# define CCC_UINT_MAX UINT_MAX
# else
# define CCC_UINT_MAX (4294967295U)
# endif
#endif
#endif /* CCC_UINT_MAX */

#ifndef CCC_INT_MAX
#ifdef INT32_MAX
#define CCC_INT_MAX INT32_MAX
#else
# ifdef INT_MAX
# define CCC_INT_MAX INT_MAX
# else
# define CCC_INT_MAX (2147483647)
# endif
#endif
#endif /* CCC_INT_MAX */

#ifndef CCC_ULONG_MAX
#ifdef ULONG_MAX
#define CCC_ULONG_MAX ULONG_MAX
#else
/* This is inaclwrate; please define CCC_ULONG_MAX
 * if this is not correct for your subsystem.
 */
# if PHYS_ADDR_LENGTH == 8U
# define CCC_ULONG_MAX (18446744073709551615ULL)
# else
# define CCC_ULONG_MAX (4294967295UL)
# endif
#endif /* !defined(ULONG_MAX) */
#endif /* CCC_ULONG_MAX */

#ifndef CCC_INT_MIN
#ifdef INT_MIN
#define CCC_INT_MIN INT_MIN
#else
#define CCC_INT_MIN ((-CCC_INT_MAX) - 1)
#endif
#endif /* CCC_INT_MIN */

#ifndef CCC_ARITHMETIC_ERROR

#if HAVE_EMSG_FROM_ARITH_ERROR
/* Can not use inline function here as emsg output may need
 * LOCAL_TRACE to be set, so use this instead.
 */
void se_util_arith_emsg(const char *msg);

#define CCC_ARITH_EMSG(x) se_util_arith_emsg(x)

#define NEED_UTIL_ARITH_EMSG
#else

/* Do not compile se_util_arith_emsg,
 * no error message output from arithmetic errors.
 */
#define CCC_ARITH_EMSG(x)

#endif /* HAVE_EMSG_FROM_ARITH_ERROR */

#if USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
#define CCC_ARITHMETIC_ERROR						\
	do { ret = SE_ERROR(ERR_NOT_VALID); CCC_ARITH_EMSG("CCC arithmetic error\n"); } while(false)
#else
#define CCC_ARITHMETIC_ERROR						\
	{ ret = SE_ERROR(ERR_NOT_VALID); CCC_ARITH_EMSG("CCC arithmetic error\n"); }
#endif

#endif /* CCC_ARITHMETIC_ERROR */

/* uint32_t operations
 * Caller must check the ret status after the arith macro.
 */
#define CCC_ADD_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_add_u32(&(lv_u32), a_u32, b_u32)
#define CCC_SUB_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_sub_u32(&(lv_u32), a_u32, b_u32)
#define CCC_MULT_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_mult_u32(&(lv_u32), a_u32, b_u32)
#define CCC_DIV_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_div_u32(&(lv_u32), a_u32, b_u32)
#define CCC_MOD_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_mod_u32(&(lv_u32), a_u32, b_u32)

/* int32_t operations
 * Caller must check the ret status after the arith macro.
 *
 * CCC does not do signed modulus: not added.
 */
#define CCC_SUB_S32(lv_s32, a_s32, b_s32)			\
	ccc_safe_sub_s32(&(lv_s32), a_s32, b_s32)
#define CCC_ADD_S32(lv_s32, a_s32, b_s32)			\
	ccc_safe_add_s32(&(lv_s32), a_s32, b_s32)
#define CCC_DIV_S32(lv_s32, a_s32, b_s32)			\
	ccc_safe_div_s32(&(lv_s32), a_s32, b_s32)

/* uint64_t operations
 * Caller must check the ret status after the arith macro.
 */
#define CCC_ADD_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_add_u64(&(lv_u64), a_u64, b_u64)
#define CCC_SUB_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_sub_u64(&(lv_u64), a_u64, b_u64)
#define CCC_MULT_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_mult_u64(&(lv_u64), a_u64, b_u64)
#define CCC_DIV_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_div_u64(&(lv_u64), a_u64, b_u64)
#define CCC_MOD_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_mod_u64(&(lv_u64), a_u64, b_u64)

/* cast longer to shorter unsigned type
 * Caller must check the ret status after the arith macro.
 */
#define CCC_CAST_U32_U8(lv_u8, a_u32)			\
	ccc_safe_cast_u32_to_u8(&(lv_u8), a_u32)
#define CCC_CAST_U64_U32(lv_u32, a_u64)			\
	ccc_safe_cast_u64_to_u32(&(lv_u32), a_u64)

/**
 * @def CCC_SAFE_ICEIL_U32(lv_u32, a_u32, b_u32)
 *
 * @brief Safe ceiling value of a_u32/b_u32 (smallest integer >= a_u32/b_u32)
 *
 * Caller must check the ret status after the arith macro.
 */
#define CCC_ICEIL_U32(lv_u32, a_u32, b_u32)			\
	ccc_safe_iceil_u32(&(lv_u32), a_u32, b_u32)

/**
 * @def CCC_SAFE_ICEIL_U64(lv_u64, a_u64, b_u64)
 *
 * @brief Safe ceiling value of a_u64/b_u64 (smallest integer >= a_u64/b_u64)
 *
 * Caller must check the ret status after the arith macro.
 */
#define CCC_ICEIL_U64(lv_u64, a_u64, b_u64)			\
	ccc_safe_iceil_u64(&(lv_u64), a_u64, b_u64)

/* Check arithmetic result */
#if USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
#define CCC_IS_SAFE_CHECK(funcall)					\
	do { ret = (funcall); CCC_ERROR_CHECK(ret); } while(false)
#else
#define CCC_IS_SAFE_CHECK(funcall)			\
	{ ret = (funcall); CCC_ERROR_CHECK(ret); }
#endif /* USE_DO_WHILE_IN_ERROR_HANDLING_MACROS */

/* uint32_t operations
 */
#define CCC_SAFE_ADD_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_add_u32(&(lv_u32), a_u32, b_u32))
#define CCC_SAFE_SUB_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_sub_u32(&(lv_u32), a_u32, b_u32))
#define CCC_SAFE_MULT_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_mult_u32(&(lv_u32), a_u32, b_u32))
#define CCC_SAFE_DIV_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_div_u32(&(lv_u32), a_u32, b_u32))
#define CCC_SAFE_MOD_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_mod_u32(&(lv_u32), a_u32, b_u32))

/* int32_t operations
 *
 * CCC does not do signed modulus: not added.
 */
#define CCC_SAFE_SUB_S32(lv_s32, a_s32, b_s32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_sub_s32(&(lv_s32), a_s32, b_s32))
#define CCC_SAFE_ADD_S32(lv_s32, a_s32, b_s32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_add_s32(&(lv_s32), a_s32, b_s32))
#define CCC_SAFE_DIV_S32(lv_s32, a_s32, b_s32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_div_s32(&(lv_s32), a_s32, b_s32))

/* uint64_t operations
 */
#define CCC_SAFE_ADD_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_add_u64(&(lv_u64), a_u64, b_u64))
#define CCC_SAFE_SUB_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_sub_u64(&(lv_u64), a_u64, b_u64))
#define CCC_SAFE_MULT_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_mult_u64(&(lv_u64), a_u64, b_u64))
#define CCC_SAFE_DIV_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_div_u64(&(lv_u64), a_u64, b_u64))
#define CCC_SAFE_MOD_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_mod_u64(&(lv_u64), a_u64, b_u64))

/* cast longer to shorter unsigned type.
 */
#define CCC_SAFE_CAST_U32_U8(lv_u8, a_u32)					\
	CCC_IS_SAFE_CHECK(ccc_safe_cast_u32_to_u8(&(lv_u8), a_u32))
#define CCC_SAFE_CAST_U64_U32(lv_u32, a_u64)					\
	CCC_IS_SAFE_CHECK(ccc_safe_cast_u64_to_u32(&(lv_u32), a_u64))

/**
 * @def CCC_SAFE_ICEIL_U32(lv_u32, a_u32, b_u32)
 *
 * @brief Safe ceiling value of a_u32/b_u32 (smallest integer >= a_u32/b_u32)
 */
#define CCC_SAFE_ICEIL_U32(lv_u32, a_u32, b_u32)				\
	CCC_IS_SAFE_CHECK(ccc_safe_iceil_u32(&(lv_u32), a_u32, b_u32))

/**
 * @def CCC_SAFE_ICEIL_U64(lv_u64, a_u64, b_u64)
 *
 * @brief Safe ceiling value of a_u64/b_u64 (smallest integer >= a_u64/b_u64)
 */
#define CCC_SAFE_ICEIL_U64(lv_u64, a_u64, b_u64)				\
	CCC_IS_SAFE_CHECK(ccc_safe_iceil_u64(&(lv_u64), a_u64, b_u64))

/* safer arithmetic functions, return error code (ERR_NOT_VALID) on
 * arithmetic overflows/underflows.
 *
 * When/if CCC_ARITHMETIC_ERROR return an error must be returned by the
 * containing function: by default ERR_NOT_VALID.
 *
 * The code in these checker functions must comply with this
 * requirement!
 */

/* Addition */

static inline status_t ccc_safe_add_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_add_overflow(ui_a, ui_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;
	uint32_t usum = 0;

	usum = ui_a + ui_b;
	if (usum < ui_a) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = usum;
	}
	return ret;
#endif
}

static inline status_t ccc_safe_add_s32(int32_t *lval, int32_t si_a, int32_t si_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_add_overflow(si_a, si_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;

	if (((si_b > 0) && (si_a > (CCC_INT_MAX - si_b))) ||
	    ((si_b < 0) && (si_a < (CCC_INT_MIN - si_b)))) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = si_a + si_b;
	}
	return ret;
#endif
}

static inline status_t ccc_safe_add_u64(uint64_t *lval, uint64_t ul_a, uint64_t ul_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_add_overflow(ul_a, ul_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;
	uint64_t usum = 0;

	usum = ul_a + ul_b;
	if (usum < ul_a) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = usum;
	}
	return ret;
#endif
}

/* Subtraction */

static inline status_t ccc_safe_sub_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_sub_overflow(ui_a, ui_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;

	if (ui_a < ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ui_a - ui_b;
	}
	return ret;
#endif
}

static inline status_t ccc_safe_sub_s32(int32_t *lval, int32_t si_a, int32_t si_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_sub_overflow(si_a, si_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;

	if (((si_b > 0) && (si_a < (CCC_INT_MIN + si_b))) ||
	    ((si_b < 0) && (si_a > (CCC_INT_MAX + si_b)))) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = si_a - si_b;
	}
	return ret;
#endif
}

static inline status_t ccc_safe_sub_u64(uint64_t *lval, uint64_t ul_a, uint64_t ul_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_sub_overflow(ul_a, ul_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;

	if (ul_a < ul_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ul_a - ul_b;
	}
	return ret;
#endif
}

/* Multiplication */

static inline status_t ccc_safe_mult_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_mul_overflow(ui_a, ui_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;
	bool doit = true;

	// don't reveal which one is zero and multiply
	// if no error (also multiply with zero).
	//
	if ((0U != ui_b) && (0U != ui_a)){
		if (ui_a > (CCC_UINT_MAX / ui_b)) {
			doit = false;
			CCC_ARITHMETIC_ERROR;
		}
	}

	if (true == doit) {
		*lval = ui_a * ui_b;
	}

	return ret;
#endif
}

static inline status_t ccc_safe_mult_u64(uint64_t *lval, uint64_t ul_a, uint64_t ul_b)
{
#if HAVE_BUILTIN_OVERFLOW
	return (__builtin_mul_overflow(ul_a, ul_b, lval) ? SE_ERROR(ERR_NOT_VALID) : NO_ERROR);
#else
	status_t ret = NO_ERROR;
	bool doit = true;

	if ((0U != ul_b) && (0U != ul_a)){
		if (ul_a > (CCC_ULONG_MAX / ul_b)) {
			doit = false;
			CCC_ARITHMETIC_ERROR;
		}
	}

	if (true == doit) {
		*lval = ul_a * ul_b;
	}

	return ret;
#endif
}

/* Division */

static inline status_t ccc_safe_div_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
	status_t ret = NO_ERROR;
	if (0U == ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ui_a / ui_b;
	}
	return ret;
}

static inline status_t ccc_safe_div_s32(int32_t *lval, int32_t si_a, int32_t si_b)
{
	status_t ret = NO_ERROR;
	if ((0 == si_b) || ((CCC_INT_MIN == si_a) && (-1 == si_b))) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = si_a / si_b;
	}
	return ret;
}

static inline status_t ccc_safe_div_u64(uint64_t *lval, uint64_t ui_a, uint64_t ui_b)
{
	status_t ret = NO_ERROR;
	if (0ULL == ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ui_a / ui_b;
	}
	return ret;
}

/* Modulus */

static inline status_t ccc_safe_mod_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
	status_t ret = NO_ERROR;
	if (0U == ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ui_a % ui_b;
	}
	return ret;
}

static inline status_t ccc_safe_mod_u64(uint64_t *lval, uint64_t ui_a, uint64_t ui_b)
{
	status_t ret = NO_ERROR;
	if (0ULL == ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = ui_a % ui_b;
	}
	return ret;
}

/* Unsigned narrow casts */

static inline status_t ccc_safe_cast_u64_to_u32(uint32_t *lval, uint64_t ul_a)
{
	status_t ret = NO_ERROR;

	if (ul_a > CCC_UINT_MAX) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = (uint32_t)ul_a;
	}
	return ret;
}

static inline status_t ccc_safe_cast_u32_to_u8(uint8_t *lval, uint32_t ui_a)
{
	status_t ret = NO_ERROR;

	if (ui_a > CCC_UINT8_MAX) {
		CCC_ARITHMETIC_ERROR;
	} else {
		*lval = (uint8_t)ui_a;
	}
	return ret;
}

static inline status_t ccc_safe_iceil_u32(uint32_t *lval, uint32_t ui_a, uint32_t ui_b)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	if (0U == ui_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		if (0U == ui_a) {
			val = 0U;
		} else {
			val = 1U + ((ui_a - 1U) / ui_b);
		}

		*lval = val;
	}
	return ret;
}

static inline status_t ccc_safe_iceil_u64(uint64_t *lval, uint64_t ul_a, uint64_t ul_b)
{
	status_t ret = NO_ERROR;
	uint64_t val = 0UL;

	if (0UL == ul_b) {
		CCC_ARITHMETIC_ERROR;
	} else {
		if (0UL == ul_a) {
			val = 0UL;
		} else {
			val = 1UL + ((ul_a - 1UL) / ul_b);
		}

		*lval = val;
	}
	return ret;
}
#endif /* INCLUDED_CCC_SAFE_ARITH_H */
