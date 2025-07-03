/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_DMA_H
#define DMVA_DMA_H

#include "dmvaselwrity.h"

#define DMA_XFER_ESIZE_MIN (0x00)  //   4-bytes
#define DMA_XFER_ESIZE_MAX (0x06)  // 256-bytes
#define DMA_XFER_MIN POW2(DMA_XFER_ESIZE_MIN + 2)
#define DMA_XFER_MAX POW2(DMA_XFER_ESIZE_MAX + 2)
#define DMA_XFER_SIZE_BYTES(e) POW2((e)+2)
#define DMA_VAL_IS_ALIGNED(a, b) ((((b)-1) & (a)) == 0)

// DMA CTX update macros
#define DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx) (((ctxSpr) & ~(0x7 << 0)) | ((dmaIdx) << 0))
#define DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx) (((ctxSpr) & ~(0x7 << 8)) | ((dmaIdx) << 8))
#define DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) (((ctxSpr) & ~(0x7 << 12)) | ((dmaIdx) << 12))

typedef enum
{
    IMPL_DM_RW = 0,
    IMPL_DM_RW2,
    IMPL_DMATRF,
    N_DMA_IMPLS,
} DmaImpl;

typedef enum
{
    DMVA_DMA_TO_FB,
    DMVA_DMA_FROM_FB,
} DmaDir;

HS_SHARED void dmvaIssueDma(LwU32 offset, LwU32 fbOff, LwU32 sizeInBytes, LwBool bSetTag,
                            LwBool bIsImem, LwBool bSetLvl, LwU32 lvl, DmaDir dmaDirection,
                            DmaImpl dmaImpl);

void dmvaIssueDmaAtUcodeLevel(LwU32 offset, LwU32 fbOff, LwU32 sizeInBytes, LwBool bSetTag,
                              LwBool bIsImem, LwBool bSetLvl, LwU32 lvl, DmaDir dmaDirection,
                              DmaImpl dmaImpl, SecMode ucodeLevel);

void loadImem(LwU32 dmemAddr, LwU32 fbImemAddr, LwU32 nBytes, LwBool bSelwre);

LwU32 dma2Impl(LwU32 fbOff, LwU32 mOff, LwU32 size, DmaDir dmaDirection, LwBool bIsImem,
               LwBool bGenerateInterrupt);

#endif
