/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support LWPKA engine : struct lwpka_engine_data related types
 */
#ifndef INCLUDED_TEGRA_LWPKA_ENGINE_DATA_H
#define INCLUDED_TEGRA_LWPKA_ENGINE_DATA_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

/* LWPKA Security Engine operand sizes */
enum pka1_op_mode {
	/* Supported RSA key op sizes */
	PKA1_OP_MODE_ILWALID	= 0,

	PKA1_OP_MODE_RSA512	= 10,
	PKA1_OP_MODE_RSA768,
	PKA1_OP_MODE_RSA1024,
	PKA1_OP_MODE_RSA1536,
	PKA1_OP_MODE_RSA2048,
	PKA1_OP_MODE_RSA3072,
	PKA1_OP_MODE_RSA4096,

	/* Supported ECC lwrve op sizes */
	PKA1_OP_MODE_ECC256	= 20,
	PKA1_OP_MODE_ECC320,
	PKA1_OP_MODE_ECC384,
	PKA1_OP_MODE_ECC448,
	PKA1_OP_MODE_ECC512,
	PKA1_OP_MODE_ECC521,
};

/* LWPKA Engine operation modes */
enum lwpka_engine_ops {
	LWPKA_OP_ILWALID		= 0,

	LWPKA_OP_GEN_MULTIPLY		= 10,	/* Non modular multiplication: c=a*b) */

	LWPKA_OP_GEN_MULTIPLY_MOD	= 11,	/*  c= (a*b) mod m
						 *
						 * Non modular multiplication and
						 *  immediate bit serial dp reduction.
						 *
						 * This is SW combined operation, HW does not
						 *  directly have this operation as one primitive.
						 */
	LWPKA_OP_MODULAR_MULTIPLICATION	= 20,
	LWPKA_OP_MODULAR_ADDITION	= 21,
	LWPKA_OP_MODULAR_SUBTRACTION	= 22,
	LWPKA_OP_MODULAR_ILWERSION	= 23,
#if HAVE_LWPKA_MODULAR_DIVISION
	LWPKA_OP_MODULAR_DIVISION	= 24,
#endif /* HAVE_LWPKA_MODULAR_DIVISION */
	LWPKA_OP_MODULAR_EXPONENTIATION	= 25,
	LWPKA_OP_MODULAR_REDUCTION	= 26, /* Single precision reduction */
	LWPKA_OP_MODULAR_REDUCTION_DP	= 27, /* Double precision reduction */
	LWPKA_OP_MODULAR_SQUARE		= 28, /* Pseudo operation: x^2 (mod m) */

	LWPKA_OP_EC_POINT_VERIFY	= 30,
	LWPKA_OP_EC_POINT_MULTIPLY	= 31,
	LWPKA_OP_EC_SHAMIR_TRICK	= 32,

	LWPKA_OP_MONTGOMERY_POINT_MULTIPLY = 40,

	LWPKA_OP_EDWARDS_POINT_VERIFY	= 50,
	LWPKA_OP_EDWARDS_POINT_MULTIPLY = 51,
	LWPKA_OP_EDWARDS_SHAMIR_TRICK	= 52,
#if HAVE_LWPKA11_SUPPORT
	/* EC keystore operations */
	LWPKA_OP_EC_KS_KEYINS_PRI		= 60,
	LWPKA_OP_EC_KS_KEYGEN_PRI		= 61,
	LWPKA_OP_EC_KS_KEYGEN_PUB		= 62,
	LWPKA_OP_EC_KS_KEYDISP			= 63,
	LWPKA_OP_EC_KS_KEYLOCK			= 64,
	LWPKA_OP_EC_KS_ECDSA_SIGN		= 65,
#endif /* HAVE_LWPKA11_SUPPORT */
};

/* Supported primitive value id's for LWPKA field for HW operations.
 */
enum lwpka_primitive_e {
	LWPKA_PRIMITIVE_ILWALID			= 0xff,	/* One of the reserved values */

	/* Normal primitives */

	LWPKA_PRIMITIVE_GEN_MULTIPLICATION	= 0x01, /* non-modular multiplication c = a*b
							 * (double length response)
							 */
	/* Modular primitives */

