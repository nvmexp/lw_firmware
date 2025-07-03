/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/**
 * @file   crypto_util.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2017
 *
 * @brief Common utility functions and defines.
 *
 * This file is included by the crypto_system_config.h files in all subsystems.
 *
 * For CCC code logic it is necessary that this file first includes the crypto_defs.h
 * include file to define remaining setting fro the common crypto core code.
 *
 * In R&D compilations (SE_RD_DEBUG != 0) also defines
 * dump_data_object() hex dumper function.
 */
#ifndef INCLUDED_CRYPTO_UTIL_H
#define INCLUDED_CRYPTO_UTIL_H

/* Backwards compatibility settings for passing checkers during
 * code upgrade.
 */
#include <ccc_compat.h>

#include <crypto_defs.h>
#include <crypto_measure.h>
#include <crypto_barrier.h>
#include <ccc_safe_arith.h>

/** @brief Misra change for old compilers that do not really have _Bool type
 *
 * @defgroup boolean_values Define boolean values CFALSE and CTRUE
 */
/*@{*/
#ifndef CFALSE
#define CFALSE (1U == 0U)	/**< Boolean value false */
#endif

#ifndef CTRUE
#define CTRUE (1U == 1U)	/**< Boolean value true */
#endif

/** @brief Compatible boolean comparison, return boolean true if expression value is true.
 *
 * Exists only to avoid issues with compilers and checkers that do not
 * really support _Bool type and there are issues with the pre-defined
 * true and false values, which then may be considered unsigned byte
 * values instead of boolean values.
 *
 * Return boolean true if expression x value is true
 */
#ifndef BOOL_IS_TRUE
#define BOOL_IS_TRUE(x) (CTRUE == (x))
#endif
/*@}*/

/** @brief Returns coverity compatible uint8_t expression value after
 * implicit "int" type promotion of expr. Use only when expr
 * value can be safely cast to uint8_t type.
 *
 * @param expr expression promoted to int
 * @return uint8_t compatible expression value
 */
#define BYTE_VALUE(expr) ((uint8_t)(((uint32_t)(expr)) & 0xFFU))

/**
 * @def sizeof_u32(x)
 *
 * @brief Misra compatible sizeof() assigned to uint32_t lvalue
 * Current complex form due to cert-c scanner.
 *
 * @param x object to sizeof
 *
 * @return uint32_t size value
 */
#if PHYS_ADDR_LENGTH == 4
#define sizeof_u32(x) ((uint32_t)sizeof(x))
#elif PHYS_ADDR_LENGTH == 8
#define sizeof_u32(x) se_util_lsw_u64((uint64_t)sizeof(x))
#else
#error "Unsupported PHYS_ADDR_LENGTH setting"
#endif

/**
 * @def CCC_ERROR_CHECK(rvalue, emsg...)
 * @brief Error handling: Jump to handler on error code in rvalue.
 *
 * Error handler uses "goto fail" statement to exit function on error.
 * Execute statement "emesg;" if provided, "emesg" should be an arbitrary
 * error message outputting void function call.
 *
 * NOTES:
 * Implicit use of goto target label "fail" in the error handler.
 * The emesg is optional, var-args macro parameter.
 *
 * Dropping of comma from the varargs macros when emesg is omitted is
 * a language extension, but supported by all used compilers (AFAIK).
 *
 * @param rvalue enter error handler code if (NO_ERROR != (rvalue))
 * @param emsg intention is to use this as arbitrary error message funcall [OPTIONAL]
 *
 * TODO: Consider adding FI_BARRIER(u32, rvalue) before goto.
 *
 * @defgroup error_handling Error handling macros
 */
/*@{*/
#if USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
#define CCC_ERROR_CHECK(rvalue, emsg...)				\
	do { if (NO_ERROR != (rvalue)) { emsg; goto fail; } } while (CFALSE)
#else
#define CCC_ERROR_CHECK(rvalue, emsg...)				\
	{ if (NO_ERROR != (rvalue)) { emsg; goto fail; } }
#endif

