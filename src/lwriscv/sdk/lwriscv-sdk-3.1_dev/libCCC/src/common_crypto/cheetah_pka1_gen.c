/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic engine : generic RSA/ECC support functions  */

#include <crypto_system_config.h>

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG

#include <tegra_pka1_rsa.h>
#include <tegra_cdev.h>
#include <tegra_pka1_gen.h>
#include <context_mem.h>

#define MEASURE_PERF_ENGINE_MONTGOMERY_ALL	0
#define MEASURE_PERF_ENGINE_MONTGOMERY_SINGLE	0

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifndef HAVE_PKA1_USES_INTERRUPTS
#define HAVE_PKA1_USES_INTERRUPTS 0
#endif

#if HAVE_PKA1_USES_INTERRUPTS
#ifndef CCC_WAIT_FOR_INTERRUPT
#error "PKA1: CCC device interrupt handler undefined"
#endif
#endif /* HAVE_PKA1_USES_INTERRUPTS */

/* This will affect performance, but make TA's harder
 * (randomizes exelwtion time of PKA1 private key operations)
 */
#ifndef HAVE_JUMP_PROBABILITY
#define HAVE_JUMP_PROBABILITY 1
#endif

/* This MUST BE nonzero after LOAD_KEY is enabled and using LW shadow
 * registers Otherwise PKA will not work correctly.
 */
#define PKA1_ENABLE_INTERRUPTS 1

#if !PKA1_ENABLE_INTERRUPTS
#if HAVE_PKA1_LOAD
#error "PKA does not work if interrupts are disabled and LOAD_KEY is supported"
#endif
#endif /* !PKA1_ENABLE_INTERRUPTS */

/* forward decl */
static status_t pka1_ctrl_base_radix(enum pka1_op_mode op_mode, uint32_t *baser_p);

#if LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION)
static const struct {
	pka1_entrypoint_t pep;
	const char *pname;
} epid[] = {
	{ PKA1_ENTRY_ILWALID, "INVALID" },
	{ PKA1_ENTRY_MONT_M,  "MONT_M" },
	{ PKA1_ENTRY_MONT_RILW, "MONT_RILW" },
	{ PKA1_ENTRY_MONT_R2, "MONT_R2" },
	{ PKA1_ENTRY_CRT, "CRT" },
	{ PKA1_ENTRY_CRT_KEY_SETUP, "CRT_KEY_SETUP" },
	{ PKA1_ENTRY_IS_LWRVE_M3, "IS_LWRVE_M3" },
	{ PKA1_ENTRY_MOD_ADDITION, "MOD_ADDITION" },
	{ PKA1_ENTRY_MOD_DIVISION, "MOD_DIVISION" },
	{ PKA1_ENTRY_MOD_EXP, "MOD_EXP" },
	{ PKA1_ENTRY_MOD_ILWERSION, "MOD_ILWERSION" },
	{ PKA1_ENTRY_MOD_MULTIPLICATION, "MOD_MULTIPLICATION" },
	{ PKA1_ENTRY_MOD_SUBTRACTION, "MOD_SUBTRACTION" },
	{ PKA1_ENTRY_EC_POINT_ADD, "EC_POINT_ADD" },
	{ PKA1_ENTRY_EC_POINT_DOUBLE, "EC_POINT_DOUBLE" },
	{ PKA1_ENTRY_EC_POINT_MULT, "EC_POINT_MULT" },
	{ PKA1_ENTRY_EC_ECPV, "EC_ECPV" },
	{ PKA1_ENTRY_MOD_REDUCTION, "MOD_REDUCTION" },
	{ PKA1_ENTRY_BIT_SERIAL_REDUCTION, "BIT_SERIAL_REDUCTION" },
	{ PKA1_ENTRY_BIT_SERIAL_REDUCTION_DP, "BIT_SERIAL_REDUCTION_DP" },
	{ PKA1_ENTRY_EC_SHAMIR_TRICK, "EC_SHAMIR_TRICK" },
#if HAVE_ELLIPTIC_20
	{ PKA1_ENTRY_GEN_MULTIPLY, "GEN_MULTIPLY" },
	{ PKA1_ENTRY_MOD_521_MONTMULT, "MOD_521_MONTMULT" },
	{ PKA1_ENTRY_MOD_MERSENNE_521_MULT, "MOD_MERSENNE_521_MULT" },
	{ PKA1_ENTRY_EC_521_POINT_MULT, "EC_521_POINT_MULT" },
	{ PKA1_ENTRY_EC_521_POINT_ADD, "EC_521_POINT_ADD" },
	{ PKA1_ENTRY_EC_521_POINT_DOUBLE, "EC_521_POINT_DOUBLE" },
	{ PKA1_ENTRY_EC_521_PV, "EC_521_PV" },
	{ PKA1_ENTRY_EC_521_SHAMIR_TRICK, "EC_521_SHAMIR_TRICK" },
	{ PKA1_ENTRY_C25519_POINT_MULT, "C25519_POINT_MULT" },
	{ PKA1_ENTRY_C25519_MOD_EXP, "C25519_MOD_EXP" },
	{ PKA1_ENTRY_C25519_MOD_MULT, "C25519_MOD_MULT" },
	{ PKA1_ENTRY_C25519_MOD_SQR, "C25519_MOD_SQR" },
	{ PKA1_ENTRY_ED25519_POINT_MULT, "ED25519_POINT_MULT" },
	{ PKA1_ENTRY_ED25519_POINT_ADD, "ED25519_POINT_ADD" },
	{ PKA1_ENTRY_ED25519_POINT_DOUBLE, "ED25519_POINT_DOUBLE" },
	{ PKA1_ENTRY_ED25519_PV, "ED25519_PV" },
	{ PKA1_ENTRY_ED25519_SHAMIR_TRICK, "ED25519_SHAMIR_TRICK" },
#endif /* HAVE_ELLIPTIC_20 */
	{ PKA1_ENTRY_ILWALID, NULL },
};

static const char *pka1_get_entrypoint_name(pka1_entrypoint_t ep)
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
#endif /* LOCAL_TRACE || MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION) */

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION)
static uint32_t gen_op_perf_usec = 0U;
static pka1_entrypoint_t gen_op_perf_entrypoint = PKA1_ENTRY_ILWALID;
static uint32_t pka1_entry_perf_cycles = 0U;
static uint32_t pka1_entry_perf_insns = 0U;

static void show_pka1_gen_op_entry_perf(pka1_entrypoint_t ep,
					uint32_t elapsed)
{
	CCC_ERROR_MESSAGE("PKA1 engine [%s (%u)]: %u usec, cycles %u, insns %u\n",
			  pka1_get_entrypoint_name(ep), ep, elapsed,
			  pka1_entry_perf_cycles, pka1_entry_perf_insns);
}
#endif /* MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION) */

/* Power of 2 byte length => partial radix must be set 0
 * This is only specified in page 51 of the IAS; elsewhere there
 * is no mention of such requirement!!!!
 */
static uint32_t get_partial_radix(uint32_t byte_size)
{
	uint32_t partial_radix = 0U;

	switch(byte_size) {
	case 16U:	// not used
	case 32U:
	case 64U:
	case 128U:
	case 256U:
	case 512U:
	case 1024U:	// not used
		break;
	default:
		partial_radix = byte_size / BYTES_IN_WORD;
		break;
	}

	return partial_radix;
}


