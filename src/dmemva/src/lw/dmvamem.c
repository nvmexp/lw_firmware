/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvamem.h"
#include "dmvainstrs.h"
#include "dmvautils.h"
#include "dmvaregs.h"
#include "dmvavars.h"
#include <lwmisc.h>

LwU32 fbBufBlkIdxDmem;
LwU32 fbBufBlkIdxImem;

LwU32 getDmemTagWidth(void)
{
    return DRF_VAL(_PFALCON, _FALCON_HWCFG1, _DMEM_TAG_WIDTH, DMVAREGRD(HWCFG1));
}

LwU32 getImemTagWidth(void)
{
    return DRF_VAL(_PFALCON, _FALCON_HWCFG1, _TAG_WIDTH, DMVAREGRD(HWCFG1));
}

LwU32 dmemcVal(LwU32 address, LwBool bAincw, LwBool bAincr, LwBool bSetTag, LwBool bSetLvl,
               LwBool bVa)
{
    return REF_NUM(LW_PFALCON_FALCON_DMEMC_ADDRESS, address) |
           REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCW, bAincw ? 1 : 0) |
           REF_NUM(LW_PFALCON_FALCON_DMEMC_AINCR, bAincr ? 1 : 0) |
           REF_NUM(LW_PFALCON_FALCON_DMEMC_SETTAG, bSetTag ? 1 : 0) |
           REF_NUM(LW_PFALCON_FALCON_DMEMC_SETLVL, bSetLvl ? 1 : 0) |
           REF_NUM(LW_PFALCON_FALCON_DMEMC_VA, bVa ? 1 : 0);
}

BlockStatus getBlockStatus(LwU32 physBlockId)
{
    dmblk_t d = dmva_dmblk(physBlockId);
    // a block must be invalid if it is pending
    UASSERT(!d.pending || !d.valid);
    if (d.pending)
    {
        return PENDING;
    }
    else if (!d.valid)
    {
        return INVALID;
    }
    else if (d.dirty)
    {
        return VALID_AND_DIRTY;
    }
    else
    {
        return VALID_AND_CLEAN;
    }
}

void setBlockStatus(BlockStatus blockStatus, LwU32 addr, LwU32 physBlockId)
{
    UASSERT(blockStatus < N_BLOCK_STATUS);
    falc_setdtag(addr, physBlockId);  // always set tag
    switch (blockStatus)
    {
        case INVALID:
            falc_dmilw(physBlockId);
            break;
        case VALID_AND_CLEAN:
            break;
        case VALID_AND_DIRTY:
            ;
            volatile LwU32 data = *(volatile LwU32 *)(physBlockId * BLOCK_SIZE);
            *(volatile LwU32 *)(physBlockId * BLOCK_SIZE) = data;
            break;
        case PENDING:
            ;
            LwU32 dmemc = DMVAREGRD(DMEMC(0));
            LwU32 dmemt = DMVAREGRD(DMEMT(0));
            DMVAREGWR(DMEMC(0), dmemcVal(physBlockId * BLOCK_SIZE, 0, 0, 1, 0, 0));
            DMVAREGWR(DMEMT(0), addr >> 8);
            DMVAREGWR(DMEMD(0), DMVAREGRD(DMEMT(0)));
            DMVAREGWR(DMEMC(0), dmemc);
            DMVAREGWR(DMEMT(0), dmemt);
            break;
        default:
            halt();
    }
}

void resetBlockStatus(LwU32 blkId)
{
    if (dmva_dmblk(blkId).levelMask != LEVEL_MASK_ALL)
    {
        callHS(HS_SET_BLK_LVL, blkId, LEVEL_MASK_ALL);
    }
    setBlockStatus(INVALID, 0, blkId);
}

void resetAllBlockStatus(void)
{
    LwU32 blockIdx;
    LwU32 dmemSize = getDmemSize();
    for (blockIdx = 0; blockIdx < dmemSize; blockIdx++)
    {
        resetBlockStatus(blockIdx);
    }
}

LwU32 trimVa(LwU32 addr)
{
    return REF_VAL(getDmemTagWidth() + 8 - 1 : 0, addr);
}

LwU32 trimTag(LwU32 tag)
{
    return REF_VAL(getDmemTagWidth() - 1 : 0, tag);
}

LwU32 getDmemSize(void)
{
    LwU32 hwcfg = DMVAREGRD(HWCFG);
    return REF_VAL(LW_PFALCON_FALCON_HWCFG_DMEM_SIZE, hwcfg);
}

LwU32 getImemSize(void)
{
    LwU32 hwcfg = DMVAREGRD(HWCFG);
    return REF_VAL(LW_PFALCON_FALCON_HWCFG_IMEM_SIZE, hwcfg);
}

LwU32 getDmemVaSize(void)
{
    return POW2(getDmemTagWidth());
}

LwU32 getImemVaSize(void)
{
    return POW2(getImemTagWidth());
}

LwU32 maxVa(void)
{
    return BIT(getDmemTagWidth() + 8) - 1;
}

void resetDmvactl(void)
{
    DMVAREGWR(DMVACTL, getDmemSize());
}

// ctx is unchanged so you can verify with the same ctx later
void fillMem(RNGctx ctx, LwU8 *addr, LwU32 nBytes)
{
    LwU32 nBytesWritten;
    union
    {
        LwU32 integer;
        LwU8 array[4];
    } uRand;
    for (nBytesWritten = 0; nBytesWritten < nBytes; nBytesWritten++, addr++)
    {
        if (aligned(nBytesWritten, 4))
        {
            uRand.integer = rand32Ctx(&ctx);
        }
        *addr = uRand.array[nBytesWritten % 4];
    }
}

// returns the number of verified bytes
LwU32 verifyMem(RNGctx ctx, const LwU8 *addr, LwU32 nBytes)
{
    LwU32 nBytesRead;
    union
    {
        LwU32 integer;
        LwU8 array[4];
    } uRand;
    for (nBytesRead = 0; nBytesRead < nBytes; nBytesRead++, addr++)
    {
        if (aligned(nBytesRead, 4))
        {
            uRand.integer = rand32Ctx(&ctx);
        }
        if (*addr != uRand.array[nBytesRead % 4])
        {
            return nBytesRead;
        }
    }
    return nBytesRead;
}
