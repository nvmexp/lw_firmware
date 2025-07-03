/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine LWPKA engine : generic RSA/ECC support functions  */

#include <crypto_system_config.h>

#if HAVE_LWPKA_RSA || HAVE_LWPKA_ECC || HAVE_LWPKA_MODULAR

#include <lwpka/tegra_lwpka_rsa.h>
#include <tegra_cdev.h>
#include <lwpka/tegra_lwpka_gen.h>
#include <lwpka/lwpka_version.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifndef HAVE_LWPKA_ERROR_CAPTURE
#define HAVE_LWPKA_ERROR_CAPTURE HAVE_ERROR_CAPTURE
#endif

#ifndef HAVE_LWPKA_USES_INTERRUPTS
#define HAVE_LWPKA_USES_INTERRUPTS 0
#endif

#if HAVE_LWPKA_USES_INTERRUPTS
#ifndef CCC_WAIT_FOR_INTERRUPT
#error "LWPKA: CCC device interrupt handler undefined"
#endif
#endif /* HAVE_LWPKA_USES_INTERRUPTS */

/* This MUST BE nonzero after LOAD_KEY is enabled and using LW shadow
 * registers Otherwise PKA will not work correctly.
 */
#define LWPKA_ENABLE_INTERRUPTS 1

#if SE_RD_DEBUG
static const struct {
	enum lwpka_primitive_e pep;
	const char       *pname;
} epid[] = {
	{ LWPKA_PRIMITIVE_ILWALID, "INVALID" },

	/* Normal primitives */

	{ LWPKA_PRIMITIVE_GEN_MULTIPLICATION,  "GEN_MULTIPLY" },

	/* Modular primitives */

	{ LWPKA_PRIMITIVE_MOD_MULTIPLICATION, "MOD_MULTIPLICATION" },
	{ LWPKA_PRIMITIVE_MOD_ADDITION, "MOD_ADDITION" },
	{ LWPKA_PRIMITIVE_MOD_SUBTRACTION, "MOD_SUBTRACTION" },
	{ LWPKA_PRIMITIVE_MOD_ILWERSION, "MOD_ILWERSION" },
	{ LWPKA_PRIMITIVE_MOD_DIVISION, "MOD_DIVISION" },
	{ LWPKA_PRIMITIVE_MOD_REDUCTION, "MOD_REDUCTION" },

	/* RSA primitives */

	{ LWPKA_PRIMITIVE_MOD_EXP, "MOD_EXP" },

	/* ECC primitives */

	{ LWPKA_PRIMITIVE_EC_ECPV, "EC_ECPV" },
	{ LWPKA_PRIMITIVE_EC_POINT_MULT, "EC_POINT_MULT" },
	{ LWPKA_PRIMITIVE_EC_SHAMIR_TRICK, "EC_SHAMIR_TRICK" },

	/* Special lwrve primitives => */

	/* montgomery lwrves (for key agreemets) */
	{ LWPKA_PRIMITIVE_MONTGOMERY_POINT_MULT, "MONTGOMERY_POINT_MULT" },

	/* edwards lwrves (for signatures) */
	{ LWPKA_PRIMITIVE_EDWARDS_ECPV, "EDWARDS_ECPV" },
	{ LWPKA_PRIMITIVE_EDWARDS_POINT_MULT, "EDWARDS_POINT_MULT" },
	{ LWPKA_PRIMITIVE_EDWARDS_SHAMIR_TRICK, "EDWARDS_SHAMIR_TRICK" },

#if HAVE_LWPKA11_SUPPORT
	{ LWPKA_PRIMITIVE_ECC_KEYINS_PRI, "ECC_KEYINS_PRI" },
	{ LWPKA_PRIMITIVE_ECC_KEYGEN_PRI, "ECC_KEYGEN_PRI" },
	{ LWPKA_PRIMITIVE_ECC_KEYGEN_PUB, "ECC_KEYGEN_PUB" },
	{ LWPKA_PRIMITIVE_ECC_KEYDISP, "ECC_KEYDISP" },
	{ LWPKA_PRIMITIVE_ECDSA_SIGN, "ECC_ECDSA_SIGN" },
#endif

	{ LWPKA_PRIMITIVE_ILWALID, NULL },
};

const char *lwpka_get_primitive_op_name(enum lwpka_primitive_e ep)
{
	uint32_t index = 0U;
	const char *ep_name = NULL;

	while (NULL != epid[index].pname) {
		if (epid[index].pep == ep) {
			ep_name = epid[index].pname;
		}
		index++;
	}
	return ((NULL == ep_name) ? "(noname)" : ep_name);
}

static const struct {
	enum lwpka_engine_ops eop;
	const char       *opname;
} eopid[] = {
	{ LWPKA_OP_ILWALID, "OP_ILWALID" },
	{ LWPKA_OP_GEN_MULTIPLY, "OP_GEN_MULTIPLY" },
	{ LWPKA_OP_GEN_MULTIPLY_MOD, "OP_GEN_MULTIPLY_MOD" },
	{ LWPKA_OP_MODULAR_MULTIPLICATION, "OP_MODULAR_MULTIPLICATION" },
	{ LWPKA_OP_MODULAR_ADDITION, "OP_MODULAR_ADDITION" },
	{ LWPKA_OP_MODULAR_SUBTRACTION, "OP_MODULAR_SUBTRACTION" },
	{ LWPKA_OP_MODULAR_ILWERSION, "OP_MODULAR_ILWERSION" },
#if HAVE_LWPKA_MODULAR_DIVISION
	{ LWPKA_OP_MODULAR_DIVISION, "OP_MODULAR_DIVISION" },
#endif
	{ LWPKA_OP_MODULAR_EXPONENTIATION, "OP_MODULAR_EXPONENTIATION" },
	{ LWPKA_OP_MODULAR_REDUCTION, "OP_MODULAR_REDUCTION" },
	{ LWPKA_OP_MODULAR_REDUCTION_DP, "OP_MODULAR_REDUCTION_DP" },
	{ LWPKA_OP_MODULAR_SQUARE, "OP_MODULAR_SQUARE" },
	{ LWPKA_OP_EC_POINT_VERIFY, "OP_EC_POINT_VERIFY" },
	{ LWPKA_OP_EC_POINT_MULTIPLY, "OP_EC_POINT_MULTIPLY" },
	{ LWPKA_OP_EC_SHAMIR_TRICK, "OP_EC_SHAMIR_TRICK" },
	{ LWPKA_OP_MONTGOMERY_POINT_MULTIPLY, "OP_MONTGOMERY_POINT_MULTIPLY" },
	{ LWPKA_OP_EDWARDS_POINT_VERIFY, "OP_EDWARDS_POINT_VERIFY" },
	{ LWPKA_OP_EDWARDS_POINT_MULTIPLY, "OP_EDWARDS_POINT_MULTIPLY" },
	{ LWPKA_OP_EDWARDS_SHAMIR_TRICK, "OP_EDWARDS_SHAMIR_TRICK" },
};

