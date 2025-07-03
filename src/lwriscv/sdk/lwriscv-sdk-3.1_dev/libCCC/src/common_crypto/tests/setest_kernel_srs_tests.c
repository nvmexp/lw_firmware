/*
 * Copyright (c) 2016-2021, LWPU CORPORATION. All rights reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * NOTE:
 * This file only contains the tests relevant to T1XX platforms
 * (e.g., t19x and t18x). It does not relevant to T23X systems.
 */

#include <crypto_system_config.h>
#include <tests/setest_kernel_tests.h>

#include <include/ccc_lwrm_drf.h>
#include <tegra_cdev.h>
#if (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X))
#include <tegra_pka1.h>
#include <tegra_pka1_gen.h>
#endif /* (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X)) */
#include <tegra_rng1.h>
#include <arse_rng1.h>
#include <arse0.h>

#if KERNEL_TEST_MODE

#if !defined(USED_RSA_KEYSLOT)
#define USED_RSA_KEYSLOT 3
#endif

#if !defined(AES_KEYSLOT)
#define AES_KEYSLOT 2U
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#ifdef TEST_SEC_SAFE_REQ_VERIFY

__STATIC__ status_t tegra_device_check_register_range_clear(se_cdev_id_t device,
		engine_class_t eclass, uint32_t reg_start_offset, uint32_t num_words)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	uint32_t reg_val = 0U;
	uint32_t reg_offset;

	ret = ccc_select_device_engine(device, &engine, eclass);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u\n",
				device));

	/* Check HW registers cleaned up */
	HW_MUTEX_TAKE_ENGINE(engine);
	for (uint32_t inx = 0U; inx < num_words; inx++) {
		reg_offset = reg_start_offset + (BYTES_IN_WORD * inx);
		reg_val = tegra_engine_get_reg(engine, reg_offset);
		if (0U != reg_val) {
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
	}
	HW_MUTEX_RELEASE_ENGINE(engine);

	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Register(offset:#0x%x) is not cleaned up! reg val=0x%x, device=%u\n",
				reg_offset, reg_val, device));
fail:
	return ret;
}

#if HAVE_CMAC_AES
status_t tegra_se_aes_cmac_hw_reg_clear_verify(engine_id_t eid)
{
	status_t ret = NO_ERROR;

	/* Verify SE0_AESx_CMAC_RESULT_0 is cleared */
	if (CCC_ENGINE_SE0_AES0 == eid) {
		ret = tegra_device_check_register_range_clear(SE_CDEV_SE0,
				CCC_ENGINE_CLASS_AES, SE0_AES0_CMAC_RESULT_0,
				SE_AES_BLOCK_LENGTH / BYTES_IN_WORD);
	} else {
		ret = tegra_device_check_register_range_clear(SE_CDEV_SE0,
				CCC_ENGINE_CLASS_AES, SE0_AES1_CMAC_RESULT_0,
				SE_AES_BLOCK_LENGTH / BYTES_IN_WORD);
	}
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("AES_CMAC_RESULT reg is not cleaned up!\n"));
fail:
	return ret;
}
#endif /* HAVE_CMAC_AES */

#ifdef TEST_DIGEST
status_t tegra_se_sha_hw_reg_clear_verify(uint32_t result_size)
{
	status_t ret = NO_ERROR;

	ret = tegra_device_check_register_range_clear(SE_CDEV_SE0,
			CCC_ENGINE_CLASS_SHA, SE0_SHA_HASH_RESULT_0,
			result_size / BYTES_IN_WORD);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("SHA_RESULT reg is not cleaned up!\n"));
fail:
	return ret;
}
#endif /* TEST_DIGEST */

__STATIC__ status_t TEST_se_aes_scc_config_verify(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	uint32_t se_config_reg = 0U;

	(void)c;
	(void)algo;
	(void)eid;

	ret = ccc_select_device_engine(SE_CDEV_SE0, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u\n",
				SE_CDEV_SE0));

	HW_MUTEX_TAKE_ENGINE(engine);
	se_config_reg = tegra_engine_get_reg(engine, SE0_AES0_CRYPTO_CONFIG_0);
	HW_MUTEX_RELEASE_ENGINE(engine);

