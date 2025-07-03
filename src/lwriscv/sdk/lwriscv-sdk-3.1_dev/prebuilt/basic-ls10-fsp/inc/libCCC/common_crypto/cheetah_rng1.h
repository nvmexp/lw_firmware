/*
 * Copyright (c) 2016-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic engine : generic rng1 drng
 * random number generator
 */
#ifndef INCLUDED_TEGRA_RNG1_H
#define INCLUDED_TEGRA_RNG1_H

#ifdef DOXYGEN
#include <ccc_doxygen.h>
#endif

#include <include/tegra_pka1.h>

#if HAVE_RNG1_DRNG

#ifndef SE_RNG1_TIMEOUT

#if TEGRA_DELAY_FUNCTION_UNDEFINED
#define SE_RNG1_TIMEOUT		(8000U*100U)  /* poll count, not microseconds */
#else
#define SE_RNG1_TIMEOUT		8000U	/*micro seconds*/
#endif

#endif /* !defined(SE_RNG1_TIMEOUT) */

#define RNG1_DRNG_BYTES 16U /* 128 bit (16 bytes) random data per call */

#ifndef SE_RNG1_MUTEX_TIMEOUT_ACTION
#define SE_RNG1_MUTEX_TIMEOUT_ACTION 0x4U	/* RST RNG1 on timeout */
#endif

status_t rng1_acquire_mutex(const struct engine_s *engine);
void rng1_release_mutex(const struct engine_s *engine);
status_t rng1_reset(const engine_t *engine);

#endif /* HAVE_RNG1_DRNG */
#endif /* INCLUDED_TEGRA_RNG1_H */