const char *lwpka_get_engine_op_name(enum lwpka_engine_ops eop)
{
	uint32_t index = 0U;
	const char *eop_name = NULL;

	while (NULL != eopid[index].opname) {
		if (eopid[index].eop == eop) {
			eop_name = eopid[index].opname;
		}
		index++;
	}
	return ((NULL == eop_name) ? "(noname op)" : eop_name);
}
#endif /* SE_RD_DEBUG */


static uint32_t lwpka_engine_max_timeout = LWPKA_MAX_TIMEOUT;

#if HAVE_NO_STATIC_DATA_INITIALIZER
void ccc_static_init_lwpka_values(void)
{
	lwpka_engine_max_timeout = LWPKA_MAX_TIMEOUT;
}
#endif /* HAVE_NO_STATIC_DATA_INITIALIZER */

status_t lwpka_set_max_timeout(uint32_t max_timeout)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (max_timeout <= LWPKA_MIN_TIMEOUT) {
		/* Use 0 value argument to see current value in log */
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				CCC_ERROR_MESSAGE("LWPKA poll count %u too small (using: %u)\n",
							   max_timeout, lwpka_engine_max_timeout));
	}

	if (max_timeout == 0xFFFFFFFFU) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	lwpka_engine_max_timeout = max_timeout;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if SE_RD_DEBUG
const char *lwpka_lor_name(lwpka_reg_id_t reg)
{
	const char *name = NULL;
	switch (reg) {
	case LOR_K0: name = "LOR_K0(0x4200)"; break;
	case LOR_K1: name = "LOR_K1(0x4400)"; break;
	case LOR_S0: name = "LOR_S0(0x4800)"; break;
	case LOR_S1: name = "LOR_S1(0x4A00)"; break;
	case LOR_S2: name = "LOR_S2(0x4C00)"; break;
	case LOR_S3: name = "LOR_S3(0x4C80)"; break;
	case LOR_D0: name = "LOR_D0(0x4D00)"; break;
	case LOR_D1: name = "LOR_D1(0x4E00)"; break;
	case LOR_C0: name = "LOR_C0(0x5000)"; break;
	case LOR_C1: name = "LOR_C1(0x5200)"; break;
	case LOR_C2: name = "LOR_C2(0x5280)"; break;
	default: name = "unknown LOR"; break;
	}

	return name;
}
#endif /* SE_RD_DEBUG */

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
static uint32_t gen_op_perf_usec = 0U;
static enum lwpka_primitive_e gen_op_perf_primitive_op = LWPKA_PRIMITIVE_ILWALID;
static uint32_t lwpka_entry_perf_cycles = 0U;
static uint32_t lwpka_entry_perf_insns = 0U;

static void show_lwpka_gen_op_entry_perf(enum lwpka_primitive_e ep,
					 uint32_t elapsed)
{
	CCC_ERROR_MESSAGE("LWPKA engine [%s (0x%x)]: %u usec, cycles %u, insns %u\n",
			  lwpka_get_entrypoint_name(ep), ep, elapsed,
			  lwpka_entry_perf_cycles, lwpka_entry_perf_insns);
}
#endif /* MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION) */

/* Re-enable error capture, write ERROR_BIT back as SET
 */
static inline void lwpka_device_clear_error_capture(const se_cdev_t *dev)
{
	if (NULL != dev) {
		ccc_device_set_reg(dev, LW_SE_LWPKA_CTRL_ERROR_CAPTURE_0,
				   LW_DRF_DEF(LW_SE_LWPKA, CTRL_ERROR_CAPTURE, ERROR_BIT, SET));
	}
}