#if HAVE_SE_SCC
	/* SE SCC ENABLED (disabled SCC_DIS) */
	if (BOOL_IS_TRUE(LW_FLD_TEST_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, SCC_DIS, DISABLE, se_config_reg))) {
		ret = NO_ERROR;
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				CCC_ERROR_MESSAGE("SE AES SCC is not enabled.\n"));
	}
#else
	/* SE SCC DISABLED (enabled SCC_DIS) */
	CCC_ERROR_MESSAGE("HAVE_SE_SCC is not configured. CCC security requirement is violated.\n");
	if (BOOL_IS_TRUE(LW_FLD_TEST_DRF_DEF(SE0_AES0, CRYPTO_CONFIG, SCC_DIS, ENABLE, se_config_reg))) {
		CCC_ERROR_MESSAGE("SE AES SCC is not enabled.\n");
	} else {
		CCC_ERROR_MESSAGE("SE AES SCC is enabled.\n");
	}
	ret = SE_ERROR(ERR_BAD_STATE);
#endif /* HAVE_SE_SCC */

fail:
	return ret;
}

#if (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X))
__STATIC__ status_t se_pka1_scc_dpa_verify(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t pka1_ctrl_scc_reg = 0U;

	LOG_ALWAYS("Verify PKA1 DPA SCC is enabled.\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	pka1_ctrl_scc_reg = tegra_engine_get_reg_NOTRACE(engine,
			SE_PKA1_CTRL_SCC_CONTROL_OFFSET);
	HW_MUTEX_RELEASE_ENGINE(engine);

	pka1_ctrl_scc_reg &= SE_PKA1_CTRL_SCC_CONTROL_MASK; // DPA field

#if HAVE_PKA1_SCC_DPA
	/* PKA1 SCC DPA ENABLED (disabled SCC_DPA) */
	if (SE_PKA1_CTRL_SCC_CONTROL_ENABLE == pka1_ctrl_scc_reg) {
		ret = NO_ERROR;
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				CCC_ERROR_MESSAGE("SE PKA1 SCC DPA is not enabled.\n"));
	}
#else
	/* PKA1 SCC DPA DISABLED (disabled SCC_DPA) */
	CCC_ERROR_MESSAGE("HAVE_PKA1_SCC_DPA is not configured. CCC security requirement is violated.\n");
	if (SE_PKA1_CTRL_SCC_CONTROL_DISABLE == pka1_ctrl_scc_reg) {
		CCC_ERROR_MESSAGE("SE PKA1 SCC DPA is not enabled.\n");
	} else {
		CCC_ERROR_MESSAGE("SE PKA1 SCC DPA is enabled.\n");
	}
	ret = SE_ERROR(ERR_BAD_STATE);
#endif /* HAVE_PKA1_SCC_DPA */
fail:
	return ret;
}

__STATIC__ status_t se_pka1_jump_prob_verify(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t pka1_ctrl_scc_reg = 0U;
	uint32_t pka1_jump_probability = 0x7FFU;

	LOG_ALWAYS("Verify PKA1 DPA SCC is enabled.\n");

	HW_MUTEX_TAKE_ENGINE(engine);
	pka1_ctrl_scc_reg = tegra_engine_get_reg_NOTRACE(engine,
			SE_PKA1_PKA_JUMP_PROBABILITY);
	HW_MUTEX_RELEASE_ENGINE(engine);

	pka1_ctrl_scc_reg &= SE_PKA1_PKA_JUMP_PROBABILITY_MASK;

	/**
	 * CCC current implementation set the PKA1 jump probability
	 * at function pka1_control_setup_unit() in tegra_pka1_gen.c
	 * with a constant value 0x7FFU.
	 */
	if (pka1_jump_probability == pka1_ctrl_scc_reg) {
		ret = NO_ERROR;
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
		    LOG_ERROR("PKA1 JUMP probability value %u does not match the setting 0x7ff.\n",
				pka1_ctrl_scc_reg));
	}
fail:
	return ret;
}

