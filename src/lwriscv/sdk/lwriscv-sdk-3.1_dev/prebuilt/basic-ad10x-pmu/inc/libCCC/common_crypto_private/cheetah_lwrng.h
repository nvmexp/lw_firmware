/*
 * Copyright (c) 2020-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
#ifndef INCLUDED_TEGRA_LWRNG_H
#define INCLUDED_TEGRA_LWRNG_H

#if HAVE_LWRNG_DRNG
/* Out of entropy may cause "invalid" values from DFIFO
 * so retry a small number of times to give HW time to generate
 * more entropy.
 */
#ifndef LWRNG_DFIFO_RETRY_COUNT
#define LWRNG_DFIFO_RETRY_COUNT 10U
#endif

#ifndef LWRNG_RESET_TIMEOUT
#define LWRNG_RESET_TIMEOUT 0x100000U
#endif

status_t lwrng_reset(const struct engine_s *engine);

status_t engine_lwrng_drng_generate_locked(
	struct se_data_params *input_params,
	const struct se_engine_rng_context *econtext);

#if HAVE_CLOSE_FUNCTIONS
void engine_lwrng_drng_close_locked(void);
#endif

/* LWRNG MUTEX HANDLERS. These do nothing because
 * LWRNG engine does not have mutex locks.
 */
static inline status_t lwrng_acquire_mutex(const struct engine_s *engine)
{
	(void)engine;
	return NO_ERROR;
}

static inline void lwrng_release_mutex(const struct engine_s *engine)
{
	(void)engine;
}
#endif /* HAVE_LWRNG_DRNG */
#endif /* INCLUDED_TEGRA_LWRNG_H */