/**
 * @def CCC_ERROR_WITH_ECODE(ecode, emsg...)
 * @brief Error handling: Jump to handler after ret = SE_ERROR(ecode)
 *
 * Error handler uses "goto fail" statement to exit function on error.
 * Execute statement "emesg;" if provided, "emesg" should be an arbitrary
 * error message outputting void function call.
 *
 * NOTES:
 * Implicit assignment of ecode into status_t variable ret
 * Implicit use of goto target label "fail" in the error handler.
 * The emesg is optional, var-args macro parameter.
 *
 * Dropping of comma from the varargs macros when emesg is omitted is
 * a language extension, but supported by all used compilers (AFAIK).
 *
 * TODO: Consider adding FI_BARRIER(u32, rvalue) before goto.
 *
 * @param ecode unconditional error handler entry after ret = SE_ERROR(ecode)
 * @param emsg intention is to use this as arbitrary error message funcall [OPTIONAL]
 */
#if USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
#define CCC_ERROR_WITH_ECODE(ecode, emsg...)				\
	do { ret = SE_ERROR(ecode); emsg; goto fail; } while (CFALSE)
#else
#define CCC_ERROR_WITH_ECODE(ecode, emsg...)				\
	{ ret = SE_ERROR(ecode); emsg; goto fail; }
#endif
/*@}*/

/**
 * @def CCC_LOOP_VAR
 * @brief CCC loops; declare loop control variable used by loop macros
 *
 * Define macros for grouping statements together in a compound statement
 * for the purpose of e.g. mutex locking, memory handling and ccm reduction.
 *
 * The CCC special loop is for gathering a sequence of instructions in
 * a compound statement.
 *
 * This mechanism is useful in cases where e.g. a mutex is locked before the loop
 * and released after the loop when statements in the loop could cause an error
 * condition but regular error macros can not be used since they would exit
 * via the fail label; this would leave the mutex locked.
 * The CCC loop contruct was added to simplify such cases.
 *
 * Another use for such a do/while loop is for conditional compilation
 * cases for optional algorithm support requiring special handling.
 *
 * Or memory pool control, or similar case.
 *
 * It is REQUIRED to check for errors with CCC_ERROR_CHECK(ret) macro after a
 * CCC loop contruct.
 *
 * The CCC loop construct is used as follows. Regular 'continue' and
 * 'break' statements can be used in a CCC loop normally.
 *
 * First declare the loop control variable in an outer block:
 *
 * CCC_LOOP_VAR;
 *
 * To write e.g. mutex locked loop using these macros:
 *
 * HW_MUTEX_TAKE_ENGINE(engine);
 *
 * CCC_LOOP_BEGIN {
 *	## sample call, loop ends on error
 *	ret = call_x();
 *	CCC_ERROR_END_LOOP(ret, LTRACEF("Loop terminated, ret=%d\n", ret));
 *
 *	## or as follows, unconditional end with error
 *	if (state_badness > BAD_STATE_THRESHOLD) {
 *	   statements;
 *	   CCC_ERROR_END_LOOP_WITH_ECODE(ERR_BAD_STATE,
 *					 LTRACEF("Loop ended with ecode, ret=%d\n", ret));
 *	}
 *
 *	## or unconditional normal end (no error)
 *	statements;
 *	CCC_LOOP_STOP;
 *
 * } CCC_LOOP_END;
 *
 * HW_MUTEX_RELEASE_ENGINE(engine);
 * CCC_ERROR_CHECK(ret, LTRACEF("Loop exited with error, ret=%d\n", ret));
 *
 * Dropping of comma from the varargs macros when emesg is omitted is
 * a language extension, but supported by all used compilers (AFAIK).
 *
 * @defgroup ccc_loop CCC special loop controls
 */
/*@{*/
#define CCC_LOOP_VAR bool ccc___loop_done = CFALSE

/**
 * @def CCC_LOOP_REINIT
 * @brief CCC loops control re-initialized for next loop using the
 * same loop control variable declared with CCC_LOOP_VAR
 */
#define CCC_LOOP_REINIT ccc___loop_done = CFALSE

/**
 * @def CCC_LOOP_BEGIN
 * @brief CCC loops start; must be followed by an opening brace.
 *
 * Nested loops are not supported but they are not used
 * anyway to reduce complexity and checker complaints.
 */
#define CCC_LOOP_BEGIN do

