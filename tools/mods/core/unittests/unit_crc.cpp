/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_crc.cpp
 * @brief  Unit tests for Crc::Callwlate
 */

#include "unittest.h"
#include "core/include/crc.h"
#include "core/include/xp.h"
#include "random.h"

namespace
{
    //----------------------------------------------------------------
    //! \brief Unit tests for Crc::Callwlate
    //!
    class CrcTest: public UnitTest
    {
    public:
        CrcTest();
        void Run() override;

    private:
        void TestOneConfig(ColorUtils::Format fmt, UINT32 width, UINT32 height, UINT32 numBins);
        static UINT32 GetElemIndex(ColorUtils::Format fmt, UINT32 bit);
        UINT08 GetRandom()
        {
            return static_cast<UINT08>(m_Random.GetRandom(0, 0xFFU));
        }
        void TestHwCrc32c();

        vector<UINT08> m_TestBuf;
        Random         m_Random;
    };
    ADD_UNIT_TEST(CrcTest);

    CrcTest::CrcTest() :
        UnitTest("CrcTest")
    {
        m_Random.SeedRandom(0xC001D00D);
    }

    void CrcTest::Run()
    {
        TestHwCrc32c();

        // List of formats to test
        ColorUtils::Format formats[] =
        {
            ColorUtils::A8R8G8B8,
            ColorUtils::Y8,
            ColorUtils::R5G6B5,
            ColorUtils::A2R10G10B10,
            ColorUtils::Z24S8,
            ColorUtils::Z32,
            ColorUtils::R16,
            ColorUtils::R16_G16
        };

        // Surface sizes to test
        struct
        {
            UINT32 width;
            UINT32 height;
        }
        surfaceSizes[] =
        {
            {1, 1},
            {16, 1},
            {15, 4}
        };

        // Numbers of bins to test
        UINT32 binSizes[] =
        {
            1,
            7
        };

        for (const auto fmt : formats)
        {
            for (const auto& dims : surfaceSizes)
            {
                for (const auto numBins : binSizes)
                {
                    if (numBins <= dims.width)
                    {
                        TestOneConfig(fmt, dims.width, dims.height, numBins);
                    }
                }
            }
        }
    }

