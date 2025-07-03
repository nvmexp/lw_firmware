/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include "libos.h"
#include "libos_assert.h"
#include "libos_asan.h"

/**
 * @file
 * @brief Implementation of ASAN callbacks and poison manipulation functions
 */

#ifndef _RET_IP_
#define _RET_IP_  ((LwUPtr) __builtin_return_address(0))
#endif

#define ASAN_NOSAN __attribute__((no_sanitize_address))
#define ASAN_WEAK __attribute__((weak))
#define ASAN_STATIC_INLINE static inline __attribute__((always_inline))
#define ASAN_UNLIKELY(x) __builtin_expect((x),LW_FALSE)

static void *_asanMemset(void *pData, LwU8 value, LwLength lengthBytes);

typedef void (*InitFunc)(void);

extern ASAN_WEAK InitFunc __init_array_start;
extern ASAN_WEAK InitFunc __init_array_end;

ASAN_NOSAN void libosAsanInit(void)
{
    // run initializers from ELF section .init_array (ASAN inserts callbacks to initialize globals here)
    LwUPtr lwr = (LwUPtr) &__init_array_start;
    while (lwr < (LwUPtr) &__init_array_end) {
        InitFunc func = *((InitFunc *) lwr);
        func();
        lwr += sizeof(InitFunc);
    }
}

ASAN_NOSAN void libosAsanPoisonWith(LwUPtr addr, LwLength size, LwU8 poison)
{
    // don't touch shadow for any address that the skip mask selects
    if (addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK) {
        return;
    }

    LIBOS_VALIDATE((addr & LIBOS_ASAN_SHADOW_GRAN_MASK) == 0);

    LwU8 *start = LIBOS_ASAN_MEM_TO_SHADOW(addr);
    LwU8 *end = LIBOS_ASAN_MEM_TO_SHADOW(addr + size);

    _asanMemset(start, poison, end - start);
}

ASAN_NOSAN void libosAsanUnpoison(LwUPtr addr, LwLength size)
{
    // don't touch shadow for any address that the skip mask selects
    if (addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK) {
        return;
    }

    LIBOS_VALIDATE((addr & LIBOS_ASAN_SHADOW_GRAN_MASK) == 0);

    libosAsanPoisonWith(addr, size, LIBOS_ASAN_POISON_NONE);

    if (size & LIBOS_ASAN_SHADOW_GRAN_MASK) {
        LwU8 *shadow = LIBOS_ASAN_MEM_TO_SHADOW(addr + size);
        *shadow = (size & LIBOS_ASAN_SHADOW_GRAN_MASK);
    }
}

/**
 * @brief portMemSet without address sanitization (for use by ASAN internals)
 * 
 * Copied from portMemSet in drivers/resman/arch/lwalloc/gsp/src/portstubs.c
 */
static ASAN_NOSAN void *_asanMemset(void *pData, LwU8 value, LwLength lengthBytes)
{
    LwU8 *pByte = (LwU8 *)pData;

    if (lengthBytes >= sizeof(LwU64))  // Don't bother with optimizations for small lengths.
    {
        LwU64 val64 = 0x0101010101010101U * value;

        // Align starting pointer.
        while ((LwUPtr)pByte & (sizeof(LwU32) - 1))
        {
           *pByte++ = value;
           lengthBytes--;
        }

        if ((LwU64)pByte & 4)
        {
           *(LwU32 *)pByte  = (LwU32)val64;
           pByte           += sizeof(LwU32);
           lengthBytes     -= sizeof(LwU32);
        }

        // Starting pointer is aligned to 8 bytes.
        while (lengthBytes >= sizeof(LwU64))
        {
           *(LwU64 *)pByte  = val64;
           pByte           += sizeof(LwU64);
           lengthBytes     -= sizeof(LwU64);
        }
    }

    // Remaining odd bytes.
    while (lengthBytes > 0)
    {
        *pByte++ = value;
        lengthBytes--;
    }

    return pData;
}

ASAN_STATIC_INLINE ASAN_NOSAN void _asanReport(LwUPtr addr, LwLength size, LwBool bWrite, LwUPtr pc, LwUPtr badAddr)
{
    libosTaskPanicWithArgs(LIBOS_TASK_PANIC_REASON_ASAN_MEMORY_ERROR, addr, size, bWrite, pc, badAddr);
}

/**
 * @brief Returns whether the byte at the given address is poisoned 
 * 
 * @param[in] addr
 *     address to check (not the corresponding shadow address)
 * 
 * @return LW_TRUE if poisoned
 * @return LW_FALSE if unpoisoned
 */
