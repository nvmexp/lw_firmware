/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2016,2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Interface to a ppc64le CPU.

#include <signal.h>
#include "core/include/cpu.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "ppu_intrinsics.h"

RC Cpu::Initialize()
{
    return OK;
}

void Cpu::Pause()
{
}

void Cpu::BreakPoint()
{
    raise(SIGTRAP);
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

const char* Cpu::Architecture()
{
    return "ppc64le";
}

const char* Cpu::Name()
{
    static string name;
    static bool   initialized = false;

    if (!initialized)
    {
        initialized = true;

        string info;
        if (Xp::InteractiveFileRead("/proc/cpuinfo", &info) == RC::OK)
        {
            vector<string> tokens;
            if (Utility::Tokenizer(info, "\n", &tokens) == RC::OK)
            {
                for (const string& tok : tokens)
                {
                    // Example:
                    // cpu     : POWER9 (raw), altivec supported
                    if (tok.size() > 5 &&
                        tok.compare(0, 3, "cpu", 3) == 0 &&
                        (tok[3] == ' ' || tok[3] == '\t' || tok[3] == ':'))
                    {
                        const size_t colon = tok.find(':');
                        if (colon != tok.npos)
                        {
                            const size_t nspace = tok.find_first_not_of(" \t", colon + 1);
                            name = tok.substr(nspace == tok.npos ? colon : nspace);
                        }
                        break;
                    }
                }
            }
        }
    }

    return name.c_str();
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
    return FlushCpuCacheRange((void *)0, (void *)0xFFFFFFFF, Xp::FLUSH_AND_ILWALIDATE_CPU_CACHE);
}

RC Cpu::FlushCpuCacheRange(void * StartAddress, void * EndAddress, /*Ignored*/ UINT32 Flags)
{
    if (StartAddress > EndAddress)
    {
        return RC::BAD_PARAMETER;
    }

    // On Power all PCI-sourced accesses to host memory are coherent,
    // and all MMIO space accesses are strictly uncached.
    // So flushing isn't strictly necessary, but a sync might still be necessary.
    __sync();

    return OK;
}

RC Cpu::FlushWriteCombineBuffer()
{
    __eieio();
    return OK;
}

void Cpu::MemSet(void *dst, UINT08 data, size_t n)
{
    memset(dst, data, n);
}

void Cpu::MemCopy(void *dst, const void *src, size_t n)
{
    memcpy(dst, src, n);
}

bool Cpu::HasWideMemCopy ()
{
    return false;
}