#if HAVE_LWPKA_ERROR_CAPTURE
static status_t lwpka_device_check_error_capture(const se_cdev_t *dev,
						 uint32_t *error_info_p,
						 bool clear)
{
	status_t ret = NO_ERROR;
	uint32_t error_info = 0U;

	LTRACEF("entry\n");

	if (NULL != dev) {
		uint32_t reg_val = ccc_device_get_reg(dev, LW_SE_LWPKA_CTRL_ERROR_CAPTURE_0);

		if (0U != LW_DRF_VAL(LW_SE_LWPKA, CTRL_ERROR_CAPTURE, ERROR_BIT, reg_val)) {
			if (BOOL_IS_TRUE(clear)) {
				lwpka_device_clear_error_capture(engine);
			}

			error_info = LW_DRF_VAL(LW_SE_LWPKA, CTRL_ERROR_CAPTURE,
						ERROR_INFO, errcap_reg);
			ret = SE_ERROR(ERR_BAD_STATE);

			CCC_ERROR_MESSAGE("LWPKA abnormal cond, error info: 0x%x\n",
					  error_info);
		}
	}

	if (NULL != error_info_p) {
		*error_info_p = error_info;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ERROR_CAPTURE */

#if HAVE_LWPKA_LOAD

/* IAS: operations using HW keyslots loads key from HW keyslot.
 *
 * The supported operations loading HW keyslot fields are:
 * - Modular exponentiation
 * - EC point multiplication
 * - MG point multiplication
 * - ED point multiplication
 */
static status_t lwpka_key_from_keyslot(const struct engine_s *engine, uint32_t lwpka_keyslot)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (lwpka_keyslot >= LWPKA_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Loading key from LWPKA HW keyslot %u\n", lwpka_keyslot);

	val = LW_FLD_SET_DRF_NUM(LW_SE_LWPKA, CTRL_KSLT_CONTROL,
				 KEYSLOT, lwpka_keyslot, val);

	val = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_KSLT_CONTROL,
				 SCRUB_CORE_MEM, DISABLE, val);

	tegra_engine_set_reg(engine, LW_SE_LWPKA_CTRL_KSLT_CONTROL_0, val);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Key slot field must be in auto-increment mode to use this
 */
static status_t lwpka_keyslot_field_zeropad(const engine_t *engine,
					    uint32_t key_slot,
					    uint32_t key_words,
					    uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	if (key_slot >= LWPKA_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	for (inx = key_words; inx < field_word_size; inx++) {
		tegra_engine_set_reg(engine,
				     LW_SE_LWPKA_CTRL_KSLT_DATA_0 + (key_slot * 4U),
				     0x00000000U);
	}

	FI_BARRIER(u32, inx);

	if (inx != field_word_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Key slot field must be in auto-increment mode to use this
 */
static status_t set_keyslot_field_value(const engine_t *engine,
					uint32_t key_slot,
					const uint8_t *param,
					uint32_t key_words,
					bool data_big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t val32 = 0U;
	uint32_t offset = 0U;
	uint32_t len = word_count_to_bytes(key_words);

	LTRACEF("entry\n");

	if ((key_slot >= LWPKA_MAX_KEYSLOTS) || (NULL == param)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(data_big_endian)) {
		/* LWPKA unit operates on little endian data, must swap byte order */
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&param[(offset - BYTES_IN_WORD)], data_big_endian);
			tegra_engine_set_reg(engine,
					     LW_SE_LWPKA_CTRL_KSLT_DATA_0 + (key_slot * 4U),
					     val32);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&param[offset], data_big_endian);
			tegra_engine_set_reg(engine,
					     LW_SE_LWPKA_CTRL_KSLT_DATA_0 + (key_slot * 4U),
					     val32);
		}

		FI_BARRIER(u32, offset);

		if (offset != len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Write the key material to the pre-defined field in the specified SE engine keyslot.
 * Zero pad the material if it is shorter than the field size.
 */
status_t lwpka_set_keyslot_field(const engine_t *engine,
				 lwpka_keymat_field_t field,
				 uint32_t key_slot,
				 const uint8_t *param,
				 uint32_t key_words,
				 bool data_big_endian,
				 uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t kslt_addr_val = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == param) || (0U == key_words) ||
	    (key_slot >= LWPKA_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Write <kslot,field> in auto-increment mode starting from beginning of field
	 */
	kslt_addr_val = LW_FLD_SET_DRF_NUM(LW_SE_LWPKA, CTRL_KSLT_ADDR,
					   WORD_ADDR, field, kslt_addr_val);
	kslt_addr_val = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_KSLT_ADDR,
					   AUTO_INCR, ENABLE, kslt_addr_val);

	tegra_engine_set_reg(engine,
			     LW_SE_LWPKA_CTRL_KSLT_ADDR_0 + (key_slot * 4U),
			     kslt_addr_val);

	LTRACEF("Setting %u words in field %u in keyslot %u (BE=%u)\n",
		key_words, field, key_slot, data_big_endian);

	LOG_HEXBUF("Hex bytes:", param, (key_words * 4U));

	ret = set_keyslot_field_value(engine, key_slot, param,
				      key_words, data_big_endian);
	CCC_ERROR_CHECK(ret);

	if (key_words < field_word_size) {
		LTRACEF("Zero padding %u words in keyslot field %u value\n",
			(field_word_size - key_words), field);

		ret = lwpka_keyslot_field_zeropad(engine, key_slot,
						  key_words, field_word_size);
		CCC_ERROR_CHECK(ret);
	}

	/* Set this again.
	 * TODO: Check why this is set again here? For debug out? Other?
	 */
	tegra_engine_set_reg(engine,
			     LW_SE_LWPKA_CTRL_KSLT_ADDR_0 + (key_slot * 4U),
			     kslt_addr_val);

	LWPKA_DUMP_REG_RANGE(engine,
			     LW_SE_LWPKA_CTRL_KSLT_DATA_0 + (key_slot * 4U),
			     key_words, 0U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* LWPKA does not need additional settings;
 * but "with HW keyslot" and "without HW keyslot"
 * operations are started differently.
 */
status_t lwpka_keyslot_load(const struct engine_s *engine, uint32_t pka_keyslot)
{
	status_t ret = NO_ERROR;
	LTRACEF("entry\n");

	(void)engine;
	(void)pka_keyslot;

	LOG_ALWAYS("LWPKA loading key from keyslot %u\n", pka_keyslot);

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_LOAD */

static status_t lwpka_operation_radix(enum pka1_op_mode op_mode, uint32_t *baser_p)
{
	status_t ret = NO_ERROR;
	uint32_t base_radix = 0U;

	LTRACEF("entry\n");

	if (NULL == baser_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*baser_p = 0U;

	switch (op_mode) {
	case PKA1_OP_MODE_ECC256:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__256;
		break;

	case PKA1_OP_MODE_ECC448:
		/* This may actually use 512; verify when support added! */
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__448;
		break;

	case PKA1_OP_MODE_RSA512:
	case PKA1_OP_MODE_ECC320:
	case PKA1_OP_MODE_ECC384:
	case PKA1_OP_MODE_ECC512:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__512;
		break;

	case PKA1_OP_MODE_ECC521:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__544;
		break;

	case PKA1_OP_MODE_RSA768:
	case PKA1_OP_MODE_RSA1024:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__1024;
		break;

	case PKA1_OP_MODE_RSA1536:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__1536;
		break;

	case PKA1_OP_MODE_RSA2048:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__2048;
		break;

	case PKA1_OP_MODE_RSA3072:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__3072;
		break;

	case PKA1_OP_MODE_RSA4096:
		base_radix = LW_SE_LWPKA_CTRL_CORE_CONTROL_0_RADIX__4096;
		break;

	default:
		CCC_ERROR_MESSAGE("Invalid operation mode: %u\n", op_mode);
		ret = SE_ERROR(ERR_NOT_VALID);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*baser_p = base_radix;
fail:
	LTRACEF("exit: %d (radix: 0x%x)\n", ret, base_radix);
	return ret;
}

/* Start primitive operation with the LWPKA engine.
 *
 * Operation with HW keyslot and without are started from different
 * registers; but the value written to both contain identical field values.
 *
 * Here op_mode is only used for RADIX SIZE, the "type" of op_mode is
 * not relevant for operation start.
 */
static status_t start_op(const struct engine_s *engine, enum pka1_op_mode op_mode,
			 enum lwpka_primitive_e primitive_op, bool use_hw_kslot,
			 const uint32_t scena)
{
	status_t ret = NO_ERROR;
	uint32_t spath = scena;
	uint32_t reg_val = 0U;
	uint32_t base_radix = 0U;
	uint32_t prim_op_val = (uint32_t)primitive_op;

	LTRACEF("entry\n");

	ret = lwpka_operation_radix(op_mode, &base_radix);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Invalid base radix op mode: %d\n",
					  ret));

	reg_val = LW_FLD_SET_DRF_NUM(LW_SE_LWPKA, CORE_OPERATION,
				     RADIX, base_radix, reg_val);

	reg_val = LW_FLD_SET_DRF_NUM(LW_SE_LWPKA, CORE_OPERATION,
				     PRIMITIVE, prim_op_val, reg_val);

	FI_BARRIER(u32, spath);

	/* SCC enabled for all operations, not just for SW key ops.
	 * Do some extra trickery for SCC.
	 */
	switch (scena) {
	case CCC_SC_ENABLE:
		spath ^= scena;
		reg_val = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CORE_OPERATION,
					     SCC_EN, ENABLE, reg_val);
		break;
	case CCC_SC_DISABLE:
		spath ^= scena;
		reg_val = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CORE_OPERATION,
					     SCC_EN, DISABLE, reg_val);
		break;
	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

	reg_val = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CORE_OPERATION,
				     START, SET, reg_val);

	LTRACEF("primitive op %s (0x%x)\n",
		lwpka_get_primitive_op_name(primitive_op), prim_op_val);
	LTRACEF("base radix: 0x%x\n", base_radix);
	LTRACEF("core operation value 0x%x\n", reg_val);
	LTRACEF(" starting %s HW keyslot\n", (use_hw_kslot ? "with" : "without"));

	/* Trigger operation through CORE_OPERATION register
	 * => does not use HW keyslots.
	 */
	if (BOOL_IS_TRUE(use_hw_kslot)) {
		tegra_engine_set_reg(engine, LW_SE_LWPKA_CTRL_CORE_CONTROL_0, reg_val);
	} else {
		tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_OPERATION_0, reg_val);
	}
fail:
	if (NO_ERROR == ret) {
		if (0U != spath) {
			ret = SE_ERROR(ERR_BAD_STATE);
		}
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Force init of some regs before every op started
 */
static status_t start_op_prepare(const struct engine_s *engine,
				 enum lwpka_primitive_e primitive_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	lwpka_device_clear_error_capture(engine->e_dev);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
	gen_op_perf_usec = GET_USEC_TIMESTAMP;
	gen_op_perf_primitive_op = primitive_op;
#endif

	/* Set the engine operation entry point */

#if LOCAL_TRACE
	LTRACEF("LWPKA primitive_op 0x%x (%s)\n",
		primitive_op, lwpka_get_primitive_op_name(primitive_op));
#else
	LTRACEF("LWPKA primitive_op 0x%x\n", primitive_op);
#endif

	if (LWPKA_PRIMITIVE_ILWALID == primitive_op) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if LWPKA_ENABLE_INTERRUPTS
	{
		/* LWPKA uses interrupts always internally.
		 * They are signalled externally if the MASK is enabled.
		 */
		uint32_t imask = 0U;

#if HAVE_LWPKA_USES_INTERRUPTS
		/* Unmask both ACCESS_ERROR and CORE_IRQ reporting.
		 */
		imask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_INTR_MASK,
					   ACCESS_ERROR, SET, imask);

		imask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_INTR_MASK,
					   CORE_IRQ, SET, imask);
#endif /* HAVE_LWPKA_USES_INTERRUPTS */

		/* Unmask LWPKA to interrupt CPU if interrupts are enabled, otherwise
		 * mask them.
		 */
		tegra_engine_set_reg(engine,
				     LW_SE_LWPKA_CTRL_INTR_MASK_0,
				     imask);
	}
#else
	/* Disable interrupts if not enabled.
	 */
	tegra_engine_set_reg(engine,
			     LW_SE_LWPKA_CTRL_INTR_MASK_0,
			     0U);
#endif /* LWPKA_ENABLE_INTERRUPTS */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * LWPKA IAS =>
 *
 * 1) To start operation WITH HW keyslot (use_hw_kslot == true) =>
 *	write KEYSLOT_CTRL and then CORE_CONTROL to start op.
 *
 * 2) To start operation WITHOUT HW keyslot (use_hw_kslot == false) =>
 *	write CORE_OPERATION to start op.
 */
status_t lwpka_go_start_keyslot(const struct engine_s *engine, enum pka1_op_mode op_mode,
				enum lwpka_primitive_e primitive_op, bool use_hw_kslot,
				uint32_t keyslot, uint32_t scena)
{
	status_t ret = NO_ERROR;
	uint32_t sc_enable = scena;

	LTRACEF("entry\n");

	(void)keyslot;

	ret = start_op_prepare(engine, primitive_op);
	CCC_ERROR_CHECK(ret);

#if HAVE_LWPKA_LOAD
	if (BOOL_IS_TRUE(use_hw_kslot)) {
		if (! LWPKA_PRIMITIVE_CAN_USE_HW_KEYSLOT(primitive_op)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
					     LTRACEF("LWPKA primitive %u does not support HW keyslots\n",
						     primitive_op));
		}

		ret = lwpka_key_from_keyslot(engine, keyslot);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Setup HW keyslot failed %d\n",
						  ret));

#if HAVE_LWPKA_KEYSLOT_FAIL_PUBLIC
		if (CCC_SC_DISABLE == sc_enable) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
					     CCC_ERROR_MESSAGE("Public values in keyslot, failing operation\n"));
		}
