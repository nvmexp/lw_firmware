/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cpustress.h"
#include "core/include/cpu.h"
#include <cmath>
#include <xmmintrin.h>
#include <immintrin.h>

class CpuStressAmd64 : public CpuStress
{
public:
    CpuStressAmd64() = default;
    virtual ~CpuStressAmd64() = default;

    bool IsSupported() override;

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

private:
    static const UINT32 FMABUFFERSIZE = (3*1024);
    vector<FLOAT64> m_Buffer;
    FLOAT64*        m_pBuffer = nullptr;
    Random          m_Random;
};

//-----------------------------------------------------------------------------
unique_ptr<CpuStress> MakeCpuStress()
{
    return make_unique<CpuStressAmd64>();
}

//-----------------------------------------------------------------------------
bool CpuStressAmd64::IsSupported()
{
    UINT32 eax = 0, ebx = 0, ecx = 0, edx = 0;

    Cpu::CPUID(1, 0, &eax, &ebx, &ecx, &edx);

    if (!(ecx & (1 << 28)))
    {
        Printf(Tee::PriError, "AVX not supported by this CPU\n");
        return false;
    }

    if (!(ecx & (1 << 12)))
    {
        Printf(Tee::PriError, "FMA not supported by this CPU\n");
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
RC CpuStressAmd64::Setup()
{
    /* The buffer size is 256KB plus 64 bytes to align on cache line.
     * The assumption is that L2 data cache is 256KB and cache lines are 64B. */
    m_Buffer.reserve(FMABUFFERSIZE + 8);

    /* The pointer is aligned on 64B cache line */
    const UINT32 cacheLineSize = 64;
    m_pBuffer = reinterpret_cast<FLOAT64*>(
                       Utility::AlignUp(reinterpret_cast<size_t>(&m_Buffer[0]),
                                        static_cast<size_t>(cacheLineSize)));

    m_Random.SeedRandom(0x4321);
    for (UINT32 i = 0; i < FMABUFFERSIZE; i++)
    {
        *(m_pBuffer + i) = m_Random.GetRandomDouble(1.0, 2.0);
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC CpuStressAmd64::Cleanup()
{
    m_Buffer.clear();
    return OK;
}

//-----------------------------------------------------------------------------
RC CpuStressAmd64::Run()
{
    FLOAT64* pData = m_pBuffer;

    static FLOAT64 _cos = cos(0.001);
    static FLOAT64 _sin = sin(0.001);
    FLOAT64* const end  = pData + FMABUFFERSIZE;
    const __m256d cosv = _mm256_set1_pd(_cos);
    const __m256d silw = _mm256_set1_pd(_sin);
    const __m256d zero = _mm256_setzero_pd();

    MASSERT((FMABUFFERSIZE & 7) == 0);

    while (pData < end) {
        const __m256d x  = _mm256_load_pd(pData);
        const __m256d y  = _mm256_load_pd(pData + 4);
        const __m256d xc = _mm256_fmadd_pd(x, cosv, zero);
        const __m256d nx = _mm256_fnmadd_pd(silw, y, xc);
        const __m256d xs = _mm256_fmadd_pd(x, silw, zero);
        const __m256d ny = _mm256_fmadd_pd(y, cosv, xs);
        _mm256_stream_pd(pData,     nx);
        _mm256_stream_pd(pData + 4, ny);
        pData += 8;
    }

    _mm_sfence();

    return OK;
}