ASAN_STATIC_INLINE ASAN_NOSAN LwBool _asanIsBytePoisoned(LwUPtr addr)
{
    // cast to signed as ASAN uses negative shadow values to signify completely poisoned
    LwS8 *shadow = (LwS8 *) LIBOS_ASAN_MEM_TO_SHADOW(addr);
    if (ASAN_UNLIKELY(*shadow)) {
        return ASAN_UNLIKELY(((LwS8) (addr & LIBOS_ASAN_SHADOW_GRAN_MASK)) >= *shadow);
    }
    return LW_FALSE;
}

/**
 * @brief Performs ASAN checks for a memory access of <= 8 bytes and > 0 bytes
 * 
 * Calls _asanReport on memory errors
 * 
 * @param[in] addr
 *     address representing the start of the memory access (not the corresponding shadow address)
 * @param[in] size
 *     size (in bytes) of the memory access (must be <= 8)
 * @param[in] bWrite
 *     whether the memory access is a write (LW_TRUE) or a read (LW_FALSE)
 * @param[in] pc
 *     pc (instruction pointer) of where the memory access oclwrred
 */
ASAN_STATIC_INLINE ASAN_NOSAN void _asanAccessSmall(LwUPtr addr, LwLength size, LwBool bWrite, LwUPtr pc)
{
    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    // check unaligned first chunk when access crosses 8-byte boundary
    if (ASAN_UNLIKELY(((addr + size - 1) & LIBOS_ASAN_SHADOW_GRAN_MASK) < size - 1)) {
        if (ASAN_UNLIKELY(*LIBOS_ASAN_MEM_TO_SHADOW(addr))) {
            return _asanReport(addr, size, bWrite, pc, addr);
        }
    }

    // check tail
    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr + size - 1))) {
        return _asanReport(addr, size, bWrite, pc, addr + size - 1);
    }
}

/**
 * @brief Performs ASAN checks for a memory access of any size > 0 bytes
 * 
 * Calls _asanReport on memory errors
 * 
 * @param[in] addr
 *     address representing the start of the memory access (not the corresponding shadow address)
 * @param[in] size
 *     size (in bytes) of the memory access
 * @param[in] bWrite
 *     whether the memory access is a write (LW_TRUE) or a read (LW_FALSE)
 * @param[in] pc
 *     pc (instruction pointer) of where the memory access oclwrred
 */
static ASAN_NOSAN void _asanAccessLarge(LwUPtr origAddr, LwLength origSize, LwBool bWrite, LwUPtr pc)
{
    LwUPtr addr = origAddr;
    LwLength size = origSize;

    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    // defer to _asanAccessSmall for access <= 8 bytes
    if (ASAN_UNLIKELY(size <= 8)) {
        return _asanAccessSmall(addr, size, bWrite, pc);
    }

    // check unaligned first chunk when addr is not 8-byte aligned
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_SHADOW_GRAN_MASK)) {
        if (ASAN_UNLIKELY(*LIBOS_ASAN_MEM_TO_SHADOW(addr))) {
            return _asanReport(origAddr, origSize, bWrite, pc, addr);
        }

        size -= LIBOS_ASAN_ROUND_UP_TO_GRAN(addr) - addr;
        addr = LIBOS_ASAN_ROUND_UP_TO_GRAN(addr);
    }

    // check all 8-byte chunks in between
    while (size > 8) {
        if (ASAN_UNLIKELY(*LIBOS_ASAN_MEM_TO_SHADOW(addr))) {
            return _asanReport(origAddr, origSize, bWrite, pc, addr);
        }
        size -= LIBOS_ASAN_SHADOW_GRAN;
        addr += LIBOS_ASAN_SHADOW_GRAN;
    }

    // check tail
    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr + size - 1))) {
        return _asanReport(origAddr, origSize, bWrite, pc, addr + size - 1);
    }
}

ASAN_STATIC_INLINE ASAN_NOSAN void _asanRegisterGlobal(LibosAsanGlobal *g)
{
    libosAsanUnpoison(g->addr, g->size);
    libosAsanPoisonWith(g->addr + LIBOS_ASAN_ROUND_UP_TO_GRAN(g->size),
                        g->sizeWithRedzone - LIBOS_ASAN_ROUND_UP_TO_GRAN(g->size),
                        LIBOS_ASAN_POISON_GLOBAL_RED);

    // TODO: optionally store global name and other metadata somewhere for pretty printing
}

ASAN_NOSAN void __asan_register_globals(LibosAsanGlobal *globals, LwLength numGlobals)
{
    for (LwU64 i = 0; i < numGlobals; i++) {
        _asanRegisterGlobal(&globals[i]);
    }
}
ASAN_NOSAN void __asan_unregister_globals(LibosAsanGlobal *globals, LwLength size)
{
    // do nothing
}

ASAN_NOSAN void __asan_handle_no_return(void)
{
    // do nothing
}

