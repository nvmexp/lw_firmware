/*
 * Copyright (c) 2019-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 * Header file for CheetAh Security Elliptic Engine: macros for
 * handling lwrve parameters.
 */

/**
 * @file   tegra_pka1_ec_param.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019
 *
 * Macros for setting up EC lwrve parameter types and objects.
 */
#ifndef INCLUDED_TEGRA_PKA1_EC_PARAM_H
#define INCLUDED_TEGRA_PKA1_EC_PARAM_H

#ifndef CCC_SOC_FAMILY_DEFAULT
#include <crypto_system_config.h>
#endif

/**
 * @def LWRVE_PARAM_FIELD(field, bsize)
 *
 * @brief Defines a BSIZE long byte vector FIELD for holding
 * a fixed EC lwrve parameter.
 *
 * @param field name of the lwrve parameter
 * @param bsize byte size of the parameter field
 */
#define LWRVE_PARAM_FIELD(field, bsize)		\
	uint8_t field[ bsize ];

/**
 * @def GENERIC_LWRVE_TYPE(name, bsize)
 *
 * @brief Define a type for a new lwrve type with given size.
 *
 * Generated struct type contains fields: p, a, b, x, y, n
 *
 * @param name lwrve type name (name suffix '_s' appended).
 * @param bsize size of lwrve parameters in bytes.
 */
#define GENERIC_LWRVE_TYPE(name, bsize)					\
	struct name ## _s {						\
		LWRVE_PARAM_FIELD(p, bsize);				\
		LWRVE_PARAM_FIELD(a, bsize);				\
		LWRVE_PARAM_FIELD(b, bsize);				\
		LWRVE_PARAM_FIELD(x, bsize);				\
		LWRVE_PARAM_FIELD(y, bsize);				\
		LWRVE_PARAM_FIELD(n, bsize);				\
	}

/**
 * @def GENERIC_LWRVE_MONT_TYPE(name, bsize)
 *
 * @brief Define a type for holding the pre-defined montgomery values
 * for a given lwrve.
 *
 * Generated struct type contains fields: m_prime and r2
 *
 * @param name lwrve montgomery type name (name suffix 'mont_s' appended).
 * @param bsize size of lwrve parameters in bytes.
 */
#define GENERIC_LWRVE_MONT_TYPE(name, bsize)				\
	struct name ## _mont_s {					\
		LWRVE_PARAM_FIELD(m_prime, bsize);			\
		LWRVE_PARAM_FIELD(r2, bsize);				\
	}

/**
 * @def GENERIC_LWRVE(name)
 *
 * @brief declare a const static variable type object for holding
 * lwrve parameters using a pre-defined lwrve struct type (defined by
 * GENERIC_LWRVE_TYPE).
 *
 * @param name lwrve type name (name suffix '_s' appended).
 * Declared object name is as: ec_:name:
 */
#define GENERIC_LWRVE(name)				\
	static const struct name ## _s ec_ ## name

/**
 * @def GENERIC_LWRVE_MONT_VALUES(name)
 *
 * @brief declare a const static variable type object for holding
 * lwrve montgomery values using a pre-defined lwrve montgomery struct
 * type (defined by GENERIC_LWRVE_MONT_TYPE).
 *
 * @param name lwrve type name (name suffix '_mont_s' appended).
 * Declared object name is as: ec_:name:_mont
 */
#define GENERIC_LWRVE_MONT_VALUES(name)			\
	static const struct name ## _mont_s ec_ ## name ## _mont

/* No montgomery pre-callwlated values */
/**
 * @def GENERIC_LWRVE_MONT_NONE(name)
 *
 * @brief declare a const static variable type object for use
 * when the pre-defined montgomery values are NOT PROVIDED for some lwrve.
 *
 * @param name lwrve type name (name suffix '_mont_s' appended).
 * Declared object name is as: ec_:name:_mont
 *
 * This is compatible in the sense that the field is always set to
 * NULL which implies that m_prime and r2 are not provided.
 */
#define GENERIC_LWRVE_MONT_NONE(name)					\
	static struct name ## _mont_s {					\
		uint8_t *m_prime;					\
		uint8_t *r2;						\
	} ec_ ## name ## _mont = { .m_prime = NULL }

#endif /* INCLUDED_TEGRA_PKA1_EC_PARAM_H */
