/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvatests.h"
#include "dmvacommon.h"
#include "dmvautils.h"
#include "dmvadma.h"
#include "dmvarw.h"
#include "dmvarand.h"
#include "dmvainstrs.h"
#include "dmvaregs.h"
#include "dmvamem.h"
#include "dmvavars.h"
#include "dmvainterrupts.h"
#include "scp/inc/scp_internals.h"
#include <lwmisc.h>

// used for communicating an error status to RM when the status cannot fit in the general purpose
// registers
static LwU32 errorStatus[2];

DMVA_RC init(void)
{
    setStackOverflowProtection(dataSize, LW_TRUE);
    programDmcya();
    enableDMEMaperture();
    setupCtxDma();

    installInterruptHandler(defaultInterruptHandler);
    installExceptionHandler(defaultExceptionHandler);

    seedRNG(rmRead32());

    fbBufBlkIdxDmem = rmRead32();
    fbBufBlkIdxImem = rmRead32();

    // Mark HS-only code secure
    loadImem(selwreCodeStart, selwreCodeStart, selwreSharedStart - selwreCodeStart, LW_TRUE);

    return DMVA_OK;
}

TESTDEF(1, readRegsTest)
{
    // test reg writes/reads as well as return codes
    DMVAREGWR(MAILBOX1, REG_TEST_MAILBOX1);
    return REG_TEST_MAILBOX0;
}

TESTDEF(2, ilwalidOnResetTest)
{
    LwU32 dmemSize = getDmemSize();
    LwU32 blkIdx;
    for (blkIdx = 0; blkIdx < dmemSize; blkIdx++)
    {
        if (getBlockStatus(blkIdx) != INVALID)
        {
            return DMVA_FAIL;
        }
    }
    return DMVA_OK;
}

// TODO: verify the bytes after the write do not change
// TODO: offset aligned naturally to read/write
TESTDEF(3, dmemReadWriteTest)
{
    LwBool bVa;
    for (bVa = 0; bVa <= 1; bVa++)
    {
        RNGctx dataCtx = newRNGctx();
        LwU32 physBlk = randPhysBlkAboveStack();
        LwU8 *src;
        LwU8 *dest;
        LwU32 bufSize = 16;
        if (bVa)
        {
            DMVAREGWR(DMVACTL, stackBottom / BLOCK_SIZE);
            LwU32 va = align(randVaAboveBound(), BLOCK_SIZE);
            setBlockStatus(VALID_AND_CLEAN, va, physBlk);
            src = (LwU8 *)(va + 0);
            dest = (LwU8 *)(va + bufSize);
        }
        else
        {
            src = (LwU8 *)(physBlk * BLOCK_SIZE);
            dest = (LwU8 *)(physBlk * BLOCK_SIZE + bufSize);
        }
        fillMem(dataCtx, src, bufSize);
        LwBool bRead;
        RwImplType implType;
        LwU32 implId;
        for (bRead = 0; bRead <= 1; bRead++)
        {
            for (implType = 0; implType <= (bVa ? RW_IMPL_SP : RW_IMPL_DMA); implType++)
            {
                for (implId = 0; implId < nImplsInFamily(bRead, implType); implId++)
                {
                    markSubtestStart();
                    fillMem(newRNGctx(), dest, bufSize);
                    LwU32 nBytes = rwImplFamily(dest, src, implId, bRead, implType);
                    UASSERT(nBytes > 0);
                    LwBool badBytes = verifyMem(dataCtx, dest, nBytes) != nBytes;
                    if (badBytes)
                    {
                        resetDmvactl();
                        resetBlockStatus(physBlk);
                        return DMVA_FAIL;
                    }
                }
            }
        }

        markSubtestStart();
        testCallRet((LwU32)dest, LW_FALSE);

        markSubtestStart();
        testCallRet((LwU32)dest, LW_TRUE);

        if (bVa)
        {
            resetDmvactl();
            resetBlockStatus(physBlk);
        }
    }
    return DMVA_OK;
}

TESTDEF(4, dmblkTest)
{
    DmInstrImpl impl;
    BlockStatus blockStatus;
    LwBool setMask;
    for (setMask = 0; setMask <= 1; setMask++)
    {
        for (impl = 0; impl < N_DM_IMPLS; impl++)
        {
            for (blockStatus = 0; blockStatus < N_BLOCK_STATUS; blockStatus++)
            {
                markSubtestStart();
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 va = randVa();
                LwU32 tag = va >> BLOCK_WIDTH;
                LwBool isValid = (blockStatus == VALID_AND_CLEAN || blockStatus == VALID_AND_DIRTY);
                LwBool isPending = (blockStatus == PENDING);
                LwBool isDirty = (blockStatus == VALID_AND_DIRTY);

                setBlockStatus(blockStatus, va, physBlk);
                LevelMask levelMask = LEVEL_MASK_ALL;
                if (setMask)
                {
                    levelMask = randLevelMask();
                    callHS(HS_SET_BLK_LVL, physBlk, levelMask);
                }

                dmblk_t d = dmblkImpl(physBlk, impl);

                LwBool badZeros = (d.zeros != 0);
                LwBool badTag = (d.tag != tag);
                LwBool badMask = (d.levelMask != levelMask);
                LwBool badValid = (d.valid != isValid);
                LwBool badPending = (d.pending != isPending);
                LwBool badDirty = (d.dirty != isDirty);

#ifdef IGNORE_DIRTY_ON_ILWALID
                if(!isValid)
                {
                    badDirty = LW_FALSE;
                }
#endif

                resetBlockStatus(physBlk);

                if (badZeros)
                {
                    return DMVA_BAD_ZEROS;
                }
                else if (badTag)
                {
                    DMVAREGWR(MAILBOX1, (tag << 16) | (d.tag));
                    return DMVA_BAD_TAG;
                }
                else if (badMask)
                {
                    DMVAREGWR(MAILBOX1, (levelMask << 16) | (d.levelMask));
                    return DMVA_BAD_MASK;
                }
                else if (badValid)
                {
                    return DMVA_BAD_VALID;
                }
                else if (badPending)
                {
                    return DMVA_BAD_PENDING;
                }
                else if (badDirty)
                {
                    return DMVA_BAD_DIRTY;
                }
            }
        }
    }
    return DMVA_OK;
}

