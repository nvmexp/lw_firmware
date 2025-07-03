/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "random.h"
#include "lwdiagutils.h"
#include <cmath>

constexpr double TwoExp32    = 4294967296.0;      // 0x100000000 (won't fit in UINT32)
constexpr double TwoExp32Ilw = 1.0 / TwoExp32;

//////////////////////////////////////////////////////////////////////////////
// TODO delete everything in Random::Old below, eventually (legacy RNG)
//
// The history of the legacy RNG is as follows:
//
// CL 54250 on 2000/2/1 added the first RNG in MODS, used rand().
// Only JS interface.
//
// CL 59697 on 2000/3/5 made the RNG usable in C++ by moving the code
// to Utility::Random().
//
// CL 83293 on 2000/6/12 changed the RNG to use CRC table for generating
// random numbers instead of rand().
//
// CL 83896 on 2000/6/14 changed the RNG to use an LCG.
//
// CL 84313 on 2000/6/15 changed the RNG to a copy of random() function
// from GNU C library, which dates back to at least 1983.
//
// CL 484247 on 2002/8/30 changed the RNG to a different algorithm,
// but the CL description says that the implementation has the same
// perf and generates the same output as the previous RNG.  This is
// the legacy implementation below.
//

// Interface to legacy RNG, swept under the rug
struct Random::Old
{
    static void SeedRandom(Random* that, UINT32 Seed);
    static UINT32 GetRandom(Random* that);
    static UINT32 GetRandom(Random* that, UINT32 Min, UINT32 Max);
    static UINT64 GetRandom64(Random* that);
    static UINT64 GetRandom64(Random* that, UINT64 Min, UINT64 Max);
    static float GetRandomFloat(Random* that, double Min, double Max);
    static double GetRandomDouble(Random* that, double Min, double Max);
};

constexpr double TwoExp32Less1    = 4294967295.0;      //  0xffffffff
constexpr double TwoExp32Less1Ilw = 1.0 / TwoExp32Less1;

void Random::Old::SeedRandom(Random* that, UINT32 Seed)
{
   int i;

   that->m_LegacyState[0] = Seed;
   for(i = 1; i < RND_DEG; i++)
      that->m_LegacyState[i] = 1103515245 * that->m_LegacyState[i - 1] + 12345;

   that->m_Front = RND_SEP;
   that->m_Rear = 0;

   that->m_Initialized = true;

   for(i = 0; i < 10 * RND_DEG; i++)
      GetRandom(that);

    that->m_HaveSavedNormal = false;
}

UINT32 Random::Old::GetRandom(Random* that)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
   LWDASSERT(that->m_Initialized);
#endif
   int value;
   LWDASSERT(that->m_Enabled);

   that->m_LegacyState[that->m_Front] += that->m_LegacyState[that->m_Rear];
   value = that->m_LegacyState[that->m_Front];
   if (++that->m_Front >= RND_DEG)
   {
      that->m_Front = 0;
      ++that->m_Rear;
   }
   else
   {
      if (++that->m_Rear >= RND_DEG)
         that->m_Rear = 0;
   }

   return value;
}

UINT32 Random::Old::GetRandom(Random* that, UINT32 Min, UINT32 Max)
{
   LWDASSERT(Min <= Max);

   // Generate the next psuedo-random number.
   UINT32 Val = GetRandom(that);

   // Offset and scale the random number to fit the range requested.
   return Min + UINT32(Val * (double(Max - Min) + 1.0) * TwoExp32Ilw);
}

UINT64 Random::Old::GetRandom64(Random* that)
{
   // Generate a 64-bit random by concatenating the next two
   // psuedo-random 32-bit numbers.
   UINT64 Hi = GetRandom(that);
   UINT64 Lo = GetRandom(that);
   return (Hi << 32) + Lo;
}