#endif

#if HAVE_LWPKA_KEYSLOT_ENABLE_SC_ALWAYS
		/* Use of HW keyslots always enables SC */
		sc_enable = CCC_SC_ENABLE;
#endif
	}
#endif /* HAVE_LWPKA_LOAD */

	ret = start_op(engine, op_mode, primitive_op, use_hw_kslot, sc_enable);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_go_start(const struct engine_s *engine, enum pka1_op_mode op_mode,
			enum lwpka_primitive_e primitive_op, uint32_t scena)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = start_op_prepare(engine, primitive_op);
	CCC_ERROR_CHECK(ret);

	ret = start_op(engine, op_mode, primitive_op, false, scena);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_get_lor_reg_word_size(lwpka_reg_id_t reg, uint32_t *reg_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t size = 0U;

	LTRACEF("entry\n");

	if (NULL == reg_word_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (reg) {
	case LOR_K0:
	case LOR_S0:
	case LOR_S1:
	case LOR_D1:
	case LOR_C0:
		size = 128U;
		break;

	case LOR_K1:
	case LOR_D0:
		size = 64U;
		break;

	case LOR_S2:
	case LOR_S3:
	case LOR_C1:
	case LOR_C2:
		size = 32U;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*reg_word_size = size;
fail:
	LTRACEF("exit: %d (reg word size: %u)\n", ret, size);
	return ret;
}

static status_t lwpka_reg_offset(enum pka1_op_mode op_mode, lwpka_reg_id_t reg,
				 uint32_t *reg_off, uint32_t *reg_size,
				 uint32_t *mode_words_p)
{
	status_t ret = NO_ERROR;
	uint32_t words = 0U;

	LTRACEF("entry\n");

	if ((NULL == reg_off) || (NULL == reg_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*reg_off = 0U;
	*reg_size = 0U;

	ret = lwpka_num_words(op_mode, &words, NULL);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_get_lor_reg_word_size(reg, reg_size);
	CCC_ERROR_CHECK(ret);

	/* Reg id is the register start byte offset
	 */
	*reg_off = reg;

	if (NULL != mode_words_p) {
		*mode_words_p = words;
	}
fail:
	LTRACEF("exit: %d (words %u)\n", ret, words);
	return ret;
}

/*
 * LWPKA HW IAS defines the RADIX field values for operation modes,
 * in number of words in operands.
 *
 * Excess words must be zero padded (little endian values, pad trailing words).
 *
 * HW implementation require a couple of operation sizes rounded up to
 * next supported HW length. For most op_modes this HW base radix matches
 * the true word size of the operation, but for those rounded up
 * it does not.
 *
 * To know the true word size of the operation, the algo_nwords_p
 * parameter ref can be passed.
 *
 * Either one or both of the word size refs can be NULL if value not
 * needed.
 */
status_t lwpka_num_words(enum pka1_op_mode op_mode,
			 uint32_t *words_p,
			 uint32_t *algo_words_p)
{
	status_t ret = NO_ERROR;
	uint32_t words = 0U;
	uint32_t algo_words = 0U;

	LTRACEF("entry\n");

	if (NULL != words_p) {
		*words_p = 0U;
	}

	if (NULL != algo_words_p) {
		*algo_words_p = 0U;
	}

	/* In LWPKA the "radix" is identical to "number of words",
	 * so use the radix function for this as well.
	 */
	ret = lwpka_operation_radix(op_mode, &words);
	CCC_ERROR_CHECK(ret);

	switch (op_mode) {
	case PKA1_OP_MODE_ECC320:
		algo_words = 320U / BITS_IN_WORD;
		break;

	case PKA1_OP_MODE_ECC384:
		algo_words = 384U / BITS_IN_WORD;
		break;

	case PKA1_OP_MODE_RSA768:
		algo_words = 768U / BITS_IN_WORD;
		break;

	default:
		algo_words = words;
		break;
	}

	if (NULL != words_p) {
		*words_p = words;
	}

	if (NULL != algo_words_p) {
		*algo_words_p = algo_words;
	}
fail:
	LTRACEF("exit (words = %u): %d\n", words, ret);
	return ret;
}

static status_t reg_rw_arg_check(const engine_t *engine,
				 const uint8_t *data,
				 uint32_t nwords)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == data) || (0U == nwords)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Zero pad given register words FIRST..EFFECTIVE_FIELD_WORD_SIZE
 */
static status_t lwpka_reg_zeropad(const engine_t *engine,
				  uint32_t reg_base,
				  uint32_t first,
				  uint32_t effective_field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t i = first;

	LTRACEF("entry\n");

	for (; i < effective_field_word_size; i++) {
		uint32_t reg_offset = 0U;

		CCC_SAFE_ADD_U32(reg_offset, reg_base, word_count_to_bytes(i));
		tegra_engine_set_reg(engine, reg_offset, 0U);
	}

	FI_BARRIER(u32, i);

	if (i != effective_field_word_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

// XXX should the reg size check be here and in read?
static status_t set_reg_value(const engine_t *engine,
			      const uint8_t *data,
			      uint32_t reg_base,
			      uint32_t nwords,
			      bool big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t len = word_count_to_bytes(nwords);
	uint32_t offset = 0U;
	uint32_t val32  = 0U;
	uint32_t reg_offset = reg_base;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(big_endian)) {
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&data[offset - BYTES_IN_WORD], big_endian);
			tegra_engine_set_reg(engine, reg_offset, val32);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&data[offset], big_endian);
			tegra_engine_set_reg(engine, reg_offset, val32);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
		}

		FI_BARRIER(u32, offset);

		if (offset != len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_register_write(const engine_t *engine,
			      enum pka1_op_mode op_mode,
			      lwpka_reg_id_t lwpka_reg,
			      const uint8_t *data,
			      uint32_t nwords,
			      bool big_endian,
			      uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t reg_base = 0U;
	uint32_t mode_words = 0U;
	uint32_t effective_field_word_size = field_word_size;
	uint32_t reg_size = 0U;

	LTRACEF("entry\n");

	ret = reg_rw_arg_check(engine, data, nwords);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_reg_offset(op_mode, lwpka_reg, &reg_base, &reg_size, &mode_words);
	CCC_ERROR_CHECK(ret,
			LTRACEF("%s (0x%x) map failed: %d\n",
				lwpka_lor_name(lwpka_reg), lwpka_reg, ret));

	/* Some cases require e.g. next power of two size to be zero padded in EC modes.
	 * But allow passing e.g. zero if the mode natural size is OK.
	 */
	if (effective_field_word_size < mode_words) {
#if SE_RD_DEBUG
		if (effective_field_word_size != 0U) {
			LTRACEF("register write field size %u adjusted to %u\n",
				effective_field_word_size, mode_words);
		}
#endif
		effective_field_word_size = mode_words;
	}

	if (reg_size < nwords) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE, LTRACEF("%s size %u too small, attempt to write %u words\n",
							    lwpka_lor_name(lwpka_reg),
							    reg_size, nwords));
	}

	LTRACEF("%u words to LWPKA %s\n", nwords, lwpka_lor_name(lwpka_reg));

	ret = set_reg_value(engine, data, reg_base,
			    nwords, big_endian);
	CCC_ERROR_CHECK(ret);

	/* Note: Value of above loop index i is used below!!!
	 */
	if (nwords < effective_field_word_size) {
		/* If not writing all lwpka_reg words => clear the rest of the LOR register
		 * This may be unnecessary (slow) for most values; however in some modes and
		 * some values this is mandatory (e.g. key values) => do it for all values for now.
		 * TODO: optimize!
		 */
		ret = lwpka_reg_zeropad(engine, reg_base,
					nwords, effective_field_word_size);
		CCC_ERROR_CHECK(ret);
	}

	LTRACEF("%u words written to %s (caller order: %s)\n",
		nwords, lwpka_lor_name(lwpka_reg),
		(BOOL_IS_TRUE(big_endian) ? "BE" : "LE"));
	LOG_HEXBUF("Written data (in caller order)", data, nwords*4U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t get_reg_value(const engine_t *engine,
			      uint8_t *data,
			      lwpka_reg_id_t reg_base,
			      uint32_t nwords,
			      bool big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t len = word_count_to_bytes(nwords);
	uint32_t offset = 0U;
	uint32_t val32 = 0U;
	uint32_t reg_offset = reg_base;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(big_endian)) {
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = tegra_engine_get_reg(engine, reg_offset);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
			se_util_u32_to_buf(val32, &data[(offset - BYTES_IN_WORD)], big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = tegra_engine_get_reg(engine, reg_offset);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
			se_util_u32_to_buf(val32, &data[offset], big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset != len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_register_read(const engine_t *engine,
			     enum pka1_op_mode op_mode,
			     lwpka_reg_id_t lwpka_reg,
			     uint8_t *data, uint32_t nwords, bool big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t reg_base = 0U;
	uint32_t reg_size = 0U;

	LTRACEF("entry\n");

	ret = reg_rw_arg_check(engine, data, nwords);
	CCC_ERROR_CHECK(ret);

	ret = lwpka_reg_offset(op_mode, lwpka_reg, &reg_base, &reg_size, NULL);
	CCC_ERROR_CHECK(ret,
			LTRACEF("%s map failed: %d\n",
				lwpka_lor_name(lwpka_reg), ret));

	if (reg_size < nwords) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE, LTRACEF("%s size %u too small, attempt to read %u words\n",
							    lwpka_lor_name(lwpka_reg),
							    reg_size, nwords));
	}

	LTRACEF("%u words from LWPKA %s\n", nwords, lwpka_lor_name(lwpka_reg));

	ret = get_reg_value(engine, data, reg_base,
			    nwords, big_endian);
	CCC_ERROR_CHECK(ret);

	LTRACEF("%u words read from LWPKA %s (caller order: %s)\n",
		nwords, lwpka_lor_name(lwpka_reg),
		(BOOL_IS_TRUE(big_endian) ? "BE" : "LE"));
	LOG_HEXBUF("Returned data (caller order)", data, nwords*4U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_soft_reset_engine(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t i = 0U;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	val = LW_DRF_DEF(LW_SE_LWPKA, CTRL_RESET, SOFT_RESET, TRIGGER);
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CTRL_RESET_0, val);

	CCC_LOOP_BEGIN {
		val = tegra_engine_get_reg_NOTRACE(engine, LW_SE_LWPKA_CTRL_RESET_0);

		if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_RESET, SOFT_RESET, val) ==
		    LW_SE_LWPKA_CTRL_RESET_0_SOFT_RESET_DONE) {
			CCC_LOOP_STOP;
		}

		i++;

		if (i > lwpka_engine_max_timeout) {
			CCC_ERROR_MESSAGE("LWPKA reset wait completion timed out (%u) \n",
					  lwpka_engine_max_timeout);
			ret = SE_ERROR(ERR_TIMED_OUT);
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* This is LWPKA_CTRL_STATUS bit error value checker for
 * HW error bits.
 */
#define CCC_NUM_CTRL_STATUS_ERR_STEPS 4U

static status_t lwpka_ctrl_status_check(const engine_t *engine,
					uint32_t ncs,
					status_t *ret2_p)
{
	status_t ret = NO_ERROR;
	uint32_t steps = 0U;
	uint32_t mask = 0U;

	if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, DMEM1_PARITY_ERR, ncs) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_STATUS, DMEM1_PARITY_ERR, SET, mask);
	}
	steps++;

	if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, DMEM2_PARITY_ERR, ncs) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_STATUS, DMEM2_PARITY_ERR, SET, mask);
	}
	steps++;

	FI_BARRIER(u32, steps);

	/* Avoid cert-c overflow */
	if (steps > 2U) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, KSLT_ECC_SEC, ncs) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_STATUS, KSLT_ECC_SEC, SET, mask);
	}
	steps++;

	if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, KSLT_ECC_DED, ncs) != 0U) {
		ret = ERR_FAULT;
		mask = LW_FLD_SET_DRF_DEF(LW_SE_LWPKA, CTRL_STATUS, KSLT_ECC_DED, SET, mask);
	}
	steps++;

	if (0U != mask) {
		tegra_engine_set_reg(engine, LW_SE_LWPKA_CTRL_STATUS_0, mask);
	}

	CCC_ERROR_CHECK(ret);

	*ret2_p = NO_ERROR;

	FI_BARRIER(u32, steps);
	FI_BARRIER(status, ret);
