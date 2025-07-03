#include "libbit.h"

// Note: This file is used by both the kernel an user-space
//       As such it may not contain calls to LibosAssert or KernelAssert.

static const LwU8 debruijn64[] = {
    0,  1,  2,  53, 3,  7,  54, 27, 4,  38, 41, 8,  34, 55, 48, 28, 62, 5,  39, 46, 44, 42,
    22, 9,  24, 35, 59, 56, 49, 18, 29, 11, 63, 52, 6,  26, 37, 40, 33, 47, 61, 45, 43, 21,
    23, 58, 17, 10, 51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12};

static const LwU64 hash = 0x022fdd63cc95386dull;

/**
 * @brief Compute the log2 of a power of 2
 * 
 * @param pow2 
 *    This value must be a power of 2.
 *    pow2 && LibosBitLowest(pow2) == pow2
 */
LwU8 LibosLog2GivenPow2(LwU64 pow2) 
{ 
    return debruijn64[pow2 * hash >> 58]; 
}

/**
 * @brief Computes the mask of the lowest bit set.
 *        Returns 0 if no bits are set.
 * 
 * @param mask 
 */
LwU64 LibosBitLowest(LwU64 mask) 
{
    return mask & (-mask);
}

/**
 * @brief Computes the mask of the highest bit set.
 *        Returns 0 if no bits are set.
 * 
 * @param mask 
 */
LwU64 LibosBitHighest(LwU64 mask)
{
    // Propagate the topmost bit set to all positions below
    mask |= mask >> 32;
    mask |= mask >> 16;
    mask |= mask >> 8; 
    mask |= mask >> 4;    
    mask |= mask >> 2;
    mask |= mask >> 1; 
    
    // At this point we know that we have 2^n-1 since all lower bits are set.
    return mask & ((~mask >> 1)^0x8000000000000000ULL);
}

/**
 * @brief Rounds up to the next power of 2. 
 *        Returns the smallest power of 2 greater than or equal to mask.
 * @param mask 
 */
LwU64 LibosBitRoundUpPower2(LwU64 mask)
{
    // Subtract one to guarantee LibosBitRoundUpPower2(1<<k) == (1<<k)
    mask--; 

    // Propagate the topmost bit set to all positions below
    mask |= mask >> 32;
    mask |= mask >> 16;
    mask |= mask >> 8; 
    mask |= mask >> 4;    
    mask |= mask >> 2;
    mask |= mask >> 1; 
    
    // At this point we know that we have 2^n-1 since all lower bits are set.
    return mask + 1;
}

/**
 * @brief Rounds up to a power of 2 and returns the bit index
 * Returns ceil(log2(mask))
 * 
 * @param mask 
 */
LwU64 LibosBitLog2Ceil (LwU64 mask)
{
    LwU64 nextPower2 = LibosBitRoundUpPower2(mask);

    if (!nextPower2)
        return 64;

    return LibosLog2GivenPow2(nextPower2);
}

/**
 * @brief Returns the index of the top bit set
 * Returns floor(log2(mask))
 * 
 * @param mask 
 */

LwU64 LibosBitLog2Floor(LwU64 mask) 
{
    LwU64 nextPower2 = LibosBitHighest(mask);
    return LibosLog2GivenPow2(nextPower2);
}