UINT64 Random::Old::GetRandom64(Random* that, UINT64 Min, UINT64 Max)
{
   LWDASSERT(Min <= Max);

   // Generate the next 64-bit pseudo-random number
   UINT64 Val = GetRandom64(that);

   // If Min & Max does not cover the entire range of 64-bit numbers,
   // then scale the random number by multiplying by (Max - Min + 1) /
   // (2 ** 64).
   //
   // Unfortunately, a "double" doesn't have enough precision to do
   // this callwlation, and "long double" isn't supported everywhere.
   // So use integer operations instead: find the 128-bit product Val
   // * (Max - Min + 1) by multiplying 32 bits at a time, and then
   // take the upper 64 bits of the result.  The callwlation avoids
   // overflow because a*b+c+d is guaranteed to fit in 64 bits, where
   // a-d are 32-bits numbers.
   //
   static const UINT64 LOW32 = 0xffffffffULL;
   if (Min != 0 || Max != 0xffffffffffffffffULL)
   {
       const UINT64 X[2] = { (Max - Min + 1) & LOW32, (Max - Min + 1) >> 32 };
       const UINT64 Y[2] = { Val & LOW32, Val >> 32 };
       UINT64 Product[3] = {0, 0, 0};

       Product[0] += X[0] * Y[0];      // Multiply the low 32 bits
       Product[1] += Product[0] >> 32; // Carry 32 bits into the next "digit"
       Product[0] &= LOW32;

       Product[1] += X[0] * Y[1];      // Multiply the mid 32 bits
       Product[2] += Product[1] >> 32; // Carry 32 bits into the next "digit"
       Product[1] &= LOW32;

       Product[1] += X[1] * Y[0];      // Multiply the mid 32 bits
       Product[2] += Product[1] >> 32; // Carry 32 bits into the next "digit"
       Product[1] &= LOW32;

       Product[2] += X[1] * Y[1];      // Multiply the high 32 bits

       Val = Product[2];               // Store the high 64 bits in Val
   }

   return Val + Min;
}

float Random::Old::GetRandomFloat(Random* that, double Min, double Max)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
   LWDASSERT(that->m_Initialized);
#endif
   LWDASSERT(that->m_Enabled);
   // Divide the range into 2**32-1 equal sized bins.
   double binsize = (Max - Min) * TwoExp32Less1Ilw;

   // Return a number with linear probability distribution across min->max inclusive.
   return static_cast<float>(Min + binsize * GetRandom(that));

   // Note: this function is faster, but less precise than GetRandomDouble().
   //
   // If the range spans more than 32 powers of two, we lose some precision.
   //
   // For example if the range is 0.0 to 4096.0, binsize will be about 1e-6,
   // and values >0.0 and <1e-6 will never be picked, even though a float
   // can represent many, many values in that range.
}

double Random::Old::GetRandomDouble(Random* that, double Min, double Max)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
   LWDASSERT(that->m_Initialized);
#endif
   LWDASSERT(that->m_Enabled);
   // Divide the range into 2**32 equal sized bins.
   double binsize = (Max - Min) * TwoExp32Ilw;

   // Pick one of the bins.
   double coarse = binsize * GetRandom(that);

   // We now have a random float that has even distribution w/in the range,
   // but the numbers are all at bin boundaries, and Max is not possible.
   //
   // Now add another random number (0.0 to binsize) to fill in up to Max
   // and give us more precision near 0.

   return Min + coarse + (GetRandom(that) * binsize * TwoExp32Less1Ilw);

   // Note on precision:
   //
   // If the range spans more than 64 powers of two, there will be some
   // values in the range that are representable by a double, but that
   // are not picked by this function.
   //
   // For example, if the range is -65536.0 to 65536.0, binsize/(2**32)
   // will be about 7e-15.  There will be many values near 0.0 that are
   // representable by a double, but not reachable by this function.
   //
   // It would be possible to write a function that fills the mantissa and
   // radix of a double with random numbers, to make all possible doubles
   // reachable.
   // Unfortunately, the distribution of the picked numbers rises exponentially
   // with proximity to 0.0, so the range 0.0 to 64.0 would return <1.0 the vast
   // majority of the time.
}