fail:
	if ((NO_ERROR == ret) && (CCC_NUM_CTRL_STATUS_ERR_STEPS != steps)) {
		ret = ERR_BAD_STATE;
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_wait_idle(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t lwpka_reg = 0U;
	uint32_t val = 0U;
	uint32_t inx = 0U;
	status_t ret2 = ERR_BAD_STATE;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

	CCC_LOOP_BEGIN {
		lwpka_reg = tegra_engine_get_reg_NOTRACE(engine, LW_SE_LWPKA_CTRL_STATUS_0);

		val = LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, PKA_STATUS, lwpka_reg);
		if (LW_SE_LWPKA_CTRL_STATUS_0_PKA_STATUS_IDLE == val) {
			ret = lwpka_ctrl_status_check(engine, lwpka_reg, &ret2);
			CCC_LOOP_STOP;
		}

		inx++;

		if (inx > lwpka_engine_max_timeout) {
			CCC_ERROR_MESSAGE("LWPKA status idle wait timed out (%u) \n",
					  lwpka_engine_max_timeout);
			ret = SE_ERROR(ERR_TIMED_OUT);
			CCC_LOOP_STOP;
		}
	} CCC_LOOP_END;
	CCC_ERROR_CHECK(ret);

fail:
	if (NO_ERROR == ret) {
		ret = ret2;
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA_RESET

/* Set this if the device needs to be reset before next mutex unlock
 */
static bool lwpka_device_reset_pending;

static void lwpka_device_reset(const engine_t *engine, bool *reset_pending_p)
{
	LTRACEF("entry\n");

	CCC_ERROR_MESSAGE("LWPKA device reset\n");
	*reset_pending_p = false;

	(void)lwpka_soft_reset_engine(engine);

	LTRACEF("exit\n");
}
#endif /* HAVE_LWPKA_RESET */

static uint32_t lwpka_version = 0U;

#ifndef CCC_LWPKA_PRIV_MASK
#define CCC_LWPKA_PRIV_MASK      0xffffffffU
#endif

uint32_t lwpka_get_version(void)
{
	return lwpka_version;
}

static status_t lwpka_build_config_check(uint32_t bconf)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_BUILD_CONFIG, HAS_SAFETY_EXT, bconf) != 0U) {
		/* Not sure if should fail; this is info only for now */
		LOG_INFO("LWPKA safety features enabled\n");

		if ((bconf & BCONF_MASK_KSLT_SAFETY) == 0U) {
			LOG_ALWAYS("LWPKA KSLT safety features OFF: 0x%x\n", bconf);
		} else {
			/* FIXME */
			if ((bconf & 0x8U) != 0U) {
				LOG_INFO("LWPKA with KSLT ECC\n");
			}
			if ((bconf & 0x4U) != 0U) {
				LOG_INFO("LWPKA with KSLT PARITY\n");
			}
		}

		if ((bconf & BCONF_MASK_DMEM_SAFETY) == 0U) {
			LOG_ALWAYS("LWPKA DMEM safety features OFF: 0x%x\n", bconf);
		} else {
			/* FIXME */
			if ((bconf & 0x2U) != 0U) {
				LOG_INFO("LWPKA with DMEM ECC\n");
			}
			if ((bconf & 0x1U) != 0U) {
				LOG_INFO("LWPKA with DMEM PARITY\n");
			}
		}

	}