// TODO: add VA support
TESTDEF(5, writeSetsDirtyBitTest)
{
    // test write
    LwU32 implId;
    for (implId = 0; implId < N_WRITE_IMPLS; implId++)
    {
        markSubtestStart();
        LwU32 physBlk = randPhysBlkAboveStack();
        setBlockStatus(VALID_AND_CLEAN, randVa(), physBlk);
        // The source address is unimportant because all data goes through the general
        // purpose registers or FB.
        LwU8 *src = (LwU8 *)align(randPa(), BLOCK_SIZE);
        LwU8 *dest = (LwU8 *)(physBlk * BLOCK_SIZE + align(rand(BLOCK_SIZE), 16));

        writeImpl(dest, src, implId);

        BlockStatus blockStatus = getBlockStatus(physBlk);
        resetBlockStatus(physBlk);

        if (blockStatus != VALID_AND_DIRTY)
        {
            return DMVA_FAIL;
        }
    }

    // test call
    LwBool bInterrupt;
    for (bInterrupt = 0; bInterrupt <= 1; bInterrupt++)
    {
        markSubtestStart();
        LwU32 physBlk = randPhysBlkAboveStack();
        setBlockStatus(VALID_AND_CLEAN, randVa(), physBlk);
        LwU32 dest = physBlk * BLOCK_SIZE + align(rand(BLOCK_SIZE), sizeof(LwU32));

        testCallRet(dest, bInterrupt);

        BlockStatus blockStatus = getBlockStatus(physBlk);
        resetBlockStatus(physBlk);

        if (blockStatus != VALID_AND_DIRTY)
        {
            return DMVA_FAIL;
        }
    }

    // test DMEMC/D
    DmvaRegRwImpl impl;
    LwBool bAincw;
    LwBool bAincr;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (bAincw = 0; bAincw <= 1; bAincw++)
        {
            for (bAincr = 0; bAincr <= 1; bAincr++)
            {
                markSubtestStart();
                LwU32 physBlk = randPhysBlkAboveStack();
                setBlockStatus(VALID_AND_CLEAN, randVa(), physBlk);

                LwU32 dest = physBlk * BLOCK_SIZE + align(rand(BLOCK_SIZE), sizeof(LwU32));

                DMVAREGWR_IMPL(DMEMC(0), dmemcVal(dest, bAincw, bAincr, 0, 0, 0), impl);
                DMVAREGWR_IMPL(DMEMD(0), rand32(), impl);

                BlockStatus blockStatus = getBlockStatus(physBlk);
                resetBlockStatus(physBlk);

                if (blockStatus != VALID_AND_DIRTY)
                {
                    return DMVA_FAIL;
                }
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(6, dmtagTest)
{
    enum
    {
        TEST_HIT = 0,
        TEST_MISS,
        TEST_MULTIHIT,
        TEST_OUT_OF_BOUNDS,
        N_DMTAG_TEST_TYPES
    } dmtagTestType;

    DmInstrImpl impl;
    BlockStatus blockStatus;
    LwBool setMask;
    for (dmtagTestType = 0; dmtagTestType < N_DMTAG_TEST_TYPES; dmtagTestType++)
    {
        for (setMask = 0; setMask <= 1; setMask++)
        {
            for (impl = 0; impl < N_DM_IMPLS; impl++)
            {
                for (blockStatus = VALID_AND_CLEAN; blockStatus <= PENDING; blockStatus++)
                {
                    markSubtestStart();
                    LwU32 physBlkArr[MULTIHIT_MAX_MAPPED_BLKS];
                    LwU32 nMappedBlks;
                    LwU32 physBlkIdx;
                    switch (dmtagTestType)
                    {
                        case TEST_HIT:
                            nMappedBlks = 1;
                            break;
                        case TEST_MISS:
                            nMappedBlks = 0;
                            break;
                        case TEST_MULTIHIT:
                            nMappedBlks = randRange(2, MULTIHIT_MAX_MAPPED_BLKS);
                            break;
                        case TEST_OUT_OF_BOUNDS:
                            nMappedBlks = rand(MULTIHIT_MAX_MAPPED_BLKS);
                            break;
                        default:
                            halt();
                            break;
                    }

                    TEST_OVLY(6) void cleanup(void)
                    {
                        resetDmvactl();
                        for (physBlkIdx = 0; physBlkIdx < nMappedBlks; physBlkIdx++)
                        {
                            resetBlockStatus(physBlkArr[physBlkIdx]);
                        }
                    }

                    randUnique(physBlkArr, nMappedBlks, stackBottom / BLOCK_SIZE,
                               getDmemSize() - 1);
                    LwBool isValid =
                        (blockStatus == VALID_AND_CLEAN || blockStatus == VALID_AND_DIRTY);
                    LwBool isPending = (blockStatus == PENDING);
                    LwBool isDirty = (blockStatus == VALID_AND_DIRTY);
                    LevelMask levelMask = LEVEL_MASK_ALL;
                    LwBool isOutOfBound = dmtagTestType == TEST_OUT_OF_BOUNDS;
                    LwBool isMiss = dmtagTestType == TEST_MISS;
                    LwBool isMultihit = dmtagTestType == TEST_MULTIHIT;

                    LwU32 va = isOutOfBound ? randRange(0, stackBottom - 1)
                                            :  // va below stack bottom
                                   randRange(stackBottom, POW2(getDmemTagWidth() + 8) -
                                                              1);  // va above stack bottom

                    for (physBlkIdx = 0; physBlkIdx < nMappedBlks; physBlkIdx++)
                    {
                        if (dmtagTestType == TEST_HIT)
                        {
                            setBlockStatus(blockStatus, va, physBlkArr[physBlkIdx]);
                        }
                        else if (dmtagTestType == TEST_MULTIHIT)
                        {
                            setBlockStatus(rand(2) ? VALID_AND_CLEAN : VALID_AND_DIRTY, va,
                                           physBlkArr[physBlkIdx]);
                        }
                        else if (dmtagTestType == TEST_OUT_OF_BOUNDS)
                        {
                            setBlockStatus(rand(N_BLOCK_STATUS), va, physBlkArr[physBlkIdx]);
                        }
                        if (setMask)
                        {
                            levelMask = randLevelMask();
                            callHS(HS_SET_BLK_LVL, physBlkArr[physBlkIdx], levelMask);
                        }
                    }

                    DMVAREGWR(DMVACTL, stackBottom / BLOCK_SIZE);

                    dmtag_t d = dmtagImpl(va, impl);

                    LwBool badOutOfBound = isOutOfBound != d.outOfBound;

                    if (isOutOfBound)
                    {
                        cleanup();
                        if (badOutOfBound)
                        {
                            return DMVA_FAIL;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    LwBool badMiss = isMiss != d.miss;
                    LwBool badMultihit = isMultihit != d.multihit;

                    if (isMiss || isMultihit)
                    {
                        cleanup();
                        if (badMiss || badMultihit)
                        {
                            return DMVA_FAIL;
                        }
                        else
                        {
                            continue;
                        }
                    }

                    LwBool badBlk = (d.blkIdx != physBlkArr[0]);
                    LwBool badMask = (d.levelMask != levelMask);
                    LwBool badValid = (d.valid != isValid);
                    LwBool badPending = (d.pending != isPending);
                    LwBool badDirty = (d.dirty != isDirty);
                    cleanup();

#ifdef IGNORE_DIRTY_ON_ILWALID
                    if(!isValid)
                    {
                        badDirty = LW_FALSE;
                    }
#endif

                    if (badOutOfBound || badMiss || badMultihit || badBlk)
                    {
                        return DMVA_FAIL;
                    }
                    else if (badMask)
                    {
                        DMVAREGWR(MAILBOX1, (levelMask << 16) | (d.levelMask));
                        return DMVA_BAD_MASK;
                    }
                    else if (badValid)
                    {
                        return DMVA_BAD_VALID;
                    }
                    else if (badPending)
                    {
                        return DMVA_BAD_PENDING;
                    }
                    else if (badDirty)
                    {
                        return DMVA_BAD_DIRTY;
                    }
                }
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(7, dmvaBoundTest)
{
    LwBool bBoundInPhysMem;
    LwBool bVa;
    for (bBoundInPhysMem = 0; bBoundInPhysMem <= 1; bBoundInPhysMem++)
    {
        for (bVa = 0; bVa <= 1; bVa++)
        {
            markSubtestStart();
            LwU32 oldData = rand32();
            LwU32 newData = rand32();

            LwU32 boundBlk;
            if (bBoundInPhysMem)
            {
                boundBlk = randRange(stackBottom / BLOCK_SIZE + 1, getDmemSize() - 1);
            }
            else
            {
                boundBlk = randRange(getDmemSize() + 1, POW2(getDmemTagWidth()) - 1);
            }

            LwU32 physBlk;
            LwU32 off = align(rand(BLOCK_SIZE), sizeof(LwU32));
            LwU32 *pDataPhysical;
            LwU32 *pData;
            if (bVa)
            {
                physBlk = randPhysBlkAboveStack();
                pData =
                    (LwU32 *)(align(randRange(boundBlk * BLOCK_SIZE, maxVa()), BLOCK_SIZE) + off);
                pDataPhysical = (LwU32 *)(physBlk * BLOCK_SIZE + off);
                setBlockStatus(VALID_AND_CLEAN, (LwU32)pData, physBlk);
            }
            else
            {
                physBlk = randRange(stackBottom / BLOCK_SIZE, MIN(boundBlk - 1, getDmemSize() - 1));
                pData = (LwU32 *)(physBlk * BLOCK_SIZE + off);
                pDataPhysical = pData;
            }

            *pDataPhysical = oldData;

            DMVAREGWR(DMVACTL, boundBlk);

            LwBool badRead = *pData != oldData;
            *pData = newData;
            resetDmvactl();
            LwBool badWrite = *pDataPhysical != newData;

            resetBlockStatus(physBlk);

            if (badRead || badWrite)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

// TODO: test emem, cmem with all instructions?
TESTDEF(8, ememTest)
{
#if !defined(DMVA_FALCON_SEC2) && !defined(DMVA_FALCON_GSP)
    return DMVA_PLATFORM_NOT_SUPPORTED;
#else
    RNGctx readCtx = newRNGctx();
    RNGctx readVerifyCtx = readCtx;
    RNGctx writeCtx = newRNGctx();
    LwU32 ememSizeBytes = REF_VAL(LW_PSEC_HWCFG_EMEM_SIZE, DMVACSBRD(LW_CSEC_HWCFG)) * BLOCK_SIZE;
    LwU32 ememBase = POW2(getDmemTagWidth() + 8);
    LwU32 ememAddr;

    // fill EMEM using EMEMC/D
    markSubtestStart();
    DMVACSBWR(LW_CSEC_EMEMC(0), REF_DEF(LW_CSEC_EMEMC_AINCW, _TRUE));
    for (ememAddr = 0; ememAddr < ememSizeBytes; ememAddr += 4)
    {
        DMVACSBWR(LW_CSEC_EMEMD(0), rand32Ctx(&readCtx));
    }

    // verify EMEM using VA
    for (ememAddr = 0; ememAddr < ememSizeBytes; ememAddr += 4)
    {
        if (*(LwU32 *)(ememBase + ememAddr) != rand32Ctx(&readVerifyCtx))
        {
            DMVAREGWR(MAILBOX1, ememAddr);
            return DMVA_EMEM_BAD_DATA;
        }
    }

    // fill EMEM using VA
    markSubtestStart();
    fillMem(writeCtx, (LwU8 *)ememBase, ememSizeBytes);

    // verify EMEM using EMEMC/D
    DMVACSBWR(LW_CSEC_EMEMC(0), REF_DEF(LW_CSEC_EMEMC_AINCR, _TRUE));
    for (ememAddr = 0; ememAddr < ememSizeBytes; ememAddr += 4)
    {
        if (DMVACSBRD(LW_CSEC_EMEMD(0)) != rand32Ctx(&writeCtx))
        {
            DMVAREGWR(MAILBOX1, ememAddr);
            return DMVA_EMEM_BAD_DATA;
        }
    }
    return DMVA_OK;
#endif
}

#if defined(DMVA_FALCON_SEC2) || defined(DMVA_FALCON_PMU) || defined(DMVA_FALCON_GSP)
static int _inRange(LwU32 min, LwU32 max, LwU32 addr, LwU32 size)
{
    if (!size)
    {
        return 0;
    }
    if ((addr + size) < min || addr >= max)
    {
        return 0;
    }
    return 1;
}
#endif

TESTDEF(9, cmemTest)
{
#if !defined(DMVA_FALCON_SEC2) && !defined(DMVA_FALCON_PMU) && !defined(DMVA_FALCON_GSP)
    return DMVA_PLATFORM_NOT_SUPPORTED;
#else

#ifdef DMVA_FALCON_SEC2
#define HAL_CMEMBASE_VAL LW_CSEC_FALCON_CMEMBASE_VAL
#define HAL_MAILBOX1 LW_CSEC_FALCON_MAILBOX1
#elif defined DMVA_FALCON_PMU
#define HAL_CMEMBASE_VAL LW_CPWR_FALCON_CMEMBASE_VAL
#define HAL_MAILBOX1 LW_CPWR_FALCON_MAILBOX1
#elif defined DMVA_FALCON_GSP
#define HAL_CMEMBASE_VAL LW_CGSP_FALCON_CMEMBASE_VAL
#define HAL_MAILBOX1 LW_CGSP_FALCON_MAILBOX1
#endif

    // It's OK if EMEM overlaps with CMEM.  ldd/std will route to CMEM before EMEM.
    // However, CMEMBASE cannot be 0, as this will disable the CMEM aperture.
    LwU32 cmemBase = 0;

    LwU32 cmemSize = 256 * 1024; // Default size 256K
    LwU32 cmemMin = POW2(getDmemTagWidth() + 8);
    LwU32 testCount = 1000;
    if (FLD_TEST_DRF(_PSEC,_FALCON_HWCFG1,_CSB_SIZE_16M,_TRUE,DMVAREGRD(HWCFG1)))
    {
        cmemSize = 16 * 1024 * 1024; // 16M
    }
    while (LW_TRUE)
    {
        const LwU32 bmem_start = 0x02100000;
        const LwU32 kmem_start = 0x02000000;
        const LwU32 bmem_size = 0x1FFF;
        const LwU32 kmem_size = 0x17FF;

        // MK Note: Due to HW design, cmemBase must be aligned to engine CSB
        // address. See http://lwbugs/2027581
        // Because HW headers are not yet updated, fix it here
        // CMEMBASE_VAL was 31:18, should be 31:20
        if (cmemSize > 256 * 1024)
        {
            cmemBase = randRange(1, 0xFF) << 24;
        }
        else
        {
            cmemBase = randRange(1, 0xFFF) << 20;//hw actually needs 1M alignment
        }

        if (cmemBase >= cmemMin && // That should be always true
                !_inRange(bmem_start, bmem_start + bmem_size, cmemBase, cmemBase + cmemSize) && // BMEM
                !_inRange(kmem_start, kmem_start + kmem_size, cmemBase, cmemBase + cmemSize)) // KMEM
        {
            break;
        }
        testCount --;

        if (!testCount)
        {
            return DMVA_FAIL;
        }
    }
    DMVAREGWR(CMEMBASE, cmemBase);

    // Read back to be sure that address was set properly.
    if (DMVAREGRD(CMEMBASE) != cmemBase)
    {
        return DMVA_FAIL;
    }

    volatile LwU32 *pMailbox1 = (LwU32 *)(cmemBase + HAL_MAILBOX1);

    LwU32 data = rand32();
    *pMailbox1 = data;
    LwU32 badWrite = DMVAREGRD(MAILBOX1) != data;

    data = rand32();
    DMVAREGWR(MAILBOX1, data);
    LwU32 badRead = *pMailbox1 != data;

    DMVAREGWR(CMEMBASE, 0);

    return badRead || badWrite ? DMVA_FAIL : DMVA_OK;

#endif
}

TESTDEF(10, dmemcPaSetDirtyTest)
{
    DmvaRegRwImpl impl;
    BlockStatus blockStatus;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (blockStatus = VALID_AND_CLEAN; blockStatus <= VALID_AND_DIRTY; blockStatus++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 off = align(rand(BLOCK_SIZE), sizeof(LwU32));
            LwU32 data = *(LwU32 *)(physBlk * BLOCK_SIZE + off);

            setBlockStatus(blockStatus, randVa(), physBlk);

            LwU32 dmemc = dmemcVal(physBlk * BLOCK_SIZE + off, 0, 0, 0, 0, 0);
            DMVAREGWR_IMPL(DMEMC(0), dmemc, impl);
            DMVAREGWR_IMPL(DMEMD(0), data, impl);

            if (getBlockStatus(physBlk) != VALID_AND_DIRTY)
            {
                resetBlockStatus(physBlk);
                return DMVA_FAIL;
            }
            resetBlockStatus(physBlk);
        }
    }
    return DMVA_OK;
}

TESTDEF(11, dmemcIlwalidBlockTest)
{
    DmvaRegRwImpl impl;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        markSubtestStart();
        LwU32 physBlk = randPhysBlkAboveStack();
        LwU32 off = align(rand(BLOCK_SIZE), sizeof(LwU32));
        LwU32 data = *(LwU32 *)(physBlk * BLOCK_SIZE + off);

        setBlockStatus(INVALID, randVa(), physBlk);

        DMVAREGWR_IMPL(DMEMC(0), dmemcVal(physBlk * BLOCK_SIZE, 0, 0, 0, 0, 0), impl);
        DMVAREGWR_IMPL(DMEMD(0), data, impl);

        // the block's status shouldn't have changed
        if (getBlockStatus(physBlk) != INVALID)
        {
            resetBlockStatus(physBlk);
            return DMVA_FAIL;
        }

        resetBlockStatus(physBlk);
    }
    return DMVA_OK;
}

TESTDEF(12, dmemcPaSetTagTest)
{
    DmvaRegRwImpl impl;
    BlockStatus blockStatus;
    LwBool pending;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (blockStatus = VALID_AND_CLEAN; blockStatus <= PENDING; blockStatus++)
        {
            for (pending = 0; pending <= 1; pending++)
            {
                markSubtestStart();
                LwU32 va = randVa();
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 off = (BLOCK_SIZE - sizeof(LwU32));
                if (pending)
                {
                    do
                    {
                        off = align(rand(BLOCK_SIZE), sizeof(LwU32));
                    } while (off == (BLOCK_SIZE - sizeof(LwU32)));
                }
                LwU32 data = *(LwU32 *)(physBlk * BLOCK_SIZE + off);

                setBlockStatus(blockStatus, randVa(), physBlk);

                DMVAREGWR_IMPL(DMEMC(0), dmemcVal(physBlk * BLOCK_SIZE + off, 0, 0, 1, 0, 0), impl);
                DMVAREGWR_IMPL(DMEMT(0), va >> BLOCK_WIDTH, impl);
                DMVAREGWR_IMPL(DMEMD(0), data, impl);

                // block must have changed tag
                // block must be valid and clean if we wrote to offset (BLOCK_SIZE - sizeof(LwU32)),
                // or pending otherwise
                LwU32 expectedTag = va >> BLOCK_WIDTH;
                BlockStatus expectedStatus = pending ? PENDING : VALID_AND_CLEAN;

                LwBool badTag = dmva_dmblk(physBlk).tag != expectedTag;
                LwBool badStatus = getBlockStatus(physBlk) != expectedStatus;

                if (badTag)
                {
                    DMVAREGWR(MAILBOX1, (expectedTag << 16) | (dmva_dmblk(physBlk).tag));
                    resetBlockStatus(physBlk);
                    return DMVA_BAD_TAG;
                }
                else if (badStatus)
                {
                    DMVAREGWR(MAILBOX1, (expectedStatus << 16) | (getBlockStatus(physBlk)));
                    resetBlockStatus(physBlk);
                    return DMVA_BAD_STATUS;
                }

                resetBlockStatus(physBlk);
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(13, dmemcVaSetDirtyTest)
{
    DmvaRegRwImpl impl;
    BlockStatus blockStatus;
    LwBool miss;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (blockStatus = INVALID; blockStatus <= VALID_AND_DIRTY; blockStatus++)
        {
            for (miss = 0; miss <= 1; miss++)
            {
                markSubtestStart();
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 vaHit = randVa();
                LwU32 vaMiss = 0;
                LwU32 data = *(LwU32 *)((physBlk << BLOCK_WIDTH) + (vaHit & 0xff));
                if (miss)
                {
                    do
                    {
                        vaMiss = randVa();
                    } while ((vaHit >> BLOCK_WIDTH) == (vaMiss >> BLOCK_WIDTH));
                }

                setBlockStatus(blockStatus, miss ? vaMiss : vaHit, physBlk);

                DMVAREGWR_IMPL(DMEMC(0), dmemcVal(vaHit, 0, 0, 0, 0, 1), impl);
                DMVAREGWR_IMPL(DMEMD(0), data, impl);

                LwBool shouldMiss = (blockStatus == INVALID) || miss;
                // in all cases, the matched block should become dirty, and remain valid, unless we
                // miss
                LwU32 didMiss = REF_VAL(LW_PFALCON_FALCON_DMEMC_MISS, DMVAREGRD(DMEMC(0)));
                if ((shouldMiss != didMiss) ||
                    (!shouldMiss && (getBlockStatus(physBlk) != VALID_AND_DIRTY ||
                                     dmva_dmblk(physBlk).tag != (vaHit >> 8))) ||
                    (shouldMiss && (getBlockStatus(physBlk) != blockStatus)))
                {
                    resetBlockStatus(physBlk);
                    return DMVA_FAIL;
                }
                resetBlockStatus(physBlk);
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(14, dmemcVaSetTagTest)
{
    DmvaRegRwImpl impl;
    BlockStatus blockStatus;
    LwBool miss;
    LwBool pending;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (blockStatus = INVALID; blockStatus <= PENDING; blockStatus++)
        {
            for (miss = 0; miss <= 1; miss++)
            {
                for (pending = 0; pending <= 1; pending++)
                {
                    markSubtestStart();
                    LwU32 physBlk = randPhysBlkAboveStack();
                    LwU32 vaBefore = align(randVa(), BLOCK_SIZE);
                    LwU32 vaAfter = randVa();
                    LwU32 vaMiss = 0;
                    if (miss)
                    {
                        do
                        {
                            vaMiss = randVa();
                        } while ((vaBefore >> BLOCK_WIDTH) == (vaMiss >> BLOCK_WIDTH));
                    }
                    LwU32 off = (BLOCK_SIZE - sizeof(LwU32));
                    if (pending)
                    {
                        do
                        {
                            off = align(rand(BLOCK_SIZE), sizeof(LwU32));
                        } while (off == (BLOCK_SIZE - sizeof(LwU32)));
                    }
                    LwU32 data = *(LwU32 *)(physBlk * BLOCK_SIZE + off);

                    setBlockStatus(blockStatus, miss ? vaMiss : vaBefore, physBlk);

                    DMVAREGWR_IMPL(DMEMC(0), dmemcVal(vaBefore + off, 0, 0, 1, 0, 1), impl);
                    DMVAREGWR_IMPL(DMEMT(0), vaAfter >> BLOCK_WIDTH, impl);
                    DMVAREGWR_IMPL(DMEMD(0), data, impl);

                    LwBool shouldMiss = (blockStatus == INVALID) || miss;
                    LwU32 didMiss =
                        REF_VAL(LW_PFALCON_FALCON_DMEMC_MISS, DMVAREGRD_IMPL(DMEMC(0), impl));
                    // the block's status should have changed to valid, clean, and tag=DMEMT unless
                    // we miss
                    if ((shouldMiss != didMiss) ||
                        (!shouldMiss && (dmva_dmblk(physBlk).tag != (vaAfter >> 8))) ||
                        (!shouldMiss && !pending && (getBlockStatus(physBlk) != VALID_AND_CLEAN)) ||
                        (!shouldMiss && pending && (getBlockStatus(physBlk) != PENDING)) ||
                        (shouldMiss && (getBlockStatus(physBlk) != blockStatus)))
                    {
                        resetBlockStatus(physBlk);
                        return DMVA_FAIL;
                    }
                    resetBlockStatus(physBlk);
                }
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(15, dmemcPaReadTest)
{
    DmvaRegRwImpl impl;
    LwBool setTag;
    BlockStatus blockStatus;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (setTag = 0; setTag <= 1; setTag++)
        {
            for (blockStatus = INVALID; blockStatus <= VALID_AND_DIRTY; blockStatus++)
            {
                markSubtestStart();
                LwU32 va = randVa();
                LwU32 physBlk = randPa() >> BLOCK_WIDTH;

                setBlockStatus(blockStatus, va, physBlk);

                DMVAREGWR_IMPL(DMEMC(0), dmemcVal(physBlk * BLOCK_SIZE, 0, 0, setTag, 0, 0), impl);
                DMVAREGRD_IMPL(DMEMD(0), impl);  // load does not get optimized out

                if (getBlockStatus(physBlk) != blockStatus)
                {
                    resetBlockStatus(physBlk);
                    return DMVA_FAIL;
                }
                resetBlockStatus(physBlk);
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(16, dmemcVaReadTest)
{
    DmvaRegRwImpl impl;
    LwBool setTag;
    BlockStatus blockStatus;
    LwBool bMiss;
    for (impl = 0; impl < N_REG_RW_IMPLS; impl++)
    {
        for (setTag = 0; setTag <= 1; setTag++)
        {
            for (blockStatus = INVALID; blockStatus <= VALID_AND_DIRTY; blockStatus++)
            {
                for (bMiss = 0; bMiss <= 1; bMiss++)
                {
                    markSubtestStart();
                    LwU32 va1 = randVa();
                    LwU32 va2 = 0;
                    if (bMiss)
                    {
                        do
                        {
                            va2 = randVa();
                        } while ((va1 >> BLOCK_WIDTH) == (va2 >> BLOCK_WIDTH));
                    }
                    // If the PA happens to land on the stack, and the block status is
                    // VALID_AND_CLEAN,
                    // it will get set to VALID_AND_DIRTY when we make function calls, so make sure
                    // it's
                    // always above the stack.
                    LwU32 physBlk = randPhysBlkAboveStack();

                    LwU32 oldData = rand32();
                    *(LwU32 *)(physBlk * BLOCK_SIZE + (va1 % BLOCK_SIZE)) = oldData;

                    setBlockStatus(blockStatus, va1, physBlk);

                    DMVAREGWR_IMPL(DMEMC(0), dmemcVal(bMiss ? va2 : va1, 0, 0, setTag, 0, 1), impl);
                    DMVAREGWR_IMPL(DMEMT(0), rand32(), impl);
                    LwU32 data = DMVAREGRD_IMPL(DMEMD(0), impl);

                    // there was a failure if the block status changed,
                    // or if DMEMC hit when it should have missed,
                    // or when DMEMC missed when it should have hit
                    LwU32 dmemc = DMVAREGRD_IMPL(DMEMC(0), impl);

                    BlockStatus newBlockStatus = getBlockStatus(physBlk);
                    resetBlockStatus(physBlk);

                    LwBool badStatus = newBlockStatus != blockStatus;

                    LwBool bShouldMiss = bMiss || (blockStatus == INVALID);
                    LwBool bDidMiss = REF_VAL(LW_PFALCON_FALCON_DMEMC_MISS, dmemc);
                    LwBool badMiss = bShouldMiss != bDidMiss;

                    LwU32 expectedData = bShouldMiss ? 0xbadd3e3a : oldData;
                    LwU32 badData = data != expectedData;

                    if (badMiss)
                    {
                        return DMVA_DMEMC_BAD_MISS;
                    }
                    if (badStatus)
                    {
                        DMVAREGWR(MAILBOX1, (blockStatus << 16) | newBlockStatus);
                        return DMVA_BAD_STATUS;
                    }
                    if (badData)
                    {
                        errorStatus[0] = expectedData;
                        errorStatus[1] = data;
                        DMVAREGWR(MAILBOX1, (LwU32)errorStatus);
                        return DMVA_BAD_DATA;
                    }
                }
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(17, dmaReadWriteTest)
{
    LwBool partialRead;
    BlockStatus blockStatus;
    DmaImpl dmaImpl;
    for (blockStatus = INVALID; blockStatus <= VALID_AND_DIRTY; blockStatus++)
    {
        for (partialRead = 0; partialRead <= 1; partialRead++)
        {
            for (dmaImpl = 0; dmaImpl < N_DMA_IMPLS; dmaImpl++)
            {
                markSubtestStart();
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 xferSize = partialRead ? POW2(rand(6) + 2) : BLOCK_SIZE;
                LwU32 off = align(rand(BLOCK_SIZE), xferSize);
                LwU32 fbOffBlk = rand(POW2(getDmemTagWidth()));
                setBlockStatus(blockStatus, randVa(), physBlk);
                dmvaIssueDma(physBlk * BLOCK_SIZE + off, fbOffBlk * BLOCK_SIZE, xferSize, 0, 0, 0,
                             0, DMVA_DMA_FROM_FB, dmaImpl);

                BlockStatus expectedStatus =
                    blockStatus == VALID_AND_CLEAN ? VALID_AND_DIRTY : blockStatus;
                BlockStatus readStatus = getBlockStatus(physBlk);
                LwBool badStatus = readStatus != expectedStatus;

                if (badStatus)
                {
                    DMVAREGWR(MAILBOX1, (expectedStatus << 16) | readStatus);
                    return DMVA_BAD_STATUS;
                }
            }
        }
    }

    return DMVA_OK;
}

TESTDEF(18, dmaSetTagTest)
{
    LwBool partialRead;
    BlockStatus blockStatus;
    DmaImpl dmaImpl;
    for (blockStatus = INVALID; blockStatus <= VALID_AND_DIRTY; blockStatus++)
    {
        for (partialRead = 0; partialRead <= 1; partialRead++)
        {
            for (dmaImpl = 0; dmaImpl < N_DMA_IMPLS; dmaImpl++)
            {
                markSubtestStart();
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 xferSize = partialRead ? POW2(randRange(2, 7)) : BLOCK_SIZE;
                LwU32 off = align(rand(BLOCK_SIZE), xferSize);
                LwU32 fbOffBlk = rand(POW2(getDmemTagWidth()));
                RNGctx dataCtx = newRNGctx();
                fillMem(dataCtx, (LwU8 *)(physBlk * BLOCK_SIZE + off), xferSize);
                dmvaIssueDma(physBlk * BLOCK_SIZE + off, fbOffBlk * BLOCK_SIZE + off, xferSize, 0,
                             0, 0, 0, DMVA_DMA_TO_FB, dmaImpl);
                fillMem(newRNGctx(), (LwU8 *)(physBlk * BLOCK_SIZE + off), xferSize);

                setBlockStatus(blockStatus, randVa(), physBlk);
                dmvaIssueDma(physBlk * BLOCK_SIZE + off, fbOffBlk * BLOCK_SIZE + off, xferSize, 1,
                             0, 0, 0, DMVA_DMA_FROM_FB, dmaImpl);
                LwU32 expectedTag = trimTag(fbOffBlk);
                LwBool badStatus = getBlockStatus(physBlk) != VALID_AND_CLEAN;
                LwBool badData =
                    verifyMem(dataCtx, (LwU8 *)(physBlk * BLOCK_SIZE + off), xferSize) != xferSize;
                LwBool badTag = dmva_dmblk(physBlk).tag != expectedTag;
                if (badStatus || badData || badTag)
                {
                    resetBlockStatus(physBlk);
                    return DMVA_FAIL;
                }
                resetBlockStatus(physBlk);
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(19, hsBootromPaTest)
{
    return callHS(HS_ALIVE) == HS_ALIVE_MAGIC ? DMVA_OK : DMVA_FAIL;
}

TESTDEF(20, hsBootromVaTest)
{
    DMVAREGWR(DMVACTL, stackBottom / BLOCK_SIZE);

    // Reserve 2 blocks for the stack.
    // Store the signature in the top 16 byts of this space.
    // The stack will go immediately under the signature.
    LwU32 stackBlk = randRange(stackBottom / BLOCK_SIZE + 1, getDmemSize() - 1);
    LwU32 stackVA = randRange(stackBottom + BLOCK_SIZE, maxVa());
    setBlockStatus(VALID_AND_CLEAN, stackVA, stackBlk);
    setBlockStatus(VALID_AND_CLEAN, stackVA - BLOCK_SIZE, stackBlk - 1);

    // copy the signature into its block
    g_pSignature = (LwU32 *)(align(stackVA, BLOCK_SIZE) + BLOCK_SIZE - 16);
    LwU32 sigDWORD;
    for (sigDWORD = 0; sigDWORD < 4; sigDWORD++)
    {
        g_pSignature[sigDWORD] = g_signature[sigDWORD];
    }
    // The signature pointer needs to be physically addressed because the trapped dmwrite
    // that reads the signature uses PA.
    g_pSignature = (LwU32 *)(stackBlk * BLOCK_SIZE + BLOCK_SIZE - 16);

    // move the stack pointer and set up stack overflow protection
    setStackOverflowProtection(0, LW_FALSE);
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, align(stackVA, BLOCK_SIZE) + BLOCK_SIZE - 16);
    setStackOverflowProtection(align(stackVA, BLOCK_SIZE), LW_TRUE);

    DMVA_RC rc = callHS(HS_ALIVE) == HS_ALIVE_MAGIC ? DMVA_OK : DMVA_FAIL;

    setStackOverflowProtection(0, LW_FALSE);
    falc_wspr(SP, sp);
    setStackOverflowProtection(dataSize, LW_TRUE);
    g_pSignature = &g_signature[0];
    resetBlockStatus(stackBlk);
    resetBlockStatus(stackBlk - 1);
    resetDmvactl();
    return rc;
}

TEST_OVLY(21) void setCarveouts(LwU32 range0Start, LwU32 range0End, LwU32 range1Start,
                                LwU32 range1End)
{
#ifndef DMVA_FALCON_PMU
    UASSERT(0);
#endif
    LwU32 range0 = 0;
    range0 = FLD_SET_DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE0, _START_BLOCK, range0Start, range0);
    range0 = FLD_SET_DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE0, _END_BLOCK, range0End, range0);
    DMVACSBWR(LW_CPWR_FALCON_DMEM_PRIV_RANGE0, range0);
    LwU32 range1 = 0;
    range1 = FLD_SET_DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE1, _START_BLOCK, range1Start, range1);
    range1 = FLD_SET_DRF_NUM(_CPWR, _FALCON_DMEM_PRIV_RANGE1, _END_BLOCK, range1End, range1);
    DMVACSBWR(LW_CPWR_FALCON_DMEM_PRIV_RANGE1, range1);
}

// TODO: add VA
TESTDEF(21, carveoutTest)
{
#ifndef DMVA_FALCON_PMU
    return DMVA_PLATFORM_NOT_SUPPORTED;
#else
#ifdef NO_PRIV_SEC
    return DMVA_OK;
#endif
    LwU32 nTests = 2 * 2 * 2;
    rmWrite32(nTests);
    LwBool bOnCarveout;
    LwBool bPrivMeetsMask;
    LwBool ucodeLevel;
    for (ucodeLevel = SEC_MODE_LS1; ucodeLevel <= SEC_MODE_LS2; ucodeLevel++)
    {
        callHS(HS_SET_LVL, ucodeLevel);
        for (bOnCarveout = 0; bOnCarveout <= 1; bOnCarveout++)
        {
            for (bPrivMeetsMask = 0; bPrivMeetsMask <= 1; bPrivMeetsMask++)
            {
                LwU32 physBlk = randPhysBlkAboveStack();
                LwU32 off = align(rand(BLOCK_SIZE), sizeof(LwU32));
                LwU32 addr = physBlk * BLOCK_SIZE + off;
                LwU32 data = rand32();
                LwU32 newData = rand32();
                *(LwU32 *)addr = data;

                DMVAREGWR(DMVACTL, randPhysBlkAboveStack());

                rmWrite32(addr);
                rmWrite32(data);
                rmWrite32(newData);
                rmWrite32(bOnCarveout);
                rmWrite32(bPrivMeetsMask);

                LwU32 carveoutMin = 0;
                LwU32 carveoutMax = getDmemSize();

                LevelMask levelMask = randLevelMask();
                if (bPrivMeetsMask)
                {
                    levelMask |= 0x01;
                }
                else
                {
                    levelMask &= ~0x01;
                }
                callHS(HS_SET_BLK_LVL, physBlk, levelMask);
                UASSERT((dmva_dmblk(physBlk).levelMask & 0x01) == bPrivMeetsMask);

                LwU32 range0Min;
                LwU32 range0Max;
                LwU32 range1Min;
                LwU32 range1Max;
                if (bOnCarveout)
                {
                    range0Min = randRange(carveoutMin, carveoutMax);
                    range0Max = randRange(carveoutMin, carveoutMax);
                    LwBool bOnRange0 = (range0Min <= physBlk) && (range0Max >= physBlk);
                    if (bOnRange0)
                    {
                        // RANGE1 can be anything
                        range1Min = randRange(carveoutMin, carveoutMax);
                        range1Max = randRange(carveoutMin, carveoutMax);
                    }
                    else
                    {
                        // RANGE1 must contain physBlk
                        range1Min = randRange(carveoutMin, physBlk);
                        range1Max = randRange(physBlk, carveoutMax);
                    }
                    UASSERT((range0Min <= physBlk && range0Max >= physBlk) ||
                            (range1Min <= physBlk && range1Max >= physBlk));
                }
                else
                {
                    // both carveouts must not contain physBlk
                    range0Min = randRange(carveoutMin, carveoutMax);
                    range0Max =
                        randRange(carveoutMin, range0Min <= physBlk ? physBlk - 1 : carveoutMax);
                    range1Min = randRange(carveoutMin, carveoutMax);
                    range1Max =
                        randRange(carveoutMin, range1Min <= physBlk ? physBlk - 1 : carveoutMax);
                    UASSERT((range0Min > physBlk || range0Max < physBlk) &&
                            (range1Min > physBlk || range1Max < physBlk));
                }
                setCarveouts(range0Min, range0Max, range1Min, range1Max);

                wait();
                callHS(HS_SET_BLK_LVL, physBlk, LEVEL_MASK_ALL);
                resetDmvactl();
                LwU32 expectedData = bOnCarveout && bPrivMeetsMask ? newData : data;
                LwU32 badData = *(LwU32 *)addr != expectedData;
                rmWrite32(badData);
            }
        }
    }
    setCarveouts(LW_CPWR_FALCON_DMEM_PRIV_RANGE0_START_BLOCK_INIT,
                 LW_CPWR_FALCON_DMEM_PRIV_RANGE0_END_BLOCK_INIT,
                 LW_CPWR_FALCON_DMEM_PRIV_RANGE1_START_BLOCK_INIT,
                 LW_CPWR_FALCON_DMEM_PRIV_RANGE1_END_BLOCK_INIT);
    callHS(HS_SET_LVL, SEC_MODE_NS);
    return DMVA_OK;
#endif
}

TESTDEF(22, dmlvlChangePermissionsTest)
{
    DmInstrImpl impl;
    SecMode ucodeLevel;
    for (impl = 0; impl < N_DM_IMPLS; impl++)
    {
        for (ucodeLevel = 0; ucodeLevel < N_SEC_MODES; ucodeLevel++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 oldMask = randLevelMask();
            LwU32 newMask = randLevelMask();

            if (ucodeLevel < 3)
            {
                oldMask |= BIT(ucodeLevel);  // we will get an exception without this
            }

            callHS(HS_SET_BLK_LVL, physBlk, oldMask);
            dmlvlAtUcodeLevelImpl(physBlk, newMask, ucodeLevel, impl);

            LwBool badMask =
                dmva_dmblk(physBlk).levelMask != getExpectedMask(oldMask, newMask, ucodeLevel, 0);

            resetBlockStatus(physBlk);

            if (badMask)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(23, dmaSetMaskTest)
{
    SecMode ucodeLevel;
    DmaImpl dmaImpl;
    for (ucodeLevel = 0; ucodeLevel < N_SEC_MODES; ucodeLevel++)
    {
        for (dmaImpl = 0; dmaImpl < N_DMA_IMPLS; dmaImpl++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 oldMask = randLevelMask();
            LwU32 newMask = randLevelMask();

            if (ucodeLevel < SEC_MODE_HS)
            {
                oldMask |=
                    BIT(ucodeLevel);  // we will get an exception using dmread without permissions
            }

            callHS(HS_SET_BLK_LVL, physBlk, oldMask);
            dmvaIssueDmaAtUcodeLevel(physBlk * BLOCK_SIZE, 0, BLOCK_SIZE, 0, 0, 1, newMask,
                                     DMVA_DMA_FROM_FB, dmaImpl, ucodeLevel);

            LwBool badMask = dmva_dmblk(physBlk).levelMask !=
                             getExpectedMask(oldMask, newMask, ucodeLevel,
                                             dmaImpl == IMPL_DM_RW || dmaImpl == IMPL_DM_RW2);

            resetBlockStatus(physBlk);

            if (badMask)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

TEST_OVLY(24) void dmlvlExceptionTestDummy(void)
{
}

TEST_OVLY(24) void dmlvlExceptionTestExceptionHandler(void)
{
    if (REF_VAL(LW_PFALCON_FALCON_EXCI_EXCAUSE, DMVAREGRD(EXCI)) !=
        LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PERMISSION_INS)
    {
        DMVAREGWR(MAILBOX0, DMVA_UNEXPECTED_EXCEPTION);
        DMVAREGWR(MAILBOX1, DMVAREGRD(EXCI));
    }
    halt();
}

// TODO: verify write does not actually occur
// TODO: reexelwte excepting instruction if not an SP test
TESTDEF(24, dmlvlExceptionTest)
{
    // The exception handler does not clear CSW.E or reti to restore
    // interrupts, so we must do these manually for each subtest.
    installInterruptHandler(defaultInterruptHandler);
    installExceptionHandler(dmlvlExceptionTestExceptionHandler);

    static const LwU32 N_BLOCK_IMPLS = 3;
    static const LwU32 N_CALL_IMPLS = 3;
    LwU32 nImpls = (N_WRITE_IMPLS + N_READ_IMPLS + N_BLOCK_IMPLS + N_CALL_IMPLS);
#ifdef NO_SEC_DISABLE_EXCEPTIONS
    LwU32 nSubtests = nImpls;
#else
    LwU32 nSubtests = 2 * nImpls;
#endif

    LwU32 subTestId = rmRead32();
    UASSERT(subTestId < nSubtests);

    rmWrite32(nSubtests);

    LwBool bShouldExcept = !(subTestId / nImpls);
    rmWrite32(bShouldExcept);

    LwU32 implId = subTestId % nImpls;

    LwU32 physBlk = randPhysBlkAboveStack();
    LwU32 bufBlk = physBlk;
    while (bufBlk == physBlk)
    {
        bufBlk = randPhysBlkAboveStack();
    }
    callHS(HS_SET_BLK_LVL, bufBlk, LEVEL_MASK_ALL);
    LwU8 *buf = (LwU8 *)(bufBlk * BLOCK_SIZE + align(rand(BLOCK_SIZE), 16));
    setBlockStatus(VALID_AND_CLEAN, bufBlk * BLOCK_SIZE,
                   bufBlk);  // map in case bufBlk is >= DMVACTL.bound

    setDmemSelwrityExceptions(bShouldExcept);

    enum
    {
        WRITE_IMPL,
        READ_IMPL,
        CALL_IMPL,
        BLOCK_IMPL,
    } implType;

    if (implId < N_WRITE_IMPLS)
    {
        implType = WRITE_IMPL;
    }
    else if ((implId -= N_WRITE_IMPLS) < N_READ_IMPLS)
    {
        implType = READ_IMPL;
    }
    else if ((implId -= N_READ_IMPLS) < N_CALL_IMPLS)
    {
        implType = CALL_IMPL;
    }
    else if ((implId -= N_CALL_IMPLS) < N_BLOCK_IMPLS)
    {
        implType = BLOCK_IMPL;
    }
    else
    {
        halt();
    }

    LwU32 dmlvlExceptionTestAddr =
        physBlk * BLOCK_SIZE +
        align(rand(BLOCK_SIZE),
              implType == CALL_IMPL ? 4 : implType == BLOCK_IMPL ? BLOCK_SIZE : 16);
    rmWrite32(dmlvlExceptionTestAddr);

    callHS(HS_SET_BLK_LVL, physBlk, LEVEL_MASK_ALL);
    *(LwU32 *)dmlvlExceptionTestAddr = rand32();
    setBlockStatus(rand(N_BLOCK_STATUS), randVa(), physBlk);
    callHS(HS_SET_BLK_LVL, physBlk, LEVEL_MASK_NONE);

    DMVAREGWR(MAILBOX0, DMVA_OK);
    falc_wspr(EXCI, 0);  // if falcon doesn't except, EXCI.exause will be 0
    switch (implType)
    {
        case WRITE_IMPL:
            *(LwU32 *)buf = rand32();
            writeImpl((LwU8 *)dmlvlExceptionTestAddr, buf, implId);
            break;
        case READ_IMPL:
            readImpl(buf, (LwU8 *)dmlvlExceptionTestAddr, implId);
            break;
        case CALL_IMPL:
            switch (implId)
            {
                case 0:
                    falc_wspr(SP, dmlvlExceptionTestAddr + 4);
                    asm volatile("call %0;" ::"r"(dmlvlExceptionTestDummy));
                    break;
                case 1:
                    falc_wspr(SP, dmlvlExceptionTestAddr);
                    asm volatile("ret;");
                    break;
                case 2:
                    falc_wspr(SP, dmlvlExceptionTestAddr);
                    asm volatile("reti;");
                    break;
            }
        case BLOCK_IMPL:
            switch (implId)
            {
                case 0:
                    dmva_dmlvl(physBlk, randLevelMask());
                    break;
                case 1:
                    falc_dmilw(physBlk);
                    break;
                case 2:
                    falc_dmclean(physBlk);
                    break;
            }
            break;
    }

    if (bShouldExcept)
    {
        DMVAREGWR(MAILBOX0, DMVA_DMLVL_SHOULD_EXCEPT);
    }
    halt();
}

TESTDEF(25, dmatrfSelwrityTest)
{
    SecMode ucodeLevel;
    LwBool readFromFB;
    LwBool selwrityFail;
    LwBool setMask;
    for (selwrityFail = 0; selwrityFail <= 1; selwrityFail++)
    {
        for (readFromFB = 0; readFromFB <= 1; readFromFB++)
        {
            for (ucodeLevel = 0; ucodeLevel <= (selwrityFail ? SEC_MODE_LS2 : SEC_MODE_HS);
                 ucodeLevel++)
            {
                for (setMask = 0; setMask <= 1; setMask++)
                {
                    markSubtestStart();
                    LwU32 physBlk = randPhysBlkAboveStack();
                    LwU32 xferSize = BIT(rand(7) + 2);
                    LwU32 off = align(rand(BLOCK_SIZE), xferSize);
                    LwU32 addr = BLOCK_SIZE * physBlk + off;
                    RNGctx dmemCtx = newRNGctx();
                    RNGctx fbCtx = newRNGctx();
                    LwU32 oldMask = randLevelMask();
                    if (selwrityFail)
                    {
                        oldMask &= ~BIT(ucodeLevel);
                    }
                    else if (ucodeLevel < 3)
                    {
                        oldMask |= BIT(ucodeLevel);
                    }
                    LwU32 attemptedMask = randLevelMask();
                    LwU32 expectedMask =
                        (setMask && readFromFB)
                            ? getExpectedMask(oldMask, attemptedMask, ucodeLevel, LW_FALSE)
                            : oldMask;
                    fillMem(fbCtx, (LwU8 *)addr, xferSize);
                    dmvaIssueDma(addr, addr, xferSize, 0, 0, 0, 0, DMVA_DMA_TO_FB, IMPL_DM_RW);
                    fillMem(dmemCtx, (LwU8 *)addr, xferSize);

                    callHS(HS_SET_BLK_LVL, physBlk, oldMask);
                    // TODO: replace some of these 0's with rand(2)
                    DMVAREGWR(DMVACTL, randPhysBlkAboveStack());
                    dmvaIssueDmaAtUcodeLevel(addr, addr, xferSize, 0, 0, setMask, attemptedMask,
                                             readFromFB ? DMVA_DMA_FROM_FB : DMVA_DMA_TO_FB,
                                             IMPL_DMATRF, ucodeLevel);
                    resetDmvactl();

                    LwBool badMask = dmva_dmblk(physBlk).levelMask != expectedMask;
                    callHS(HS_SET_BLK_LVL, physBlk, LEVEL_MASK_ALL);

                    LwBool badDmem =
                        verifyMem(selwrityFail ? dmemCtx : (readFromFB ? fbCtx : dmemCtx),
                                  (LwU8 *)addr, xferSize) != xferSize;
                    dmvaIssueDma(addr, addr, xferSize, 0, 0, 0, 0, DMVA_DMA_FROM_FB, IMPL_DM_RW);
                    LwBool badFB = verifyMem(selwrityFail ? fbCtx : (readFromFB ? fbCtx : dmemCtx),
                                             (LwU8 *)addr, xferSize) != xferSize;
                    LwBool badDmatrfError = DRF_VAL(_PFALCON, _FALCON_DMATRFCMD, _ERROR,
                                                    DMVAREGRD(DMATRFCMD)) != selwrityFail;
                    resetBlockStatus(physBlk);
                    if (badDmatrfError || badDmem || badFB || badMask)
                    {
                        return DMVA_FAIL;
                    }
                }
            }
        }
    }
    return DMVA_OK;
}

// TODO: VA
TESTDEF(26, dmemcSelwrityTest)
{
    SecMode selwrityLevel;
    LwBool selwrityFail;
    DmvaRegRwImpl impl = IMPL_CSB;
    LwBool setMask;
    LwBool read;
    for (selwrityFail = 0; selwrityFail <= 1; selwrityFail++)
    {
        for (read = 0; read <= 1; read++)
        {
            for (selwrityLevel = SEC_MODE_NS; selwrityLevel <= SEC_MODE_LS2; selwrityLevel++)
            {
#ifndef NO_PRIV_SEC
                for (impl = IMPL_CSB; impl <= (selwrityLevel == 0 ? IMPL_PRIV : IMPL_CSB); impl++)
                {
#endif
                    for (setMask = 0; setMask <= 1; setMask++)
                    {
                        markSubtestStart();
                        LwU32 physBlk = randPhysBlkAboveStack();
                        LwU32 oldData = rand32();
                        LwU32 newData = rand32();
                        LwU32 off = align(rand(BLOCK_SIZE), sizeof(LwU32));
                        LwU32 oldMask = randLevelMask();
                        if (selwrityFail)
                        {
                            oldMask &= ~BIT(selwrityLevel);
                        }
                        else if (selwrityLevel < SEC_MODE_HS)
                        {
                            oldMask |= BIT(selwrityLevel);
                        }
                        LwU32 attemptedMask = randLevelMask();
                        LwU32 expectedMask =
                            (setMask && !read && !selwrityFail)
                                ? getExpectedMask(oldMask, attemptedMask, selwrityLevel, LW_FALSE)
                                : oldMask;
                        if (impl == IMPL_PRIV)
                        {
                            DMVAREGWR_IMPL(DMEMC(0), dmemcVal(physBlk * BLOCK_SIZE + off, rand(2),
                                                              rand(2), 0, setMask, 0),
                                           impl);
                            DMVAREGWR_IMPL(DMEML(0), attemptedMask, impl);
                        }
                        else
                        {
                            stxAtUcodeLevel(CSBADDR(DMEMC(0)),
                                            dmemcVal(physBlk * BLOCK_SIZE + off, rand(2), rand(2),
                                                     0, setMask, 0),
                                            selwrityLevel);
                            stxAtUcodeLevel(CSBADDR(DMEML(0)), attemptedMask, selwrityLevel);
                        }
                        *(LwU32 *)(physBlk * BLOCK_SIZE + off) = oldData;
                        callHS(HS_SET_BLK_LVL, physBlk, oldMask);
                        LwU32 dataRead = 0;
                        if (read)
                        {
                            dataRead = (impl == IMPL_PRIV)
                                           ? DMVAREGRD_IMPL(DMEMD(0), impl)
                                           : ldxAtUcodeLevel(CSBADDR(DMEMD(0)), selwrityLevel);
                        }
                        else
                        {
                            if (impl == IMPL_PRIV)
                            {
                                DMVAREGWR_IMPL(DMEMD(0), newData, impl);
                            }
                            else
                            {
                                stxAtUcodeLevel(CSBADDR(DMEMD(0)), newData, selwrityLevel);
                            }
                        }
                        LwU32 dmemc = (impl == IMPL_PRIV)
                                          ? DMVAREGRD_IMPL(DMEMC(0), impl)
                                          : ldxAtUcodeLevel(CSBADDR(DMEMC(0)), selwrityLevel);
                        LevelMask newMask = dmva_dmblk(physBlk).levelMask;
                        LwBool badMask = newMask != expectedMask;
                        callHS(HS_SET_BLK_LVL, physBlk, LEVEL_MASK_ALL);
                        LwU32 badDataRead =
                            read && (dataRead != (selwrityFail ? 0xdead1e10 : oldData));
                        LwU32 badDataWritten = !read && (*(LwU32 *)(physBlk * BLOCK_SIZE + off) !=
                                                         (selwrityFail ? oldData : newData));
                        LwU32 badDMEMClvlerr =
                            DRF_VAL(_PFALCON, _FALCON_DMEMC, _LVLERR, dmemc) != selwrityFail;
                        resetBlockStatus(physBlk);
                        if (badDMEMClvlerr)
                        {
                            return DMVA_DMEMC_BAD_LVLERR;
                        }
                        else if (badDataRead)
                        {
                            return DMVA_DMEMC_BAD_DATA_READ;
                        }
                        else if (badDataWritten)
                        {
                            return DMVA_DMEMC_BAD_DATA_WRITTEN;
                        }
                        else if (badMask)
                        {
                            DMVAREGWR(MAILBOX1, (expectedMask << 16) | (newMask));
                            return DMVA_BAD_MASK;
                        }
                    }
#ifndef NO_PRIV_SEC
                }
#endif
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(27, scpTest)
{
    LwBool virtualAddrIsMapped;
    LwBool addrIsAboveBound;
    for (virtualAddrIsMapped = 0; virtualAddrIsMapped <= 1; virtualAddrIsMapped++)
    {
        for (addrIsAboveBound = 0; addrIsAboveBound <= 1; addrIsAboveBound++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 off = align(rand(BLOCK_SIZE), 16);
            LwU32 dataAddr = physBlk * BLOCK_SIZE + off;
            LwU8 *pDataPhysical = (LwU8 *)dataAddr;
            RNGctx ctxData = newRNGctx();
            RNGctx ctxGarbage = newRNGctx();

            // 16 bytes of test data
            fillMem(ctxData, pDataPhysical, 16);

            if (virtualAddrIsMapped)
            {
                setBlockStatus(VALID_AND_CLEAN,
                               randPhysBlkAboveStack() + align(rand(BLOCK_SIZE), sizeof(LwU32)),
                               physBlk);
            }
            if (addrIsAboveBound)
            {
                DMVAREGWR(DMVACTL, physBlk);
            }

            // move the 16 bytes into SCP R5
            falc_scp_trap(TC_INFINITY);
            falc_trapped_dmwrite(falc_sethi_i(dataAddr, SCP_R5));
            falc_dmwait();

            // overwrite the destination DMEM with garbage data before transfer
            LwU32 bound = DMVAREGRD(DMVACTL);
            resetDmvactl();
            fillMem(ctxGarbage, pDataPhysical, 16);
            DMVAREGWR(DMVACTL, bound);

            // move 16 bytes from SCP R5 into DMEM
            falc_trapped_dmread(falc_sethi_i(dataAddr, SCP_R5));
            falc_dmwait();
            falc_scp_trap(0);

            if (virtualAddrIsMapped)
            {
                resetBlockStatus(physBlk);
            }
            if (addrIsAboveBound)
            {
                resetDmvactl();
            }

            LwBool badData = verifyMem(ctxData, pDataPhysical, 16) != 16;

            if (badData)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

static enum
{
    DMEM_EXC_TEST_MISS = 0,
    DMEM_EXC_TEST_MULTIHIT,
    DMEM_EXC_TEST_PAFAULT,
    N_DMEM_EXC_TEST_TYPES,
} dmemExcTestType;

static LwU8 dmemExcTestPhysBuf[4] ATTR_ALIGNED(4);
static LwU32 dmemExcTestVA;
static LwU32 dmemExcTestPhysBlkArr[MULTIHIT_MAX_MAPPED_BLKS];
static LwU32 dmemExcTestNMappedBlocks;
static LwU32 dmemExcTestOff;
static LwBool bHandlerWasCalled;
static LwBool bCorrectAddr;
static LwBool bShouldDoubleFault;

TEST_OVLY(28) void dmemExcTestExceptionHandler(void)
{
    UASSERT(!bShouldDoubleFault);
    UASSERT(bHandlerWasCalled == LW_FALSE);  // we could not map memory the first time, so just fail

    bHandlerWasCalled = LW_TRUE;

    LwU32 exCause = DRF_VAL(_PFALCON, _FALCON_EXCI, _EXCAUSE, DMVAREGRD(EXCI));
    switch (dmemExcTestType)
    {
        case DMEM_EXC_TEST_MISS:
            UASSERT(exCause == LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_MISS_INS);
            break;
        case DMEM_EXC_TEST_MULTIHIT:
            UASSERT(exCause == LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_DHIT_INS);
            break;
        case DMEM_EXC_TEST_PAFAULT:
            UASSERT(exCause == LW_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PAFAULT_INS);
            break;
        default:
            DMVAREGWR(MAILBOX0, DMVA_UNEXPECTED_EXCEPTION);
            DMVAREGWR(MAILBOX1, DMVAREGRD(EXCI));
            halt();
    }

    LwU32 exceptingAddr = dmemExcTestVA + dmemExcTestOff;
    bCorrectAddr = (exceptingAddr == DMVAREGRD(EXCI2));

    UASSERT(bCorrectAddr);

#ifdef NO_PRECISE_EXCEPTIONS
    // Don't go back to the excepting code, as it may not be safe to do so.
    DMVAREGWR(MAILBOX0, DMVA_OK);
    halt();
#endif

    switch (dmemExcTestType)
    {
        case DMEM_EXC_TEST_MISS:
            // Map the block so when we return, we don't fault when reexelwting the faulting
            // instruction.
            setBlockStatus(VALID_AND_CLEAN, dmemExcTestVA, dmemExcTestPhysBlkArr[0]);
            break;
        case DMEM_EXC_TEST_MULTIHIT:
            ;
            // Unmap all extra blocks, so that only one block is mapped to the VA.
            LwU32 i;
            for (i = 1; i < dmemExcTestNMappedBlocks; i++)
            {
                setBlockStatus(INVALID, 0, dmemExcTestPhysBlkArr[i]);
            }
            break;
        case DMEM_EXC_TEST_PAFAULT:
            // Move the DMEMVA bound lower, such that the excepting address is now in VA space.
            DMVAREGWR(DMVACTL, stackBottom / BLOCK_SIZE);
            break;
        default:
            halt();
    }
}

TESTDEF(28, dmemExcTest)
{
    // The exception handler does not clear CSW.E or reti to restore
    // interrupts, so we must do these manually for each subtest.
    installExceptionHandler(dmemExcTestExceptionHandler);
    installInterruptHandler(defaultInterruptHandler);

    // There may be mapped blocks left over from previous subtests, so reset all of them.
    resetAllBlockStatus();

    static const LwU32 N_CALL_IMPLS = 3;
    LwU32 nImpls =
        N_READ_DMEM_IMPLS + N_READ_SP_IMPLS + N_WRITE_DMEM_IMPLS + N_WRITE_SP_IMPLS + N_CALL_IMPLS;
    LwU32 nSubTests = N_DMEM_EXC_TEST_TYPES * nImpls;
    rmWrite32(nSubTests);
    LwU32 implId = rmRead32();
    UASSERT(implId < nSubTests);

    dmemExcTestType = implId / nImpls;
    implId %= nImpls;

    // Decode implId
    enum
    {
        READ_DMEM_IMPL,
        WRITE_DMEM_IMPL,
        READ_SP_IMPL,
        WRITE_SP_IMPL,
        CALL_IMPL,
    } implType;

    if (implId < N_READ_DMEM_IMPLS)
    {
        implType = READ_DMEM_IMPL;
    }
    else if ((implId -= N_READ_DMEM_IMPLS) < N_WRITE_DMEM_IMPLS)
    {
        implType = WRITE_DMEM_IMPL;
    }
    else if ((implId -= N_WRITE_DMEM_IMPLS) < N_READ_SP_IMPLS)
    {
        implType = READ_SP_IMPL;
    }
    else if ((implId -= N_READ_SP_IMPLS) < N_WRITE_SP_IMPLS)
    {
        implType = WRITE_SP_IMPL;
    }
    else if ((implId -= N_WRITE_SP_IMPLS) < N_CALL_IMPLS)
    {
        implType = CALL_IMPL;
    }
    else
    {
        halt();
    }

    bShouldDoubleFault =
        implType == READ_SP_IMPL || implType == WRITE_SP_IMPL || implType == CALL_IMPL;

    dmemExcTestNMappedBlocks = randRange(2, MULTIHIT_MAX_MAPPED_BLKS);
    randUnique(dmemExcTestPhysBlkArr, dmemExcTestNMappedBlocks, stackBottom / BLOCK_SIZE,
               getDmemSize() - 1);

    DMVAREGWR(DMVACTL, stackBottom / BLOCK_SIZE);
    if (dmemExcTestType == DMEM_EXC_TEST_PAFAULT)
    {
        // make sure the VA is above physical memory, so we get a PA fault
        dmemExcTestVA = BLOCK_SIZE * randRange(getDmemSize(), POW2(getDmemTagWidth()) - 1);
    }
    else
    {
        // If this is not a PA fault test, we can test from any VA
        dmemExcTestVA = align(randVaAboveBound(), BLOCK_SIZE);
    }
    setBlockStatus(VALID_AND_CLEAN, dmemExcTestVA, dmemExcTestPhysBlkArr[0]);

    dmemExcTestOff = align(rand(BLOCK_SIZE), 4);
    LwU8 *dmemExcTestAddr = (LwU8 *)(dmemExcTestVA + dmemExcTestOff);

    LwBool bRead = implType == READ_DMEM_IMPL || implType == READ_SP_IMPL;

    LwU8 *src = bRead ? dmemExcTestAddr : dmemExcTestPhysBuf;
    LwU8 *dest = bRead ? dmemExcTestPhysBuf : dmemExcTestAddr;

    RNGctx dataCtx = newRNGctx();
    // overwrite the destination with garbage
    fillMem(newRNGctx(), dest, sizeof(dmemExcTestPhysBuf));
    // fill the source with test data
    fillMem(dataCtx, src, sizeof(dmemExcTestPhysBuf));

    switch (dmemExcTestType)
    {
        case DMEM_EXC_TEST_MISS:
            // Unmap the VA.
            setBlockStatus(INVALID, dmemExcTestVA, dmemExcTestPhysBlkArr[0]);
            break;
        case DMEM_EXC_TEST_MULTIHIT:
            ;
            // Map too many VAs.
            LwU32 i;
            for (i = 1; i < dmemExcTestNMappedBlocks; i++)
            {
                setBlockStatus(VALID_AND_CLEAN, dmemExcTestVA, dmemExcTestPhysBlkArr[i]);
            }
            break;
        case DMEM_EXC_TEST_PAFAULT:
            ;
            // Move the DMVA bound above the VA.
            DMVAREGWR(DMVACTL, LW_U32_MAX);
            break;
        default:
            halt();
    }

    bHandlerWasCalled = LW_FALSE;
    bCorrectAddr = LW_FALSE;

    if (bShouldDoubleFault)
    {
        // It's correct behavior for falcon to halt on invalid stack access.
        DMVAREGWR(MAILBOX0, DMVA_OK);
    }

    LwU32 nBytes = 0;
    switch (implType)
    {
        case READ_DMEM_IMPL:
            nBytes = readDmemImpl(dest, src, implId);
            break;
        case WRITE_DMEM_IMPL:
            nBytes = writeDmemImpl(dest, src, implId);
            break;
        case READ_SP_IMPL:
            nBytes = readSpImpl(dest, src, implId);
            break;
        case WRITE_SP_IMPL:
            nBytes = writeSpImpl(dest, src, implId);
            break;
        case CALL_IMPL:
            switch (implId)
            {
                case 0:
                    falc_wspr(SP, dmemExcTestAddr + 4);
                    asm volatile("call %0;" ::"r"(dmlvlExceptionTestDummy));
                    break;
                case 1:
                    falc_wspr(SP, dmemExcTestAddr);
                    asm volatile("ret;");
                    break;
                case 2:
                    falc_wspr(SP, dmemExcTestAddr);
                    asm volatile("reti;");
                    break;
            }
    }

    // A miss on the stack should cause a halt.
    // Therefore, this code should never run after the call to read/writeImpl.
    UASSERT(bHandlerWasCalled);
    UASSERT(bCorrectAddr);
    UASSERT(!bShouldDoubleFault);

    UASSERT(nBytes > 0);
    // verify the test data was successfully moved from source to destination
    UASSERT(verifyMem(dataCtx, dest, nBytes) == nBytes);

    resetBlockStatus(dmemExcTestPhysBlkArr[0]);
    resetDmvactl();
    installExceptionHandler(defaultExceptionHandler);

    DMVAREGWR(MAILBOX0, DMVA_OK);
    halt();
}

TESTDEF(29, dmilwTest)
{
    DmInstrImpl impl;
    BlockStatus blockStatus;
    for (impl = 0; impl < N_DM_IMPLS; impl++)
    {
        for (blockStatus = 0; blockStatus < N_BLOCK_STATUS; blockStatus++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 va = randVa();
            LwU32 tag = va >> BLOCK_WIDTH;
            setBlockStatus(blockStatus, va, physBlk);
            dmilwImpl(physBlk, impl);
            BlockStatus newBlockStatus = getBlockStatus(physBlk);
            LwU32 newTag = dmva_dmblk(physBlk).tag;
            LwBool badStatus = (newBlockStatus != INVALID);
            LwBool badTag = (newTag != tag);
            resetBlockStatus(physBlk);
            if (badStatus || badTag)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(30, dmcleanTest)
{
    DmInstrImpl impl;
    BlockStatus blockStatus;
    for (impl = 0; impl < N_DM_IMPLS; impl++)
    {
        for (blockStatus = 0; blockStatus < N_BLOCK_STATUS; blockStatus++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 va = randVa();
            LwU32 tag = va >> BLOCK_WIDTH;
            setBlockStatus(blockStatus, va, physBlk);
            BlockStatus expectedStatus =
                (blockStatus == INVALID || blockStatus == PENDING) ? blockStatus : VALID_AND_CLEAN;
            falc_dmclean(physBlk);
            BlockStatus newBlockStatus = getBlockStatus(physBlk);
            LwU32 newTag = dmva_dmblk(physBlk).tag;
            LwBool badStatus = (newBlockStatus != expectedStatus);
            LwBool badTag = (newTag != tag);
            resetBlockStatus(physBlk);
            if (badStatus || badTag)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

TESTDEF(31, setdtagTest)
{
    DmInstrImpl impl;
    BlockStatus blockStatus;
    for (impl = 0; impl < N_DM_IMPLS; impl++)
    {
        for (blockStatus = 0; blockStatus < N_BLOCK_STATUS; blockStatus++)
        {
            markSubtestStart();
            LwU32 physBlk = randPhysBlkAboveStack();
            LwU32 va = randVa();
            LwU32 tag = va >> BLOCK_WIDTH;
            setBlockStatus(blockStatus, randVa(), physBlk);
            setdtagImpl(va, physBlk, impl);
            BlockStatus newBlockStatus = getBlockStatus(physBlk);
            LwU32 newTag = dmva_dmblk(physBlk).tag;
            LwBool badStatus = (newBlockStatus != VALID_AND_CLEAN);
            LwBool badTag = (newTag != tag);
            resetBlockStatus(physBlk);
            if (badStatus || badTag)
            {
                return DMVA_FAIL;
            }
        }
    }
    return DMVA_OK;
}

static volatile LwBool bDmaTagTestInterruptHandlerCalled = LW_FALSE;

TEST_OVLY(32) void dmaTagTestInterruptHandler(void)
{
    if (DRF_VAL(_PFALCON, _FALCON_IRQSTAT, _SWGEN0, DMVAREGRD(IRQSTAT)))
    {
        DMVAREGWR(IRQSCLR, FLD_SET_DRF(_PFALCON, _FALCON_IRQSCLR, _SWGEN0, _SET, 0));
    }
    if (DRF_VAL(_PFALCON, _FALCON_IRQSTAT, _DMA, DMVAREGRD(IRQSTAT)))
    {
        bDmaTagTestInterruptHandlerCalled = LW_TRUE;
        DMVAREGWR(IRQSCLR, FLD_SET_DRF(_PFALCON, _FALCON_IRQSCLR, _DMA, _SET, 0));
    }
}

TESTDEF(32, dmaTagTest)
{
    installInterruptHandler(dmaTagTestInterruptHandler);

    LwU32 irqDest = DMVAREGRD(IRQDEST);
    irqDest = FLD_SET_DRF(_PFALCON, _FALCON_IRQDEST2, _HOST_DMA, _FALCON, irqDest);
    irqDest = FLD_SET_DRF(_PFALCON, _FALCON_IRQDEST2, _TARGET_DMA, _FALCON_IRQ0, irqDest);
    DMVAREGWR(IRQDEST, irqDest);

    LwU32 irqMode = DMVAREGRD(IRQMODE);
    irqMode = FLD_SET_DRF(_PFALCON, _FALCON_IRQMODE, _LVL_DMA, _FALSE, irqMode);
    DMVAREGWR(IRQMODE, irqMode);

    DMVAREGWR(IRQMSET, FLD_SET_DRF(_PFALCON, _FALCON_IRQMSET, _DMA, _SET, 0));

    LwU32 dmemBlkStart = stackBottom / BLOCK_SIZE;
    LwU32 dmemBlkEnd = getDmemSize() - 1;
    LwU32 imemBlkStart = (selwreCodeEnd + ((LwU32)&_tc32e[0] - (LwU32)&_tc32s[0])) / BLOCK_SIZE;
    LwU32 imemBlkEnd = getImemSize() - 1;
    LwU32 dmemFbBlkStart = dmemBlkStart;
    LwU32 dmemFbBlkEnd = getImemVaSize() - 1;
    LwU32 imemFbBlkStart = ((LwU32)&_tc32e[0]) / BLOCK_SIZE;
    LwU32 imemFbBlkEnd = getImemVaSize() - 1;

    TEST_OVLY(32) void cleanup(void)
    {
        falc_dmwait();
        falc_imwait();
        DMVAREGWR(IRQMCLR, FLD_SET_DRF(_PFALCON, _FALCON_IRQMCLR, _DMA, _SET, 0));
        installInterruptHandler(defaultInterruptHandler);

        bDmaTagTestInterruptHandlerCalled = LW_FALSE;

        // ilwalidate all imem blocks that we DMA'ed
        LwU32 imemBlk;
        for (imemBlk = imemBlkStart; imemBlk <= imemBlkEnd; imemBlk++)
        {
            falc_imilw(imemBlk);
        }
    }

    LwBool bGenerateInterrupt;
    DmaDir dmaDirection;
    LwU32 testRound; // some more extensive testing
    const static LwU32 N_TEST_ROUNDS = 50;
    for (testRound = 0; testRound < N_TEST_ROUNDS; testRound++)
    {
        for (bGenerateInterrupt = 0; bGenerateInterrupt <= 1; bGenerateInterrupt++)
        {
            for (dmaDirection = DMVA_DMA_TO_FB; dmaDirection <= DMVA_DMA_FROM_FB; dmaDirection++)
            {
                markSubtestStart();

                UASSERT(!bDmaTagTestInterruptHandlerCalled);

                LwU32 nTransfers = randRange(1, 8);
                LwU32 nInterrupts = bGenerateInterrupt ? randRange(1, nTransfers) : 0;
                LwU32 nextExpectedTag = dmaDirection == DMVA_DMA_TO_FB
                    ? DMVAREGRD(DMAINFO_LWRRENT_FBWR_LOW)
                    : DMVAREGRD(DMAINFO_LWRRENT_FBRD_LOW);
                LwU32 tagLow = 0;
                LwBool bSomeInterrupt = LW_FALSE;
                while (nTransfers > 0)
                {
                    LwU32 xferSizeEncoded = rand(7);
                    LwU32 xferSize = POW2(xferSizeEncoded + 2);
                    LwBool bIsImem = 0;
                    if (dmaDirection == DMVA_DMA_FROM_FB)
                    {
                        bIsImem = rand(2);
                    }

                    LwU32 fbOff = align(rand(BLOCK_SIZE), xferSize);
                    LwU32 mOff = align(rand(BLOCK_SIZE), xferSize);
                    if (bIsImem)
                    {
                        // HW should automatically zero the lower 8 bits of fbOff and mOff for IMEM
                        // xfers.
                        // Additionally, the xfer size is implicitly 256B when using imread2.
                        fbOff += randRange(imemFbBlkStart, imemFbBlkEnd) * BLOCK_SIZE;
                        mOff += randRange(imemBlkStart, imemBlkEnd) * BLOCK_SIZE;
                    }
                    else
                    {
                        fbOff += randRange(dmemFbBlkStart, dmemFbBlkEnd) * BLOCK_SIZE;
                        mOff += randRange(dmemBlkStart, dmemBlkEnd) * BLOCK_SIZE;
                    }

                    UASSERT(nTransfers > 0);
                    // We want to generate an interrupt with a nInterrupts/nTransfers chance.
                    // If nInterrupts is 0, we will never generate an interrupt.
                    // If nTransfers == nInterrupts, we will always generate an interrupt.
                    LwBool bGenerateInterruptOnThisTransfer = (rand(nTransfers) < nInterrupts);
                    if(bGenerateInterruptOnThisTransfer)
                    {
                        bSomeInterrupt = LW_TRUE;
                    }

                    tagLow = dma2Impl(fbOff, mOff, xferSizeEncoded, dmaDirection, bIsImem,
                                      bGenerateInterruptOnThisTransfer);
                    if (tagLow != (nextExpectedTag++))
                    {
                        cleanup();
                        return DMVA_DMA_TAG_BAD_NEXT_TAG;
                    }

                    nTransfers--;
                }

                UASSERT(!bGenerateInterrupt || bSomeInterrupt);

                // Poll for DMAINFO_LOW. We will timeout if there is any incorrect behavior.
                while ((dmaDirection == DMVA_DMA_TO_FB
                        ? DMVAREGRD(DMAINFO_FINISHED_FBWR_LOW)
                        : DMVAREGRD(DMAINFO_FINISHED_FBRD_LOW)) != tagLow)
                {
                    continue;
                }

                if (bGenerateInterrupt)
                {
                    // Due to race conditions, the best coverage we can get is to check if the interrupt
                    // handler was ever called.
                    if (!bDmaTagTestInterruptHandlerCalled)
                    {
                        cleanup();
                        return DMVA_DMA_TAG_HANDLER_WAS_NOT_CALLED;
                    }
                    bDmaTagTestInterruptHandlerCalled = LW_FALSE;
                }
                else
                {
#ifdef FMODEL_DMA2_INTERRUPT_FIX
                    bDmaTagTestInterruptHandlerCalled = LW_FALSE;
#endif
                }
            }
        }
    }
    cleanup();
    return DMVA_OK;
}

TestOverlay testArr[DMEMVA_NUM_TESTS] =
{
    { init, 0, 0 }, // init resides in resident code
    TESTSTRUCT(1,  readRegsTest                    ),
    TESTSTRUCT(2,  ilwalidOnResetTest              ),
    TESTSTRUCT(3,  dmemReadWriteTest               ),
    TESTSTRUCT(4,  dmblkTest                       ),
    TESTSTRUCT(5,  writeSetsDirtyBitTest           ),
    TESTSTRUCT(6,  dmtagTest                       ),
    TESTSTRUCT(7,  dmvaBoundTest                   ),
    TESTSTRUCT(8,  ememTest                        ),
    TESTSTRUCT(9,  cmemTest                        ),
    TESTSTRUCT(10, dmemcPaSetDirtyTest             ),
    TESTSTRUCT(11, dmemcIlwalidBlockTest           ),
    TESTSTRUCT(12, dmemcPaSetTagTest               ),
    TESTSTRUCT(13, dmemcVaSetDirtyTest             ),
    TESTSTRUCT(14, dmemcVaSetTagTest               ),
    TESTSTRUCT(15, dmemcPaReadTest                 ),
    TESTSTRUCT(16, dmemcVaReadTest                 ),
    TESTSTRUCT(17, dmaReadWriteTest                ),
    TESTSTRUCT(18, dmaSetTagTest                   ),
    TESTSTRUCT(19, hsBootromPaTest                 ),
    TESTSTRUCT(20, hsBootromVaTest                 ),
    TESTSTRUCT(21, carveoutTest                    ),
    TESTSTRUCT(22, dmlvlChangePermissionsTest      ),
    TESTSTRUCT(23, dmaSetMaskTest                  ),
    TESTSTRUCT(24, dmlvlExceptionTest              ),
    TESTSTRUCT(25, dmatrfSelwrityTest              ),
    TESTSTRUCT(26, dmemcSelwrityTest               ),
    TESTSTRUCT(27, scpTest                         ),
    TESTSTRUCT(28, dmemExcTest                     ),
    TESTSTRUCT(29, dmilwTest                       ),
    TESTSTRUCT(30, dmcleanTest                     ),
    TESTSTRUCT(31, setdtagTest                     ),
    TESTSTRUCT(32, dmaTagTest                      ),
};

int main(void)
{
    while (LW_TRUE)
    {
        LwU32 testId = getTestId();
        UASSERT(testId < DMEMVA_NUM_TESTS);

        initSubtestIndicator();

        loadTestOverlay(testId);

        DMVA_RC rc = testArr[testId].testFunc();
        DMVAREGWR(MAILBOX0, rc);

        wait();
    }
}

LwU32 getTestId(void)
{
    LwU32 testId = DMVAREGRD(MAILBOX1);
    DMVAREGWR(MAILBOX1, DMVA_NULL);
    return testId;
}

void loadTestOverlay(LwU32 testId)
{
    if (testId == 0)
    {
        return;
    }
    LwU32 codeFbStart = testArr[testId].codeStart;
    LwU32 codeFbEnd = testArr[testId].codeEnd;
    LwU32 codeFbSize = codeFbEnd - codeFbStart;
    loadImem(selwreCodeEnd, codeFbStart, codeFbSize, LW_FALSE);

    // make sure the test overlay got loaded
    LwU32 blockOff;
    for (blockOff = 0; blockOff < codeFbSize; blockOff += BLOCK_SIZE)
    {
        LwU32 imemBlk = (selwreCodeEnd + blockOff) / BLOCK_SIZE;
        LwU32 expectedTag = (codeFbStart + blockOff) / BLOCK_SIZE;
        LwU32 d;
        falc_imblk(&d, imemBlk);
        LwU32 tag = (d >> 8) & 0xffff;
        UASSERT(tag == expectedTag);
        LwBool valid = (d >> 24) & 1;
        LwBool pending = (d >> 25) & 1;
        LwBool secure = (d >> 26) & 1;
        UASSERT(valid);
        UASSERT(!pending);
        UASSERT(!secure);
    }
}

static LwU32 subTestIndex;

void initSubtestIndicator(void)
{
    subTestIndex = 0;
}

void markSubtestStart(void)
{
    DMVAREGWR(MAILBOX0, DMVA_FORWARD_PROGRESS);
    DMVAREGWR(MAILBOX1, subTestIndex++);
    wait();
}
