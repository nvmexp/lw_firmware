/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvarand.h"
#include "dmvautils.h"
#include "dmvaassert.h"
#include "dmvaregs.h"
#include "dmvamem.h"
#include "dmvavars.h"

RNGctx rngCtx;

LwU32 rand32(void)
{
    return rand32Ctx(&rngCtx);
}

LwU32 rand(LwU32 range)
{
    return randCtx(&rngCtx, range);
}

#define INITIAL_RAND_CYCLES 0

void seedRNG(LwU32 seed)
{
    rngCtx.x = seed ^ 0x0d181353;
    rngCtx.y = seed ^ 0xd555d140;
    rngCtx.z = seed ^ 0x7f3c6286;
    rngCtx.w = seed ^ 0x9e46edd8;
    LwU32 i;
    for (i = 0; i < INITIAL_RAND_CYCLES; i++)
    {
        rand32Ctx(&rngCtx);
    }
}

LwU32 randRange(LwU32 start, LwU32 end)
{
    return randRangeCtx(&rngCtx, start, end);
}

RNGctx newRNGctx(void)
{
    RNGctx newCtx;
    newCtx.x = rand32();
    newCtx.y = rand32();
    newCtx.z = rand32();
    newCtx.w = rand32();
    return newCtx;
}

LwU32 rand32Ctx(RNGctx *rngCtx)
{
    LwU32 t = rngCtx->x ^ (rngCtx->x << 11);
    rngCtx->x = rngCtx->y;
    rngCtx->y = rngCtx->z;
    rngCtx->z = rngCtx->w;
    return rngCtx->w = rngCtx->w ^ (rngCtx->w >> 19) ^ t ^ (t >> 8);
}

LwU32 randCtx(RNGctx *rngCtx, LwU32 range)
{
    return rand32Ctx(rngCtx) % range;
}

LwU32 randRangeCtx(RNGctx *rngCtx, LwU32 start, LwU32 end)
{
    UASSERT(end >= start);
    if (start == 0 && end == LW_U32_MAX)
    {
        return rand32Ctx(rngCtx);
    }
    LwU32 range = end - start + 1;
    return start + (rand32Ctx(rngCtx) % range);
}

void randUnique(LwU32 *nums, LwU32 amt, LwU32 min, LwU32 max)
{
    UASSERT(min <= max);
    UASSERT((amt == 0) || ((max - min) >= (amt - 1)));
    LwU32 i;
    for (i = 0; i < amt; i++)
    {
    regenerate:
        nums[i] = randRange(min, max);
        LwU32 j;
        for (j = 0; j < i; j++)
        {
            if (nums[i] == nums[j])
            {
                goto regenerate;
            }
        }
    }
}

LwU32 randLevelMask(void)
{
    return rand32() & LEVEL_MASK_ALL;
}

LwU32 randPa(void)
{
    return (rand32() % (getDmemSize() << 8)) & ~0x03;
}

LwU32 randVa(void)
{
    return (rand32() % (maxVa() + 1)) & ~0x03;
}

LwU32 randPhysBlkAboveStack(void)
{
    return randRange(stackBottom / BLOCK_SIZE, getDmemSize() - 1);
}

LwU32 randVaAboveBound(void)
{
    return randRange(DMVAREGRD(DMVACTL) * BLOCK_SIZE, POW2(getDmemTagWidth() + 8) - 1) & ~0x03;
}