/**
 * @def CCC_LOOP_END
 * @brief CCC loops start; must be preceeded by a closing brace
 * to complete the compound statement started after CCC_LOOP_BEGIN
 *
 * Nested loops are not supported to avoid complexity.
 *
 * It is required to check error values after this loop termination
 * when the CCC loop can set an error condition, by e.g. with
 * CCC_ERROR_CHECK(ret)
 */
#define CCC_LOOP_END while (!BOOL_IS_TRUE(ccc___loop_done))

/**
 * @def CCC_ERROR_END_LOOP(ret, emsg...)
 * @brief Error handling: end CCC loop if ret != NO_ERROR
 * after exelwting the optional emsg statement.
 */
#define CCC_ERROR_END_LOOP(ret, emsg...) \
	{ if (NO_ERROR != (ret)) { emsg; ccc___loop_done = CTRUE; continue; } }

/**
 * @def CCC_ERROR_END_LOOP_WITH_ECODE(ecode, emsg...)
 * @brief Error handling: execute continue statement in a special loop after
 * setting ret = SE_ERROR(ecode) and the optional emsg statement.
 *
 * Please see special loop description in CCC_ERROR_END_LOOP above.
 */
#define CCC_ERROR_END_LOOP_WITH_ECODE(ecode, emsg...) \
	{ ret = SE_ERROR(ecode); emsg; ccc___loop_done = CTRUE; continue; }

/**
 * @def CCC_LOOP_STOP
 * @brief Error handling: Unconditionally terminate CCC loop;
 * error code not modified.
 *
 * This is meant to terminate the loop without modifying error code.
 */
#define CCC_LOOP_STOP { ccc___loop_done = CTRUE; continue; }
/*@}*/

#if !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES)
/** @brief These erro handler macro names are defined only for
 * backwards compatibility, no longer used by CCC. The CCC_E* names
 * are used by CCC code for Cert-C compatibility.
 *
 * For macro documentation see the Cert-C compatible macro name docs.
 * The E* names are no longer used in CCC core code, may be used by
 * tests and subsystems.
 *
 * These are for the transition period for renaming the ERROR_HANDLER_xxx macros
 * FIXME: Remove these when final colwersion commit merged.
 *
 * [Defining macros starting with "E" appears to be a cert-c violation]
 *
 * @ingroup certc_compat_names
 */
/*@{*/
/** @brief: Backwards compatible name for CCC_ERROR_CHECK (DEPRECATED) */
#define ERROR_HANDLER_CHECK CCC_ERROR_CHECK

/** @brief: Backwards compatible name for CCC_ERROR_WITH_ECODE (DEPRECATED) */
#define ERROR_HANDLER_WITH_ECODE CCC_ERROR_WITH_ECODE
/*@}*/
#endif /* !defined(HAVE_CERT_C_COMPATIBLE_MACRO_NAMES) */

#define BYTES_IN_WORD_SHIFT 2U /** BYTES_IN_WORD_SHIFT must change with BYTES_IN_WORD */

/** @brief return the number of bytes of the words
 *
 * @param nwords the number of words to count
 * @return the number of bytes
 */
static inline uint32_t word_count_to_bytes(uint32_t nwords)
{
	return (uint32_t)(nwords << BYTES_IN_WORD_SHIFT);
}

/**
 * @brief set len elements of dst to value ch
 *
 * @param dst pointer to destination buffer.
 * @param ch  8 bit value to fill dst with
 * @param len size of dst buffer
 *
 * @return void
 */
void se_util_mem_set(uint8_t *dst, uint8_t ch, uint32_t len);

/**
 * @brief move len elements of src to dst
 *
 * @param dst pointer to destination buffer.
 * @param src pointer of source buffer.
 * @param len number of bytes to move
 *
 * @return void
 */
void se_util_mem_move(uint8_t *dst, const uint8_t *src, uint32_t len);

/**
 * @brief move num_words words of src32 to dst32
 *
 * @param dst32 word pointer to destination buffer.
 * @param src32 word pointer of source buffer.
 * @param num_words number of words to move
 *
 * @return void
 */
static inline void se_util_mem_move32(uint32_t *dst32, const uint32_t *src32,
					uint32_t num_words)
{
	uint32_t inx = 0U;

	for (;inx < num_words; inx++) {
		dst32[inx] = src32[inx];
	}
}

