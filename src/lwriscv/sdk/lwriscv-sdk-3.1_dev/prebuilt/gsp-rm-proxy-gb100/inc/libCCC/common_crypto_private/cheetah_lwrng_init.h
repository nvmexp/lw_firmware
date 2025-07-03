/*
 * Copyright (c) 2020-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/*
 * LWRNG device first time initialization for ROM/MB1.
 * See the compilation options below, they define what this code does.
 */
#ifndef INCLUDED_TEGRA_LWRNG_INIT_H
#define INCLUDED_TEGRA_LWRNG_INIT_H

#ifndef CCC_SOC_FAMILY_DEFAULT
#include <crypto_system_config.h>
#endif

#ifndef LWRNG_TIMEOUT
#define LWRNG_TIMEOUT (8000U*100U)  /* poll count, not microseconds */
#endif

#define LWRNG_INITIAL_BUF_BYTES 64U
/* Generate around 1024 bytes with a slow clock
 */
#define LWRNG_INITIAL_BUF_BYTE_CYCLES (1024U/LWRNG_INITIAL_BUF_BYTES)

#define LWRNG_STARTUP_RETRY_COUNT	400000U /* SE Arch suggested value */

/* Pass the (mapped) LWRNG device base address to
 * this function.
 *
 * It will do initial setup of the LWRNG unit after
 * system reset.
 */
status_t lwrng_init(uint8_t *lwrng_base_address);

#endif /* INCLUDED_TEGRA_LWRNG_INIT_H */
