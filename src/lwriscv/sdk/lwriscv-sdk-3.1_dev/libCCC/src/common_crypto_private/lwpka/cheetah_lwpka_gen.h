/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support LWPKA engine : generic RSA/ECC support defs  */

#ifndef INCLUDED_TEGRA_LWPKA_GEN_H
#define INCLUDED_TEGRA_LWPKA_GEN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <lwpka_defs.h>
#include <tegra_lwpka.h>
#include <lwpka_engine_data.h>

/* Generic defines for LWPKA
 */
// XXX TODO: best place for this define?
#define LWPKA_MAX_KEYSLOTS   4U	/**< Number of LWPKA engine HW keyslots */

struct engine_s;

#ifndef MEASURE_PERF_ENGINE_LWPKA_OPERATION
#define MEASURE_PERF_ENGINE_LWPKA_OPERATION	0
#endif

#ifndef HAVE_LWPKA_ZERO_MODULUS_CHECK
#define HAVE_LWPKA_ZERO_MODULUS_CHECK 0
#endif

#if SE_RD_DEBUG

#define LWPKA_DUMP_REG_RANGE(engine, reg, count, increment)		\
	do { if (LOCAL_TRACE > 0U) {					\
		LTRACEF("%d register dump from " #reg ", increment %u\n", (count), (increment)); \
		uint32_t inc = 0U;					\
		uint32_t i__nx = 0U;					\
		for (; i__nx < count; i__nx++) {			\
			(void)tegra_engine_get_reg(engine, (reg+inc));	\
			inc += (increment);				\
		}							\
	     }								\
	} while(false)

#define LWPKA_DUMP_REG(engine, reg) LWPKA_DUMP_REG_RANGE(engine, reg, 1U, 0U)

#else

#define LWPKA_DUMP_REG_RANGE(engine, reg, count, increment)
#define LWPKA_DUMP_REG(engine, reg)

#endif /* LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION) */

static inline bool m_is_supported_rsa_modulus_byte_length(uint32_t len)
{
	return ((((1024U/8U) == len) ||
		 ((1536U/8U) == len) ||
		 ((2048U/8U) == len) ||
		 ((3072U/8U) == len) ||
		 ((4096U/8U) == len)) &&
		(len >= (RSA_MIN_KEYSIZE_BITS/8U)));
}

#define IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(len)	\
	m_is_supported_rsa_modulus_byte_length(len)

/* RSA private key length matches modulus length, public key length is fixed one word.
 * Caller must zero pad keys to supported lengths.
 */
static inline bool m_is_supported_rsa_exponent_byte_length(uint32_t len)
{
	return (IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(len) || (BYTES_IN_WORD == len));
}

#define IS_SUPPORTED_RSA_EXPONENT_BYTE_LENGTH(len) \
		m_is_supported_rsa_exponent_byte_length(len)

/* P-521 lwrves use 544 bit length (68 bytes) (zero paded).
 *
 * P-384 lwrve uses 512 bit length (64 bytes) (zero padded).
 * BP-320 lwrves used 512 bit length (64 bytes) (zero padded).
 * ED-448 lwrve uses 512 bit length (64 bytes) (zero padded).
 *
 * FIXME: How about 320 bit brainpool lwrves?
 */
static inline bool m_is_supported_ec_lwrve_byte_length(uint32_t len)
{
	bool rv = false;

	if (len >= (CCC_EC_MIN_PRIME_BITS/8U)) {
		rv = (((256U/8U) == len) ||
		      ((320U/8U) == len) ||
		      ((384U/8U) == len) ||
		      ((512U/8U) == len) ||
		      ((544U/8U) == len));
	}
	return rv;
}

#define IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(len)	\
	m_is_supported_ec_lwrve_byte_length(len)

// XX Where does this value come from (IAS?)
#define SE_LWPKA_MUTEX_WDT_UNITS	0x600000U

#ifndef LWPKA_MAX_TIMEOUT

#if TEGRA_DELAY_FUNCTION_UNDEFINED
#define LWPKA_MAX_TIMEOUT		(40000U*200U) /* poll count, not microseconds */
#else
#define LWPKA_MAX_TIMEOUT		40000U	/*micro seconds*/
#endif /* TEGRA_DELAY_FUNCTION_UNDEFINED */

#endif /* !defined(LWPKA_MAX_TIMEOUT) */

#ifndef LWPKA_MIN_TIMEOUT

#if TEGRA_DELAY_FUNCTION_UNDEFINED
#define LWPKA_MIN_TIMEOUT		(10000U*200U) /* poll count, not microseconds */
#else
#define LWPKA_MIN_TIMEOUT		10000U	/*micro seconds*/
#endif /* TEGRA_DELAY_FUNCTION_UNDEFINED */

#endif /* !defined(LWPKA_MIN_TIMEOUT) */

/** Set engine status poll limit
 *
 * Default is LWPKA_MAX_TIMEOUT.
 */
status_t lwpka_set_max_timeout(uint32_t max_timeout);

#define CCC_SC_ENABLE	0xE13FC0D1U
#define CCC_SC_DISABLE	0xB2A53FE1U

#define MAX_LWPKA_OPERAND_SIZE		(MAX_RSA_BIT_SIZE / 8U)
#define MAX_LWPKA_OPERAND_WORD_SIZE	(MAX_LWPKA_OPERAND_SIZE / BYTES_IN_WORD)

/* HW defines offset values, specified in LWPKA IAS
 * as WORD offset range:
 *
 * RSA exponent : 0-127		= RSA exponent
 * Modulus	: 128-155	= RSA modulus
 *
 * SW writes the field data in auto-increment mode.
 */
#define	KMAT_RSA_EXPONENT		0U
#define	KMAT_RSA_EXPONENT_WORDS		128U

#define	KMAT_RSA_MODULUS		128U
#define	KMAT_RSA_MODULUS_WORDS		128U

/* HW defines offset values, specified in LWPKA IAS
 * as WORD offset range:
 *
 * a		: 0-16		= Lwrve parameter a
 * b/d		: 24-40		= Lwrve parameter b/d
 * p		: 48-64		= Lwrve prime p
 * n		: 72-88		= Order-of-base-point (OBP) n
 * Xp		: 96-112	= Base point x coordinate
 * Yp		: 120-136	= Base point y coordinate
 * Xq		: 144-160	= Public key x coordinate
 * Yq		: 168-184	= Public key y coordinate
 * k		: 192-208	= Scalar k
 *
 * SW writes the field data in auto-increment mode from the offset
 * below.
 */
#define	KMAT_EC_A		0U
#define	KMAT_EC_B		24U
#define	KMAT_EC_P		48U
#define	KMAT_EC_ORDER		72U
#define	KMAT_EC_PX		96U
#define	KMAT_EC_PY		120U
#define	KMAT_EC_QX		144U
#define	KMAT_EC_QY		168U
#define	KMAT_EC_KEY		192U

typedef uint32_t lwpka_keymat_field_t;

uint32_t lwpka_get_version(void);

#if HAVE_LWPKA_LOAD
status_t lwpka_set_keyslot_field(const struct engine_s *engine,
				 lwpka_keymat_field_t field,
				 uint32_t key_slot,
				 const uint8_t *param,
				 uint32_t key_words,
				 bool data_big_endian,
				 uint32_t field_word_size);

status_t lwpka_keyslot_load(const struct engine_s *engine, uint32_t pka_keyslot);

#endif /* HAVE_LWPKA_LOAD */

uint32_t lwpka_get_version(void);

status_t lwpka_num_words(enum pka1_op_mode op_mode, uint32_t *words_p, uint32_t *algo_words_p);

#if HAVE_LWPKA_ZERO_MODULUS_CHECK
status_t lwpka_load_modulus(const engine_t *engine,
			    const struct pka_engine_data *lwpka_data,
			    const uint8_t *modulus,
			    uint32_t m_len,
			    bool m_BE);
#endif

/* lwpka engine data contains no data to release
 */
void lwpka_data_release(struct pka_engine_data *lwpka_data);

status_t lwpka_go_start_keyslot(const struct engine_s *engine, enum pka1_op_mode op_mode,
				enum lwpka_primitive_e primitive_op,
				bool use_hw_kslot, uint32_t keyslot, uint32_t scena);

status_t lwpka_go_start(const struct engine_s *engine, enum pka1_op_mode op_mode,
			enum lwpka_primitive_e primitive_op, uint32_t scena);

status_t lwpka_wait_op_done(const struct engine_s *engine);

/* LWPKA register values are the LOR_xx (large operand register) values
 * in tegra_lwpka.h, defined as byte offsets of the first word in the
 * LOR register.
 */
typedef uint32_t lwpka_reg_id_t;

status_t lwpka_register_read(const struct engine_s *engine,
			     enum pka1_op_mode op_mode,
			     lwpka_reg_id_t lwpka_reg,
			     uint8_t *data, uint32_t nwords, bool big_endian);

status_t lwpka_register_write(const struct engine_s *engine,
			      enum pka1_op_mode op_mode,
			      lwpka_reg_id_t lwpka_reg,
			      const uint8_t *data, uint32_t nwords, bool big_endian,
			      uint32_t field_word_size);

/* Return status (in case of timeout...)
 * But what can we do if there is a timeout
 * => let the caller decide for now (terminate
 * current operation on error)
 */
status_t lwpka_acquire_mutex(const struct engine_s *engine);
void lwpka_release_mutex(const struct engine_s *engine);

#if SE_RD_DEBUG
/* R&D function for naming things in logs
 */
const char *lwpka_lor_name(lwpka_reg_id_t reg);
const char *lwpka_get_engine_op_name(enum lwpka_engine_ops eop);
const char *lwpka_get_primitive_op_name(enum lwpka_primitive_e ep);
#endif /* SE_RD_DEBUG */

#endif /* INCLUDED_TEGRA_LWPKA_GEN_H */