//////////////////////////////////////////////////////////////////////////////
// TODO delete above, up to here
//////////////////////////////////////////////////////////////////////////////

// The PCG XSH RR 32 algorithm by Melissa O'Neill, http://www.pcg-random.org
// The algorithm snippet has been copied from that website on 2019/09/25
// in CL 27350738 and reformatted to follow best practices.  The algorithm
// has been approved for use in http://lwbugs/2718502
// We chose this algorithm due to its excellent statistical properties
// while maintaining relatively high speed and simplicity.

/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */
static UINT32 PCGRandom(Random::PCG32State* pcg)
{
    const     UINT64 state      = pcg->state;
    constexpr UINT64 multiplier = 0x5851'F42D'4C95'7F2DULL;

    // Advance internal state using LCG
    pcg->state = state * multiplier + pcg->stream;

    // Use xorshift with bit rotation to spread randomness across all bits
    const UINT32 xorshifted = static_cast<UINT32>(((state >> 18) ^ state) >> 27);
    const int    rot        = static_cast<int>(state >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

static void PCGInit(Random::PCG32State* pcg, UINT64 initState, UINT64 initStream)
{
    pcg->stream = (initStream << 1) | 1U;
    pcg->state  = pcg->stream + initState;

    PCGRandom(pcg);
}

//------------------------------------------------------------------------------
// The DefaultRandom instance.
//------------------------------------------------------------------------------
Random DefaultRandom::s_Instance;

//------------------------------------------------------------------------------
// Set the random seed.
//------------------------------------------------------------------------------
void Random::SeedRandom(UINT32 seed)
{
    SeedRandom(&seed, 1);
}

//------------------------------------------------------------------------------
// Seeding function for people who don't care about the underlying
// algorithm, who just want to generate a decent unique seed from some
// numbers related to their code.
//------------------------------------------------------------------------------
void Random::SeedRandom(UINT32* pSeedArray, size_t arraySize)
{
    LWDASSERT(pSeedArray != nullptr);
    LWDASSERT(arraySize > 0);

    if (m_UseOld)
    {
        UINT32 seed = 0;
        for (size_t ii = 0; ii < arraySize; ++ii)
        {
            seed ^= pSeedArray[ii];
        }
        Old::SeedRandom(this, seed);
        return;
    }

    // Use pSeedArray to init a temporary PCG RNG, which generates the
    // seeds for this Random object.  The PCG-generated seed means
    // that two Random objects will have very different initial
    // states, and generate very different random sequences, even if
    // they're seeded with almost-identical pSeedArray arrays.
    //
    constexpr size_t SEED_SIZE = 8;
    UINT32 generatedSeeds[SEED_SIZE] = { };
    PCG32State seedGenerator = {1, 1};

    for (size_t ii = 0; ii < arraySize; ++ii)
    {
        // Use pSeedArray to randomize seedGenerator.  Store the
        // intermediate results in generatedSeeds[] so that the seeded
        // Random depends on the values in pSeedArray as well as the
        // final seedGenerator.
        seedGenerator.stream += static_cast<UINT64>(pSeedArray[ii]) << 1;
        generatedSeeds[ii % SEED_SIZE] += PCGRandom(&seedGenerator);
    }
    // Flush the last pSeedArray[] value into seedGenerator.state
    generatedSeeds[arraySize % SEED_SIZE] += PCGRandom(&seedGenerator);
    for (size_t ii = 0; ii <  SEED_SIZE; ++ii)
    {
        // Finish generating generatedSeed[] with the
        // fully-initialized seedGenerator
        generatedSeeds[ii] += PCGRandom(&seedGenerator);
    }

    SeedRandom((static_cast<UINT64>(generatedSeeds[0]) << 32) |
               generatedSeeds[4],
               (static_cast<UINT64>(generatedSeeds[1]) << 32) |
               generatedSeeds[5],
               (static_cast<UINT64>(generatedSeeds[2]) << 32) |
               generatedSeeds[6],
               (static_cast<UINT64>(generatedSeeds[3]) << 32) |
               generatedSeeds[7]);
}

//------------------------------------------------------------------------------
// Seeding function for people who understand the PCG algorithm well
// enough to write good seeds.
//------------------------------------------------------------------------------
void Random::SeedRandom(UINT64 seedA, UINT64 seedB, UINT64 seedC, UINT64 seedD)
{
    if (m_UseOld)
    {
        const UINT64 seed = seedA ^ seedB ^ seedC ^ seedD;
        Old::SeedRandom(this, static_cast<UINT32>(seed ^ (seed >> 32)));
        return;
    }

    PCGInit(&m_State[0], seedA, seedB);
    PCGInit(&m_State[1], seedC, seedD);
    m_HaveSavedNormal = false;
    m_Initialized     = true;
}

//------------------------------------------------------------------------------
// Get random UINT32 between 0 and 0xFFFFFFFF, inclusive.  Very fast.
//------------------------------------------------------------------------------
UINT32 Random::GetRandom()
{
    if (m_UseOld)
    {
        return Old::GetRandom(this);
    }

#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    if (!m_Enabled)
    {
        return 0;
    }

    return PCGRandom(&m_State[0]);
}

//------------------------------------------------------------------------------
// Get random UINT32 between Min and Max, inclusive.  Slower.
//------------------------------------------------------------------------------
UINT32 Random::GetRandom(UINT32 min, UINT32 max)
{
    if (m_UseOld)
    {
        return Old::GetRandom(this, min, max);
    }

    return static_cast<UINT32>(GetRandom64(min, max));
}

//------------------------------------------------------------------------------
// Get random UINT64
//------------------------------------------------------------------------------
UINT64 Random::GetRandom64()
{
    if (m_UseOld)
    {
        return Old::GetRandom64(this);
    }

#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    if (!m_Enabled)
    {
        return 0;
    }

    // Generate a 64-bit random by concatenating two psuedo-random 32-bit numbers.
    const UINT64 hi = PCGRandom(&m_State[0]);
    const UINT64 lo = PCGRandom(&m_State[1]);
    return (hi << 32) + lo;
}

//------------------------------------------------------------------------------
// Get random UINT64 between Min and Max, inclusive.
//------------------------------------------------------------------------------
UINT64 Random::GetRandom64(UINT64 min, UINT64 max)
{
    if (m_UseOld)
    {
        return Old::GetRandom64(this, min, max);
    }

#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    LWDASSERT(min <= max);
    if (!m_Enabled || min > max)
    {
        return 0;
    }

    if (min == max)
    {
        return min;
    }

    if (min == 0 && max == ~0ULL)
    {
        return GetRandom64();
    }

    const UINT64 delta = max - min;

    if (delta == ~0U)
    {
        return min + static_cast<UINT64>(GetRandom());
    }

    // Need only 32 bits
    if (delta < ~0U)
    {
        const UINT32 mask      = delta + 1U;
        const UINT32 threshold = (~mask + 1U) % mask;
        int          sel       = 0; // flip-flop between PCG state 0 and 1

        for (;; sel ^= 1)
        {
            const UINT32 r = PCGRandom(&m_State[sel]);
            if (r >= threshold)
            {
                return min + (r % mask);
            }
        }
    }
    // Need full 64 bits
    else
    {
        const UINT64 mask      = delta + 1U;
        const UINT64 threshold = (~mask + 1U) % mask;

        for (;;)
        {
            const UINT64 r = GetRandom64();
            if (r >= threshold)
            {
                return min + (r % mask);
            }
        }
    }
}

//------------------------------------------------------------------------------
// Get random single-precision float between Min (inclusive) and Max (exclusive).
// Returns one of 2*32 float values evenly spaced across the range.
// About as fast as GetRandom(UINT32, UINT32).
//------------------------------------------------------------------------------
float Random::GetRandomFloat(double min, double max)
{
    if (m_UseOld)
    {
        return Old::GetRandomFloat(this, min, max);
    }

#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    LWDASSERT(min <= max);
    if (!m_Enabled || min > max)
    {
        return 0;
    }

    if (min == max)
    {
        return min;
    }

    // Generate a value uniformly distributed between 1 and 2, with 1 inclusive
    // and 2 exclusive.
    const UINT32 mantissa = GetRandom(0, 0x007F'FFFFU);
    union
    {
        UINT32 u;
        float  f;
    } u2f;
    u2f.u = mantissa | 0x3F80'0000U; // Set exponent to 0
    const double r = u2f.f;

    // Colwert to [min, max) range, use double to reduce loss of randomness/uniformity
    const double delta = max - min;
    return static_cast<float>(min + delta * (r - 1.0));
}

//------------------------------------------------------------------------------
// Get random double between Min (inclusive) and Max (exclusive).
// Returns one of 2*64 double values evenly spaced across the range.
// Twice as slow as GetRandomFloat().
//------------------------------------------------------------------------------
double Random::GetRandomDouble(double min, double max)
{
    if (m_UseOld)
    {
        return Old::GetRandomDouble(this, min, max);
    }

#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    LWDASSERT(min <= max);
    if (!m_Enabled || min > max)
    {
        return 0;
    }

    if (min == max)
    {
        return min;
    }

    // Generate a value uniformly distributed between 1 and 2, with 1 inclusive
    // and 2 exclusive.
    const UINT64 mantissa = GetRandom64(0, 0x000F'FFFF'FFFF'FFFFU);
    union
    {
        UINT64 u;
        double d;
    } u2d;
    u2d.u = mantissa | 0x3FF0'0000'0000'0000U; // Set exponent to 0
    const double r = u2d.d;

    // Colwert to [min, max) range
    const double delta = max - min;
    return min + delta * (r - 1.0);
}

//------------------------------------------------------------------------------
// Get float with a random mantissa and sign, with the binary exponent
// clamped to a range.  Useful to prevent overflow/underflow.
// If maxPow2 is 128 or greater, NAN and INF values may be returned.
//------------------------------------------------------------------------------
float Random::GetRandomFloatExp(INT32 minPow2, INT32 maxPow2)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    LWDASSERT(minPow2 <= maxPow2);
    if (!m_Enabled || minPow2 > maxPow2)
    {
        return 0;
    }

    const INT32 float32ExpBits  = 8;
    const INT32 float32ExpMask  = ((1<<float32ExpBits)-1);
    const INT32 float32ExpBias  = 127;
    const INT32 float32MinExp   = -127;
    const INT32 float32MaxExp   = 128;
    const INT32 float32MantBits = 23;
    const INT32 float32MantMask = ((1<<float32MantBits)-1);

    // Range enforcement:
    //   -127 <= minPow2 <= maxPow2 <= 128
    maxPow2 = MINMAX(float32MinExp, maxPow2, float32MaxExp);
    minPow2 = MINMAX(float32MinExp, minPow2, maxPow2);

    const INT32 exp =
            float32ExpMask &
            (minPow2 + float32ExpBias +
             static_cast<INT32>(GetRandom(
                    0,
                    static_cast<UINT32>(maxPow2 - minPow2))));

    const INT32 mant = static_cast<INT32>(GetRandom()) >> (32-float32MantBits);

    INT32 fAsI =
            (mant & 0x80000000U) |     // sign
            (exp << float32MantBits) | // exponent-127 in base-2
            (mant & float32MantMask);  // mantissa

    union
    {
        float f;
        INT32 i;
    }
    tmp;
    tmp.i = fAsI;
    return tmp.f;
}

//------------------------------------------------------------------------------
// Get double with a random mantissa and sign, with the binary exponent
// clamped to a range.  Useful to prevent overflow/underflow.
// If maxPow2 is 1024 or greater, NAN and INF values may be returned.
//------------------------------------------------------------------------------
double Random::GetRandomDoubleExp(INT32 minPow2, INT32 maxPow2)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    LWDASSERT(minPow2 <= maxPow2);
    if (!m_Enabled || minPow2 > maxPow2)
    {
        return 0;
    }

    const INT64 float64ExpBits  = 11;
    const INT64 float64ExpMask  = ((1<<float64ExpBits)-1);
    const INT64 float64ExpBias  = 1023;
    const INT32 float64MinExp   = -1023;
    const INT32 float64MaxExp   = 1024;
    const INT64 float64MantBits = 52;
    const INT64 float64MantMask = ((1LL<<float64MantBits)-1);

    // Range enforcement:
    //   -1023 <= minPow2 <= maxPow2 <= 1024
    maxPow2 = MINMAX(float64MinExp, maxPow2, float64MaxExp);
    minPow2 = MINMAX(float64MinExp, minPow2, maxPow2);

    const INT64 exp =
            float64ExpMask &
            (minPow2 + float64ExpBias +
             static_cast<INT32>(GetRandom(
                    0,
                    static_cast<UINT32>(maxPow2 - minPow2))));

    const INT64 mant =
            static_cast<INT64>(GetRandom()) |
            (static_cast<INT64>(GetRandom()) << 32);

    INT64 dAsI =
            (mant & 0x8000000000000000LL) |     // sign
            (exp << float64MantBits) |          // exponent-1023 in base-2
            (mant & float64MantMask);           // mantissa

    union
    {
        double d;
        INT64  i;
    }
    tmp;
    tmp.i = dAsI;
    return tmp.d;
}

//------------------------------------------------------------------------------
// Shuffle the deck in place.
// If numSwaps == 0, goes through the deck exactly once (decksize-1 swaps).
// If (0 < numSwaps < deckSize-1), only the first numSwaps items are guaranteed
// to be random.
//------------------------------------------------------------------------------
template<class T> void TShuffle
(
   Random * pRandom,
   UINT32   deckSize,
   T      * deck,
   UINT32   numSwaps
)
{
   LWDASSERT( deck );
   if( deckSize == 0 )
      return;

   if( numSwaps == 0 )
      numSwaps = deckSize-1;

   // The outer loop lets us to skip a % operation in the inner loop,
   // and still allow double shuffling by paranoid programmers.

   while( numSwaps )
   {
      UINT32 numSwapsThisLoop = numSwaps;

      if( numSwapsThisLoop > deckSize-1 )
         numSwapsThisLoop = deckSize-1;

      UINT32 swap;
      for(swap = 0; swap < numSwapsThisLoop; ++swap )
      {
         UINT32 idx = pRandom->GetRandom(swap, deckSize-1);

         T tmp  = deck[swap];
         deck[swap]  = deck[idx];
         deck[idx]   = tmp;
      }
      numSwaps -= numSwapsThisLoop;
   }
}
void Random::Shuffle (UINT32 deckSize, UINT32 * deck, UINT32 numSwaps /* =0 */)
{
   TShuffle(this, deckSize, deck, numSwaps);
}
void Random::Shuffle (UINT32 deckSize, UINT64 * deck, UINT32 numSwaps /* =0 */)
{
   TShuffle(this, deckSize, deck, numSwaps);
}
void Random::Shuffle (UINT32 deckSize, void ** deck, UINT32 numSwaps /* =0 */)
{
   TShuffle(this, deckSize, deck, numSwaps);
}

//------------------------------------------------------------------------------
// Generate a normally-distributed float.
// Algorithm copied from Numerical Recipes in C, 2nd edition, section 7.2.
// This is the polar variation of the Box-Muller method.
//------------------------------------------------------------------------------
float Random::GetNormalFloat()
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
    LWDASSERT(m_Initialized);
#endif
    LWDASSERT(m_Enabled);
    if (!m_HaveSavedNormal)
    {
        // Pick two uniformly-distrubuted numbers between -1 and 1.
        // Keep trying until we get a tuple within the unit circle.

        float v1, v2;
        float rsq;
        do
        {
            v1 = GetRandomFloat(-1.0F, 1.0F);
            v2 = GetRandomFloat(-1.0F, 1.0F);
            rsq = v1*v1 + v2*v2;
        }
        while (rsq >= 1.0 || rsq == 0.0);

        // Box-Muller transform to get 2 values -- save one, return the other.

        const float fac = sqrt(-2.0F * log(rsq)/rsq);
        m_SavedNormal = v1 * fac;
        m_HaveSavedNormal = true;
        return v2 * fac;
    }
    else
    {
        // Every other call, return the spare value from the last time.

        m_HaveSavedNormal = false;
        return m_SavedNormal;
    }
}

//------------------------------------------------------------------------------
// Get a random int with specified range and weighted probability.
//------------------------------------------------------------------------------
UINT32 Random::Pick(const PickItem* pItems)
{
#ifndef _WIN32 // mdiag crashes on Windows due to uninitialied Random objects somewhere
   LWDASSERT(m_Initialized);
#endif
   LWDASSERT(m_Enabled);
   LWDASSERT(pItems);
   LWDASSERT(pItems->ScaledProb > 0); // make sure PrepareItems() has run.

   if (pItems->ScaledProb != 0xFFFFFFFF)
   {
      // If there is more than one item, pick one item.
      // WARNING: if PrepareItems() hasn't run, this is an infinite loop!

      UINT32 r = GetRandom();

      while (pItems->ScaledProb < r)
         ++pItems;
   }

   // Get a random number within the range for this choice, using the
   // precallwlated range ilwerse to avoid a divide.
   UINT32 r = GetRandom();
   return pItems->Min +
            UINT32(pItems->ScaledRange * double(r));
}

//------------------------------------------------------------------------------
// PreparePickItems:
//
// Colwert the probability of each choice in the array to a fraction of
// the 32-bit range, and store the aclwmulated value as ScaledProb.
// Also, callwlate a floating-point colwersion factor between the 32-bit
// random-number generator range and each Item.Range.
//
// By precallwlating all this, we speed up the Pick() function a bit.
//------------------------------------------------------------------------------
void Random::PreparePickItems(INT32 NumItems, PickItem * pItems)
{
   LWDASSERT(NumItems > 0);
   LWDASSERT(pItems);

   double ProbSum = 0.0;
   INT32 i;

   // Get the sum of the relative probabilities.
   for(i = 0; i < NumItems; i++)
   {
      LWDASSERT(pItems[i].RelProb);     // should not be 0% likely!

      ProbSum += pItems[i].RelProb;
   }

   // Get the colwersion factor to scale probability to the full 32-bit range.
   double ProbScaler = TwoExp32 / ProbSum;

   // Fill in ScaledProb and the range colwersion factor for each Item.
   ProbSum = 0.0;
   for(i = 0; i < NumItems; i++)
   {
      ProbSum += ProbScaler * double(pItems[i].RelProb);

      // Watch for rollover here -- ProbSum ends up approx. = TwoExp32.
      pItems[i].ScaledProb = UINT32(floor(ProbSum + 0.5) - 1.0);

      if(pItems[i].Max < pItems[i].Min)
      {
         // min, max out of order -- swap them.
         UINT32 tmp = pItems[i].Min;
         pItems[i].Min = pItems[i].Max;
         pItems[i].Max = tmp;
      }
      pItems[i].ScaledRange = (double(pItems[i].Max - pItems[i].Min) + 1.0)
                              * TwoExp32Ilw;
   }

   // The last item must always have the ScaledProb of 2^32 - 1.
   LWDASSERT(pItems[NumItems-1].ScaledProb == 0xFFFFFFFF);
}
