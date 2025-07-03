/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "unittest.h"
#include "core/include/utility.h"

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
    // T is the output floating-point type
    // expPos is the position of the lowest exponent bit in the float type
    // expSize is the number of exponent bits in the float type
    // expBias is the value of the exponent which is equivalent to exponent=0
    // U is the input integer type
    template<typename T, int expPos, int expSize, int expBias, typename U>
    T MakeValidImpl(U value)
    {
        // We allow a few low bits of exponent to be random
        constexpr int wiggleBits = 3;

        constexpr U one  = 1;
        constexpr U wiggleMask = ~((one << wiggleBits) - one);
        constexpr U wiggleBias = one << (wiggleBits - 1);
        constexpr U mask = ~((((one << expSize) - one) & wiggleMask) << expPos);
        constexpr U bias = (static_cast<U>(expBias - wiggleBias)) << expPos;

        union
        {
            U in;
            T out;
        } typecolw;

        typecolw.in = (value & mask) + bias;
        return typecolw.out;
    }

    float MakeValidFloat(UINT32 value)
    {
        return MakeValidImpl<float, 23, 8, 127, UINT32>(value);
    }

    double MakeValidDouble(UINT64 value)
    {
        return MakeValidImpl<double, 52, 11, 1023, UINT64>(value);
    }

    class RandomTest: public UnitTest
    {
        public:
            RandomTest() : UnitTest("Random") { }

            virtual ~RandomTest() { }

            virtual void Run()
            {
                InitSeedMaker();

                constexpr int numLoops = 1024;

                // Ensure random distribution is sane (32-bit)
                for (UINT32 n = 0; n < 5; n++)
                {
                    Random r;
                    InitSeed(r);

                    constexpr int bits = 32;

                    INT32 bitCount[bits] = { };

                    constexpr int numLoops32 = 1024;

                    for (int i = 0; i < numLoops32; i++)
                    {
                        const UINT32 value = r.GetRandom();

                        for (UINT32 bit = 0; bit < bits; bit++)
                        {
                            if (value & (1U << bit))
                            {
                                ++bitCount[bit];
                            }
                        }
                    }

                    INT32 total = 0;
                    INT32 minBits = numLoops32;
                    for (int i = 0; i < bits; i++)
                    {
                        UNIT_ASSERT_GT(bitCount[i], 1);
                        total += bitCount[i];
                        minBits = min(minBits, bitCount[i]);
                    }
                    const INT32 mean = total / bits;

                    Printf(Tee::PriLow, "Random32 mean %d min %d\n", mean, minBits);

                    UNIT_ASSERT_LT(abs(mean - numLoops32 / 2), 16);
                    UNIT_ASSERT_LT(abs(mean - minBits), 64);
                }

                // Ensure random distribution is sane (64-bit)
                for (int n = 0; n < 5; n++)
                {
                    Random r;
                    InitSeed(r);

                    constexpr int bits = 64;

                    INT32 bitCount[bits] = { };

                    constexpr int numLoops64 = 2048;

                    for (int i = 0; i < numLoops64; i++)
                    {
                        const UINT64 value = r.GetRandom64();

                        for (UINT32 bit = 0; bit < bits; bit++)
                        {
                            if (value & (1ULL << bit))
                            {
                                ++bitCount[bit];
                            }
                        }
                    }

                    INT32 total = 0;
                    INT32 minBits = numLoops64;
                    for (UINT32 i = 0; i < bits; i++)
                    {
                        UNIT_ASSERT_GT(bitCount[i], 1);
                        total += bitCount[i];
                        minBits = min(minBits, bitCount[i]);
                    }
                    const INT32 mean = total / bits;

                    Printf(Tee::PriLow, "Random64 mean %d min %d\n", mean, minBits);

                    UNIT_ASSERT_LT(abs(mean - numLoops64 / 2), 16);
                    UNIT_ASSERT_LT(abs(mean - minBits), 128);
                }

                // GetRandom (32-bit) with range
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        UINT32 a = rRange.GetRandom();
                        UINT32 b = rRange.GetRandom();

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const UINT32 value = r.GetRandom(a, b);
                            UNIT_ASSERT_GE(value, a);
                            UNIT_ASSERT_LE(value, b);
                        }
                    }
                }

                // GetRandom64 with range
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        UINT64 a = rRange.GetRandom64();
                        UINT64 b = rRange.GetRandom64();

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const UINT64 value = r.GetRandom64(a, b);
                            UNIT_ASSERT_GE(value, a);
                            UNIT_ASSERT_LE(value, b);
                        }
                    }
                }

                // GetRandomFloat
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        float a = MakeValidFloat(rRange.GetRandom());
                        float b = MakeValidFloat(rRange.GetRandom());

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const float value = r.GetRandomFloat(a, b);
                            UNIT_ASSERT_GE(value, a);
                            UNIT_ASSERT_LE(value, b);
                        }
                    }
                }

                // GetRandomDouble
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        double a = MakeValidDouble(rRange.GetRandom64());
                        double b = MakeValidDouble(rRange.GetRandom64());

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const double value = r.GetRandomDouble(a, b);
                            UNIT_ASSERT_GE(value, a);
                            UNIT_ASSERT_LE(value, b);
                        }
                    }
                }

                // GetRandomFloatExp
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        int a = static_cast<int>(rRange.GetRandom(0, 32)) - 16;
                        int b = static_cast<int>(rRange.GetRandom(0, 32)) - 16;

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        const float milwal = powf(2, a);
                        // Note: the maximum value is 2^b plus the maximum value
                        // of mantissa, so the value from GetRandomFloatExp(a, b)
                        // has to be less than 2^(b+1).
                        const float maxVal = powf(2, b + 1);

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const float value = r.GetRandomFloatExp(a, b);
                            UNIT_ASSERT_GE(abs(value), milwal);
                            UNIT_ASSERT_LT(abs(value), maxVal);
                        }
                    }
                }

                // GetRandomDoubleExp
                {
                    Random rRange;
                    InitSeed(rRange);

                    for (int i = 0; i < numLoops; i++)
                    {
                        int a = static_cast<int>(rRange.GetRandom(0, 32)) - 16;
                        int b = static_cast<int>(rRange.GetRandom(0, 32)) - 16;

                        if (a > b)
                        {
                            swap(a, b);
                        }

                        const double milwal = pow(2, a);
                        // Note: the maximum value is 2^b plus the maximum value
                        // of mantissa, so the value from GetRandomDoubleExp(a, b)
                        // has to be less than 2^(b+1).
                        const double maxVal = pow(2, b + 1);

                        Random r;
                        InitSeed(r);

                        for (UINT32 j = 0; j < 16; j++)
                        {
                            const double value = r.GetRandomDoubleExp(a, b);
                            UNIT_ASSERT_GE(abs(value), milwal);
                            UNIT_ASSERT_LT(abs(value), maxVal);
                        }
                    }
                }

                // Ensure least significant bit value distribution is sane across seeds
                {
                    Random r;

                    constexpr int numSeeds = 100;
                    constexpr int numDraws = 10;
                    int numEven[numDraws] = {};
                    int numOdd[numDraws] = {};
                    for (int seedLoopIdx = 0; seedLoopIdx < numSeeds; seedLoopIdx++)
                    {
                        r.SeedRandom(0x12345678 ^ (123 * seedLoopIdx));
                        for (int drawIdx = 0; drawIdx < numDraws; drawIdx++)
                        {
                            if (r.GetRandom() & 1)
                            {
                                numOdd[drawIdx]++;
                            }
                            else
                            {
                                numEven[drawIdx]++;
                            }
                        }
                    }
                    for (int drawIdx = 0; drawIdx < numDraws; drawIdx++)
                    {
                        UNIT_ASSERT_GT(numEven[drawIdx], numSeeds / 3);
                        UNIT_ASSERT_GT(numOdd[drawIdx], numSeeds / 3);
                        // Use these to get the stats:
                        //UNIT_ASSERT_GT(numEven[drawIdx], 1000 + drawIdx);
                        //UNIT_ASSERT_GT(numOdd[drawIdx], 1000 + drawIdx);
                    }
                }
            }

        private:
            void InitSeedMaker()
            {
                UINT32 init = static_cast<UINT32>(time(nullptr));
#ifdef __linux__
                const int fd = open("/dev/urandom", O_RDONLY);
                if (fd != -1)
                {
                    UNIT_ASSERT_EQ(static_cast<int>(read(fd, &init, sizeof(init))),
                                   static_cast<int>(sizeof(init)));
                    close(fd);
                }
#endif
                m_SeedMaker.SeedRandom(0xBAD00A55 ^ init);
            }

            void InitSeed(Random& r)
            {
                r.SeedRandom(m_SeedMaker.GetRandom());
            }

            Random m_SeedMaker;
    };

    ADD_UNIT_TEST(RandomTest);
}
