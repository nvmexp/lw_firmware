/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwos_dma_hs.c
 * @brief  Falcon DMA Library
 *
 * A simple set of DMA routines for reading/writing external memory using the
 * Falcon's DMA engine in HS mode. The routines make no assumptions on the
 * alignment of the source and destination buffers provided by the caller and
 * will always attempt to minimize the number of DMA requests to maximize DMA
 * throughput. The greater the alignment of the provided buffers, the fewer transactions that
 * will be required.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwos_dma.h"
#include "lwos_dma_hs.h"
#include "lwoslayer.h"
#include "lwrtos.h"
#include "lwostask.h"
#include "csb.h"
#include "common_hslib.h"
#include "linkersymbols.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_lw64.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Update the 'dmread_ctx' field (see Falcon IAS) in the CTX SPR.
 *
 * @param[in]  ctxSpr  The current value of the CTX SPR. The value passed in
 *                     will not be directly updated by this macro.
 *
 * @param[in]  dmaIdx  ctxdma index or transcfg code to update the field with
 *
 * @return 'ctxSpr' with the updated 'dmread_ctx' field.
 */
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~(0x7 << 8)) | ((dmaIdx) << 8))

/*!
 * Update the 'dmwrite_ctx' field (see Falcon IAS) in the CTX SPR.
 *
 * @param[in]  ctxSpr  The current value of the CTX SPR. The value passed in
 *                     will not be directly updated by this macro.
 *
 * @param[in]  dmaIdx  ctxdma index or transcfg code to update the field with
 *
 * @return 'ctxSpr' with the updated 'dmwrite_ctx' field.
 */
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~(0x7 << 12)) | ((dmaIdx) << 12))

#define PTR32_LO(_ptr64)    ((LwU32 *)(_ptr64))
#define PTR32_HI(_ptr64)    (PTR32_LO(_ptr64) + 1)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void _libDmaHsEntry(void)
    GCC_ATTRIB_SECTION("imem_libDmaHs", "start")
    GCC_ATTRIB_USED();

/* ------------------------- Private Functions ------------------------------ */
/*!
 * _libMemHSEntry
 *
 * @brief Entry function of HS DMA library. This function merely returns. It
 *        is used to validate the overlay blocks corresponding to the secure
 *        overlay before jumping to arbitrary functions inside it. The linker
 *        script should make sure that it puts this section at the top of the
 *        overlay to ensure that we can jump to the overlay's 0th offset before
 *        exelwting code at arbitrary offsets inside the overlay.
 */
static void
_libDmaHsEntry(void)
{
    VALIDATE_THAT_CALLER_IS_HS();
    return;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Use the Falcon's DMA controller to perform a transfer of data
 *          between the given FB / SYSMEM address (DMA address) and DMEM.
 *
 * The Falcon DMA controller has a minimum transfer size of 16 / 4 bytes for
 * reads / writes respectively. The number of transfered bytes should be an
 * even multiple of this minimal value. Additionally, it is expected that the
 * given DMEM pointer as well as DMA addresses have same minimum alignment.
 * Zero bytes will be transfered if this requirement is not satisfied.
 *
 * Function checks DMA suspension state before issuing DMA transfers.
 * It raises DMA_EXCEPTION if DMA is suspended and waits until resumed.
 *
 * @param[in]   pBuf        Pointer to the DMEM buffer.
 * @param[in]   pMemDesc    FB / SYSMEM surface descriptor.
 * @param[in]   offset      Offset of the first byte within the surface.
 * @param[in]   numBytes    The number of bytes to transfer.
 * @param[in]   bRead       Direction of the transfer.
 *
 * @note    !RECOMMENDATION!
 *  If Task is performing back to back calls to this API then consider wrapping
 *  these calls with @ref lwosDmaLockAcquire() and @ref lwosDmaLockRelease().
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 */
FLCN_STATUS
dmaTransfer_hs
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes,
    LwBool              bRead
)
{
    LwU32       hi;
    LwU32       lo;
    LwU32       base;
    LwU32       dmaOffset;
    LwU32       dmb;
    LwU16       ctx;
#if PA_47_BIT_SUPPORTED
    LwU16       dmb1;
    LwU32       base1;
#endif
    LwU8        dmaIdx;
    FLCN_STATUS retval = FLCN_OK;
#ifdef DMEM_VA_SUPPORTED

    if (((LwU32)pBuf + numBytes) > DMEM_IDX_TO_ADDR((LwU32)_dmem_va_bound))
    {
        if ((LwU32)pBuf < DMEM_IDX_TO_ADDR((LwU32)_dmem_va_bound))
        {
            // Do not allow DMEM buffer to span across VA_BOUND limit.
            OS_HALT();
        }
    }
#endif

    dmaIdx = DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, pMemDesc->params);

    // Compute the actual FB / SYSMEM address of the first transfered byte.
    hi = pMemDesc->address.hi;
    lo = pMemDesc->address.lo + offset;
    if (lo < pMemDesc->address.lo)
    {
        hi += 1;
    }

    //
    // Break the FB / SYSMEM address to DMB1, DMB, and DMA offset, assuring
    // that the DMA offset is minimal maximizing size of potential transfer.
    //
    base = (lo >> 8) | (hi << 24);
