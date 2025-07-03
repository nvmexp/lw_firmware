/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include <sdk/lwpu/inc/lwtypes.h>
#include "libos.h"

/**
 * @file
 * @brief Top-level ASAN header
 * 
 * Contains shadow memory helpers, poison manipulation functions, and forward
 * declarations for __asan_* callback functions.
 */

/**
 * @brief Mangles an identifier for use in an "interceptor"
 * 
 * Used for intercepting LIBOS syscall wrappers (that use mixed inline assembly
 * and C statements which is problematic due to ASAN injecting callbacks)
 */
#define LIBOS_ASAN_MANGLE(name) name ## _real

#if __GNUC__ >= 7
// Note: these are compiler ABIs, do not change
typedef struct {
    const char *file;
    LwU32 line;
    LwU32 col;
} LibosAsanSourceLoc;

typedef struct {
    const LwU64 addr;
    LwU64 size;
    LwU64 sizeWithRedzone;
    const char *name;
    const char *_unused1;
    LwU64 _unused2;
    LibosAsanSourceLoc *loc;
    char *_unused3;
} LibosAsanGlobal;

#define LIBOS_ASAN_POISON_STACK_LEFT    0xf1
#define LIBOS_ASAN_POISON_STACK_MID     0xf2
#define LIBOS_ASAN_POISON_STACK_RIGHT   0xf3
#define LIBOS_ASAN_POISON_STACK_PARTIAL 0xf4
#else
#error "libos_asan.h requires GCC version >= 7"
#endif

//
// Note: LIBOS_ASAN_POISON_STACK_* are defined above
//       (as they are compiler ABIs)
//
// Note: LIBOS_ASAN_POISON_HEAP_* and LIBOS_ASAN_POISON_INTERNAL are duplicated in
//       drivers/resman/src/physical/os/gsp/muslc/malloc.c
//
#define LIBOS_ASAN_POISON_MANUAL        0xf7
#define LIBOS_ASAN_POISON_GLOBAL_RED    0xf9
#define LIBOS_ASAN_POISON_HEAP_LEFT     0xfa
#define LIBOS_ASAN_POISON_HEAP_RIGHT    0xfb
#define LIBOS_ASAN_POISON_HEAP_INIT     0xfc
#define LIBOS_ASAN_POISON_HEAP_FREED    0xfd
#define LIBOS_ASAN_POISON_INTERNAL      0xfe
#define LIBOS_ASAN_POISON_GENERIC       0xff

#define LIBOS_ASAN_POISON_NONE          0x00

//
// Note: Do not change LIBOS_ASAN_SHADOW_SHIFT without careful review
//       (a lot of support code here as well compiler instrumentation
//        make assumptions about this)
//
// Note: LIBOS_ASAN_ROUND_UP_TO_GRAN (and dependents) are duplicated in
//       drivers/resman/src/physical/os/gsp/muslc/malloc.c
//
#define LIBOS_ASAN_SHADOW_SHIFT         3
#define LIBOS_ASAN_SHADOW_GRAN          (1 << (LIBOS_ASAN_SHADOW_SHIFT))  // 8-byte granulariy
#define LIBOS_ASAN_SHADOW_GRAN_MASK     ((LIBOS_ASAN_SHADOW_GRAN) - 1)    // 7 (to select bits inside granularity)
#define LIBOS_ASAN_ROUND_UP_TO_GRAN(x)  (((x) + LIBOS_ASAN_SHADOW_GRAN_MASK) & ~LIBOS_ASAN_SHADOW_GRAN_MASK)
#ifndef LIBOS_ASAN_SHADOW_OFFSET
#error "libos_asan.h requires that LIBOS_ASAN_SHADOW_OFFSET be defined"
#endif
#define LIBOS_ASAN_MEM_TO_SHADOW(addr)  ((LwU8 *) ((((LwUPtr) (addr)) >> (LIBOS_ASAN_SHADOW_SHIFT)) + (LIBOS_ASAN_SHADOW_OFFSET)))

/**
 * @brief Initializes internal ASAN data (e.g. shadow memory for globals)
 * @note This should be called as early in the ASAN-enabled task as possible (prior to allocator init).
 */
