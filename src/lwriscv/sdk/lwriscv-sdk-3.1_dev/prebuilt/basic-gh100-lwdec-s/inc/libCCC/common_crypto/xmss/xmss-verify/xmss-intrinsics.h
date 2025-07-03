/*
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef XMSS_INTRINSICS_H
#define XMSS_INTRINSICS_H

/*
 * Allows the caller to avoid setting
 * XMSS_RISCV_TARGET, XMSS_ARM_TARGET, or XMSS_X86_TARGET.
 *
 * In production code these currently only control optimizations.
 */
#ifndef XMSS_TARGET_SET

#ifdef __riscv
#define XMSS_RISCV_TARGET 1
#else
#undef XMSS_RISCV_TARGET
#endif

#ifdef __AARCH64EL__
#define XMSS_ARM_TARGET 1
#else
#undef XMSS_ARM_TARGET
#endif

#ifdef __x86_64__
#define XMSS_X86_TARGET 1
#else
#undef XMSS_X86_TARGET
#endif

#endif /* XMSS_TARGET_SET */


#if defined(XMSS_RISCV_TARGET)
#define X86_RISCV_ARM(x86,riscv,arm) (riscv)
#elif defined(XMSS_ARM_TARGET)
#define X86_RISCV_ARM(x86,riscv,arm) (arm)
#else
#define X86_RISCV_ARM(x86,riscv,arm) (x86)
#endif

/* Defaults */
#ifndef XMSS_LARGE_LOCAL
#define XMSS_LARGE_LOCAL
#endif

#ifndef XMSS_SIZEOF
#define XMSS_SIZEOF(v) sizeof(v)
#endif


#ifndef XMSS_MEMSET
#define XMSS_MEMSET(d,v,b) (void)memset(d,v,b)
#endif

#ifndef XMSS_MEMCPY
#define XMSS_MEMCPY(d,s,b) (void)memcpy(d,s,b)
#endif

#ifndef XMSS_MEMCMP
#define XMSS_MEMCMP(s1,s2,b) memcmp(s1,s2,b)
#endif

#ifndef SWAP32
#define SWAP32(x) __builtin_bswap32(x)
#endif

#endif // XMSS_INTRINSICS_H
