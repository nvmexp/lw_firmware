/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
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
    __asm__("BKPT #3"); // immediate is ignored
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
    return "armv7";
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
    // Full memory barrier uses the dmb instruction.
    // According to Krishna Reddy this is not sufficient.
    // We need dsb or dmb and then we need to flush L2.
    // We call Xp::FlushWriteCombineBuffer(), which calls into
    // the kernel driver (ESC_MEMORY_BARRIER) which in
    // turns calls kernel's wmb() macro which does what we need.
    return Xp::FlushWriteCombineBuffer();
}

void Cpu::MemCopy(void *dst, const void *src, size_t n)
{
    // @@@
    // Unless the runtime library memcpy is proven to use wider-than-4-byte
    // read/write instructions, this is inadequate for full HW test coverage.
    // Need inline asm here.
    memcpy(dst, src, n);
}

bool Cpu::HasWideMemCopy ()
{
    // @@@ see above
    return false;
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    memset(dst, data, n);
}