#if HAVE_LWPKA11_SUPPORT
	if ((bconf & BCONF_MASK_LWPKA11_EC_KS) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("LWPKA11 without EC keystore 0x%x\n", bconf));
	}
fail:
#endif
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t check_ctrl_build_config(uint32_t bconf)
{
	status_t ret = NO_ERROR;

        if (BOOL_IS_TRUE(LWPKA_VERSION_SUPPORTED(lwpka_version))) {
            ret = lwpka_build_config_check(bconf);
            CCC_ERROR_CHECK(ret);
        } else {
            /* Do not terminate here, since the unknown version was already
             * ignored in the previous version check in call path.
             */
            LOG_ALWAYS("*** Unknown NPVKA version, build config not checked: 0x%x\n", bconf);
        }
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_version_check(uint32_t version)
{
	status_t ret = NO_ERROR;

        if (!BOOL_IS_TRUE(LWPKA_VERSION_SUPPORTED(version))) {
            LOG_ALWAYS("*** Unexpected LWPKA version: 0x%x\n", version);
            CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
        }

	lwpka_version = version;
	LTRACEF("LWPKA version 0x%x\n", version);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t setup_lwpka_device(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry: Setting up device %s\n", engine->e_dev->cd_name);

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CTRL_VERSION_0);

	LTRACEF("LWPKA device version: 0x%x\n", val);

	ret = lwpka_version_check(val);
	CCC_ERROR_CHECK(ret);

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CTRL_BUILD_CONFIG_0);

	ret = check_ctrl_build_config(val);
	CCC_ERROR_CHECK(ret);

	val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CTRL_PRIV_LEVEL_MASK_0);

	LTRACEF("LWPKA priv_mask value: 0x%x\n", val);

	if (CCC_LWPKA_PRIV_MASK != val) {
		LOG_ALWAYS("*** LWPKA priv mask 0x%x, setting default\n", val);

		val = CCC_LWPKA_PRIV_MASK;

		tegra_engine_set_reg(engine,
				     LW_SE_LWPKA_CTRL_PRIV_LEVEL_MASK_0,
				     val);

		val = tegra_engine_get_reg(engine, LW_SE_LWPKA_CTRL_PRIV_LEVEL_MASK_0);
		LTRACEF("LWPKA priv_mask value after setting 0x%x: 0x%x\n",
			CCC_LWPKA_PRIV_MASK, val);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_control_setup_unit(const engine_t *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	{
		static bool setup_device = true;

		if (setup_device) {
			setup_device = false;
			ret = setup_lwpka_device(engine);
			CCC_ERROR_CHECK(ret,
					CCC_ERROR_MESSAGE("LWPKA setup failed: 0x%x\n", ret));
		}
	}

	/* IAS sequence contains LWPKA engine soft reset in every operation.
	 * Until modified => do just that.
	 */
	ret = lwpka_soft_reset_engine(engine);
	CCC_ERROR_CHECK(ret, LTRACEF("LWPKA soft reset failure\n"));

	ret = lwpka_wait_idle(engine);
	CCC_ERROR_CHECK(ret, LTRACEF("LWPKA idle wait failure\n"));

	/* LWPKA does not support local HW timeouts/watchdogs */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_acquire_mutex(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_LWPKA_MUTEX == 0
	LTRACEF("Mutex acquire: LWPKA mutex inactive\n");
#else
	{
		uint32_t val = 0U;
		uint32_t i = 0U;
		bool locked = false;

		/* Acquire lwpka mutex */
		do {
			if (i > lwpka_engine_max_timeout) {
				CCC_ERROR_MESSAGE("Acquire LWPKA Mutex timed out\n");
				ret = SE_ERROR(ERR_TIMED_OUT);
				break;
			}

			/* It is not possible to trace this register get (may
			 * loop a lot of times)
			 */
			val = tegra_engine_get_reg_NOTRACE(engine, LW_SE_LWPKA_CTRL_MUTEX_0);
			locked = (LW_DRF_VAL(LW_SE_LWPKA, CTRL_MUTEX, PKA_LOCK, val) == 1U);

#if TEGRA_UDELAY_VALUE_LWPKA_MUTEX > 0
			if (!BOOL_IS_TRUE(locked)) {
				TEGRA_UDELAY(TEGRA_UDELAY_VALUE_LWPKA_MUTEX);
			}
#endif

			i++;
		} while (! BOOL_IS_TRUE(locked));

		CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("LWPKA mutex lock failed: %d\n", ret));
	}
#endif /* HAVE_LWPKA_MUTEX == 0 */

	ret = lwpka_control_setup_unit(engine);
	if (NO_ERROR != ret) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("LWPKA setup failed: %d\n", ret));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void lwpka_release_mutex(const struct engine_s *engine)
{
#if HAVE_LWPKA_MUTEX == 0
	(void)engine;

	LTRACEF("Mutex release: LWPKA mutex inactive\n");
#else
	uint32_t val = LW_DRF_DEF(LW_SE_LWPKA, CTRL_MUTEX, PKA_LOCK, RELEASE);

	LTRACEF("entry\n");

#if HAVE_LWPKA_RESET
	if (BOOL_IS_TRUE(lwpka_device_reset_pending)) {
		lwpka_device_reset(engine, &lwpka_device_reset_pending);
	}
#endif /* HAVE_LWPKA_RESET */

	/* Release mutex and auto-scrub the LOR memory */
	tegra_engine_set_reg(engine, LW_SE_LWPKA_CTRL_MUTEX_0, val);

	LTRACEF("exit\n");
#endif /* HAVE_LWPKA_MUTEX == 0 */
}

#if HAVE_LWPKA_USES_INTERRUPTS == 0

static status_t poll_op_done(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t count = 0U;
	CCC_LOOP_VAR;

	LTRACEF("entry\n");

#if TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL > 0
	TEGRA_UDELAY(TEGRA_UDELAY_VALUE_LWPKA_OP_INITIAL);
#endif

	CCC_LOOP_BEGIN {
		val = tegra_engine_get_reg_NOTRACE(engine, LW_SE_LWPKA_CTRL_STATUS_0);

		if (count > lwpka_engine_max_timeout) {
			CCC_ERROR_MESSAGE("LWPKA Done status timed out (%u): reset pending\n",
					  lwpka_engine_max_timeout);
#if HAVE_LWPKA_RESET
			lwpka_device_reset_pending = true;
#endif /* HAVE_LWPKA_RESET */
			ret = SE_ERROR(ERR_TIMED_OUT);
			CCC_LOOP_STOP;
		}

		if (LW_DRF_VAL(LW_SE_LWPKA, CTRL_STATUS, PKA_STATUS, val) ==
		    LW_SE_LWPKA_CTRL_STATUS_0_PKA_STATUS_IDLE) {
			CCC_LOOP_STOP;
		}

		count++;

#if TEGRA_UDELAY_VALUE_LWPKA_OP > 0
		TEGRA_UDELAY(TEGRA_UDELAY_VALUE_LWPKA_OP);
#endif
	} CCC_LOOP_END;

	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	LTRACEF(" count=%u (status=0x%x)\n", count, val);
	return ret;
}
#endif /* HAVE_LWPKA_USES_INTERRUPTS == 0 */

#ifndef MAX_LWPKA_IRQ_DONE_POLL_COUNT
#define MAX_LWPKA_IRQ_DONE_POLL_COUNT 10U
#endif

/* Check IRQ status set.
 *
 * Request pending engine reset if not set when called (if enabled).
 */
static void lwpka_check_irq_status_done(const engine_t *engine)
{
	uint32_t val = 0U;
	uint32_t icount = 0U;

	do {
		if (icount > MAX_LWPKA_IRQ_DONE_POLL_COUNT) {
			CCC_ERROR_MESSAGE("LWPKA interrupt not set after operation\n");
#if HAVE_LWPKA_RESET
			lwpka_device_reset_pending = true;
#endif /* HAVE_LWPKA_RESET */
			break;
		}

		val = tegra_engine_get_reg_NOTRACE(engine, LW_SE_LWPKA_CORE_IRQ_STATUS_0);

		val = LW_DRF_VAL(LW_SE_LWPKA, CORE_IRQ_STATUS, OP_DONE, val);

#if TEGRA_UDELAY_VALUE_LWPKA_OP > 0
		if (LW_SE_LWPKA_CORE_IRQ_STATUS_0_OP_DONE_UNSET == val) {
			TEGRA_UDELAY(TEGRA_UDELAY_VALUE_LWPKA_OP);
		}
#endif
		icount++;
	} while (LW_SE_LWPKA_CORE_IRQ_STATUS_0_OP_DONE_UNSET == val);
}

#if LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
/* If debugging and tracing show instructions and cycles
 * since GO_START (Would be an accurate perf measure).
 *
 * XXX If unit freq known => should also callwlate time used by op (TODO)
 */
static void lwpka_operation_statistics(const engine_t *engine)
{
	uint32_t lwpka_cycles =
		tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_PROFILE_0);
	uint32_t lwpka_insns =
		tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_PROFILE_0 + 4U);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
	lwpka_entry_perf_cycles = lwpka_cycles;
	lwpka_entry_perf_insns  = lwpka_insns;