/**
 * @brief reverse the given input buffer
 *
 * @param dst pointer to destination buffer.
 * @param src pointer to buffer
 * @param list_size size of input buffer (size must be multiple of 4 bytes)
 *
 * @return error out if any
 *
 * Buffers can be fully overlapping (src == dst) or distinct but not partially overlapping.
 */
status_t se_util_reverse_list(
	uint8_t *dst, const uint8_t *src, uint32_t list_size);

/**
 * @brief move blen elements of src to dst, possibly reversing byte order.
 *
 * Shorthand for calling se_util_reverse_list() or se_util_mem_move()
 * selected by the need to reverse byte order.
 *
 * @param dst pointer to destination buffer.
 * @param src pointer of source buffer.
 * @param blen number of bytes to move
 * @param reverse true when bytes need to be reversed
 *
 * @return NO_ERROR on success, error code otherwise.
 */
static inline status_t se_util_move_bytes(uint8_t *dst, const uint8_t *src,
					  uint32_t blen, bool reverse)
{
	status_t ret = NO_ERROR;

	if (BOOL_IS_TRUE(reverse)) {
		ret = se_util_reverse_list(dst, src, blen);
		CCC_ERROR_CHECK(ret);
	} else {
		se_util_mem_move(dst, src, blen);
	}
fail:
	return ret;
}

#if HAVE_CMAC_AES
/**
 * @brief left shift entire input buffer by 1 bit
 *
 * @param in_buf pointer to buffer
 * @param size size of input buffer
 *
 * @return error out if any
 */
status_t se_util_left_shift_one_bit(
	uint8_t *in_buf, uint32_t size);
#endif /* HAVE_CMAC_AES */

/**
 * @brief Return the least significant 32 bit word of uint64_t value.
 *
 * This function exits due to various scanners preventing
 * truncation by casting.
 *
 * @param data64 64 bit unsigned value
 *
 * @return uint32_t least significant word of the argument
 */
static inline uint32_t se_util_lsw_u64(uint64_t data64)
{
    uint32_t value_32;
    uint64_t value_64 = data64;

    if (value_64 >= 0x100000000ULL) {
	value_64 &= 0xFFFFFFFFULL;
    }
    value_32 = (uint32_t)value_64;
    return value_32;
}

/**
 * @brief Return the most significant 32 bit word of uint64_t value.
 *
 * This function exits due to various scanners preventing
 * truncation by casting.
 *
 * @param data64 64 bit unsigned value
 *
 * @return uint32_t most significant word of the argument
 */
static inline uint32_t se_util_msw_u64(uint64_t data64)
{
	uint64_t val64 = (data64 & 0xFFFFFFFF00000000ULL);
	return se_util_lsw_u64(val64 >> 32ULL);
}

/**
 * @brief swap bytes in given input 32-bit value
 *	ex: 0x01020304 becomes 0x04030201
 *
 * @param val input 32-bit value
 *
 * @return byte swapped 32-bit value
 */
static inline uint32_t se_util_swap_word_endian(const uint32_t val)
{
	return (((val) >> 24U) |
		(((val) & 0x00FF0000U) >> (uint32_t)8U) |
		(((val) & 0x0000FF00U) << (uint32_t)8U) |
		((val) << 24U));
}

/**
 * @brief swap bytes in given input 64-bit value
 *	ex: 0x0102030405060708 becomes 0x0807060504030201
 *
 * @param val64 input 64-bit value
 *
 * @return byte swapped 64-bit value
 */
static inline uint64_t se_util_swap_longword_endian(const uint64_t val64)
{
	return (((uint64_t)se_util_swap_word_endian(se_util_lsw_u64(val64)) << 32U) |
		(uint64_t)se_util_swap_word_endian(se_util_msw_u64(val64)));
}

/**
 * @brief Return true if the most significant bit in the byte vector is set
 *
 * @param vec vector bytes
 * @param nbytes vector byte length
 * @param big_endian true if byte vector is big_endian
 *
 * @return true if msb of the MSB is set
 */
static inline bool se_util_vec_is_msb_set(const uint8_t *vec, uint32_t nbytes, bool big_endian)
{
	uint32_t n;
	bool rbool = false;

	if (BOOL_IS_TRUE(big_endian)) {
		n = nbytes;
	} else {
		n = 1U;
	}

	/* coverity incorrectly does not allow using (0U != nbytes) here.
	 */
	if (nbytes >= n) {
		rbool = (vec[nbytes - n] & 0x80U) != 0U;
	}

	return rbool;
}

/**
 * @brief Return true if the byte vector has zero value
 *
 * Constant time value loop.
 *
 * @param vec vector bytes
 * @param nbytes vector byte length
 * @param ret2_p referred status value set NO_ERROR on success, error code otherwise.
 *
 * @return true if byte vector has zero value
 */
bool se_util_vec_is_zero(const uint8_t *vec, uint32_t nbytes, status_t *ret2_p);

/**
 * @brief Return true if the given N byte vector has odd value
 *
 * @param vec vector bytes
 * @param nbytes vector byte length
 * @param big_endian true if byte vector is big_endian
 *
 * @return true if LSB is odd
 */
bool se_util_vec_is_odd(const uint8_t *vec, uint32_t nbytes, bool big_endian);

/**
 * @brief Byte vector comparisons.
 *
 * @defgroup util_vec_cmp vector Vector comparison endian flags
 */
/*@{*/
#define CMP_FLAGS_VEC1_LE 0x1U	/**< vec1 is LE byte order flag */
#define CMP_FLAGS_VEC2_LE 0x2U	/**< vec2 is LE byte order flag */

/**
 * @brief Returns true if vec1 > vec2
 *
 * Flags specify the endianess of vectors:
 * if CMP_FLAGS_VEC1_LE is set => vec1 is little endian
 * if CMP_FLAGS_VEC2_LE is set => vec2 is little endian
 *
 * Default byte order: big endian (BE).
 *
 * Constant time value loop.
 *
 * @param vec1 vector 1 byte buffer
 * @param vec2 vector 2 byte buffer
 * @param nbytes byte length of both vectors
 * @param endian_flags endian flags for both vectors
 * @param ret2_p referred status value set NO_ERROR on success, error code otherwise.
 *
 * @return true if vec1 > vec2
 */
bool se_util_vec_cmp_endian(const uint8_t *vec1, const uint8_t *vec2, uint32_t nbytes,
			    uint32_t endian_flags, status_t *ret2_p);
/*@}*/

/**
 * @brief Returns true if vec1 == vec2
 *
 * Default byte order: same byte order in vectors.
 *
 * Constant time value loop.
 *
 * @param v1 vector 1 byte buffer
 * @param v2 vector 2 byte buffer
 * @param vlen byte length of both vectors
 * @param swap vectors v1 and v2 have different byte order
 * @param ret2_p referred status value set NO_ERROR on success, error code otherwise.
 *
 * @return true if vec1 == vec2
 */
bool se_util_vec_match(const uint8_t *v1, const uint8_t *v2,
		       uint32_t vlen, bool swap, status_t *ret2_p);

/** @brief return a 4 byte u32 little endian value read from buffer in given endian
 *
 * @param buf address to write value to
 * @param buf_BE endianess of the buffer value
 * @return uint32_t value in little endian
 *
 * Compiler must not optimize this into any larger scalar moves than
 * what we have here. In case the byte array is in device memory it
 * only supports naturally aligned accesses to such.
 *
 * Use barriers to prevent that.
 */
static inline uint32_t se_util_buf_to_u32(const uint8_t *buf, bool buf_BE)
{
	uint32_t val32 = 0U;
	uint8_t ab, bb, cb, db;

	if (BOOL_IS_TRUE(buf_BE)) {
		ab = buf[0];
		bb = buf[1];
		cb = buf[2];
		db = buf[3];
	} else {
		ab = buf[3];
		bb = buf[2];
		cb = buf[1];
		db = buf[0];
	}

	FI_BARRIER(u8, ab);
	FI_BARRIER(u8, bb);
	FI_BARRIER(u8, cb);
	FI_BARRIER(u8, db);

	val32 = (((uint32_t)ab << 24U) | ((uint32_t)bb << 16U) |
		 ((uint32_t)cb << 8U)  | ((uint32_t)db << 0U));

	return val32;
}

