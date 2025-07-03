/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_RAND_H
#define DMVA_RAND_H

#include <lwtypes.h>

typedef struct
{
    LwU32 x;
    LwU32 y;
    LwU32 z;
    LwU32 w;
} RNGctx;

// implemenation of Xorshift with 128 bits of state
LwU32 rand32(void);
LwU32 rand(LwU32 range);
void seedRNG(LwU32 seed);

// The distribution of this generator will not be perfect
// if the range is not close to a power of 2.
LwU32 randRange(LwU32 start, LwU32 end);

// We will want to fill a buffer with test data, and later
// verify what was written was correct. With a rand ctx, we
// can duplicate the RNG state so we can fill/verify.
RNGctx newRNGctx(void);
LwU32 rand32Ctx(RNGctx *rngCtx);

LwU32 randCtx(RNGctx *rngCtx, LwU32 range);
LwU32 randRangeCtx(RNGctx *rngCtx, LwU32 start, LwU32 end);

// this is intended to be used with < 10 numbers
void randUnique(LwU32 *nums, LwU32 amt, LwU32 min, LwU32 max);

LwU32 randLevelMask(void);
LwU32 randPa(void);
LwU32 randVa(void);
LwU32 randPhysBlkAboveStack(void);
LwU32 randVaAboveBound(void);

#endif
