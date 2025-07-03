/*
 * Copyright (c) 2015-2021, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#include <crypto_system_config.h>

#if CCC_STACK_PROTECTOR

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* CCC version of stack protector code => not used
 * unless -fstack-protector-{all,strong} option is used
 * when compiling CCC.
 * The subsystem may also define these already somewhere,
 * in which case do not define CCC_STACK_PROTECTOR
 * to avoid definning these multiple times.
 *
 * TODO:
 * - stack guard value should be randomized.
 */
#if UINT32_MAX == UINTPTR_MAX
uintptr_t __stack_chk_guard = 0xde3cae96;
#else
uintptr_t __stack_chk_guard = 0x595e9fbd94fda766;
#endif

void __stack_chk_fail(void);

__attribute__((noreturn)) void __stack_chk_fail(void)
{
#if HAVE_ERROR_STACKTRACE && defined(DO_STACKTRACE)
	DO_STACKTRACE;
#endif

#if !defined(CCC_PANIC)
#error "CCC_PANIC(string) macro not defined in config file"
#else
	CCC_PANIC("Stack protector failure\n");
#endif
}
#endif /* CCC_STACK_PROTECTOR */