#if PA_47_BIT_SUPPORTED
    base1 = (hi >> 8);
#endif
    dmaOffset = (lo & 0xFF);

    //
    // Both buffer addresses (source/destination) as well as the transfer
    // size ifself must be aligned to the minimal supported transfer size.
    //
    if (!VAL_IS_ALIGNED((dmaOffset | (LwU32)pBuf | numBytes),
                        DMA_XFER_SIZE_BYTES(bRead ?
                                            DMA_XFER_ESIZE_MIN_READ :
                                            DMA_XFER_ESIZE_MIN_WRITE)))
    {
        return FLCN_ERR_DMA_ALIGN;
    }

    // Save and update 'DMB1', 'DMB', and 'CTX' falcon's SPRs.
#if PA_47_BIT_SUPPORTED
    falc_rspr(dmb1, DMB1);
    falc_wspr(DMB1, base1);
#endif
    falc_rspr(dmb, DMB);
    falc_rspr(ctx, CTX);
    falc_wspr(DMB, base);
    falc_wspr(CTX, bRead ?
              DMA_SET_DMREAD_CTX(ctx, dmaIdx) :
              DMA_SET_DMWRITE_CTX(ctx, dmaIdx));

    // Start (multiple) DMA transfers, each being relative to programmed base.
    while (numBytes > 0)
    {
        LwU32 xferSize;
        LwU32 temp;
        LwU8  encSize = 0;
        LwU32 pa;

        // Determine the MAX transfer size based on buffers' alignment.
        xferSize = dmaOffset | (LwU32)pBuf |
            DMA_XFER_SIZE_BYTES(bRead ? DMA_XFER_ESIZE_MAX_READ :
                                DMA_XFER_ESIZE_MAX_WRITE);
        xferSize = LOWESTBIT(xferSize);

        // Reduce the MAX transfer size to not exceed remaining data.
        while (xferSize > numBytes)
        {
            xferSize >>= 1;
        }

        // Encode transfer size to the HW format: size = 2^(e + 2)
        temp = xferSize >> 3;
        while (temp > 0)
        {
            temp >>= 1;
            encSize++;
        }

        pa = osDmemAddressVa2PaGet(pBuf);
        if (bRead)
        {
            falc_dmread(dmaOffset,
                        DRF_NUM(_FLCN, _DMREAD, _PHYSICAL_ADDRESS, pa) |
                        DRF_NUM(_FLCN, _DMREAD, _ENC_SIZE, encSize));
        }
        else
        {
            falc_dmwrite(dmaOffset,
                         DRF_NUM(_FLCN, _DMWRITE, _PHYSICAL_ADDRESS, pa) |
                         DRF_NUM(_FLCN, _DMWRITE, _ENC_SIZE, encSize));
        }
        dmaOffset += xferSize;
        pBuf += xferSize;
        numBytes -= xferSize;
    }

    // Restore falcon's SPRs before exiting the critical section.
#if PA_47_BIT_SUPPORTED
    falc_wspr(DMB1, dmb1);
#endif
    falc_wspr(DMB, dmb);
    falc_wspr(CTX, ctx);

    //
    // TODO: If DMA_NACK_SUPPORTED supported, we need to add HS version
    // vApplicationDmaNackCheckAndClear().
    //
    falc_dmwait();

    return retval;
}