    void CrcTest::TestOneConfig
    (
        ColorUtils::Format fmt,
        UINT32             width,
        UINT32             height,
        UINT32             numBins
    )
    {
        const UINT32 bpp           = ColorUtils::PixelBytes(fmt);
        const UINT32 numElems      = ColorUtils::PixelElements(fmt);
        const UINT32 guardBandSize = 8;
        const UINT32 pitch         = ALIGN_UP(width * bpp, 4U) + guardBandSize;

        vector<UINT08>& buf = m_TestBuf;

        const UINT32 newSize = pitch * height + 2 * guardBandSize;
        buf.resize(newSize);
        for (auto& value : buf)
        {
            value = GetRandom();
        }

        const UINT32 totalBins = numElems * numBins;
        vector<UINT32> origBins(totalBins);
        Crc::Callwlate(&buf[guardBandSize], width, height, fmt, pitch, numBins, &origBins[0]);

        for (UINT32 i = 0; i < totalBins; i++)
        {
            UNIT_ASSERT_NEQ(origBins[i], 0U);
            UNIT_ASSERT_NEQ(origBins[i], ~0U);
        }

        // Fill guard bands with random values to ensure that the CRC
        // function does not pick up anything outside of the surface
        for (UINT32 i = 0; i < guardBandSize; i++)
        {
            buf[i]                              = GetRandom();
            buf[buf.size() - guardBandSize + i] = GetRandom();
        }
        for (UINT32 y = 0; y < height; y++)
        {
            UINT32       offs = y * pitch + width * bpp;
            const UINT32 end  = (y + 1) * pitch;
            for ( ; offs < end; offs++)
            {
                buf[offs + guardBandSize] = GetRandom();
            }
        }

        vector<UINT32> bins(totalBins);

        for (UINT32 bit = 0; bit < bpp * 8; bit++)
        {
            const UINT32 bitMask = 1 << (bit % 8U);
            const UINT32 elem    = GetElemIndex(fmt, bit);
            MASSERT(bitMask < 256);
            MASSERT(elem < numElems);
            for (UINT32 y = 0; y < height; y++)
            {
                for (UINT32 x = 0; x < width; x++)
                {
                    const UINT32 offs = y * pitch + x * bpp + bit / 8U + guardBandSize;
                    MASSERT(offs + guardBandSize < static_cast<UINT32>(buf.size()));
                    const UINT08 orig = buf[offs];
                    buf[offs] = orig ^ bitMask;
                    Crc::Callwlate(&buf[guardBandSize], width, height, fmt, pitch, numBins, &bins[0]);
                    buf[offs] = orig;

                    bool         anyFail = false;
                    const UINT32 binOffs = ((y * width + x) % numBins) * numElems + elem;
                    MASSERT(binOffs < totalBins);
                    for (UINT32 i = 0; i < totalBins; i++)
                    {
                        const UINT32 actual   = bins[i];
                        const UINT32 expected = origBins[i];
                        bool fail = false;
                        if (i == binOffs)
                        {
                            UNIT_ASSERT_NEQ(actual, expected);
                            if (actual == expected)
                            {
                                fail = true;
                            }
                        }
                        else
                        {
                            UNIT_ASSERT_EQ(actual, expected);
                            if (actual != expected)
                            {
                                fail = true;
                            }
                        }
                        if (fail)
                        {
                            Printf(Tee::PriNormal,
                                   "Failed at x=%u y=%u width=%u height=%u "
                                   "pitch=%u fmt=%s bpp=%u bit=%u bin=%u/%u\n",
                                   x, y, width, height, pitch,
                                   ColorUtils::FormatToString(fmt).c_str(),
                                   bpp, bit, i, totalBins);
                            anyFail = true;
                        }
                    }

                    if (anyFail)
                    {
                        return;
                    }
                }
            }
        }
    }

    UINT32 CrcTest::GetElemIndex(ColorUtils::Format fmt, UINT32 bit)
    {
        struct
        {
            ColorUtils::Format fmt;
            UINT32             numBits[4];
        }
        indices[] =
        {
            { ColorUtils::A8R8G8B8,    {  8,  8,  8, 8 } },
            { ColorUtils::Y8,          {  8,  0,  0, 0 } },
            { ColorUtils::R5G6B5,      {  5,  6,  5, 0 } },
            { ColorUtils::A2R10G10B10, { 10, 10, 10, 2 } },
            { ColorUtils::Z24S8,       {  8, 24,  0, 0 } },
            { ColorUtils::Z32,         { 32,  0,  0, 0 } },
            { ColorUtils::R16,         { 16,  0,  0, 0 } },
            { ColorUtils::R16_G16,     { 16, 16,  0, 0 } }
        };
        for (const auto& desc : indices)
        {
            if (desc.fmt == fmt)
            {
                UINT32 endBit = 0;
                for (UINT32 i = 0; i < NUMELEMS(desc.numBits); i++)
                {
                    endBit += desc.numBits[i];
                    if (bit < endBit)
                    {
                        return i;
                    }
                }
                break;
            }
        }
        MASSERT(!"Missing color format description!");
        return 0;
    }
}

#if defined(__amd64) || defined(__i386)
__attribute__((target("sse4.2")))
#endif
void CrcTest::TestHwCrc32c()
{
    if (!Crc32c::IsHwCrcSupported())
    {
        Printf(Tee::PriWarn, "HwCRC32C is unsupported on this CPU\n");
        return;
    }

    const UINT32 numCrcsToTest = 1031;

    Crc32c::GenerateCrc32cTable();

    UINT32 hwCrc = ~0U;
    UINT32 swCrc = hwCrc;

    for (UINT32 crcIdx = 0; crcIdx < numCrcsToTest; crcIdx++)
    {
        UINT32 data = GetRandom();

        hwCrc = CRC32C4(hwCrc, data);
        swCrc = Crc32c::StepSw(swCrc, data);
        UNIT_ASSERT_EQ(hwCrc, swCrc);
    }
}
