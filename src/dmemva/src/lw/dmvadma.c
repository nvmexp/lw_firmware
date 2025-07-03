/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <lwtypes.h>
#include "dev_falcon_v4.h"

#include "dmvadma.h"
#include "dmvacommon.h"
#include "dmvaassert.h"
#include "dmvaregs.h"
#include "dmvautils.h"
#include "dmvamem.h"
#include "dmvainstrs.h"
#include <lwmisc.h>

HS_SHARED static LwU32 dmvaIssueDmaUsingDmReadWrite(LwU32 offset, LwU32 fbBase, LwU32 fbOff,
                                                    LwU32 sizeInBytes, LwBool bSetTag,
                                                    LwBool bIsImem, LwBool bSetLvl, LwU32 lvl,
                                                    DmaDir dmaDirection, LwU32 ctxDma, LwBool b2)
{
    LwU32 bytesXfered = 0;
    LwU32 memOff = offset;
    LwU32 memSrc = offset;
    LwU32 xferSize;
    LwU32 e;
    LwU16 ctx;

    // Set new DMA and CTX
    if (bIsImem)
    {
        falc_wspr(IMB, fbBase);
    }
    else
    {
        falc_wspr(DMB, fbBase);
    }

    // Update the CTX, this depends on mem type and dir
    falc_rspr(ctx, CTX);
    if (bIsImem)
    {
        falc_wspr(CTX, DMA_SET_IMREAD_CTX(ctx, ctxDma));
    }
    else
    {
        falc_wspr(CTX, (dmaDirection == DMVA_DMA_FROM_FB) ? DMA_SET_DMREAD_CTX(ctx, ctxDma)
                                                          : DMA_SET_DMWRITE_CTX(ctx, ctxDma));
    }

    e = DMA_XFER_ESIZE_MAX;
    xferSize = DMA_XFER_SIZE_BYTES(e);

    while (bytesXfered != sizeInBytes)
    {
        if (((sizeInBytes - bytesXfered) >= xferSize) &&
            DMA_VAL_IS_ALIGNED((LwU32)memOff, xferSize) && DMA_VAL_IS_ALIGNED(fbOff, xferSize))
        {
            if (dmaDirection == DMVA_DMA_TO_FB)
            {
                memSrc = (LwU32)memOff;

                if (bIsImem)
                {
                    // Error, no imwrite
                    return bytesXfered;
                }
                else
                {
                    LwU32 fbOffset = fbOff + bytesXfered;
                    LwU32 dmemSizeOffset = memSrc | (e << 16);

                    if (b2)
                    {
                        falc_dmwrite2(fbOffset, dmemSizeOffset);
                    }
                    else
                    {
                        falc_dmwrite(fbOffset, dmemSizeOffset);
                    }
                }
            }
            else
            {
                if (bIsImem)
                {
                    LwU32 fbOffset = fbOff + bytesXfered;
                    LwU32 imemSizeOffset = ((LwU32)memOff + bytesXfered) | (e << 16);

                    if (b2)
                    {
                        dmva_imread2(fbOffset, imemSizeOffset);
                    }
                    else
                    {
                        falc_imread(fbOffset, imemSizeOffset);
                    }
                }
                else
                {
                    LwU32 setTag = bSetTag ? BIT(31) : 0;
                    LwU32 setLvl = bSetLvl ? BIT(27) : 0;
                    LwU32 dmLvl = REF_NUM(26 : 24, lvl);

                    LwU32 fbOffset = fbOff + bytesXfered;
                    LwU32 dmemSizeOffset =
                        ((LwU32)memOff + bytesXfered) | (e << 16) | setTag | setLvl | dmLvl;

                    if (b2)
                    {
                        falc_dmread2(fbOffset, dmemSizeOffset);
                    }
                    else
                    {
                        falc_dmread(fbOffset, dmemSizeOffset);
                    }
                }
            }
            bytesXfered += xferSize;

            // Restore e
            e = DMA_XFER_ESIZE_MAX;
            xferSize = DMA_XFER_SIZE_BYTES(e);
        }
        // try the next largest transfer size
        else
        {
            //
            // Ensure that we have not dropped below the minimum transfer size
            // supported by the HW. Such and event indicates that the requested
            // number of bytes cannot be written either due to mis-alignment of
            // the source and destination pointers or the requested write-size
            // not being a multiple of the minimum transfer size.
            //
            if (e > DMA_XFER_ESIZE_MIN)
            {
                e--;
                xferSize = DMA_XFER_SIZE_BYTES(e);
            }
            else
            {
                break;
            }
        }
    }

    if (bIsImem)
    {
        falc_imwait();
    }
    else
    {
        falc_dmwait();
    }
    return bytesXfered;
}

