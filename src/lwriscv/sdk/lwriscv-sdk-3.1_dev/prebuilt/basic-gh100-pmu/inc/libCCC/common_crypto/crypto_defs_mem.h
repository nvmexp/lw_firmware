/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_DEFS_MEM_H
#define INCLUDED_CRYPTO_DEFS_MEM_H

#ifndef HAVE_CONTEXT_MEMORY
/* Support context memory, subsystem memory pool is used
 * if context memory not set for any memory object call.
 *
 * See FORCE_CONTEXT_MEMORY for disabling subsystem memory pool
 * in this case.
 */
#define HAVE_CONTEXT_MEMORY 0
#endif

#ifndef FORCE_CONTEXT_MEMORY
#define FORCE_CONTEXT_MEMORY 0
#endif

#ifndef HAVE_IMPLICIT_CONTEXT_MEMORY_USE
/* This can only be set on a single threaded exelwtion model where
 * every algorithm invocation is finalized before next algorithm
 * context is initialized, i.e. everything completes sequentially.
 *
 * This uses a single static page for the context memory of all CCC
 * ilwocations via CCC crypto API (memory pool is not used for CCC
 * core and caller does not need to provide a separate context memory
 * for CCC calls).
 *
 * HAVE_CONTEXT_MEMORY must be set as well.
 */
#define HAVE_IMPLICIT_CONTEXT_MEMORY_USE 0
#endif

#if HAVE_CONTEXT_MEMORY == 0
#if FORCE_CONTEXT_MEMORY
#error "HAVE_CONTEXT_MEMORY must be set for FORCE_CONTEXT_MEMORY"
#endif

#if HAVE_IMPLICIT_CONTEXT_MEMORY_USE
#error "HAVE_CONTEXT_MEMORY must be set for HAVE_IMPLICIT_CONTEXT_MEMORY_USE"
#endif
#endif /* HAVE_CONTEXT_MEMORY == 0 */

/* Backwards compatibility: separate DMA and KEY memories forced
 * on by subsystem macro setup.
 *
 * Also possible to define explicitly CCC_WITH_CONTEXT_KEY_MEMORY
 * and CCC_WITH_CONTEXT_DMA_MEMORY settings for require/ignore these.
 */
#if HAVE_CONTEXT_MEMORY
/* If the subsystem defined this => it means it wants to use
 * a separate memory for the KEY allocations.
 *
 * Mandate CMEM KEY buffer in this case as well.
 */
#ifdef GET_KEY_MEMORY
#ifndef CCC_WITH_CONTEXT_KEY_MEMORY
#define CCC_WITH_CONTEXT_KEY_MEMORY 1
#endif
#endif /* GET_KEY_MEMORY */

/* If the subsystem defined this => it means it wants to use
 * a separate memory for the DMA allocations.
 *
 * Mandate CMEM DMA buffer in this case as well.
 */
#ifdef GET_ALIGNED_DMA_MEMORY
#ifndef CCC_WITH_CONTEXT_DMA_MEMORY
#define CCC_WITH_CONTEXT_DMA_MEMORY 1
#endif
#endif /* GET_ALIGNED_DMA_MEMORY */

#ifndef CCC_WITH_CONTEXT_KEY_MEMORY
#define CCC_WITH_CONTEXT_KEY_MEMORY 0
#endif

#ifndef CCC_WITH_CONTEXT_DMA_MEMORY
#define CCC_WITH_CONTEXT_DMA_MEMORY 0
#endif

#endif /* HAVE_CONTEXT_MEMORY */

#endif /* INCLUDED_CRYPTO_DEFS_MEM_H */
