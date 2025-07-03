/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cpustress.h"
#include "random.h"
#include <arm_neon.h>
#include <cmath>

class CpuStressAarch64 : public CpuStress
{
public:
    CpuStressAarch64() = default;
    virtual ~CpuStressAarch64() = default;

    bool IsSupported() override;

    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;

private:
    void CopyBuffer(const float* pSrc, float* pDest);
    void RunTest(float* pDest, UINT32 numLoops);
    bool Verify(const float* pBuf1, const float* pBuf2);

    vector<UINT08> m_Scratch;
    float*         m_pInit       = nullptr;
    float*         m_pGolden     = nullptr;
    float*         m_pTest       = nullptr;
    UINT32         m_Seed        = 0;
    UINT32         m_BufferSize  = static_cast<UINT32>(256_KB);
    bool           m_Initialized = false;
};

//-----------------------------------------------------------------------------
unique_ptr<CpuStress> MakeCpuStress()
{
    return make_unique<CpuStressAarch64>();
}

//-----------------------------------------------------------------------------
bool CpuStressAarch64::IsSupported()
{
    return true;
}

//-----------------------------------------------------------------------------
RC CpuStressAarch64::Setup()
{
    const UINT32 cacheLineSize = 64;

    m_Scratch.resize(3 * m_BufferSize + cacheLineSize);
    m_pInit = reinterpret_cast<float*>(
                    Utility::AlignUp(reinterpret_cast<size_t>(&m_Scratch[0]),
                                     static_cast<size_t>(cacheLineSize)));
    m_pGolden = m_pInit   + m_BufferSize / sizeof(float);
    m_pTest   = m_pGolden + m_BufferSize / sizeof(float);
    m_Initialized = false;

    // Fill init buffer with random floats from 1.0 to 2.0
    Random rand;
    rand.SeedRandom(0xCAFEF00Du);
    float*       out    = m_pInit;
    float* const endOut = out + m_BufferSize / sizeof(float);
    for ( ; out < endOut; out++)
    {
        *out = rand.GetRandomFloat(1, 2);
    }

    m_Seed = rand.GetRandom();

    return RC::OK;
}

//-----------------------------------------------------------------------------
RC CpuStressAarch64::Cleanup()
{
    m_pInit   = nullptr;
    m_pGolden = nullptr;
    m_pTest   = nullptr;
    vector<UINT08> empty;
    m_Scratch.swap(empty);
    return RC::OK;
}

//-----------------------------------------------------------------------------
void CpuStressAarch64::CopyBuffer(const float* pSrc, float* pDest)
{
    float* const pEnd  = pDest + m_BufferSize / sizeof(float);

    for (; pDest < pEnd; pDest += 8, pSrc += 8)
    {
        const float32x4_t a = vld1q_f32(pSrc);
        const float32x4_t b = vld1q_f32(pSrc + 4);
        vst1q_f32(pDest, a);
        vst1q_f32(pDest + 4, b);
    }
}

//-----------------------------------------------------------------------------
void CpuStressAarch64::RunTest(float* pDest, UINT32 numLoops)
{
    // The LCG we use below requires the seed to be an odd number
    UINT32 lcg = m_Seed | 1;

    // This trivial LCG is used in glibc, according to Wikipedia.
    // It gets compiled to a single madd instruction on ARM.
    const auto GetRandom = [&lcg]() -> UINT32
    {
        constexpr UINT32 lcgMultiplier = 1103515245;
        constexpr UINT32 lcgStep       = 12345;

        lcg = lcg * lcgMultiplier + lcgStep;

        return lcg & 0x7FFFFFFF;
    };

    // One entry is 4 vectors with 4 float elements each
    // This covers entire 64-byte cache line
    const UINT32 numEntries = m_BufferSize / (16 * sizeof(float));

    const float32x4_t zero  = vmovq_n_f32(0.0f);
    constexpr float   angle = 0.001f;
    const float32x4_t silw  = vmovq_n_f32(std::sin(angle));
    const float32x4_t cosv  = vmovq_n_f32(std::cos(angle));

    // Each loop processes four parallel vectors.  The data is assumed to be:
    //     [x1, x2, x3, x4, y1, y2, y3, y4]
    // For each (xn, yn) we apply rotation around a constant 'angle'.
    // Each loop jumps to a random offset in the buffer (using trivial LCG).
    // These random jumps improve power draw from caches.
    for (UINT32 i = 0; i < numLoops; i++)
    {
        float* const ptr = pDest + 16 * (GetRandom() % numEntries);

        const auto Rotate = [&](float* ptr)
        {
            const float32x4_t x  = vld1q_f32(ptr);           // x  = ptr[0..3]
            const float32x4_t y  = vld1q_f32(ptr + 4);       // y  = ptr[4..7]
            const float32x4_t xc = vmlaq_f32(zero, x, cosv); // xc = zero + (x * cosv)
            const float32x4_t nx = vmlsq_f32(xc, silw, y);   // nx = xc - (silw * y)
            const float32x4_t xs = vmlaq_f32(zero, x, silw); // xs = zero + (x * silw)
            const float32x4_t ny = vmlaq_f32(xs, y, cosv);   // ny = y + (cosv * xs)
            vst1q_f32(ptr, nx);                              // ptr[0..3] = nx
            vst1q_f32(ptr + 4, ny);                          // ptr[4..7] = ny
        };

        Rotate(ptr);
        Rotate(ptr + 8);
    }
}

//-----------------------------------------------------------------------------
bool CpuStressAarch64::Verify(const float* pBuf1, const float* pBuf2)
{
    const UINT32* pSrc1 = reinterpret_cast<const UINT32*>(pBuf1);
    const UINT32* pSrc2 = reinterpret_cast<const UINT32*>(pBuf2);

    const UINT32* const pEnd1 = pSrc1 + m_BufferSize / sizeof(UINT32);

    uint32x4_t result = vmovq_n_u32(0);

    for (; pSrc1 < pEnd1; pSrc1 += 4, pSrc2 += 4)
    {
        const uint32x4_t v1  = vld1q_u32(pSrc1);       // v1  = pSrc1[0..3]
        const uint32x4_t v2  = vld1q_u32(pSrc2);       // v2  = pSrc2[0..3]
        const uint32x4_t cmp = veorq_u32(v1, v2);      // cmp = v1 ^ v2
        result               = vorrq_u32(result, cmp); // result = result | cmp
    }

    UINT32 totalBadBits = 0;

    alignas(4 * sizeof(UINT32)) UINT32 badBits[4];
    vst1q_u32(badBits, result);

    for (UINT32 i = 0; i < 4; i++)
    {
        totalBadBits |= badBits[i];
        if (badBits[i])
        {
            Printf(Tee::PriError, "Difference in lane %u, bad bits: 0x%08X\n", i, badBits[i]);
        }
    }

    return totalBadBits == 0;
}

//-----------------------------------------------------------------------------
RC CpuStressAarch64::Run()
{
    constexpr UINT32 numLoops = 1024 * 1024;

    if (!m_Initialized)
    {
        CopyBuffer(m_pInit, m_pGolden);
        RunTest(m_pGolden, numLoops);
        m_Initialized = true;
    }

    CopyBuffer(m_pInit, m_pTest);
    RunTest(m_pTest, numLoops);
    if (!Verify(m_pGolden, m_pTest))
    {
        return RC::BAD_LWIDIA_CHIP;
    }

    return RC::OK;
}