__STATIC__ status_t TEST_pka1_scc_config_verify(crypto_context_t *c, te_crypto_algo_t algo, engine_id_t eid)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;

	(void)c;
	(void)algo;
	(void)eid;

	LOG_ALWAYS("PKA1 side-channel countermeasure (SCCs) test.\n");

	ret = ccc_select_device_engine(SE_CDEV_PKA1, &engine, CCC_ENGINE_CLASS_EC);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Could not find active device %u.\n",
					  SE_CDEV_PKA1));
	/* Verify PKA1 SCC DPA */
	ret = se_pka1_scc_dpa_verify(engine);
	CCC_ERROR_CHECK(ret);

	/**
	 * PKA1 key blinding is a operation flag which is set by the
	 * PKA1 engine layer operation handler. This feature should be
	 * verified by code inspection.
	 */

	/* Verify Jump probability */
	ret = se_pka1_jump_prob_verify(engine);
	CCC_ERROR_CHECK(ret);
fail:
	return ret;
}

#if HAVE_MUTEX_LOCK_STATUS
#if HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA
#define SE0_MUTEX_LOCK_TIMEOUT		40000U	/* loop count to check mutex timeout */
/**
 * This test is to verify SE mutex timeout handling by acquiring SE mutex then
 * calling CCC api to test if CCC api handle mutex timeout correctly.
 */
static status_t TEST_se_mutex_timeout_handling(void)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	uint32_t val = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(SE_CDEV_SE0, &engine, CCC_ENGINE_CLASS_AES);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to select AES engine\n"));

	/* simulate other SW entity to acquire SE mutex */
	do {
		/* loop SE0_MUTEX_LOCK_TIMEOUT check for mutex lock timeout */
		if (i > SE0_MUTEX_LOCK_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire SE Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
		} else {
			/* It is not possible to trace this register get (may loop a lot of times) */
			val = tegra_engine_get_reg_NOTRACE(engine, SE0_MUTEX_REQUEST_RELEASE_0);
			val = LW_DRF_VAL(SE0_MUTEX, REQUEST_RELEASE, LOCK, val);

		if (SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE == val) {
			break;
		}
			i++;
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("SE mutex lock failed\n"));

	/* set aes keyslot to see if it handle mutex timeout correctly */
	ret = se_clear_device_aes_keyslot(SE_CDEV_SE0, AES_KEYSLOT);
	if (ERR_TIMED_OUT == ret) {
		/* released SE mutex which is acquired for testing */
		tegra_engine_set_reg(engine,
				     SE0_MUTEX_REQUEST_RELEASE_0,
				     SE0_MUTEX_REQUEST_RELEASE_0_LOCK_TRUE);
		LOG_ERROR("[ CCC handle SE mutex timeout successfully ]\n");
		ret = NO_ERROR; /* return NO_ERROR as CCC handle mutex timeout correctly */
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("SE timeout mutex handling fail...\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA */

#if HAVE_RNG1_DRNG
#include <tegra_rng1.h>
#include <arse_rng1.h>
/**
 * This test is to verify RNG1 mutex timeout handling by acquiring RNG1 mutex then
 * generating drng by calling CCC api to test if CCC api handle mutex timeout correctly.
 */
static status_t TEST_rng1_mutex_timeout_handling(void)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	uint32_t value = 0U;
	uint32_t val = 0U;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(SE_CDEV_RNG1, &engine, CCC_ENGINE_CLASS_DRNG);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to select RNG1 engine\n"));

	/* simulate other SW entity to acquire RNG1 mutex */
	do {
		/* loop SE_RNG1_TIMEOUT check for mutex lock timeout */
		if (i > SE_RNG1_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire RNG1 Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
		} else {
			/* It is not possible to trace this register get (may loop a lot of times) */
			val = tegra_engine_get_reg_NOTRACE(engine, SE_RNG1_RNG1_MUTEX_REQUEST_RELEASE_0);
			if ((0x01U & val) == 0U) {
				break;
			}
			i++;
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("RNG1 mutex lock failed\n"));

	/* generate DRNG to see if CCC handle mutex lock correctly */
	ret = rng1_generate_drng((uint8_t *)&value, sizeof_u32(value));
	if (NO_ERROR != ret) {
		/* released RNG1 mutex which is acquired for testing */
		tegra_engine_set_reg(engine, SE_RNG1_RNG1_MUTEX_REQUEST_RELEASE_0, 0x01U);
		LOG_ERROR("[ CCC handle RNG1 mutex timeout successfully ]\n");
		ret = NO_ERROR; /* return NO_ERROR as CCC handle mutex timeout correctly */
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("RNG1 timeout mutex handling fail...\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_RNG1_DRNG */

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG
/**
 * This test is to verify PKA1 mutex timeout handling by acquiring PKA1 mutex then
 * calling CCC api to test if CCC api handle mutex timeout correctly.
 */
static status_t TEST_pka1_mutex_timeout_handling(void)
{
	status_t ret = NO_ERROR;
	const engine_t *engine = NULL;
	uint32_t val = 0;
	uint32_t i = 0U;

	LTRACEF("entry\n");

	ret = ccc_select_device_engine(SE_CDEV_PKA1, &engine, CCC_ENGINE_CLASS_RSA);
	CCC_ERROR_CHECK(ret, LTRACEF("Can't find active PKA1 device\n"));

	/* Simulate other SW entity to acquire the mutex */
	do {
		/* loop SE_PKA1_TIMEOUT check for mutex lock timeout */
		if (i > SE_PKA1_TIMEOUT) {
			CCC_ERROR_MESSAGE("Acquire PKA1 Mutex timed out\n");
			ret = SE_ERROR(ERR_TIMED_OUT);
		} else {
			/* It is not possible to trace this register get (may loop a lot of times) */
			val = tegra_engine_get_reg_NOTRACE(engine, SE_PKA1_MUTEX_OFFSET);
			if (val == 0x01U) {
				break;
			}
			i++;
		}
	} while (NO_ERROR == ret);
	CCC_ERROR_CHECK(ret, CCC_ERROR_MESSAGE("PKA1 mutex lock failed\n"));

	/* Clear PKA1 keyslot to see if it handle mutex timeout correctly */
	ret = se_clear_device_rsa_keyslot(SE_CDEV_PKA1, USED_RSA_KEYSLOT);
	if (ERR_TIMED_OUT == ret) {
		/* released PKA1 mutex which is acquired for testing */
		tegra_engine_set_reg(engine, SE_PKA1_MUTEX_OFFSET, 0x01U);
		LOG_ERROR("[ CCC handle PKA1 mutex timeout successfully ]\n");
		ret = NO_ERROR; /* return NO_ERROR as CCC handle mutex timeout correctly */
	} else {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     LOG_ERROR("PKA1 mutex timeout handling fail...\n"));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if (defined(TEST_ECDSA) || defined(TEST_ECDSA_SIGN))
/**
 * The test is to verify that CCC returns an expected error code when
 * SE PKA1 operation timeout and issue an PKA1 SW reset to restore
 * PKA1 engine back to normal functionality.
 *
 * The test steps are as follows:
 * 1. Load invalid modulus (zero) to PKA1 register(D0)
 * 2. Cause the PKA1 engine stuck
 * 3. CCC is supposed to detect the PKA1 engine timeout and trigger PKA1 SW reset
 * 4. After PKA1 reset, run ECDSA tests to make sure PKA1 restore properly
 */
static status_t TEST_pka1_err_handling_reset(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;
	status_t rbad = NO_ERROR;
	const engine_t *engine = NULL;
	struct pka1_engine_data pka1_data = {
		.mont_flags = PKA1_MONT_FLAG_OUT_OF_BAND,
	};
	const uint8_t ilwalid_m[64U] = { 0U };
	CCC_LOOP_VAR;

	LOG_ALWAYS("\n[ Starting test %s for safety requirement ]\n",
			__func__);

	ret = ccc_select_device_engine(SE_CDEV_PKA1, &engine, CCC_ENGINE_CLASS_EC);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Can't find active PKA1 device\n"));

	pka1_data.op_mode = PKA1_OP_MODE_ECC512;
	pka1_data.pka1_op = PKA1_OP_EC_POINT_MULTIPLY;

	HW_MUTEX_TAKE_ENGINE(engine);
	/* Write D0 */
	ret = pka1_bank_register_write(engine, PKA1_OP_MODE_ECC512,
			BANK_D, R0, ilwalid_m, sizeof_u32(ilwalid_m)/BYTES_IN_WORD,
			false, 64U/BYTES_IN_WORD);
	if (NO_ERROR != ret) {
		HW_MUTEX_RELEASE_ENGINE(engine);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Write Bank D0 reg failed\n"));
	}

	CCC_LOOP_BEGIN {
		LTRACEF("Run pka1_montgomery_values_calc with invalid modulus to fail PKA1 ...\n");

		/* CMEM argument can not be NULL when FORCE_CONTEXT_MEMORY is set
		 *
		 * FIX: construct a CMEM buffer for the callee and pass that.
		 *
		 * I do not do that right now, just disable FORCE_CONTEXT_MEMORY now
		 * to run this test (that uses mempool for memory needs in the callee).
		 */
		rbad = pka1_montgomery_values_calc(NULL, engine,
						   sizeof_u32(ilwalid_m), NULL,
						   &pka1_data, false);
		CCC_ERROR_END_LOOP(ret);

		CCC_LOOP_STOP;
	} CCC_LOOP_END;
	HW_MUTEX_RELEASE_ENGINE(engine);

	if (ERR_TIMED_OUT != rbad) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_BLOCKED,
		    LOG_ERROR("Test case failed to hang PKA1 ...\n"));
	}
	LTRACEF("PKA1 hang as expected. PKA1 SW reset should have been kicked in!\n");

	/**
	 * PKA1 engine is supposed to being reset at this moment.
	 * Run PKA1 crypto tests to prove PKA1 reset has restored
	 * PKA1 engine functionality to normal.
	 */
	ret = run_ecdsa_sign_test_cases(crypto_ctx);
	CCC_ERROR_CHECK(ret);

	ret = run_ecdsa_test_cases(crypto_ctx);
	CCC_ERROR_CHECK(ret);

	LOG_ALWAYS("\n[ completed test %s for safety requirement ]\n",
			__func__);
fail:
	if (NO_ERROR != ret) {
		LOG_ERROR("[ ***ERROR: PKA1 error reset test FAILED! (err 0x%x) ]\n",
			  ret);
	}
	return ret;
}
#endif /* (defined(TEST_ECDSA) || defined(TEST_ECDSA_SIGN)) */
#endif /* HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG */
#endif /* HAVE_MUTEX_LOCK_STATUS */
#endif /* (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X)) */

status_t run_ccc_req_verify_tests(crypto_context_t *crypto_ctx)
{
	status_t ret = NO_ERROR;

	/* Integration test for AES Side-channel Countermeasures (SCCs) */
	TEST_ERRCHK(TEST_se_aes_scc_config_verify, CCC_ENGINE_SE0_AES0, TE_ALG_AES_ECB_NOPAD);

#if (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X))
	/* Integration test for PKA1 Side-channel Countermeasures (SCCs) */
	TEST_ERRCHK(TEST_pka1_scc_config_verify, CCC_ENGINE_PKA1_PKA, TE_ALG_ECDSA);

#if HAVE_MUTEX_LOCK_STATUS
#if HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA
	ret = TEST_se_mutex_timeout_handling();
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_SE_SHA || HAVE_SE_AES || HAVE_SE_RSA */

#if HAVE_RNG1_DRNG
	ret = TEST_rng1_mutex_timeout_handling();
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_RNG1_DRNG */

#if HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG
	ret = TEST_pka1_mutex_timeout_handling();
	CCC_ERROR_CHECK(ret);

#if (defined(TEST_ECDSA) || defined(TEST_ECDSA_SIGN))
	ret = TEST_pka1_err_handling_reset(crypto_ctx);
	CCC_ERROR_CHECK(ret);
#endif /* (defined(TEST_ECDSA) || defined(TEST_ECDSA_SIGN)) */

#endif /* HAVE_PKA1_RSA || HAVE_PKA1_ECC || HAVE_PKA1_MODULAR || HAVE_PKA1_TRNG */
#endif /* HAVE_MUTEX_LOCK_STATUS */
#endif /* (CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T19X) || CCC_SOC_FAMILY_IS(CCC_SOC_FAMILY_T18X)) */
fail:
	return ret;
}
#endif /* TEST_SEC_SAFE_REQ_VERIFY */
#endif /* KERNEL_TEST_MODE */
