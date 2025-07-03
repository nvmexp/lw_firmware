/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * Written in 2014-2016 by Sebastiano Vigna (vigna@acm.org)
 *
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 *
 * See <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

//
// The RNG we are using isn't that sensitive to initial conditions, but we should
// still ensure that threads are unlikely to have the same initial state.
//
// Let's seed using the SplitMix64 generator (which is a bit overkill for our purposes)
//
template<UINT32 N>
__device__ __forceinline__ void InitRand(UINT64 s[N], const UINT64 randseed)
{
    UINT64 x = randseed;
    UINT64 z;
    for (UINT32 i = 0; i < N; i++)
    {
        z = (x += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        s[i] = z ^ (z >> 31);
    }
}

//
// We use the xorshift128+ algorithm to generate 64-bit random numbers
//
// xorshift128+ uses 128 bits of state and is a very robust generator.
// There is no weakness in the lower-order bits, unlike the Linear Congruence Generator (LCG)
// used by LwdaRandom. The period is also 2^128 - 1, which is better than the LCG's period
// of 2^32.
//
// It is slower than a LCG though, so it isn't a drop-in replacement.
//
__device__ __forceinline__ UINT64 FastRand(UINT64 s[2])
{
    UINT64 x = s[0];
    const UINT64 y = s[1];
    s[0] = y;
    x ^= x << 23;
    s[1] = x ^ y ^ (x >> 17) ^ (y >> 26);
    return s[1] + y;
}