#else
	LTRACEF("LWPKA: cycles %u, instruction %u since GO\n",
		lwpka_cycles, lwpka_insns);
#endif
}
#endif /* LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION) */

static status_t lwpka_check_core_status(const char *sname, const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t lwpka_reg = 0U;
	uint32_t cval = 0U;

	LTRACEF("entry\n");

	(void)sname;

	lwpka_reg = tegra_engine_get_reg(engine, LW_SE_LWPKA_CORE_STATUS_0);

	cval = LW_DRF_VAL(LW_SE_LWPKA, CORE_STATUS, BUSY, lwpka_reg);

	if (LW_SE_LWPKA_CORE_STATUS_0_BUSY_IDLE != cval ) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("CORE STATUS(%s): busy: 0x%x\n",
					     sname, cval));
	}

	/* Caller verifies this when appropriate is just informative:
	 */
	LTRACEF("CORE STATUS(%s): point valid = %u\n",
		sname,
		LW_DRF_VAL(LW_SE_LWPKA, CORE_STATUS, IS_VALID_POINT, lwpka_reg));

	cval = LW_DRF_VAL(LW_SE_LWPKA, CORE_STATUS, RETURN_CODE, lwpka_reg);
	if (LW_SE_LWPKA_CORE_STATUS_0_RETURN_CODE_PASS != cval) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("CORE STATUS(%s): error code 0x%x\n",
					     sname, cval));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_wait_op_done(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

#if HAVE_LWPKA_USES_INTERRUPTS
	/* This call returns when the
	 * subsystem has received the interrupt and the operation
	 * has completed.
	 *
	 * CCC code below checks that the interrupt has oclwrred and it will also
	 * clear the interrupt before returning to caller.
	 *
	 * Callee should unmask the interrupt in the GIC, wait for interrupt,
	 * when received mask it again.
	 */
	ret = CCC_WAIT_FOR_INTERRUPT(engine->e_dev->cd_id, engine->e_id, engine);

	/* Mask LWPKA interrupts after the interrupt has been received.
	 */
	tegra_engine_set_reg(engine,
			     LW_SE_LWPKA_CTRL_INTR_MASK_0,
			     0U);

	CCC_ERROR_CHECK(ret);
#else
	ret = poll_op_done(engine);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_LWPKA_USES_INTERRUPTS */

	/* Check that OP_DONE set in IRQ_STATUS register.
	 *
	 * IAS states that OP_DONE shall be asserted whenever operation terminates
	 * irrespectively of the return code.
	 */
	lwpka_check_irq_status_done(engine);

	ret = lwpka_check_core_status("op done", engine);
	CCC_ERROR_CHECK(ret);

#if HAVE_LWPKA_ERROR_CAPTURE
	{
		uint32_t error_info = 0U;
		ret = lwpka_device_check_error_capture(engine->e_dev, &error_info, false);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("LWPKA error capture: 0x%x\n",
						  error_info));
	}