static status_t start_op(const struct engine_s *engine, enum pka1_op_mode op_mode,
			 uint32_t byte_size, pka1_entrypoint_t entrypoint)
{
	status_t ret = NO_ERROR;
	uint32_t go_start = 0U;
	uint32_t base_radix = 0U;
	uint32_t partial_radix = 0U;

	LTRACEF("entry\n");

	(void)entrypoint;

	/* Step 6 */
	ret = pka1_ctrl_base_radix(op_mode, &base_radix);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Invalid base radix op mode: %d\n",
					  ret));

	partial_radix = get_partial_radix(byte_size);

	/* collecting fields to GO operation starting here */
	go_start = 0U;

	// XXXXX FIXME NOTE P521 => partial radix must be set to 20 (depends on alu width?)
	//
	// see page 22/48 for setting Mersenne M-521 partial radix value to 17
	// (ceiling of 521/32 => 17)
	//
	// Restrict number of processed words by the partial radix
	//  when base radix must be a power of 2.
	//
	// XXX Does the PARTIAL_RADIX need to be set when the operation size is power of 2 ??
	// XXX LW doc says => yes it must be set... => this should be ok
	// XXX FIXME (verify this, then remove this comment)
	//
	if (PKA1_OP_MODE_ECC521 == op_mode) {
#if HAVE_ELLIPTIC_20
		/* The ALU width of the PKA1 is configured to 128 bits
		 * => AFAIK, this means that we need to configure the partial radix to
		 * 20 words. TODO => VERIFY (TODO)!!!
		 *
		 * See PKA FW User guide, page 46
		 */
		partial_radix = 20U;

		/* Set M521 operation mode */
		go_start |= SE_PKA1_CTRL_M521_MODE(SE_PKA1_CTRL_M521_MODE_SET);
#else
		/* 544 / 32 == 17, 544 is the next word multiple larger than 521
		 * See EDN-0577 (version 1.1) => page 6
		 *
		 * There is a separate flag (F1) for P521 so it can be identified.
		 */
		partial_radix = 17U;
#endif
	}

#if SE_RD_DEBUG && LOCAL_TRACE
	{
		uint32_t dval = 0U;
#if HAVE_PKA1_LOAD
		dval = tegra_engine_get_reg(engine, SE_PKA1_CTRL_PKA_CONTROL_OFFSET);
#else
		dval = tegra_engine_get_reg(engine, SE_PKA1_CTRL_OFFSET);
#endif

		LTRACEF("go start... CONTROL reg before setting go_start: 0x%x (check load key bit)\n",
			dval);

		dval = tegra_engine_get_reg(engine, SE_PKA1_STATUS_OFFSET);
		LTRACEF("go start... STATUS reg before setting go_start: 0x%x (check load key bit)\n",
			dval);

		LTRACEF("go start... OP SIZE: %u (0x%x)\n", byte_size, byte_size);
		LTRACEF("go start... BASE RADIX: 0x%x\n", base_radix);
		LTRACEF("go start... PARTIAL RADIX: 0x%x\n", partial_radix);
		LTRACEF("go start entry %s (0x%x)\n",
			pka1_get_entrypoint_name(entrypoint), entrypoint);
	}
#endif /* SE_RD_DEBUG && LOCAL_TRACE */

	go_start |= (SE_PKA1_CTRL_BASE_RADIX(base_radix) |
		     SE_PKA1_CTRL_PARTIAL_RADIX(partial_radix));

	go_start |= SE_PKA1_CTRL_GO(SE_PKA1_CTRL_GO_START);

#if HAVE_PKA1_LOAD
	LTRACEF("XXX GO_START value 0x%x\n", go_start);

	/* NOTE: Must not write NOP to CTRL_GO => collect all bits and
	 * write them with CTRL_GO.
	 */
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_PKA_CONTROL_OFFSET, go_start);
#else
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_OFFSET, go_start);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Force init of some regs before every op started
 */
status_t pka1_go_start(const struct engine_s *engine, enum pka1_op_mode op_mode,
		       uint32_t byte_size, pka1_entrypoint_t entrypoint)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == engine) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION)
	gen_op_perf_usec = GET_USEC_TIMESTAMP;
	gen_op_perf_entrypoint = entrypoint;
#endif

	/* Set the engine operation entry point */

#if LOCAL_TRACE
	LTRACEF("Setting PKA1 entrypoint 0x%x (%s)\n",
		entrypoint, pka1_get_entrypoint_name(entrypoint));
#else
	LTRACEF("Setting PKA1 entrypoint 0x%x\n", entrypoint);
#endif

	if (PKA1_ENTRY_ILWALID == entrypoint) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Step 3 */
	tegra_engine_set_reg(engine, SE_PKA1_FSTACK_PTR_OFFSET, 0U);

	/* Step 4 */
	tegra_engine_set_reg(engine, SE_PKA1_PRG_ENTRY_OFFSET, (uint32_t)entrypoint);

	/* Step 5 */
#if PKA1_ENABLE_INTERRUPTS
	{
		/* SE PKA IAS requires the IRQ to be enabled (Section 6.5) for the
		 * PKA1 keyslot loading feature. The LW extension for handling
		 * keyslot loading uses interrupts internally.
		 */
		uint32_t imask = 0U;

		tegra_engine_set_reg(engine,
				     SE_PKA1_INT_ENABLE_OFFSET,
				     SE_PKA1_INT_ENABLE_IE_IRQ_EN(SE_ELP_ENABLE));

#if HAVE_PKA1_USES_INTERRUPTS
		/* Mask the TRNG interrupt and unmask the PKA1 interrupt;
		 * in this case the PKA1 "operation completed" polling loop is
		 * replaced by a PKA1 interrupt handler.
		 *
		 * SW does not need/use the TRNG interrupts: keep it masked.
		 *
		 * TODO: [NOTE] In case there is LFSR_LOCKUP condition in TRNG the
		 *  driver should reseed the TRNG unit. This fault triggers
		 *  a TRNG interrupt (as per Q&A with Jerry) and may require
		 *  unmasking EIP1 and creating a handler for the exception.
		 *
		 *  Consider adding this later after more clarifications...
		 */
		imask = (SE_PKA1_CTRL_SE_INTR_MASK_EIP1_TRNG(SE_ELP_DISABLE) |
			 SE_PKA1_CTRL_SE_INTR_MASK_EIP0_PKA(SE_ELP_ENABLE));
#else
		/* Mask PKA1 interrupt to CPU; using polling for op completion.
		 */
		imask = (SE_PKA1_CTRL_SE_INTR_MASK_EIP1_TRNG(SE_ELP_DISABLE) |
			 SE_PKA1_CTRL_SE_INTR_MASK_EIP0_PKA(SE_ELP_DISABLE));
#endif /* HAVE_PKA1_USES_INTERRUPTS */

		/* Unmask PKA1 to interrupt CPU if interrupts are enabled, otherwise
		 * mask them.
		 *
		 * TRNG interrupt is kept masked (but see NOTE above).
		 */
		tegra_engine_set_reg(engine,
				     SE_PKA1_CTRL_SE_INTR_MASK_OFFSET,
				     imask);
	}
#else
	/* Disable interrupts if not forced on!
	 */
	tegra_engine_set_reg(engine,
			     SE_PKA1_INT_ENABLE_OFFSET,
			     SE_PKA1_INT_ENABLE_IE_IRQ_EN(SE_ELP_DISABLE));
#endif /* PKA1_ENABLE_INTERRUPTS */

	ret = start_op(engine, op_mode, byte_size, entrypoint);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
