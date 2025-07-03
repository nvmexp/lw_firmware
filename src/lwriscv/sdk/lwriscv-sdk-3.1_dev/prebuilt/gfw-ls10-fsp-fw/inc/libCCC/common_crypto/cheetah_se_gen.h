/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   tegra_se_gen.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2018
 *
 * @brief Generic values for CCC engines and contexts.
 *
 * Contains the object and other definitions for code using engine
 * operations.
 */
#ifndef INCLUDED_TEGRA_SE_GEN_H
#define INCLUDED_TEGRA_SE_GEN_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

/* Clears results from HW regs when it is read out (for security...) */
#define CLEAR_HW_RESULT	1 /**< Erase engine state before mutex release */

/** @brief Avoid compiler warnings on computers with 32 bit phys
 * addresses (sizeof_u32(PHYS_ADDR_T) == 4)
 */
#if PHYS_ADDR_LENGTH == 4
#define SE_GET_32_UPPER_PHYSADDR_BITS(physaddr) 0U
#elif PHYS_ADDR_LENGTH == 8
#define SE_GET_32_UPPER_PHYSADDR_BITS(physaddr) se_util_msw_u64((uint64_t)(physaddr))
#else
#error "Unsupported PHYS_ADDR_LENGTH setting"
#endif

#if !defined(IS_40_BIT_PHYSADDR)
/**
 * @def IS_40_BIT_PHYSADDR(physaddr)
 *
 * @brief Check if the physaddr fits in 40 bit address space.
 * This is a design limit for addresses programmed to the SE engine.
 *
 * Subsystem can override this if required.
 *
 * For example in systems using SMMU mapped IOVA addresses the "phys
 * addr from SMMU" may be longer than 40 bits (e.g. 41 bits).  This
 * IOVA address can not be masked at V2P time to be 40 bit long if it
 * is also used in some other contexts than SE addressing, e.g. like
 * for aperture masking. In such systems this macro may need to pass
 * 41 bit IOVA addresses as valid 40 bit addresses; CCC will silently
 * mask the address to a supported 40 bit address when programming it
 * to the SE HW in/out address registers.
 */
#if PHYS_ADDR_LENGTH == 4
#define IS_40_BIT_PHYSADDR(physaddr)	(CTRUE)
#elif PHYS_ADDR_LENGTH == 8
#define IS_40_BIT_PHYSADDR(physaddr)	(0ULL == ((physaddr) & 0xFFFFFF0000000000ULL))
#else
#error "Unsupported PHYS_ADDR_LENGTH setting"
#endif
#endif /* !defined(IS_40_BIT_PHYSADDR) */

#if SE_RD_DEBUG
/* @brief DEBUG_ASSERT if SE HW mutex not flagged locked when called.
 *
 * Allow this to be overridden (in case HW_MUTEX_TAKE_ENGINE,etc are redefined).
 * No-op in production code.
 */
#ifndef DEBUG_ASSERT_HW_MUTEX
#define DEBUG_ASSERT_HW_MUTEX					   \
        { if (0U == se_hw_mstate_get()) {			      \
		CCC_ERROR_MESSAGE("hw_mstate => mutex not locked\n"); \
		DEBUG_ASSERT(0U != se_hw_mstate_get());		   \
	     }							   \
	}
#endif
#else
/** @brief No-op in production code
 */
#define DEBUG_ASSERT_HW_MUTEX
#endif /* SE_RD_DEBUG */

#if SE_RD_DEBUG
/** @brief hw_mstate for mutex checks [Not relevant in production code]
 */
uint32_t se_hw_mstate_get(void);
#endif

/* @brief Generic engine HW error check macro.
 * @def CCC_ENGINE_ERR_CHECK(engine,str)
 *
 * Macro calls m_ccc_engine_err_check(engine, str, ret)
 * inline function which is defined after the se_engine_get_err_status()
 * prototype below.
 *
 * Implictly sets status_t type ret variable and calls the
 * CCC_ERROR_CHECK(ret) macro which will enter the fail label on any
 * error. All functions accessing HW in CCC are compliant with this.
 *
 * @param engine Engine for status check
 * @param str Fixed string passed to CCC_ERROR_MESSAGE if error
 */
#if USE_DO_WHILE_IN_ERROR_HANDLING_MACROS
#define CCC_ENGINE_ERR_CHECK(engine, str)				\
	do { ret = m_ccc_engine_err_check(engine, str, ret);		\
	     CCC_ERROR_CHECK(ret);					\
	} while(CFALSE)
#else
#define CCC_ENGINE_ERR_CHECK(engine, str)				\
	{ ret = m_ccc_engine_err_check(engine, str, ret);		\
	  CCC_ERROR_CHECK(ret);						\
	}
#endif /* USE_DO_WHILE_IN_ERROR_HANDLING_MACROS */

/**
 * @brief Check generic SE engine errors
 * @def SE_ERR_CHECK(engine, str)
 *
 * @param engine Engine for status check
 * @param str Fixed string passed to CCC_ERROR_MESSAGE if error
 */
#define SE_ERR_CHECK(engine, str) CCC_ENGINE_ERR_CHECK(engine, str)

#if HAVE_SE_AES || HAVE_SE_RANDOM
/**
 * @brief Check AES engine errors
 * @def AES_ERR_CHECK(engine, str)
 *
 * @param engine Engine for status check
 * @param str Fixed string passed to CCC_ERROR_MESSAGE if error
 */
#define AES_ERR_CHECK(engine, str) CCC_ENGINE_ERR_CHECK(engine, str)

status_t se_write_linear_counter(const engine_t *engine,
				 const uint32_t *cvalue);

status_t se_update_linear_counter(struct se_engine_aes_context *econtext,
				  uint32_t num_blocks);

#if HAVE_AES_COUNTER_OVERFLOW_CHECK
status_t do_aes_counter_overflow_check(const async_aes_ctx_t *ac);
#endif

status_t engine_aes_op_mode_write_linear_counter(
	const struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm);

#endif /* HAVE_SE_AES || HAVE_SE_RANDOM */

#if HAVE_SE_SHA
/**
 * @brief Check SHA engine errors
 * @def SHA_ERR_CHECK(engine, str)
 *
 * @param engine Engine for status check
 * @param str Fixed string passed to CCC_ERROR_MESSAGE if error
 */
#define SHA_ERR_CHECK(engine, str) CCC_ENGINE_ERR_CHECK(engine, str)
#endif

/**
 * @brief SE engine layer functions
 *
 * @ingroup engine_layer
 * @defgroup engine_common SE engine common functions
 */
/*@{*/

/**
 * @brief Start specific SE operation
 *
 * Start the given engine operation as programmed before
 * calling this function.
 *
 * The SE engine operations expect to have the LASTBUF=1
 * set with every completed "SE TASK".
 *
 * Because CCC passes all data chunks, including intermediate
 * data chunks, as a separate "SE TASK" => the LASTBUF=1 field value must always
 * be set in this function. (host1x may later do things differently)
 *
 * [Does not really matter, because HW enforces the LASTBUF=1
 *  for every APB operation.]
 *
 * As earlier requested by HW team, set the GSCID register to zero
 * just in case it is left nonzero. Leaving it nonzero may have
 * side-effects => see IAS for more information.
 *
 * @param engine Engine to start the operation at.
 * @param se_op_reg_preset Optional flags to pass to start operation.
 *  All legacy modes should pass 0U in se_op_reg_preset, but for new
 *  operations using AES-GCM engine in T23X additional flags may be
 *  required.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t tegra_start_se0_operation_locked(const engine_t *engine,
					  uint32_t se_op_reg_preset);
/**
 * @brief Check engine ERR_STATUS register, if nonzero, output error message
 *
 * @param engine Engine to check
 * @param str Text fragment to include in the error message
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_engine_get_err_status(const engine_t *engine, const char *str);

/* CCC_ENGINE_ERR_CHECK calls this inline function to check for HW errors
 * and this function needs the above prototype.
 */
static inline status_t m_ccc_engine_err_check(const engine_t *engine,
					      const char *str, status_t ret)
{
	status_t rt = se_engine_get_err_status(engine, str);
	return ((NO_ERROR == rt) ? ret : rt);
}

/**
 * @brief tight loop poll for engine IDLE status se_engine_max_timeout times
 *
 * @param engine engine for which the idle query is made
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_wait_engine_free(const engine_t *engine);

#if HAVE_SE_ASYNC
/**
 * @brief Check if engine is IDLE in an asynchronous use case
 *
 * If yes => *idle_p = true otherwise false.
 * If error => *idle_p value is invalid.
 *
 * This also PRESET the SE MUTEX WATCHDOG TIMER and start counting
 *  again each time this check for engine free is made. So the async
 *  operation will not trigger watchdog timer in case it is polled
 *  often enough.
 *
 * @param engine engine to check
 * @param is_idle_p ref var set true if engine free, false otherwise
 *
 * @return NO_ERROR on success, in that case *idle_p contains the engine
 *  free boolean value.
 *
 *  ERR_ILWALID_ARGS returned if the engine->id is not an SE engine,
 *  also any argument error returns ERR_ILWALID_ARGS.
 */
status_t se_async_check_engine_free(const engine_t *engine,
				    bool *is_idle_p);
#endif
/*@}*/

#if HAVE_SE_SHA
/**
 * @ingroup engine_sha_support
 */
/*@{*/

/**
 * @brief Copy SHA engine result from engine registers or from an aligned
 * memory buffer to econtext->tmphash buffer.
 *
 * When copying from SHA_RESULT_0 registers the result of SHA2 using
 * internally 64 bit long words need to be word swapped when copied as
 * 32 bit words (little endian 64 bit long words).  If so => set
 * swap_result_words true.  32 bit words are always mapped to big
 * endian byte order (engine operates in little endian).
 *
 * If aligned_memory_result is NULL => copy from SHA_RESULT_0
 * registers, otherwise just mem copy from the provided result buffer.
 * No byte/word swapping is required in this case, buffer results are
 * in correct byte order (big endian byte vector).
 *
 * The source value is cleared after copying when CLEAR_HW_RESULT is 1
 * (default setting for security reasons).
 *
 * @param econtext sha engine context to return the result to
 *
 * @param aligned_memory_result NULL or cache line aligned buffer where
 *  the engine has written the answer to (SHA_DST_MEMORY nonzero).
 *  Normal case is to pass NULL when reading SHA results from HASH_REGs.
 *
 * @param swap_result_words SHA-384/SHA-512 operate in 64 bit words instead
 *  of 32 bit words like the shorter digests. When reading results out of the
 *  HASH_REGs the words need to be swapped for the long SHA digests.
 *
 *  This swap must not be done if the output was written to memory,
 *  only when reading HASH_REGs.
 */
void se_sha_engine_process_read_result_locked(struct se_engine_sha_context *econtext,
					      uint8_t *aligned_memory_result,
					      bool swap_result_words);

/**
 * @brief Program the SHA HW engine size_left and msg_length registers
 * for the next SHA engine call.
 *
 * @param input_params Caller in/out data parameters.
 * @param econtext sha engine context used for the SHA operation.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_sha_engine_set_msg_sizes(const struct se_data_params *input_params,
				     const struct se_engine_sha_context *econtext);

/**
 * @brief Program the SHA HW engine address registers for the next SHA operation.
 *
 * Caller must make sure the data is contiguous in used memory (physical)
 * and that it meets engine requirements (e.g. that it is not larger than largest
 * allowed chunk size).
 *
 * @param input_params Caller in/out data parameters.
 * @param econtext sha engine context used for the SHA operation.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_write_sha_engine_inout_regs(const struct se_data_params *input_params,
					const struct se_engine_sha_context *econtext);

/**
 * @brief configure SHA HW engine TASK_CONFIG register and write intermediate
 * values back to engine registers if this is a continuation call.
 *
 * @param econtext sha engine context used for this SHA digest.
 *
 * @return NO_ERROR on success, error code otherwise.
 */
status_t se_set_sha_task_config(const struct se_engine_sha_context *econtext);
/*@}*/
#endif /* HAVE_SE_SHA */

#define PADDR_NULL ((PHYS_ADDR_T)(uintptr_t)0UL) /**< PADDR_T type zero address */

#if HAVE_SE_APERTURE
/**
 * @defgroup aperture_handling Aperture handling
 */
/*@{*/
/*@}*/

/**
 * @brief In case memory apertures need to be managed before accessing them by DMA.
 *
 * The actual aperture management function must be defined
 * by the subsystem with the name CCC_SET_SE_APERTURE
 * [ Prototype matches se_set_aperture prototype ]
 *
 * This is used for PSC environment.
 *
 * src_paddr or dst_paddr can be PADDR_NULL, in that case memory aperture
 * management for that address is skipped.
 *
 * @param src_paddr source phys address (in)
 * @param slen Memory aperture size from src_paddr
 * @param dst_paddr source phys address (out)
 * @param dlen Memory aperture size from dst_paddr
 *
 * @return NO_ERROR on success, error code otherwise.
 *
 * @ingroup aperture_handling
 */
/*@{*/
status_t se_set_aperture(PHYS_ADDR_T src_paddr, uint32_t slen,
			 PHYS_ADDR_T dst_paddr, uint32_t dlen);
/*@}*/
#endif /* HAVE_SE_APERTURE */

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
/**
 * @brief Output the elapsed time used.
 *
 * Optional compile time feature support function: output
 * the caller measured microseconds for the caller.
 *
 * @param oname Algorithm output prefix
 * @param elapsed elapsed time in uSec
 */
void show_se_gen_op_entry_perf(const char *oname, uint32_t elapsed);
#endif

#endif /* INCLUDED_TEGRA_SE_GEN_H */