HS_SHARED static void dmvaIssueDmaUsingDMCTL(LwU32 offset, LwU32 fbBase, LwU32 fbOff,
                                             LwU32 sizeInBytes, LwBool bSetTag, LwBool bIsImem,
                                             LwBool bSetLvl, LwU32 lvl, DmaDir dmaDirection,
                                             LwU32 ctxDma)
{
    LwU32 data = 0;
    LwU32 dmaCmd = 0;
    LwU32 bytesXfered = 0;
    LwU32* fbifCtl = (LwU32*)CAT(CAT(LW_C, PREFIX), _FBIF_CTL);

    //
    // Disable CTX requirement for falcon DMA engine
    //
    data = DMVACSBRD(fbifCtl);
    data = FLD_SET_DRF(_PFALCON, _FBIF_CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, data);
    DMVACSBWR(fbifCtl, data);

    DMVAREGWR(DMACTL, 0);

    DMVAREGWR(DMATRFBASE, fbBase);
    DMVAREGWR(DMATRFBASE1, 0x00);

    // prepare DMA command
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _IMEM, bIsImem ? 1 : 0, dmaCmd);
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _WRITE, dmaDirection != DMVA_DMA_FROM_FB,
                             dmaCmd);
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _SET_DMTAG, bSetTag ? 1 : 0, dmaCmd);
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _SET_DMLVL, bSetLvl ? 1 : 0, dmaCmd);
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _LVL, lvl, dmaCmd);

    LwU32 transSize = log_2(sizeInBytes) - 2;
    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _SIZE, transSize, dmaCmd);

    dmaCmd = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFCMD, _CTXDMA, ctxDma, dmaCmd);

    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFMOFFS, _OFFS, (offset + bytesXfered), 0);
    DMVAREGWR(DMATRFMOFFS, data);

    data = FLD_SET_DRF_NUM(_PFALCON, _FALCON_DMATRFFBOFFS, _OFFS, (fbOff + bytesXfered), 0);
    DMVAREGWR(DMATRFFBOFFS, data);

    // Write the command
    DMVAREGWR(DMATRFCMD, dmaCmd);

    //
    // Poll for completion
    //
    do
    {
        data = DMVAREGRD(DMATRFCMD);
    } while (FLD_TEST_DRF(_PFALCON_FALCON, _DMATRFCMD, _IDLE, _FALSE, data));

    bytesXfered += 256;
}

void dmvaIssueDma(LwU32 offset, LwU32 fbOff, LwU32 sizeInBytes, LwBool bSetTag, LwBool bIsImem,
                  LwBool bSetLvl, LwU32 lvl, DmaDir dmaDirection, DmaImpl dmaImpl)
{
    UASSERT(isPow2(sizeInBytes));
    UASSERT(sizeInBytes >= DMA_XFER_MIN);
    UASSERT(sizeInBytes <= DMA_XFER_MAX);
    switch (dmaImpl)
    {
        case IMPL_DM_RW:  // fallthrough
        case IMPL_DM_RW2:
            dmvaIssueDmaUsingDmReadWrite(offset, fbBufBlkIdxDmem, fbOff, sizeInBytes, bSetTag,
                                         bIsImem, bSetLvl, lvl, dmaDirection, 0,
                                         dmaImpl == IMPL_DM_RW2);
            return;
        case IMPL_DMATRF:
            dmvaIssueDmaUsingDMCTL(offset, fbBufBlkIdxDmem, fbOff, sizeInBytes, bSetTag, bIsImem,
                                   bSetLvl, lvl, dmaDirection, 0);
            return;
        default:
            halt();
    }
}