status_t pka1_keyslot_load(const struct engine_s *engine, uint32_t pka_keyslot)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (pka_keyslot >= PKA1_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Loading key from SE PKA1 keyslot %u\n", pka_keyslot);

	val = 0U;

	val |= SE_PKA1_CTRL_CONTROL_KEYSLOT(pka_keyslot);

	/* LOAD_KEY (add LOAD_KEY bit to current CONTROL register value)
	 */
	val |= SE_PKA1_CTRL_CONTROL_LOAD_KEY(SE_ELP_ENABLE);

#if HAVE_PKA1_SCC_DPA
	/* Never allow AUTO_RESEED to be clear when SCC DPA is enabled
	 */
	val |= SE_PKA1_CTRL_CONTROL_AUTO_RESEED(SE_ELP_ENABLE);
#endif

	/* use the LW reg version for the added LOAD_KEY */
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_CONTROL_OFFSET, val);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_keyslot_zeropad(const struct engine_s *engine,
				     uint32_t key_slot,
				     uint32_t key_words,
				     uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	if (key_slot >= PKA1_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	for (i = key_words; i < field_word_size; i++) {
		tegra_engine_set_reg(engine, SE_PKA1_KEYSLOT_DATA_OFFSET(key_slot),
				     0x00000000U);
	}

	FI_BARRIER(u32, i);

	if (i != field_word_size) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t set_keyslot_field_value(const struct engine_s *engine,
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

	if (key_slot >= PKA1_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (BOOL_IS_TRUE(data_big_endian)) {
		/* PKA unit operates on little endian data, must swap byte order */
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&param[(offset - BYTES_IN_WORD)], data_big_endian);
			tegra_engine_set_reg(engine, SE_PKA1_KEYSLOT_DATA_OFFSET(key_slot), val32);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&param[offset], data_big_endian);
			tegra_engine_set_reg(engine, SE_PKA1_KEYSLOT_DATA_OFFSET(key_slot), val32);
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
status_t pka1_set_keyslot_field(const struct engine_s *engine,
				pka1_keymat_field_t field,
				uint32_t key_slot,
				const uint8_t *param,
				uint32_t key_words,
				bool data_big_endian,
				uint32_t field_word_size)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == param) || (0U == key_words) ||
	    (key_slot >= PKA1_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Write <kslot,field> in auto-increment mode starting from beginnign of field */
	tegra_engine_set_reg(engine,
			     SE_PKA1_KEYSLOT_ADDR_OFFSET(key_slot),
			     SE_PKA1_KEYSLOT_ADDR_FIELD(field) |
			     SE_PKA1_KEYSLOT_ADDR_WORD(0U) |
			     SE_PKA1_KEYSLOT_ADDR_AUTO_INC(SE_ELP_ENABLE));

	LTRACEF("Setting %u words in field %u in keyslot %u (BE=%u)\n",
		key_words, field, key_slot, data_big_endian);

	ret = set_keyslot_field_value(engine, key_slot, param,
				      key_words, data_big_endian);
	CCC_ERROR_CHECK(ret);

	if (key_words < field_word_size) {
		LTRACEF("Zero padding %u words in keyslot field %u value\n",
			(field_word_size - key_words), field);

		ret = pka1_keyslot_zeropad(engine, key_slot,
					   key_words, field_word_size);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_LOAD */

static status_t pka1_ctrl_base_radix(enum pka1_op_mode op_mode, uint32_t *baser_p)
{
	status_t ret = NO_ERROR;
	uint32_t words = 0U;
	uint32_t base_radix = 0U;

	LTRACEF("entry\n");
	if (NULL == baser_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*baser_p = 0U;

	ret = pka1_num_words(op_mode, &words);
	CCC_ERROR_CHECK(ret);

	switch (words) {
	case (256U / BITS_IN_WORD):
		base_radix = SE_PKA1_CTRL_BASE_256;
		break;
	case (512U / BITS_IN_WORD):
		base_radix = SE_PKA1_CTRL_BASE_512;
		break;
	case (1024U / BITS_IN_WORD):
		base_radix = SE_PKA1_CTRL_BASE_1024;
		break;
	case (2048U / BITS_IN_WORD):
		base_radix = SE_PKA1_CTRL_BASE_2048;
		break;
	case (4096U / BITS_IN_WORD):
		base_radix = SE_PKA1_CTRL_BASE_4096;
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid op mode word count (%u)\n", words);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*baser_p = base_radix;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#define PKA1_MAX_BANK_BASE (PKA1_BANK_START_A + (PKA1_MAX_BANK * 0x400U))

/* Returns the bank base address for PKA1 banks A..D */
static status_t pka1_bank_start(pka1_bank_t bank, uint32_t *bank_start)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (bank > PKA1_MAX_BANK) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
	*bank_start = PKA1_BANK_START_A + (bank * 0x400U);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

// FIXME => probably I got confused or this only applies to RSA?
//
// Disabled trapping registers which may not exist in a given bank, because
// EC operations always write e.g. key to D7 and a to A6 also when doing P521 operations
//
#define KNOW_LAST_REG 0

#if KNOW_LAST_REG
/* If the last bank register is known by operation size, verify that
 * only existing registers from each bank get used.
 *
 * TODO: It is unclear if the last regs only apply to RSA operations
 * or others as well? Rgeression tests pass with this check and
 * without, but do not compile until verified.
 *
 * CRT mode is not supported by CCC
 * [ For CRT mode: last_reg = R3 ]
 */
static status_t check_bank_reg_known_lt_4096(enum pka1_op_mode op_mode,
					     pka1_bank_t bank, pka1_reg_id_t reg)
{
	status_t ret = NO_ERROR;
	pka1_reg_id_t last_reg = R1;

	LTRACEF("entry\n");

	(void)op_mode;

	if (bank == BANK_D) {
		/* For CRT mode: last_reg = R7 */
		last_reg = R3;
	}

	if (reg > last_reg) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA1 op_mode %u bank %u reg %u is undefined\n",
						       op_mode, bank, reg));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t check_bank_reg_known_eq_4096(enum pka1_op_mode op_mode,
					     pka1_bank_t bank, pka1_reg_id_t reg)
{
	status_t ret = NO_ERROR;
	pka1_reg_id_t last_reg = R1;

	LTRACEF("entry\n");

	(void)op_mode;

	if (bank == BANK_D) {
		last_reg = R3;
	}

	if (reg > last_reg) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA1 op_mode %u bank %u reg %u is undefined\n",
						       op_mode, bank, reg));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* KNOW_LAST_REG */

static status_t pka1_bank_reg_off(pka1_bank_t bank, pka1_reg_id_t reg,
				  uint32_t words, uint32_t *reg_off)
{
	status_t ret = NO_ERROR;
	uint32_t bank_start = 0U;
	uint32_t roff = 0U;

	LTRACEF("entry\n");

	ret = pka1_bank_start(bank, &bank_start);
	CCC_ERROR_CHECK(ret);

	if ((reg > PKA1_MAX_REG_VALUE) ||
	    (words > MAX_PKA1_OPERAND_WORD_SIZE) ||
	    (bank_start > PKA1_MAX_BANK_BASE)) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG);
	}

	roff = bank_start + ((reg * 4U) * words);
	*reg_off = roff;
fail:
	LTRACEF("exit (reg_off=0x%x): %d\n", roff, ret);
	return ret;
}

static status_t pka1_reg_offset(enum pka1_op_mode op_mode, pka1_bank_t bank, pka1_reg_id_t reg,
				uint32_t *reg_off, uint32_t *mode_words_p)
{
	status_t ret = NO_ERROR;
	uint32_t words = 0U;

	LTRACEF("entry\n");

	if (NULL == reg_off) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*reg_off = 0U;

	ret = pka1_num_words(op_mode, &words);
	CCC_ERROR_CHECK(ret);

	switch (words) {
	case (256U / BITS_IN_WORD):
	case (512U / BITS_IN_WORD):
		/* All regs R0..R7 exist in all banks */
		break;
	case (1024U / BITS_IN_WORD):
	case (2048U / BITS_IN_WORD):
#if KNOW_LAST_REG
		ret = check_bank_reg_known_lt_4096(op_mode, bank, reg);
#endif
		break;
	case (4096U / BITS_IN_WORD):
		/* Regs BANK_A(R0..R1), BANK_B(R0..R1), BANK_C(R0..R1), BANK_D(RD0..RD3)
		 * exist (in all cases).
		 */
#if KNOW_LAST_REG
		ret = check_bank_reg_known_eq_4096(op_mode, bank, reg);
#endif
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid op mode word count (%u)\n", words);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = pka1_bank_reg_off(bank, reg, words, reg_off);
	CCC_ERROR_CHECK(ret);

	if (NULL != mode_words_p) {
		*mode_words_p = words;
	}
fail:
	LTRACEF("exit: %d (words %u)\n", ret, words);
	return ret;
}

/*
 * PKA1 hw requires power of 2 values for the operation mode sizes
 * colwerted to number of words.
 */
status_t pka1_num_words(enum pka1_op_mode op_mode, uint32_t *words_p)
{
	status_t ret = NO_ERROR;
	uint32_t words = 0U;

	LTRACEF("entry\n");
	if (NULL == words_p) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*words_p = 0U;

	switch (op_mode) {
	case PKA1_OP_MODE_ECC160:
	case PKA1_OP_MODE_ECC192:
	case PKA1_OP_MODE_ECC224:
	case PKA1_OP_MODE_ECC256:
		words = 256U / 32U;	// 8 words
		break;
	case PKA1_OP_MODE_RSA512:
	case PKA1_OP_MODE_ECC320:
	case PKA1_OP_MODE_ECC384:
	case PKA1_OP_MODE_ECC512:
		words = 512U / 32U;	// 16 words
		break;
	case PKA1_OP_MODE_RSA768:
	case PKA1_OP_MODE_RSA1024:
	case PKA1_OP_MODE_ECC521:
		words = 1024U / 32U;	// 32 words
		break;
	case PKA1_OP_MODE_RSA1536:
	case PKA1_OP_MODE_RSA2048:
		words = 2048U / 32U;	// 64 words
		break;
	case PKA1_OP_MODE_RSA3072:
	case PKA1_OP_MODE_RSA4096:
		words = 4096U / 32U;	// 128 words
		break;
	default:
		CCC_ERROR_MESSAGE("Invalid operation mode: %u\n", op_mode);
		ret = SE_ERROR(ERR_NOT_VALID);
		break;
	}
	CCC_ERROR_CHECK(ret);

	*words_p = words;
fail:
	LTRACEF("exit (words = %u): %d\n", words, ret);
	return ret;
}

static status_t bank_reg_rw_arg_check(const engine_t *engine,
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

static status_t pka1_bank_reg_zeropad(const engine_t *engine,
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

static status_t set_bank_reg_value(const engine_t *engine,
				   const uint8_t *data,
				   uint32_t reg_offset,
				   uint32_t nwords,
				   bool big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t len = word_count_to_bytes(nwords);
	uint32_t offset = 0U;
	uint32_t val32  = 0U;
	uint32_t bank_reg = reg_offset;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(big_endian)) {
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&data[offset - BYTES_IN_WORD], big_endian);
			tegra_engine_set_reg(engine, bank_reg, val32);
			CCC_SAFE_ADD_U32(bank_reg, bank_reg, BYTES_IN_WORD);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val32 = se_util_buf_to_u32(&data[offset], big_endian);
			tegra_engine_set_reg(engine, bank_reg, val32);
			CCC_SAFE_ADD_U32(bank_reg, bank_reg, BYTES_IN_WORD);
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

status_t pka1_bank_register_write(const struct engine_s *engine,
				  enum pka1_op_mode op_mode,
				  pka1_bank_t bank, pka1_reg_id_t breg,
				  const uint8_t *data, uint32_t nwords,
				  bool big_endian,
				  uint32_t field_word_size)
{
	status_t ret = NO_ERROR;
	uint32_t reg_base = 0U;
	uint32_t mode_words = 0U;
	uint32_t effective_field_word_size = field_word_size;

	LTRACEF("entry\n");

	ret = bank_reg_rw_arg_check(engine, data, nwords);
	CCC_ERROR_CHECK(ret);

	ret = pka1_reg_offset(op_mode, bank, breg, &reg_base, &mode_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("reg bank%u (%u) map failed: %d\n",
					  bank, breg, ret));

	/* Some cases require e.g. next power of two size to be zero padded in EC modes.
	 * But allow passing e.g. zero if the mode natural size is OK.
	 */
	if (effective_field_word_size < mode_words) {
		if (effective_field_word_size != 0U) {
			LTRACEF("register write field size %u adjusted to %u\n",
				effective_field_word_size, mode_words);
		}
		effective_field_word_size = mode_words;
	}

	ret = set_bank_reg_value(engine, data, reg_base,
				 nwords, big_endian);
	CCC_ERROR_CHECK(ret);

	/* Note: Value of above loop index i is used below!!!
	 */
	if (nwords < effective_field_word_size) {
		/* If not writing all breg words => clear the rest of the bank register
		 * This may be unnecessary (slow) for most values; however in some modes and
		 * some values this is mandatory (e.g. key values) => do it for all values for now.
		 * TODO: optimize!
		 *
		 * For example when writing public exponent (just one word) the rest of the
		 * exponent bank register might contain junk. It is most often ignored by PKA1.
		 */
		ret = pka1_bank_reg_zeropad(engine, reg_base,
					    nwords, effective_field_word_size);
		CCC_ERROR_CHECK(ret);
	}

	LTRACEF("%u words written to PKA bank(reg) %c(%u) (caller order: %s)\n",
		nwords, ((uint32_t)'A' + bank), breg, (BOOL_IS_TRUE(big_endian) ? "BE" : "LE"));
	LOG_HEXBUF("Written data (in caller order)", data, nwords*4U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t pka1_bank_register_read(const struct engine_s *engine,
				 enum pka1_op_mode op_mode,
				 pka1_bank_t bank, pka1_reg_id_t breg,
				 uint8_t *data, uint32_t nwords, bool big_endian)
{
	status_t ret = NO_ERROR;
	uint32_t reg_base = 0U;
	uint32_t val = 0U;
	uint32_t offset = 0U;
	uint32_t len = word_count_to_bytes(nwords);
	uint32_t reg_offset = 0U;

	LTRACEF("entry\n");

	ret = bank_reg_rw_arg_check(engine, data, nwords);
	CCC_ERROR_CHECK(ret);

	ret = pka1_reg_offset(op_mode, bank, breg, &reg_base, NULL);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("reg bank%u (%u) map failed: %d\n",
					  bank, breg, ret));

	reg_offset = reg_base;

	if (BOOL_IS_TRUE(big_endian)) {
		for (offset = len; offset >= BYTES_IN_WORD; offset -= BYTES_IN_WORD) {
			val = tegra_engine_get_reg(engine, reg_offset);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
			se_util_u32_to_buf(val, &data[(offset - BYTES_IN_WORD)], big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset >= BYTES_IN_WORD) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	} else {
		for (offset = 0U; offset < len; offset += BYTES_IN_WORD) {
			val = tegra_engine_get_reg(engine, reg_offset);
			CCC_SAFE_ADD_U32(reg_offset, reg_offset, BYTES_IN_WORD);
			se_util_u32_to_buf(val, &data[offset], big_endian);
		}

		FI_BARRIER(u32, offset);

		if (offset != len) {
			CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
		}
	}

	LTRACEF("%u words read from PKA bank(reg) %c(%u) (caller order: %s)\n",
		nwords, ((uint32_t)'A' + bank), breg, (BOOL_IS_TRUE(big_endian) ? "BE" : "LE"));
	LOG_HEXBUF("Returned data (caller order)", data, nwords*4U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_RESET

/* Set this if the device needs to be reset before next mutex unlock
 */
static bool pka1_device_reset_pending;

static void pka1_device_reset(const engine_t *engine, bool *reset_pending_p)
{
	uint32_t val = 0U;
	uint32_t i = 0U;
	bool done = false;

	LTRACEF("entry\n");

	CCC_ERROR_MESSAGE("PKA1 device reset\n");
	*reset_pending_p = false;

	val = SE_PKA1_CTRL_RESET_SE_SOFT_RESET(SE_ELP_ENABLE);
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_RESET_OFFSET, val);

	do {
		val = tegra_engine_get_reg_NOTRACE(engine, SE_PKA1_CTRL_RESET_OFFSET);
		done = ((val & SE_PKA1_CTRL_RESET_SE_SOFT_RESET(SE_ELP_ENABLE)) == 0U);

		i++;

		if (i > SE_PKA1_TIMEOUT) {
			CCC_ERROR_MESSAGE("PKA1 reset wait completion timed out (%u) \n", SE_PKA1_TIMEOUT);
			done = true;
		}
	} while (!BOOL_IS_TRUE(done));

	LTRACEF("exit\n");
}
#endif /* HAVE_PKA1_RESET */

static void restart_pka1_mutex_wdt(const engine_t *engine)
{
	LTRACEF("entry\n");
	tegra_engine_set_reg(engine, SE_PKA1_MUTEX_WATCHDOG_OFFSET,
			     SE_PKA1_MUTEX_WDT_UNITS);
	LTRACEF("exit\n");
}

static status_t pka1_control_setup_unit(const engine_t *engine)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Make sure PKA is not swapping byte order internally */
	tegra_engine_set_reg(engine, SE_PKA1_PKA_CONFIGURATION_OFFSET, 0U);

#if HAVE_JUMP_PROBABILITY
	{
		/* XXXX For now just set fixed 0x7FF which is about 25% probability
		 * What ever else it is => must not be left zero.
		 *
		 * TODO => Does this need some randomizing?
		 */
		uint32_t val = 0x7FFU;

		tegra_engine_set_reg(engine, SE_PKA1_PKA_JUMP_PROBABILITY, val);
	}
#endif

	/* Reset the bank registers; Just in case someone is trying be clever */
	tegra_engine_set_reg(engine, SE_PKA1_BANK_SWITCH_A_OFFSET, 0U);
	tegra_engine_set_reg(engine, SE_PKA1_BANK_SWITCH_B_OFFSET, 0U);
	tegra_engine_set_reg(engine, SE_PKA1_BANK_SWITCH_C_OFFSET, 0U);
	tegra_engine_set_reg(engine, SE_PKA1_BANK_SWITCH_D_OFFSET, 0U);

	{
		uint32_t val = 0U;

		/* Setup the SE_PKA1_CTRL_CONTROL_0 register to default value
		 * (TRNG autoreseed; clear all other bits)
		 */
#if HAVE_PKA1_SCC_DPA
		/* Never allow AUTO_RESEED to be clear when SCC DPA is enabled
		 */
		val |= SE_PKA1_CTRL_CONTROL_AUTO_RESEED(SE_ELP_ENABLE);
#endif
		tegra_engine_set_reg(engine, SE_PKA1_CTRL_CONTROL_OFFSET, val);
	}

#if HAVE_PKA1_SCC_DPA
	/* Enable SCC-DPA; requires AUTO_RESEED for TRNG or PKA will not work
	 *
	 * TODO: Not sure if this can be enabled early during system boot.
	 * If not define HAVE_PKA1_SCC_DPA zero.
	 *
	 */
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_SCC_CONTROL_OFFSET,
			     SE_PKA1_CTRL_SCC_CONTROL(SE_PKA1_CTRL_SCC_CONTROL_ENABLE));
#else
	// XXXX FIXME CHECK => should PKA1 SCC DPA be DISABLED if not explicitly enabled?
	/* XXXXXX FIXME: Not sure, but I will DISABLE SCC unless enabled by config!!! */
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_SCC_CONTROL_OFFSET,
			     SE_PKA1_CTRL_SCC_CONTROL(SE_PKA1_CTRL_SCC_CONTROL_DISABLE));
#endif

#if HAVE_PKA1_TRNG
	ret = pka1_trng_setup_locked(engine);
	if (NO_ERROR != ret) {
		CCC_ERROR_MESSAGE("PKA1 trng init failed\n");
	}
#endif

	LTRACEF("exit\n");
	return ret;
}

status_t pka1_acquire_mutex(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	/* Acquire pka mutex */
	do {
		if (i > SE_PKA1_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire PKA1 Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
			break;
		}

		/* It is not possible to trace this register get (may
		 * loop a lot of times)
		 */
		val = tegra_engine_get_reg_NOTRACE(engine, SE_PKA1_MUTEX_OFFSET);

#if TEGRA_UDELAY_VALUE_PKA1_MUTEX > 0
		if (val != 0x01U) {
			TEGRA_UDELAY(TEGRA_UDELAY_VALUE_PKA1_MUTEX);
		}
#endif

		i++;
	} while (val != 0x01U);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("PKA1 mutex lock failed: %d\n", ret));

	/// XXXXXX READ IAS !!!! FIXME ==> is this OK?
	// Watchdog (probably traps) if the mutex is not released in time.
	// does it need clearing or is it auto-cleared when mutex released?

	/* One unit is 256 SE Cycles */
	restart_pka1_mutex_wdt(engine);
	tegra_engine_set_reg(engine, SE_PKA1_MUTEX_TIMEOUT_ACTION_OFFSET,
			     SE_PKA1_MUTEX_TIMEOUT_ACTION);

	ret = pka1_control_setup_unit(engine);
	if (NO_ERROR != ret) {
		CCC_ERROR_MESSAGE("PKA1 setup failed: %d\n", ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void pka1_release_mutex(const struct engine_s *engine)
{
	LTRACEF("entry\n");

#if HAVE_PKA1_RESET
	if (BOOL_IS_TRUE(pka1_device_reset_pending)) {
		pka1_device_reset(engine, &pka1_device_reset_pending);
	}
#endif /* HAVE_PKA1_RESET */

	tegra_engine_set_reg(engine, SE_PKA1_MUTEX_OFFSET, 0x01U);
	LTRACEF("exit\n");
}

#if HAVE_PKA1_USES_INTERRUPTS == 0

static status_t poll_op_done(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t count = 0U;

	LTRACEF("entry\n");

#if TEGRA_UDELAY_VALUE_PKA1_OP_INITIAL > 0
	TEGRA_UDELAY(TEGRA_UDELAY_VALUE_PKA1_OP_INITIAL);
#endif

	do {
		if (count > SE_PKA1_TIMEOUT) {
			CCC_ERROR_MESSAGE("PKA1 Done status timed out (%u): reset pending\n", SE_PKA1_TIMEOUT);
#if HAVE_PKA1_RESET
			pka1_device_reset_pending = true;
#endif /* HAVE_PKA1_RESET */
			ret = SE_ERROR(ERR_TIMED_OUT);
			break;
		}

		val = tegra_engine_get_reg_NOTRACE(engine, SE_PKA1_CTRL_STATUS_OFFSET);

#if TEGRA_UDELAY_VALUE_PKA1_OP > 0
		if ((val & SE_PKA1_CTRL_SE_STATUS(SE_ELP_ENABLE)) != 0U) {
			TEGRA_UDELAY(TEGRA_UDELAY_VALUE_PKA1_OP);
		}
#endif
		count++;
	} while ((val & SE_PKA1_CTRL_SE_STATUS(SE_ELP_ENABLE)) != 0U);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d, count=%u (status=0x%x)\n", ret, count, val);
	return ret;
}
#endif /* HAVE_PKA1_USES_INTERRUPTS == 0 */

status_t pka1_wait_op_done(const struct engine_s *engine)
{
	status_t ret = NO_ERROR;
	uint32_t val = 0U;
	uint32_t icount = 0U;
	uint32_t abnormal_mask = 0U;

	LTRACEF("entry\n");

#if HAVE_PKA1_USES_INTERRUPTS
	/* Step 7 */

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

	/* Mask PKA1 and TRNG interrupts after the interrupt has been received.
	 */
	tegra_engine_set_reg(engine,
			     SE_PKA1_CTRL_SE_INTR_MASK_OFFSET,
			     (SE_PKA1_CTRL_SE_INTR_MASK_EIP1_TRNG(SE_ELP_DISABLE) |
			      SE_PKA1_CTRL_SE_INTR_MASK_EIP0_PKA(SE_ELP_DISABLE)));

	CCC_ERROR_CHECK(ret);
#else
	/* Step 7 */
	ret = poll_op_done(engine);
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_PKA1_USES_INTERRUPTS */

	/* Check that IRQ_STAT set */
	do {
		if (icount > 10U) {
			CCC_ERROR_MESSAGE("PKA1 interrupt not set after done\n");
#if HAVE_PKA1_RESET
			pka1_device_reset_pending = true;
#endif /* HAVE_PKA1_RESET */
			break;
		}

		// It is not possible to trace this register get (may loop a lot of times)
		val = tegra_engine_get_reg_NOTRACE(engine, SE_PKA1_STATUS_OFFSET);

#if TEGRA_UDELAY_VALUE_PKA1_OP > 0
		if ((val & SE_PKA1_STATUS_IRQ_STAT(SE_ELP_ENABLE)) == 0U) {
			TEGRA_UDELAY(TEGRA_UDELAY_VALUE_PKA1_OP);
		}
#endif
		icount++;
	} while ((val & SE_PKA1_STATUS_IRQ_STAT(SE_ELP_ENABLE)) == 0U);
	/* Above loop does not set error */

	val = tegra_engine_get_reg(engine, SE_PKA1_RETURN_CODE_OFFSET);

	abnormal_mask = SE_PKA1_RETURN_CODE_STOP_REASON(
		SE_PKA1_RETURN_CODE_STOP_REASON_ABNORMAL);

	if ((val & abnormal_mask) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA1 Operation ended Abnormally: 0x%x\n",
						       val));
	}

#if SE_RD_DEBUG && LOCAL_TRACE
	{
		/* If debugging and tracing show instructions and cycles
		 * since GO_START (Would be an accurate perf measure;
		 * but unit scaling unknown...)
		 *
		 * XXX Sincef unit freq is known => could callwlate
		 * time used by op (TODO)
		 */
		uint32_t pka1_cycles =
			tegra_engine_get_reg(engine,
					     SE_PKA1_CYCLES_SINCE_GO_OFFSET);
		uint32_t pka1_insns =
			tegra_engine_get_reg(engine,
					     SE_PKA1_INSTR_SINCE_GO_OFFSET);
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION)
		pka1_entry_perf_cycles = pka1_cycles;
		pka1_entry_perf_insns  = pka1_insns;
#else
		LTRACEF("PKA1: cycles %u, instruction %u since GO\n",
			pka1_cycles, pka1_insns);
#endif
	}
#endif /* SE_RD_DEBUG */

fail:

	/* Write Status Register to acknowledge interrupt */
	/* Step 8 */
	val = tegra_engine_get_reg(engine, SE_PKA1_STATUS_OFFSET);
	tegra_engine_set_reg(engine, SE_PKA1_STATUS_OFFSET, val);

	/* Clear any possible KEYSLOT and LOAD_KEY values from
	 * SE_PKA1_CTRL_CONTROL_OFFSET.
	 *
	 * This register appears to be persistent across PKA1 MUTEX locks etc.
	 *
	 * Leave AUTORESEED on.
	 */
	val = 0U;
#if HAVE_PKA1_SCC_DPA
	/* Never allow AUTO_RESEED to be clear when SCC DPA is enabled
	 */
	val |= SE_PKA1_CTRL_CONTROL_AUTO_RESEED(SE_ELP_ENABLE);
#endif
	tegra_engine_set_reg(engine, SE_PKA1_CTRL_CONTROL_OFFSET, val);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_PKA1_OPERATION)
	if (0U != gen_op_perf_usec) {
		show_pka1_gen_op_entry_perf(gen_op_perf_entrypoint,
					    get_elapsed_usec(gen_op_perf_usec));
	}
	gen_op_perf_entrypoint = PKA1_ENTRY_ILWALID;
	gen_op_perf_usec = 0U;
	pka1_entry_perf_cycles = 0U;
	pka1_entry_perf_insns  = 0U;
#endif

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/*
 * After montgomery op completes OK, read result bank register
 * to DST with this.
 */
static status_t pka1_montgomery_result_get(const engine_t *engine,
					   enum pka1_montgomery_setup_ops op,
					   uint32_t nwords,
					   struct pka1_engine_data *pka1_data,
					   uint8_t *dst)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (op) {
	case CALC_R_ILW:
		/* Save R_ILW from C0 (as PKA1 overwrites
		 * the C0 reg in M_PRIME calc)
		 */
		ret = pka1_bank_register_read(engine, pka1_data->op_mode,
					      BANK_C, R0,
					      dst,
					      nwords, false);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("R_ILW read from reg bank (C0) failed: %d\n", ret);
			break;
		}
		break;

	case CALC_M_PRIME:
		/* Save M_PRIME from D1 to context */

		ret = pka1_bank_register_read(engine, pka1_data->op_mode,
					      BANK_D, R1,
					      dst,
					      nwords, false);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("M_PRIME read from reg bank (D1) failed: %d\n", ret);
			break;
		}
		pka1_data->mont_flags |= PKA1_MONT_FLAG_M_PRIME_LITTLE_ENDIAN;
		break;

	case CALC_R_SQR:
		/* Save R_SQR from D3 to context */

		ret = pka1_bank_register_read(engine, pka1_data->op_mode,
					      BANK_D, R3,
					      dst,
					      nwords, false);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("R2 read from reg bank (D3) failed: %d\n", ret);
			break;
		}
		pka1_data->mont_flags |= PKA1_MONT_FLAG_R2_LITTLE_ENDIAN;
		break;

	default:
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Callwlate Montgomery colwersion values with PKA1 engine.
 *
 * If MODULUS is NULL => do not write modulus (it is preserved by most operations
 * and this implies it is already loaded).
 *
 * NOTE: This function is only called from
 * pka1_montgomery_values_calc() in a locked context.  The R_ILW value
 * is left to to PKA1 C0 register bank after callwlating it for the
 * next calls in sequence (callwlate M_PRIME && R_SQR). Because
 * M_PRIME callwlation destroys C0 the R_ILW is then copied back to
 * the C0 register bank before doing R_SQR
 *
 * => only call this function in the correct sequence order!
 */
static status_t pka1_montgomery_arg_check(const engine_t *engine,
					  uint32_t m_len,
					  const struct pka1_engine_data *pka1_data,
					  const uint8_t *r_ilw,
					  const uint8_t *dst)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Used for EC lwrve and RSA modulus lengths */
	if ((NULL == engine) || (NULL == pka1_data) || (NULL == r_ilw) ||
	    (NULL == dst) ||
	    !(IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(m_len) ||
	      IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(m_len))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_mont_load_modulus(const engine_t *engine,
				       const struct pka1_engine_data *pka1_data,
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
				     LTRACEF("Zero modulus for montgomery operations\n"));
	}

	if (NO_ERROR != rbad) {
		CCC_ERROR_WITH_ECODE(rbad);
	}

	/* Load MODULUS to D0 (common in all Montgomery ops below)
	 * D0 register is preserved across Montgomery operations.
	 */
	ret = pka1_bank_register_write(engine, pka1_data->op_mode,
				       BANK_D, R0,
				       modulus, nwords, m_BE, 0U);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write modulus to D0: %d\n",
					  ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_MONTGOMERY_SINGLE)
static void show_mont_perf(enum pka1_montgomery_setup_ops op,
			   uint3_t perf_usec)
{
	if (CALC_R_ILW == op) {
		show_elapsed_time("PKA1 montgomery R_ILW", perf_usec);
	} else if (CALC_M_PRIME == op) {
		show_elapsed_time("PKA1 montgomery M_PRIME", perf_usec);
	} else if (CALC_R_SQR == op) {
		show_elapsed_time("PKA1 montgomery R_SQR", perf_usec);
	} else {
		/* Unreachable branch, not compiled in production */
	}
}
#endif

static status_t montgomery_value_gen(const engine_t *engine,
				     enum pka1_montgomery_setup_ops op,
				     uint32_t m_len,
				     const uint8_t *modulus,
				     struct pka1_engine_data *pka1_data,
				     bool m_BE,
				     const uint8_t *r_ilw,
				     uint8_t *dst)
{
	status_t ret = NO_ERROR;
	pka1_entrypoint_t entrypoint = PKA1_ENTRY_ILWALID;
	uint32_t nwords = m_len / BYTES_IN_WORD;

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_MONTGOMERY_SINGLE)
	uint32_t perf_usec = GET_USEC_TIMESTAMP;
#endif

	LTRACEF("entry\n");

	if (NULL != modulus) {
		ret = pka1_mont_load_modulus(engine, pka1_data, modulus,
					     m_len, m_BE);
		CCC_ERROR_CHECK(ret);
	}

	switch (op) {
	case CALC_R_ILW:
		LTRACEF("Montgomery R_ILW\n");

		/* R_ILW op */
		entrypoint = PKA1_ENTRY_MONT_RILW;

		/* R_ILW will be callwlated to C0 register. */
		break;

	case CALC_M_PRIME:
		LTRACEF("Montgomery M_PRIME\n");

		/* Load the R_ILW to C0.... No need to because the montgomery op
		 * sequence is R_ILW => M_PRIME => R2
		 *
		 * D0 (modulus) is preserved by all ops.
		 * C0 is already present from the R_ILW callwlation.
		 */

		/* M_PRIME op */
		entrypoint = PKA1_ENTRY_MONT_M;

		/* M_PRIME result will be in D1 */
		break;

	case CALC_R_SQR:
		LTRACEF("Montgomery R_SQR\n");

		/* Copy the R_ILW to C0. Because M_PRIME callwlation destroys C0 ! */

		ret = pka1_bank_register_write(engine, pka1_data->op_mode,
					       BANK_C, R0,
					       (const uint8_t *)r_ilw,
					       nwords, false, 0U);
		if (NO_ERROR != ret) {
			CCC_ERROR_MESSAGE("Failed to write R_ILW to C0: %d\n", ret);
			break;
		}

		/* M_PRIME already in D1 */

		/* R^2 op */
		entrypoint = PKA1_ENTRY_MONT_R2;

		/* R^2 result will be in D3 */
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		CCC_ERROR_MESSAGE("Unknown Montgomery calc op %u\n", op);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = pka1_go_start(engine, pka1_data->op_mode, m_len, entrypoint);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Montgomery calc op %u start failed %d (entry 0x%x)\n",
					  op, ret, entrypoint));

	ret = pka1_wait_op_done(engine);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Montgomery calc op %u returned %d\n",
					  op, ret));

	ret = pka1_montgomery_result_get(engine, op, nwords,
					 pka1_data, dst);
	CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_MONTGOMERY_SINGLE)
	show_mont_perf(op, perf_usec);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_montgomery_values_op(const engine_t *engine,
					  enum pka1_montgomery_setup_ops op,
					  uint32_t m_len,
					  const uint8_t *modulus,
					  struct pka1_engine_data *pka1_data,
					  bool modulus_big_endian,
					  uint8_t *r_ilw,
					  uint8_t *dst)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = pka1_montgomery_arg_check(engine, m_len, pka1_data, r_ilw, dst);
	CCC_ERROR_CHECK(ret);

	/* R_ILW is used for callwlating M_PRIME and R^2.
	 * It is a 512 byte buffer (for 4096 bit RSA).
	 */
	if (PKA1_OP_MODE_ECC521 != pka1_data->op_mode) {
		ret = montgomery_value_gen(engine, op, m_len, modulus, pka1_data,
					   modulus_big_endian, r_ilw, dst);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* This is called only for RSA and EC engine algorithms
 */
static status_t mont_values_calc_args_check(const engine_t *engine,
					    const struct pka1_engine_data *pka1_data,
					    uint32_t m_len)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL != pka1_data->mont_buffer) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("PKA1_DATA montgomery buffer already set\n"));
	}

	if (BOOL_IS_TRUE(CCC_ENGINE_CLASS_IS_RSA(engine->e_class))) {
		if (!IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(m_len)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("RSA modulus byte length (%u) incorrect\n",
						     m_len));
		}
	} else if (BOOL_IS_TRUE(CCC_ENGINE_CLASS_IS_EC(engine->e_class))) {
		if (!IS_SUPPORTED_EC_LWRVE_BYTE_LENGTH(m_len)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("EC modulus byte length (%u) incorrect\n",
						     m_len));
		}
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LTRACEF("Montgomery calculator called with engine class 0x%x\n",
					     engine->e_class));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t mont_calc_values(const engine_t *engine,
				 struct pka1_engine_data *pka1_data,
				 const uint8_t *modulus, uint32_t m_len,
				 bool modulus_big_endian,
				 uint8_t *m_prime_buf, uint8_t *r2_buf,
				 uint8_t *r_ilw)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = pka1_montgomery_values_op(engine, CALC_R_ILW,
					m_len, modulus,
					pka1_data,
					modulus_big_endian,
					&r_ilw[0], &r_ilw[0]);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Montgomery 1/R failed: %d\n",
					  ret));

	ret = pka1_montgomery_values_op(engine, CALC_M_PRIME,
					m_len, NULL,
					pka1_data,
					modulus_big_endian, &r_ilw[0],
					&m_prime_buf[0]);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Montgomery M' failed: %d\n",
					  ret));

	ret = pka1_montgomery_values_op(engine, CALC_R_SQR,
					m_len, NULL,
					pka1_data,
					modulus_big_endian, &r_ilw[0],
					&r2_buf[0]);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Montgomery R^2 failed: %d\n",
					  ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Callwlate the Montgomery colwersion values:
 *
 * r_ilw   = 1/r mod m			(Modular ilwerse of r)
 * m_prime =				(Montgomery residue constant m')
 *  r and m are coprime
 *  m * m' == -1 mod r
 *  r * r' + m * m' == 1 (Bezout's identity)
 *   where 0 < r' < m and 0 < m' < r
 * r_sqr   = (r * r) mod m		(Montgomery residue colwersion constant r^2 mod m)
 *
 * Value of r is internally deduced by the unit from the size of the operation.
 *
 * If MODULUS is NULL => do not write modulus (it is preserved by most operations
 * and this implies it is already loaded).
 *
 * Allocates the buffer for montgomery values and assigns the const pointers
 * to the callwlated values stored in the dynamic buffer.
 *
 * When crypto operation is done the buffer pka1_data->mont_buffer must
 * be released and set NULL by the caller!
 *
 * This buffer is not allocated if the montgomery values are provided
 * out-of-band by some meachnism: in this
 * case the montgomery const pointers point directly to the out-of-band
 * data objects. The mont_flags field contains information to the call chain
 * on how to handle the montgomery value pointers.
 */
status_t pka1_montgomery_values_calc(struct context_mem_s *cmem,
				     const struct engine_s *engine,
				     uint32_t m_len,
				     const uint8_t *modulus,
				     struct pka1_engine_data *pka1_data,
				     bool modulus_big_endian)
{
	status_t ret = NO_ERROR;
	uint8_t *mont_values = NULL;
	uint8_t *r_ilw = NULL;
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_MONTGOMERY_ALL)
	uint32_t perf_usec = GET_USEC_TIMESTAMP;
#endif

	LTRACEF("entry\n");

	if ((NULL == engine) || (NULL == pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = mont_values_calc_args_check(engine, pka1_data, m_len);
	CCC_ERROR_CHECK(ret);

	pka1_data->m_prime = NULL;
	pka1_data->r2 = NULL;

	/* Add Cert-C boundary check */
	if (m_len > MAX_RSA_BYTE_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_LEN);
	}

	/* This is released later by the callers before the context
	 * operations ends; it contains the M' and R2 callwlated
	 * in this call.
	 */
	mont_values = CMTAG_MEM_GET_BUFFER(cmem,
					   CMTAG_BUFFER,
					   0U,
					   (2U * m_len));
	if (NULL == mont_values) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	r_ilw = CMTAG_MEM_GET_BUFFER(cmem,
				     CMTAG_BUFFER,
				     0U,
				     m_len);
	if (NULL == r_ilw) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	ret = mont_calc_values(engine, pka1_data, modulus, m_len,
			       modulus_big_endian,
			       &mont_values[0],
			       &mont_values[m_len],
			       r_ilw);
	CCC_ERROR_CHECK(ret);

	/* Commit the montgomery values to PKA1_DATA object
	 *
	 * mont_buffer released by pka1_data_release()
	 */
	pka1_data->m_prime	= &mont_values[0];
	pka1_data->r2		= &mont_values[m_len];
	pka1_data->mont_buffer	= mont_values;
	mont_values = NULL;

	pka1_data->mont_flags |= PKA1_MONT_FLAG_VALUES_OK;

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_MONTGOMERY_ALL)
	show_elapsed_time("PKA1 montgomery callwlations", perf_usec);
#endif

fail:
	if (NULL != mont_values) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_BUFFER,
				  mont_values);
	}

	if (NULL != r_ilw) {
		CMTAG_MEM_RELEASE(cmem,
				  CMTAG_BUFFER,
				  r_ilw);
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Use out-of-band provided montgomery values.
 *
 * Callwlating these takes a long time so this is
 * significant in speeding up the PKA1 EC/RSA operations
 * if the EC/RSA modulus is known to be a constant for
 * the use case.
 *
 * For EC this is the case, for RSA this is not really the
 * case in general, since mont values depend on the modulus.
 *
 * But a special api is provided to use OOB values
 * to speed up the operations.
 */
status_t tegra_pka1_set_fixed_montgomery_values(struct pka1_engine_data *pka1_data,
						const uint8_t *m_prime,
						const uint8_t *r2,
						bool big_endian)
{
	status_t ret = NO_ERROR;

	if ((NULL == pka1_data) ||
	    (NULL == m_prime) ||
	    (NULL == r2)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	pka1_data->m_prime = m_prime;
	pka1_data->r2      = r2;

	/* TODO (change if required): montgomery values
	 * are now required to be of the same endian.
	 */
	if (!BOOL_IS_TRUE(big_endian)) {
		pka1_data->mont_flags |= PKA1_MONT_FLAG_M_PRIME_LITTLE_ENDIAN;
		pka1_data->mont_flags |= PKA1_MONT_FLAG_R2_LITTLE_ENDIAN;
	}

	/* Montgomery values set for this pka1_data object */
	pka1_data->mont_flags |= PKA1_MONT_FLAG_NO_RELEASE;
	pka1_data->mont_flags |= PKA1_MONT_FLAG_VALUES_OK;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

void pka1_data_release(struct context_mem_s *cmem, struct pka1_engine_data *pka1_data)
{
	if (NULL != pka1_data) {
		if ((pka1_data->mont_flags & PKA1_MONT_FLAG_NO_RELEASE) == 0U) {
			if (NULL != pka1_data->mont_buffer) {
#if SE_RD_DEBUG
				if (pka1_data->mont_buffer != pka1_data->m_prime) {
					CCC_ERROR_MESSAGE("XXX PKA1_DATA buffer inconsistent\n");
				}
#endif
				CMTAG_MEM_RELEASE(cmem,
						  CMTAG_BUFFER,
						  pka1_data->mont_buffer);
			}
		}

#if SE_RD_DEBUG
		if ((pka1_data->mont_flags & PKA1_MONT_FLAG_NO_RELEASE) != 0U) {
			if (NULL != pka1_data->mont_buffer) {
				CCC_ERROR_MESSAGE("XXX PKA1_DATA calc buffer exists, flags indicate it should not\n");
			}
		}
#endif

		se_util_mem_set((uint8_t *)pka1_data, 0U, sizeof_u32(struct pka1_engine_data));
	}
}

#if HAVE_RSA_MONTGOMERY_CALC
static status_t get_rsa_montgomery_values_check_args(const uint8_t *modulus,
						     const uint8_t *m_prime,
						     const uint8_t *r2,
						     uint32_t rsa_size_bytes,
						     bool is_big_endian)
{
	status_t ret = NO_ERROR;
	bool is_valid = true;

	LTRACEF("entry\n");

	if ((NULL == modulus) || (NULL == m_prime) || (NULL == r2)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* High order bit of modulus must be set as well */
	is_valid = (se_util_vec_is_msb_set(modulus,
					   rsa_size_bytes,
					   is_big_endian) &&
		    is_valid);

	/* modulus must be odd */
	is_valid = (se_util_vec_is_odd(modulus,
				       rsa_size_bytes,
				       is_big_endian) &&
		    is_valid);

	if (! BOOL_IS_TRUE(is_valid)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA modulus invalid\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Library function to callwlate montgomery values for the RSA modulus
 *
 * This function is NOT USED BY CCC, it is provided as support/R&D function
 * for external users to callwlate montgomery values for e.g.
 * loading those as constants or other out-of-band use.
 *
 * Thus, it is not thread safe when the MEMORY POOL is not usable (in this
 * case it uses a static buffer
 *
 * m_prime and r_sqr must be rsa_size_bytes long buffers and are overwritten
 * by the callwlated values. Byte order will be same as modulus byte order,
 * defined by is_big_endian.
 *
 * NOTE: This is NOT THREAD SAFE in case both of these are defined non-zero:
 *
 * HAVE_CONTEXT_MEMORY
 * FORCE_CONTEXT_MEMORY
 *
 * The combination prevents using the subsystem memory poool functions for
 * memory object management and this function requires one RSA size memory
 * area for callwlating the montgomery values. In this case a STATIC BUFFER
 * is used for holding MAX_RSA_BYTE_SIZE value.
 */
status_t se_get_rsa_montgomery_values(uint32_t rsa_size_bytes,
				      uint8_t *modulus,
				      bool is_big_endian,
				      uint8_t *m_prime,
				      uint8_t *r2)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	struct pka1_engine_data pka1_data =
		{ .mont_flags = PKA1_MONT_FLAG_NONE, };
	uint8_t *r_ilw = NULL;

	LTRACEF("entry\n");

	ret = get_rsa_montgomery_values_check_args(modulus, m_prime, r2,
						   rsa_size_bytes,
						   is_big_endian);
	CCC_ERROR_CHECK(ret);

	switch (rsa_size_bytes * 8U) {
	case  512U: pka1_data.op_mode = PKA1_OP_MODE_RSA512;  break;
	case  768U: pka1_data.op_mode = PKA1_OP_MODE_RSA768;  break;
	case 1024U: pka1_data.op_mode = PKA1_OP_MODE_RSA1024; break;
	case 1536U: pka1_data.op_mode = PKA1_OP_MODE_RSA1536; break;
	case 2048U: pka1_data.op_mode = PKA1_OP_MODE_RSA2048; break;
	case 3072U: pka1_data.op_mode = PKA1_OP_MODE_RSA3072; break;
	case 4096U: pka1_data.op_mode = PKA1_OP_MODE_RSA4096; break;
	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret, LTRACEF("Unsupported RSA size\n"));

	pka1_data.pka1_op = PKA1_OP_MODULAR_EXPONENTIATION;

	ret = ccc_select_device_engine(SE_CDEV_PKA1, &engine, CCC_ENGINE_CLASS_RSA);
	CCC_ERROR_CHECK(ret, LTRACEF("Can't find active PKA1 device\n"));

	{
#if HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY
		/* Adding this object to stack is not really possible.
		 *
		 * In most subsystem the stack is way too small to contain
		 * up to 512 byte objects.
		 *
		 * Since use of subsystem memory pool is prevented by these
		 * options, use static memory instead. This makes
		 * the function non-thread safe, but since CCC core does not
		 * use this in production it does not really matter.
		 */
		static uint8_t static_buf[MAX_RSA_BYTE_SIZE];
		r_ilw = &static_buf[0];
#else
		r_ilw = GET_ZEROED_MEMORY(MAX_RSA_BYTE_SIZE);
		if (NULL == r_ilw) {
			CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
		}
#endif /* HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY */

		/* Use the target buffers for mont calculator to reduce
		 * operation internal memory use to 1/3 of the earlier version.
		 *
		 * This does not access the buffer fields of pka1_data object.
		 */
		ret = mont_calc_values(&engine,
				       &pka1_data,
				       &modulus[0],
				       rsa_size_bytes,
				       is_big_endian,
				       &m_prime[0],
				       &r2[0],
				       &r_ilw[0]);
		CCC_ERROR_CHECK(ret,
				LTRACEF("Failed to callwlate montgomery values\n"));
	}

	if (BOOL_IS_TRUE(is_big_endian)) {
		/*
		 * Return vectors in same endian modulus was received in
		 */
		ret = se_util_reverse_list(&m_prime[0], &m_prime[0],
					   rsa_size_bytes);
		CCC_ERROR_CHECK(ret);

		ret = se_util_reverse_list(&r2[0], &r2[0],
					   rsa_size_bytes);
		CCC_ERROR_CHECK(ret);
	}
fail:
#if (HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY) == 0
	if (NULL != r_ilw) {
		RELEASE_MEMORY(r_ilw);
	}
#endif /* (HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY) == 0 */

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RSA_MONTGOMERY_CALC */
#endif /* HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG */
