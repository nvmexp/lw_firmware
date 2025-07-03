/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/**
 * @file   crypto_barrier.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2018
 *
 * @brief FI barrier functions preventing compiler optimizations.
 */
#ifndef INCLUDED_CRYPTO_BARRIER_H
#define INCLUDED_CRYPTO_BARRIER_H

/* Coverity incorrectly claims that an ASM isolated in an
 * inline function would violate Misra rule 4.3.
 *
 * This is not true: the Misra C2012 spec explicitly recommends
 * using inline functions for this purpose with C99.
 *
 * @defgroup barriers Barriers for compiler optimizations
 */
/*@{*/
#define ISOLATED_ASM_BARRIER(x) __asm__ volatile ("" : "+r"(x));

/**
 * @def BARRIER_FUNCTION(type, id)
 *
 * @brief Construct a new static inline function
 * for handling barrier of given type for FI mitigation.
 *
 * @param type barrier argument and return type
 * @param id barrier identification
 */
#define BARRIER_FUNCTION(type, id)					\
	static inline void se_util_barrier_ ## id(type (*x_p))		\
	{								\
		type bv = *x_p;						\
		ISOLATED_ASM_BARRIER(bv);				\
		*x_p = bv;						\
	}

/**
 * @def BARRIER_CONST_FUNCTION(type, id)
 *
 * @brief Construct a new static inline function
 * for handling barrier of given const type for FI mitigation.
 *
 * @param type barrier const argument and return type
 * @param id barrier identification
 */
#define BARRIER_CONST_FUNCTION(type, id)				\
	static inline type se_util_barrier_const_ ## id(type (x))		\
	{								\
		ISOLATED_ASM_BARRIER(x);				\
		return x;						\
	}

/** Prevent compiler from optimizing out uint32_t variable.
 *
 * Constructs a new barrier function with ID u32 handling
 * uint32_t type argument.
 *
 * Useful for e.g. FI mitigation.
 */
BARRIER_FUNCTION(uint32_t, u32)

/** Prevent compiler from optimizing out uint8_t variable.
 *
 * Constructs a new barrier function with ID u8 handling
 * uint8_t type argument.
 *
 * Useful for e.g. FI mitigation.
 */
BARRIER_FUNCTION(uint8_t, u8)

/** Prevent compiler from optimizing out uint8_t *variable.
 *
 * Constructs a new barrier function with ID u8_ptr handling
 * uint8_t * pointer type argument.
 *
 * Useful for e.g. FI mitigation.
 */
BARRIER_FUNCTION(uint8_t *, u8_ptr)

/** Prevent compiler from optimizing out const uint8_t *variable.
 *
 * Constructs a new barrier function with ID u8_ptr handling
 * const uint8_t * pointer type argument.
 *
 * Useful for e.g. FI mitigation.
 */
BARRIER_CONST_FUNCTION(const uint8_t *, u8_ptr)

/** Prevent compiler from optimizing out status_t variable.
 *
 * Constructs a new barrier function with ID status handling
 * status_t pointer type argument.
 *
 * Useful for e.g. FI mitigation.
 */
BARRIER_FUNCTION(status_t, status)


/** @brief Optimization barrier, usage:
 *
 * FI_BARRIER(u32, i)
 * FI_BARRIER(u8, b)
 * FI_BARRIER(u8_ptr, i)
 * FI_BARRIER(status, rbad)
 * FI_BARRIER_CONST(u8_ptr, i)
 *
 * Prevent compiler from optimizing out variables.
 * Calls the inline functions constructed with
 * BARRIER_FUNCTION generator.
 *
 * Supported barrier type names:
 *  u32 for (uint32_t) type scalars
 *  u8_ptr for (uint8_t *) type pointers
 */

/**
 * @def FI_BARRIER(type, val)
 *
 * @param type name of supported barrier type
 * @param lvalue variable to guard with a barrier
 */
#define FI_BARRIER(type, lvalue) se_util_barrier_ ## type(&(lvalue))

/**
 * @def FI_BARRIER_CONST(type, val)
 *
 * @param type name of supported barrier const type
 * @param val const variable to guard with a barrier
 */
#define FI_BARRIER_CONST(type, val) \
	do { (val) = se_util_barrier_const_ ## type(val); } while(CFALSE)
/*@}*/
#endif /* INCLUDED_CRYPTO_BARRIER_H */