#endif

#if LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
	lwpka_operation_statistics(engine);
#endif

fail:
	if (NULL != engine) {
		/* Re-enable error capture */
		lwpka_device_clear_error_capture(engine->e_dev);

		/* ACK the interrupt, clear intr bit in  */
		tegra_engine_set_reg(engine, LW_SE_LWPKA_CORE_IRQ_STATUS_0,
				     LW_DRF_DEF(LW_SE_LWPKA, CORE_IRQ_STATUS, OP_DONE, SET));
	}

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_LWPKA_OPERATION)
	if (0U != gen_op_perf_usec) {
		show_lwpka_gen_op_entry_perf(gen_op_perf_primitive_op,
					     get_elapsed_usec(gen_op_perf_usec));
	}
	gen_op_perf_primitive_op = LWPKA_PRIMITIVE_ILWALID;
	gen_op_perf_usec = 0U;
	lwpka_entry_perf_cycles = 0U;
	lwpka_entry_perf_insns  = 0U;
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA_ZERO_MODULUS_CHECK
/* Modular operations use LOR_C0 for modulus value always
 * This function returns error is modulus value is zero.
 *
 * XXX This is not used => because LWPKA IAS says that HW would give
 * an error if "m is not odd". Since zero is not an odd number
 * HW traps this and SW does not need to check for zero modulus.
 *
 * Instead of this modulus is written directly to LOR_C0.
 *
 * Even if this is enabled: The callers need to call this instead
 * of writing directly to LOR_C0 (in EC, RSA and MOD operations).
 */
status_t lwpka_load_modulus(const engine_t *engine,
			    const struct pka_engine_data *lwpka_data,
			    const uint8_t *modulus,
			    uint32_t m_len,
			    bool m_BE)
{
	status_t ret  = NO_ERROR;
	status_t rbad = ERR_BAD_STATE;
	uint32_t nwords = m_len / BYTES_IN_WORD;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(se_util_vec_is_zero(modulus, m_len, &rbad))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Zero modulus for modular operations\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	/* Load MODULUS to LOR_C0 (common in all ops using modulus value)
	 */
	ret = lwpka_register_write(engine, lwpka_data->op_mode,
				   LOR_C0, modulus, nwords, m_BE, 0U);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write modulus to LOR_C0: %d\n",
					  ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_ZERO_MODULUS_CHECK */

void lwpka_data_release(struct pka_engine_data *lwpka_data)
{
	/* No resources held in LWPKA version of this object,
	 * just clear it.
	 */
	if (NULL != lwpka_data) {

// XXXXX clearing two ordinal fields => rework

		se_util_mem_set((uint8_t *)lwpka_data, 0U, sizeof_u32(struct pka_engine_data));
	}
}

#endif /* HAVE_LWPKA_RSA || HAVE_LWPKA_ECC || HAVE_LWPKA_MODULAR */