	LWPKA_PRIMITIVE_MOD_MULTIPLICATION	= 0x02, /* x*y mod m */
	LWPKA_PRIMITIVE_MOD_ADDITION		= 0x03, /* x+y mod m */
	LWPKA_PRIMITIVE_MOD_SUBTRACTION		= 0x04, /* x-y mod m */
	LWPKA_PRIMITIVE_MOD_ILWERSION		= 0x05, /* 1/x mod m */
	LWPKA_PRIMITIVE_MOD_DIVISION		= 0x06, /* y/x mod m */
	LWPKA_PRIMITIVE_MOD_REDUCTION		= 0x07, /* x mod m (double prec) */

	/* Generic Modular Exponentiation (used e.g. for RSA)  */

	LWPKA_PRIMITIVE_MOD_EXP			= 0x08, /* x^y mod m */

	/* ECC primitives */

	LWPKA_PRIMITIVE_EC_ECPV			= 0x09, /* Verify point is on given
							 * Weierstrass lwrve
							 */
	LWPKA_PRIMITIVE_EC_POINT_MULT		= 0x0A, /* Q = kP */
	LWPKA_PRIMITIVE_EC_SHAMIR_TRICK		= 0x0B, /* R = kP + lQ */

	/* Special lwrve primitives */

	LWPKA_PRIMITIVE_MONTGOMERY_POINT_MULT	= 0x0C, /* Q = kP, for Montgomery lwrves?
							 * C25519 is a Montgomery lwrve...
							 * XXXX Verify if this can be used for e.g. X25519
							 */
	LWPKA_PRIMITIVE_EDWARDS_ECPV		= 0x0D, /* Verify point is on
							 * Edwards lwrve
							 */
	LWPKA_PRIMITIVE_EDWARDS_POINT_MULT	= 0x0E, /* Q = kP for Edwards lwrves
							 * ED25519/ED448
							 *
							 * C25519 ???
							 */
	LWPKA_PRIMITIVE_EDWARDS_SHAMIR_TRICK	= 0x0F, /* Q = kP for Edwards lwrves
							 * ED25519/ED448
							 */
#if HAVE_LWPKA11_SUPPORT
	LWPKA_PRIMITIVE_ECC_KEYINS_PRI		= 0x10, /* Insert ECC private key from K0 LOR or DRGB to
							 * ECC keystore.
							 */
	LWPKA_PRIMITIVE_ECC_KEYGEN_PRI		= 0x11, /* Generate ECC private key d and store to
							 * ECC keystore.
							 */
	LWPKA_PRIMITIVE_ECC_KEYGEN_PUB		= 0x12, /* Generate ECC public key A from private key d in
							 * ECC keystore.
							 */
	LWPKA_PRIMITIVE_ECC_KEYDISP		= 0x13, /* Dispose ECC keypair and revert manifest/control
							 * settings to blank state in ECC keystore.
							 */
	LWPKA_PRIMITIVE_ECDSA_SIGN		= 0x14, /* Do ECDSA sign operation with keys in
							 * ECC keystore.
							 */
#endif /* HAVE_LWPKA11_SUPPORT */
};

/*
 * LWPKA HW can use the HW keyslots only for the following operations.
 */
#define LWPKA_PRIMITIVE_CAN_USE_HW_KEYSLOT(primitive)			\
	((LWPKA_PRIMITIVE_MOD_EXP == (primitive)) ||			\
	 (LWPKA_PRIMITIVE_EC_POINT_MULT == (primitive)) ||		\
	 (LWPKA_PRIMITIVE_MONTGOMERY_POINT_MULT == (primitive)) ||	\
	 (LWPKA_PRIMITIVE_EDWARDS_POINT_MULT == (primitive)))

struct pka_engine_data {
	enum pka1_op_mode op_mode; /* for the EC operations this is same as lwrve->mode */
	enum lwpka_engine_ops lwpka_op;
};
#endif /* INCLUDED_TEGRA_LWPKA_ENGINE_DATA_H */