void dmvaIssueDmaAtUcodeLevel(LwU32 offset, LwU32 fbOff, LwU32 sizeInBytes, LwBool bSetTag,
                              LwBool bIsImem, LwBool bSetLvl, LwU32 lvl, DmaDir dmaDirection,
                              DmaImpl dmaImpl, SecMode ucodeLevel)
{
    UASSERT(ucodeLevel < N_SEC_MODES);
    if (ucodeLevel == SEC_MODE_HS)
    {
        callHS(HS_ISSUE_DMA, offset, fbOff, sizeInBytes, bSetTag, bIsImem, bSetLvl, lvl,
               dmaDirection, dmaImpl);
    }
    else
    {
        LwU32 oldLevel = getUcodeLevel();
        callHS(HS_SET_LVL, ucodeLevel);
        dmvaIssueDma(offset, fbOff, sizeInBytes, bSetTag, bIsImem, bSetLvl, lvl, dmaDirection,
                     dmaImpl);
        callHS(HS_SET_LVL, oldLevel);
    }
}

void loadImem(LwU32 dmemAddr, LwU32 fbImemAddr, LwU32 nBytes, LwBool bSelwre)
{
    // TODO: add these
    // UASSERT(aligned(dmemAddr, BLOCK_SIZE));
    // UASSERT(aligned(fbImemAddr, BLOCK_SIZE));
    // UASSERT(aligned(nBytes, BLOCK_SIZE));

    LwU32 sec;
    falc_rspr(sec, SEC);
    falc_wspr(SEC, SVEC_REG(0, 0, bSelwre ? 1 : 0, 0));

    falc_wspr(IMB, fbBufBlkIdxImem);
    LwU16 ctx;
    falc_rspr(ctx, CTX);
    falc_wspr(CTX, DMA_SET_IMREAD_CTX(ctx, 0));

    LwU32 lwrrentBlock;
    for (lwrrentBlock = 0; lwrrentBlock < nBytes; lwrrentBlock += 256)
    {
        falc_imread(fbImemAddr + lwrrentBlock,
                    (dmemAddr + lwrrentBlock) | (DMA_XFER_ESIZE_MAX << 16));
    }

    falc_imwait();
    falc_wspr(SEC, sec);
}

LwU32 dma2Impl(LwU32 fbOff, LwU32 mOff, LwU32 size, DmaDir dmaDirection, LwBool bIsImem,
               LwBool bGenerateInterrupt)
{
    UASSERT(dmaDirection == DMVA_DMA_TO_FB || dmaDirection == DMVA_DMA_FROM_FB);
    UASSERT(!(bIsImem && dmaDirection == DMVA_DMA_TO_FB));  // no imwrite2
    union
    {
        struct PACKED
        {
            LwU32 MOFFS : 16;         /* 15:0  */
            LwU32 SIZE : 3;           /* 18:16 */
            LwU32 UNUSED1 : 1;        /* 19:19 */
            LwBool GEN_INTERRUPT : 1; /* 20:20 */
            LwU32 UNUSED2 : 11;       /* 31:21 */
        } structure;
        LwU32 integer;
    } memSizeOffset;
    CASSERT(sizeof(memSizeOffset.structure) == sizeof(memSizeOffset.integer));
    memSizeOffset.structure.MOFFS = mOff;
    memSizeOffset.structure.SIZE = size;
    memSizeOffset.structure.GEN_INTERRUPT = bGenerateInterrupt;

    LwU32 ctx;
    falc_rspr(ctx, CTX);

    LwU32 tagLow;

    if (bIsImem)
    {
        falc_wspr(IMB, fbBufBlkIdxImem);
        falc_wspr(CTX, DMA_SET_IMREAD_CTX(ctx, 0));
        tagLow = dmva_imread2(fbOff, memSizeOffset.integer);
    }
    else
    {
        falc_wspr(DMB, fbBufBlkIdxDmem);
        falc_wspr(CTX, (dmaDirection == DMVA_DMA_FROM_FB) ? DMA_SET_DMREAD_CTX(ctx, 0)
                                                          : DMA_SET_DMWRITE_CTX(ctx, 0));
        if (dmaDirection == DMVA_DMA_TO_FB)
        {
            tagLow = falc_dmwrite2(fbOff, memSizeOffset.integer);
        }
        else
        {
            tagLow = falc_dmread2(fbOff, memSizeOffset.integer);
        }
    }
#ifdef FMODEL_DMA2_FIX
    return tagLow - 1;
#else
    return tagLow;
#endif
}