/** @brief Write 4 byte val32 little endian value to buffer in given endian.
 *
 * The barriers are required to prevent compiler optimizing this
 * back into a word write which could cause data traps in some memory
 * systems when buf address is not word aligned.
 *
 * @param val32 uint32_t value to write
 * @param buf address to write value to
 * @param buf_BE endianess of the buffer value
 */
static inline void se_util_u32_to_buf(uint32_t val32, uint8_t *buf, bool buf_BE)
{
	if (BOOL_IS_TRUE(buf_BE)) {
		buf[0] = BYTE_VALUE(val32 >> 24U);
		FI_BARRIER(u8, buf[0]);
		buf[1] = BYTE_VALUE(val32 >> 16U);
		FI_BARRIER(u8, buf[1]);
		buf[2] = BYTE_VALUE(val32 >>  8U);
		FI_BARRIER(u8, buf[2]);
		buf[3] = BYTE_VALUE(val32);
	} else {
		buf[3] = BYTE_VALUE(val32 >> 24U);
		FI_BARRIER(u8, buf[3]);
		buf[2] = BYTE_VALUE(val32 >> 16U);
		FI_BARRIER(u8, buf[2]);
		buf[1] = BYTE_VALUE(val32 >>  8U);
		FI_BARRIER(u8, buf[1]);
		buf[0] = BYTE_VALUE(val32);
	}
}

#if HAVE_CCC_64_BITS_DATA_COLWERSION
/** @brief return a 8 byte u64 little endian value read from buffer in given endian
 *
 * @param buf address to write value to
 * @param buf_BE endianess of the buffer value
 * @return uint64_t value in little endian
 */
uint64_t se_util_buf_to_u64(const uint8_t *buf, bool buf_BE);

/** @brief Write 8 byte val64 little endian value to buffer in given endian.
 *
 * @param val64_param uint64_t value to write
 * @param buf address to write value to
 * @param buf_BE endianess of the buffer value
 */
void se_util_u64_to_buf(uint64_t val64, uint8_t *buf, bool buf_BE);
#endif

/**
 * @brief Returns 0U if vec1 == vec2, 1U otherwise
 *
 * If (len == 0U) comparison returns 0U only if both a and b
 * are not NULL. If either one is NULL return 1U.
 *
 * Function not used by CCC core, used by clients (mainly in regression tests).
 * Exists for backwards compatibility only.
 *
 * @param a vector a byte buffer
 * @param b vector b byte buffer
 * @param len byte length of both vectors
 *
 * @return 0U if vec1 == vec2, 1U otherwise
 */
uint32_t se_util_mem_cmp(const uint8_t *a, const uint8_t *b, uint32_t len);

#if SE_RD_DEBUG || defined(CCC_HEXDUMP_LOG)
/**
 * @brief R&D dump buffer in hex, 64 hex nibbles per line
 *
 * Used by R&D macro calls for dumping hex buffer data with
 * LOG_INFO output calls.
 *
 * Function is defined only if SE_RD_DEBUG or the subsystem
 * defines CCC_HEXDUMP_LOG mechanism to log hex dumps.
 *
 * @param name prefix to output. If NULL, using "" prefix string.
 * @param base Start of byte buffer to dump
 * @param size byte length of buffer base
 * @param bytes_out number of bytes to dump
 */
void dump_data_object(const char *name, const uint8_t *base,
		      uint32_t size, uint32_t bytes_out);
#endif /* SE_RD_DEBUG || defined(CCC_HEXDUMP_LOG) */

#if HAVE_SE_UTIL_VPRINTF_AP
/* Optional R&D log function, with runtime control of
 * LOG_INFO/LOG_SPEW level logs.
 *
 * Call ccc_rd_log_cond_set_state() to enable (default) and disable
 * conditional logging (as defined by config file).
 */
#define CCC_RD_LOG_COND_STATE_ENABLE	0x0U
#define CCC_RD_LOG_COND_STATE_DISABLE	0x1U

void ccc_rd_log_cond(const char *fmt, ...);
void ccc_rd_log_cond_set_state(uint32_t set_state);
#endif /* HAVE_SE_UTIL_VPRINTF_AP */

#endif /* INCLUDED_CRYPTO_UTIL_H */