ASAN_NOSAN void __asan_load1(LwUPtr addr)
{
    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr))) {
        return _asanReport(addr, 1, LW_FALSE, _RET_IP_, addr);
    }
}
ASAN_NOSAN void __asan_store1(LwUPtr addr)
{
    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr))) {
        return _asanReport(addr, 1, LW_TRUE, _RET_IP_, addr);
    }
}
ASAN_NOSAN void __asan_load2(LwUPtr addr) {
    _asanAccessSmall(addr, 2, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store2(LwUPtr addr) {
    _asanAccessSmall(addr, 2, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load4(LwUPtr addr) {
    _asanAccessSmall(addr, 4, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store4(LwUPtr addr) {
    _asanAccessSmall(addr, 4, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load8(LwUPtr addr) {
    _asanAccessSmall(addr, 8, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store8(LwUPtr addr) {
    _asanAccessSmall(addr, 8, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load16(LwUPtr addr) {
    _asanAccessLarge(addr, 16, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store16(LwUPtr addr) {
    _asanAccessLarge(addr, 16, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_loadN(LwUPtr addr, LwLength size) {
    _asanAccessLarge(addr, size, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_storeN(LwUPtr addr, LwLength size) {
    _asanAccessLarge(addr, size, LW_TRUE, _RET_IP_);
}

ASAN_NOSAN void __asan_load1_noabort(LwUPtr addr)
{
    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr))) {
        return _asanReport(addr, 1, LW_FALSE, _RET_IP_, addr);
    }
}
ASAN_NOSAN void __asan_store1_noabort(LwUPtr addr)
{
    // ignore accesses for any address that the skip mask selects
    if (ASAN_UNLIKELY(addr & LIBOS_ASAN_VA_SKIP_SHADOW_MASK)) {
        return;
    }

    if (ASAN_UNLIKELY(_asanIsBytePoisoned(addr))) {
        return _asanReport(addr, 1, LW_TRUE, _RET_IP_, addr);
    }
}
ASAN_NOSAN void __asan_load2_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 2, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store2_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 2, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load4_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 4, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store4_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 4, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load8_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 8, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store8_noabort(LwUPtr addr) {
    _asanAccessSmall(addr, 8, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_load16_noabort(LwUPtr addr) {
    _asanAccessLarge(addr, 16, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_store16_noabort(LwUPtr addr) {
    _asanAccessLarge(addr, 16, LW_TRUE, _RET_IP_);
}
ASAN_NOSAN void __asan_loadN_noabort(LwUPtr addr, LwLength size) {
    _asanAccessLarge(addr, size, LW_FALSE, _RET_IP_);
}
ASAN_NOSAN void __asan_storeN_noabort(LwUPtr addr, LwLength size) {
    _asanAccessLarge(addr, size, LW_TRUE, _RET_IP_);
}

ASAN_NOSAN void __asan_report_load1(LwUPtr addr) {
    _asanReport(addr, 1, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store1(LwUPtr addr) {
    _asanReport(addr, 1, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load2(LwUPtr addr) {
    _asanReport(addr, 2, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store2(LwUPtr addr) {
    _asanReport(addr, 2, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load4(LwUPtr addr) {
    _asanReport(addr, 4, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store4(LwUPtr addr) {
    _asanReport(addr, 4, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load8(LwUPtr addr) {
    _asanReport(addr, 8, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store8(LwUPtr addr) {
    _asanReport(addr, 8, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load16(LwUPtr addr) {
    _asanReport(addr, 16, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store16(LwUPtr addr) {
    _asanReport(addr, 16, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load_n(LwUPtr addr, LwLength size) {
    _asanReport(addr, size, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store_n(LwUPtr addr, LwLength size) {
    _asanReport(addr, size, LW_TRUE, _RET_IP_, addr);
}

ASAN_NOSAN void __asan_report_load1_noabort(LwUPtr addr) {
    _asanReport(addr, 1, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store1_noabort(LwUPtr addr) {
    _asanReport(addr, 1, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load2_noabort(LwUPtr addr) {
    _asanReport(addr, 2, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store2_noabort(LwUPtr addr) {
    _asanReport(addr, 2, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load4_noabort(LwUPtr addr) {
    _asanReport(addr, 4, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store4_noabort(LwUPtr addr) {
    _asanReport(addr, 4, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load8_noabort(LwUPtr addr) {
    _asanReport(addr, 8, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store8_noabort(LwUPtr addr) {
    _asanReport(addr, 8, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load16_noabort(LwUPtr addr) {
    _asanReport(addr, 16, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store16_noabort(LwUPtr addr) {
    _asanReport(addr, 16, LW_TRUE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_load_n_noabort(LwUPtr addr, LwLength size) {
    _asanReport(addr, size, LW_FALSE, _RET_IP_, addr);
}
ASAN_NOSAN void __asan_report_store_n_noabort(LwUPtr addr, LwLength size) {
    _asanReport(addr, size, LW_TRUE, _RET_IP_, addr);
}
