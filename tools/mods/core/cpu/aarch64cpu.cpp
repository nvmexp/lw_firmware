/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Interface to an ARM CPU.

#include "core/include/cpu.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "cheetah/include/tegchip.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include <string.h>

#ifndef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
#error "Atomic intrinsics not available in this GCC version"
#endif

#if defined(__linux__)
#include <csignal>
#endif

RC Cpu::Initialize()
{
    return OK;
}

void Cpu::BreakPoint()
{
#if defined(__linux__)
    raise(SIGTRAP);
#else
    __builtin_trap();
#endif
}

void Cpu::Pause()
{
    __asm__ __volatile__("yield");
}

bool Cpu::IsCPUIDSupported()
{
    return false;
}

bool Cpu::IsFeatureSupported(Feature feature)
{
    return false;
}

void Cpu::CPUID
(
   UINT32   Op,
   UINT32   SubOp,
   UINT32 * pEAX,
   UINT32 * pEBX,
   UINT32 * pECX,
   UINT32 * pEDX
)
{
}

namespace
{
    const char* GetTegraChipName()
    {
#ifdef QNX
        return nullptr;
#else
        static string name;
        static bool   initialized = false;

        if (!initialized)
        {
            if (!CheetAh::IsInitialized())
            {
                return nullptr;
            }

            initialized = true;

            string chip;
            RC rc = CheetAh::GetChipName(&chip);
            if (rc == RC::OK)
            {
                name = move(chip);
            }
        }

        return name.empty() ? nullptr : name.c_str();
#endif
    }
}

const char* Cpu::Architecture()
{
    return "aarch64";
}

const char* Cpu::Name()
{
    const char* name = GetTegraChipName();

    return name ? name : "ARM CPU";
}

UINT32 Cpu::SetFpControlWord(UINT32 NewFpcw)
{
    return 0;
}

UINT32 Cpu::GetFpControlWord()
{
    return 0;
}

UINT32 Cpu::SetMXCSRControlWord(UINT32 NewMxcsr)
{
    return 0;
}

UINT32 Cpu::GetMXCSRControlWord()
{
    return 0;
}

RC Cpu::RdIdt(UINT32 *pBase, UINT32 *pLimit)
{
   return RC::UNSUPPORTED_FUNCTION;
}

RC Cpu::IlwalidateCache()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC Cpu::FlushCache()
{
    return Xp::FlushCpuCacheRange((void *)0, (void *)0xFFFFFFFF, Xp::FLUSH_AND_ILWALIDATE_CPU_CACHE);
}

RC Cpu::FlushCpuCacheRange(void * StartAddress, void * EndAddress, UINT32 Flags)
{
    return Xp::FlushCpuCacheRange(StartAddress, EndAddress, (Xp::CacheFlushFlags)Flags);
}

RC Cpu::FlushWriteCombineBuffer()
{
    __asm__ __volatile__ ("dsb sy");
    return OK;
}

namespace
{
    template<bool acquire>
    struct IO
    {
        template<typename T>
        static inline T Load(const T* src)
        {
            return __atomic_load_n(src, __ATOMIC_RELAXED);
        }
    };

    template<>
    struct IO<true>
    {
        template<typename T>
        static inline T Load(const T* src)
        {
            return __atomic_load_n(src, __ATOMIC_ACQUIRE);
        }
    };

    template<bool acquire, typename T>
    inline void CopyDataIO(const T*& src, T*& dst)
    {
        const T value = IO<acquire>::Load(src);
        __atomic_store_n(dst, value, __ATOMIC_RELAXED);
        ++src;
        ++dst;
    }

    template<bool acquire>
    void MemCopyIO(void *dst, const void *src, size_t n)
    {
        const UINT08*       src8    = static_cast<const UINT08*>(src);
        UINT08*             dst8    = static_cast<UINT08*>(dst);
        const UINT08* const endSrc8 = src8 + n;

        // Use 64-bit copy only if alignment of src and dst matches
        if ((reinterpret_cast<UINT64>(dst) & 7U) == (reinterpret_cast<UINT64>(src) & 7U))
        {
            // Align on 64-bit boundary
            while ((reinterpret_cast<UINT64>(src8) & 7U) && (src8 < endSrc8))
            {
                CopyDataIO<acquire>(src8, dst8);
            }

            // Perform 64-bit copy
            const UINT64*       src64    = reinterpret_cast<const UINT64*>(src8);
            UINT64*             dst64    = reinterpret_cast<UINT64*>(dst8);
            const UINT64* const endSrc64 = reinterpret_cast<const UINT64*>(
                                            reinterpret_cast<UINT64>(endSrc8) & ~7ULL);
            while (src64 < endSrc64)
            {
                CopyDataIO<acquire>(src64, dst64);
            }
            src8 = reinterpret_cast<const UINT08*>(src64);
            dst8 = reinterpret_cast<UINT08*>(dst64);
        }

        // Copy the tail
        while (src8 < endSrc8)
        {
            CopyDataIO<acquire>(src8, dst8);
        }
    }

    // By default use relaxed loads, which are compatible with both UC and WC mappings,
    // but some architectures may need loads with acquire semantics.
    bool s_UseRelaxedLoads = true;
}

void Cpu::MemCopy(void *dst, const void *src, size_t n)
{
    if (s_UseRelaxedLoads)
    {
        MemCopyIO<false>(dst, src, n);
    }
    else
    {
        MemCopyIO<true>(dst, src, n);
    }
}

bool Cpu::HasWideMemCopy ()
{
    return false;
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    UINT08*       ptr8    = static_cast<UINT08*>(dst);
    UINT08* const endPtr8 = ptr8 + n;

    // Align on 64-bit boundary
    while ((reinterpret_cast<UINT64>(ptr8) & 7U) && (ptr8 < endPtr8))
    {
        __atomic_store_n(ptr8, data, __ATOMIC_RELAXED);
        ++ptr8;
    }

    // Perform 64-bit fill
    const UINT64  fillValue = 0x0101010101010101ULL * data;
    UINT64*       ptr64     = reinterpret_cast<UINT64*>(ptr8);
    UINT64* const endPtr64  = reinterpret_cast<UINT64*>(reinterpret_cast<UINT64>(endPtr8) & ~7ULL);
    while (ptr64 < endPtr64)
    {
        __atomic_store_n(ptr64, fillValue, __ATOMIC_RELAXED);
        ++ptr64;
    }

    // Fill the tail
    ptr8 = reinterpret_cast<UINT08*>(ptr64);
    while (ptr8 < endPtr8)
    {
        __atomic_store_n(ptr8, data, __ATOMIC_RELAXED);
        ++ptr8;
    }
}
