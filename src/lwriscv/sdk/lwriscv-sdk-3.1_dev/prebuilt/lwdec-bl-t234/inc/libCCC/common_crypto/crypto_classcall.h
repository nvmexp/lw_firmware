/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   crypto_classcall.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2019
 *
 */
#ifndef INCLUDED_CRYPTO_CLASSCALL_H
#define INCLUDED_CRYPTO_CLASSCALL_H

#include <crypto_ops_intern.h>
/**
 * @ingroup context_layer_api
 * @defgroup class_caller Context layer macros calling class handlers
 */
/*@{*/

status_t context_call_class_init(struct crypto_context_s *ctx, te_args_init_t *args);
status_t context_call_class_update(const struct crypto_context_s *ctx,
				   struct run_state_s *run_state,
				   te_args_data_t *args);
status_t context_call_class_dofinal(const struct crypto_context_s *ctx,
				    struct run_state_s *run_state,
				    te_args_data_t *args,
				    te_args_init_t *init_args);
status_t context_call_class_reset(struct crypto_context_s *ctx);
status_t context_call_class_set_key(struct crypto_context_s *ctx, te_crypto_key_t *key,
				    const te_args_key_data_t *kargs);

/** @brief Macros calling the class handlers directly,
 * not via function pointers.
 *
 * All calls return status_t.
 */

/**
 * @def CONTEXT_CALL_CLASS_INIT(cx, cinit)
 *
 * @param cx crypto context object
 * @param cinit te_args_init_t object from client
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_INIT(cx, cinit)	\
	context_call_class_init(cx, cinit)

/**
 * @def CONTEXT_CALL_CLASS_SET_KEY(cx, ckey, ckdata)
 *
 * @param cx crypto context object
 * @param ckey te_crypto_key_t object
 * @param ckdata te_args_key_data_t object from client
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_SET_KEY(cx, ckey, ckdata)	\
	context_call_class_set_key(cx, ckey, ckdata)

/**
 * @def CONTEXT_CALL_CLASS_UPDATE(cx, crun_state, cdata)
 *
 * @param cx crypto context object
 * @param crun_state struct run_state_s object created in init
 * @param cdata te_args_data_t object from client
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define	CONTEXT_CALL_CLASS_UPDATE(cx, crun_state, cdata) \
	context_call_class_update(cx, crun_state, cdata)

/**
 * @def CONTEXT_CALL_CLASS_DOFINAL(cx, crun_state, cdata, cinit)
 *
 * @param cx crypto context object
 * @param crun_state struct run_state_s object created in init
 * @param cdata te_args_data_t object from client
 * @param cinit te_args_init_t object from client
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define	CONTEXT_CALL_CLASS_DOFINAL(cx, crun_state, cdata, cinit) \
	context_call_class_dofinal(cx, crun_state, cdata, cinit)

/**
 * @def CONTEXT_CALL_CLASS_RESET(cx)
 *
 * @param cx crypto context object
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_RESET(cx)		\
	context_call_class_reset(cx)

#if HAVE_SE_ASYNC

status_t context_call_class_async_check_state(const struct crypto_context_s *ctx,
					      const struct run_state_s *run_state,
					      te_args_init_t *init_args);
status_t context_call_class_async_update(const struct crypto_context_s *ctx,
					 struct run_state_s *run_state,
					 te_args_data_t *args,
					 bool start_operation);
status_t context_call_class_async_dofinal(const struct crypto_context_s *ctx,
					  struct run_state_s *run_state,
					  te_args_data_t *args,
					  te_args_init_t *init_args,
					  bool start_operation);

/**
 * @def CONTEXT_CALL_CLASS_ASYNC_CHECK_STATE(cx, crun_state, cinit)
 *
 * @param cx crypto context object
 * @param crun_state struct run_state_s object created in init
 * @param cinit te_args_init_t object from client
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_ASYNC_CHECK_STATE(cx, crun_state, cinit)	\
	context_call_class_async_check_state(cx, crun_state, cinit)

/**
 * @def CONTEXT_CALL_CLASS_ASYNC_UPDATE(cx, crun_state, cdata, cstart)
 *
 * @param cx crypto context object
 * @param crun_state struct run_state_s object created in init
 * @param cdata te_args_data_t object from client
 * @param cinit te_args_init_t object from client
 * @param cstart true if starting async update, false if completing it
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_ASYNC_UPDATE(cx, crun_state, cdata, cstart)	\
	context_call_class_async_update(cx, crun_state, cdata, cstart)

/**
 * @def CONTEXT_CALL_CLASS_ASYNC_DOFINAL(cx, crun_state, cdata, cinit, cstart)
 *
 * @param cx crypto context object
 * @param crun_state struct run_state_s object created in init
 * @param cdata te_args_data_t object from client
 * @param cinit te_args_init_t object from client
 * @param cstart true if starting async dofinal, false if completing it
 *
 * @return NO_ERROR on success error code otherwise.
 */
#define CONTEXT_CALL_CLASS_ASYNC_DOFINAL(cx, crun_state, cdata, cinit, cstart) \
	context_call_class_async_dofinal(cx, crun_state, cdata, cinit, cstart)

#endif /* HAVE_SE_ASYNC */
/*@}*/
#endif /* INCLUDED_CRYPTO_CLASSCALL_H */