void libosAsanInit(void);

/**
 * @brief Poisons a memory range with a given poison
 *
 * @param[in] addr
 *     Address starting from which to poison
 *     (must be aligned to LIBOS_ASAN_SHADOW_GRAN)
 * @param[in] size
 *     Number of bytes to poison
 *     (bytes towards the end that are not aligned to LIBOS_ASAN_SHADOW_GRAN
 *     will not be poisoned)
 * @param[in] poison
 *     Poison type (one of the LIBOS_ASAN_POISON_* variants)
 */
void libosAsanPoisonWith(LwUPtr addr, LwLength size, LwU8 poison);

/**
 * @brief Unpoisons a memory range
 *
 * @param[in] addr
 *     Address starting from which to unpoison
 *     (must be aligned to LIBOS_ASAN_SHADOW_GRAN)
 * @param[in] size
 *     Number of bytes to unpoison
 *     (bytes towards the end that are not aligned to LIBOS_ASAN_SHADOW_GRAN
 *     will be unpoisoned via a partially-poisoned corresponding shadow byte)
 */
void libosAsanUnpoison(LwUPtr addr, LwLength size);

//
// Prototypes for compiler-generated callbacks
//

void __asan_register_globals(LibosAsanGlobal *globals, LwLength size);
void __asan_unregister_globals(LibosAsanGlobal *globals, LwLength size);
void __asan_handle_no_return(void);

void __asan_load1(LwUPtr addr);
void __asan_store1(LwUPtr addr);
void __asan_load2(LwUPtr addr);
void __asan_store2(LwUPtr addr);
void __asan_load4(LwUPtr addr);
void __asan_store4(LwUPtr addr);
void __asan_load8(LwUPtr addr);
void __asan_store8(LwUPtr addr);
void __asan_load16(LwUPtr addr);
void __asan_store16(LwUPtr addr);
void __asan_loadN(LwUPtr addr, LwLength size);
void __asan_storeN(LwUPtr addr, LwLength size);

void __asan_load1_noabort(LwUPtr addr);
void __asan_store1_noabort(LwUPtr addr);
void __asan_load2_noabort(LwUPtr addr);
void __asan_store2_noabort(LwUPtr addr);
void __asan_load4_noabort(LwUPtr addr);
void __asan_store4_noabort(LwUPtr addr);
void __asan_load8_noabort(LwUPtr addr);
void __asan_store8_noabort(LwUPtr addr);
void __asan_load16_noabort(LwUPtr addr);
void __asan_store16_noabort(LwUPtr addr);
void __asan_loadN_noabort(LwUPtr addr, LwLength size);
void __asan_storeN_noabort(LwUPtr addr, LwLength size);

void __asan_report_load1(LwUPtr addr);
void __asan_report_store1(LwUPtr addr);
void __asan_report_load2(LwUPtr addr);
void __asan_report_store2(LwUPtr addr);
void __asan_report_load4(LwUPtr addr);
void __asan_report_store4(LwUPtr addr);
void __asan_report_load8(LwUPtr addr);
void __asan_report_store8(LwUPtr addr);
void __asan_report_load16(LwUPtr addr);
void __asan_report_store16(LwUPtr addr);
void __asan_report_load_n(LwUPtr addr, LwLength size);
void __asan_report_store_n(LwUPtr addr, LwLength size);

void __asan_report_load1_noabort(LwUPtr addr);
void __asan_report_store1_noabort(LwUPtr addr);
void __asan_report_load2_noabort(LwUPtr addr);
void __asan_report_store2_noabort(LwUPtr addr);
void __asan_report_load4_noabort(LwUPtr addr);
void __asan_report_store4_noabort(LwUPtr addr);
void __asan_report_load8_noabort(LwUPtr addr);
void __asan_report_store8_noabort(LwUPtr addr);
void __asan_report_load16_noabort(LwUPtr addr);
void __asan_report_store16_noabort(LwUPtr addr);
void __asan_report_load_n_noabort(LwUPtr addr, LwLength size);
void __asan_report_store_n_noabort(LwUPtr addr, LwLength size);