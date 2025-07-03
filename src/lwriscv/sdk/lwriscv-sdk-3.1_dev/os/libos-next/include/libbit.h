#include <lwtypes.h>

static inline LwU64 LibosRoundDown(LwU64 value, LwU64 pow2)
{
    return (value) &~ (pow2 - 1);
}
static inline LwU64 LibosRoundUp(LwU64 value, LwU64 pow2)
{
    return LibosRoundDown(value + pow2 - 1, pow2);
}

static inline LwU64 LibosMinU64(LwU64 a, LwU64 b)
{
    return a < b ? a : b;
}

static inline LwU64 LibosMaxU64(LwU64 a, LwU64 b)
{
    return a > b ? a : b;
}

static inline LwU64 LibosBitAndBitsBelow(LwU64 a) {
    // @note: This is modular arithmetic and we ensure LibosBitAndBitsBelow(0x8000000000000000ULL)=-1ULL
    return 2 * a - 1;
}

static inline LwU64 LibosBitAndBitsAbove(LwU64 a) {
    return ~(a-1);
}

static inline LwU64 LibosBitsAbove(LwU64 a) {
    return ~LibosBitAndBitsBelow(a);
}

LwU64 LibosBitLowest(LwU64 mask);
LwU64 LibosBitHighest(LwU64 x);

LwU8  LibosLog2GivenPow2(LwU64 pow2);

LwU64 LibosBitRoundUpPower2(LwU64 x);

LwU64 LibosBitLog2Ceil (LwU64 x);
LwU64 LibosBitLog2Floor(LwU64 x);