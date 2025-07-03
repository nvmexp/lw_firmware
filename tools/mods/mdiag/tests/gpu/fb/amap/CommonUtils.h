#ifndef _COMMONUTILS__H_
#define _COMMONUTILS__H_

#include <vector>
#include "lwos.h"

#include "mdiag/utils/types.h"

#include <boost/static_assert.hpp>

// typedef UINT32 uint32;
// typedef UINT64 uint64;

// This header file is for common utility functions that should have a single implementation and be shared by all units.
// Don't reinvent the wheel.

//////// Bit manipulation

// check a bit in a bitmask
template<typename T>
bool bitSet(T src, int i)
{
    return (src >> i) & 1;
}

// extract the bit at position b
template<int b>
UINT32 bit(UINT32 src)
{
    BOOST_STATIC_ASSERT(b>=0 && b<32);
    return (src >> (UINT32)b) & 1;
}
template<int b>
UINT64 bit(UINT64 src)
{
    BOOST_STATIC_ASSERT(b>=0 && b<64);
    return (src >> (UINT64)b) & 1;
}

// extract the bits in the posisitions between msb and lsb inclusive
template<int msb, int lsb>
UINT32 bits(UINT32 src)
{
    BOOST_STATIC_ASSERT(msb>=0 && lsb>=0 && msb>=lsb && msb<32);
    return (msb==31 && lsb==0) ? src : ((src>>lsb) & (((UINT32)1<<(UINT32)(msb-lsb+1))-1));
}
template<int msb, int lsb>
UINT64 bits(UINT64 src)
{
    BOOST_STATIC_ASSERT(msb>=0 && lsb>=0 && msb>=lsb && msb<64);
    return (msb==63 && lsb==0) ? src : ((src>>lsb) & (((UINT64)1<<(UINT64)(msb-lsb+1))-1));
}

template<typename T>
UINT32 forwardBitScan(T src)
{
    if (src != 0) {
        UINT32 result = 0;
        while ((src & 1) != 1) {
            src >>= 1;
            result++;
        }
        return result;
    } else {
        return (UINT32)~0;
    }
}

//////// Bit counting

template<int N>
UINT32 bitCount(UINT32 x) {
    static const UINT32 bitCount4[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};

    if (N <= 4)
        return bitCount4[x];
    else if (N <= 8)
        return bitCount4[x & 15] + bitCount4[x >> 4];
    else if (N <= 16)
        return bitCount4[x & 15] + bitCount4[(x >> 4) & 15] + bitCount4[(x >> 8) & 15] + bitCount4[x >> 12];
    else
        return bitCount4[x & 15] + bitCount4[(x >> 4) & 15] + bitCount4[(x >> 8) & 15] + bitCount4[(x >> 12) & 15] +
                bitCount4[(x >> 16) & 15] + bitCount4[(x >> 20) & 15] + bitCount4[(x >> 24) & 15] + bitCount4[x >> 28];
}

//////// STL helpers

template<typename T>
void initialize2DArray(std::vector<std::vector<T> >& array, size_t M, size_t N)
{
    array.resize(M);
    for(size_t m = 0; m < M; m++) {
        array[m].resize(N);
    }
}

#endif
