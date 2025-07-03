/*
 * Copyright (c) 2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#include <crypto_system_config.h>

#if CCC_WITH_XMSS

#define XMSS_NO_SYSTEM_INCLUDES

#if HAVE_XMSS_USE_TARGET_OPTIONS == 0
/* When set use generic code instead of architecture specific optimizations,
 * like vector units and assembly instructions.
 *
 * It may be required to use the arch psec settings for e.g. performance reasons
 * when possible. In such cases set HAVE_XMSS_USE_TARGET_OPTIONS 1.
 */
#define XMSS_TARGET_SET
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#include <xmss/ccc_xmss_defs.h>
#include <xmss/xmss-verify/xmss-verify.c>

#endif /* CCC_WITH_XMSS */
