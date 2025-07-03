/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lwos_dma.c
 * @brief  Falcon DMA Library
 *
 * A simple set of DMA routines for reading/writing external memory using the
 * Falcon's DMA engine. The routines make no assumptions on the alignment of the
 * source and destination buffers provided by the caller and will always attempt
 * to minimize the number of DMA requests to maximize DMA throughput. The
 * greater the alignment of the provided buffers, the fewer transactions that
 * will be required.
 */

/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "lwos_dma.h"
#include "lwrtos.h"
#include "lwoslayer.h"
#include "lwos_ovl_priv.h"
#include "linkersymbols.h"
#include "lwtypes.h"
#include "lwrtosHooks.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib_lw64.h"

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
    (((ctxSpr) & ~(0x7UL << 8)) | (((LwU16) dmaIdx) << 8))

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
    (((ctxSpr) & ~(0x7UL << 12)) | (((LwU16) dmaIdx) << 12))

#define PTR32_LO(_ptr64)    ((LwU32 *)(_ptr64))
#define PTR32_HI(_ptr64)    (PTR32_LO(_ptr64) + 1)

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
#ifdef DMA_NACK_SUPPORTED
extern LwBool vApplicationDmaNackCheckAndClear(void);
#endif //DMA_NACK_SUPPORTED
#ifdef DMA_REGION_CHECK
extern LwBool vApplicationIsDmaIdxPermitted(LwU8 dmaIdx);
#endif //DMA_REGION_CHECK
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
#ifndef UPROC_RISCV
#ifdef DMA_REGION_CHECK
static FLCN_STATUS s_dmaTransfer(void *pBuf,RM_FLCN_MEM_DESC *pMemDesc,LwU32 offset, LwU32 numBytes, LwBool bRead, LwBool bCheckRegion);
#else // DMA_REGION_CHECK
static FLCN_STATUS s_dmaTransfer(void *pBuf,RM_FLCN_MEM_DESC *pMemDesc,LwU32 offset, LwU32 numBytes, LwBool bRead);
#endif // DMA_REGION_CHECK
#endif // UPROC_RISCV

/* ------------------------- Public Functions ------------------------------- */
#ifndef UPROC_RISCV
/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to read a buffer of
 *          data from the given FB / SYSMEM address (DMA address) into DMEM.
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaRead_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
#ifndef DMA_REGION_CHECK
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE);
#else // DMA_REGION_CHECK
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE, LW_TRUE);
#endif // DMA_REGION_CHECK
}

/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to write a buffer of
 *          data in DMEM to given FB / SYSMEM address (DMA address)
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaWrite_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
#ifndef DMA_REGION_CHECK
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE);
#else // DMA_REGION_CHECK
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE, LW_TRUE);
#endif // DMA_REGION_CHECK
}

#ifdef DMA_REGION_CHECK
/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to read a buffer of
 *          data from the given FB / SYSMEM address (DMA address) into DMEM.
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaReadUnrestricted_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_TRUE, LW_FALSE);
}

/*!
 * @brief   Wrapper function around @ref s_dmaTransfer used to write a buffer of
 *          data in DMEM to given FB / SYSMEM address (DMA address)
 *
 * @copydoc s_dmaTransfer
 */
FLCN_STATUS
dmaWriteUnrestricted_IMPL
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes
)
{
    return s_dmaTransfer(pBuf, pMemDesc, offset, numBytes, LW_FALSE, LW_FALSE);
}
#endif // DMA_REGION_CHECK

/* ------------------------ Static Functions  ------------------------------- */
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
 * @param[in]   pBuf         Pointer to the DMEM buffer.
 * @param[in]   pMemDesc     FB / SYSMEM surface descriptor.
 * @param[in]   offset       Offset of the first byte within the surface.
 * @param[in]   numBytes     The number of bytes to transfer.
 * @param[in]   bRead        Direction of the transfer.
 * @param[in]   bCheckRegion DMA region check enabled or not
 *
 * @note    !RECOMMENDATION!
 *  If Task is performing back to back calls to this API then consider wrapping
 *  these calls with @ref lwrtosENTER_DMA() and @ref lwrtosEXIT_DMA().
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 * @return  FLCN_ERR_DMA_NACK
 *              on virtual address fault, if HW supported and enabled
 * @return  FLCN_ERR_DMA_UNEXPECTED_DMAIDX
 *              on trying to use a dmaIdx not permitted with these interfaces
 * @return  FLCN_ERR_ILWALID_ARGUMENT
 *              on bad address
 */
#ifndef DMA_REGION_CHECK
static FLCN_STATUS
s_dmaTransfer
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes,
    LwBool              bRead
)
#else // DMA_REGION_CHECK
static FLCN_STATUS
s_dmaTransfer
(
    void               *pBuf,
    RM_FLCN_MEM_DESC   *pMemDesc,
    LwU32               offset,
    LwU32               numBytes,
    LwBool              bRead,
    LwBool              bCheckRegion
)
#endif // DMA_REGION_CHECK
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
#endif // PA_47_BIT_SUPPORTED
    LwU8        dmaIdx;
    FLCN_STATUS retval = FLCN_OK;

#ifdef DMEM_VA_SUPPORTED
    LwU32       dmemVa = (LwU32)pBuf;

    if ((dmemVa + numBytes) > DMEM_IDX_TO_ADDR((LwU32)(LwUPtr)_dmem_va_bound))
    {
        if (dmemVa < DMEM_IDX_TO_ADDR((LwU32)(LwUPtr)_dmem_va_bound))
        {
            // Do not allow DMEM buffer to span across VA_BOUND limit.
            OS_HALT();
        }
    }
#endif // DMEM_VA_SUPPORTED


    dmaIdx = (LwU8) DRF_VAL(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, pMemDesc->params);

#ifdef DMA_REGION_CHECK
    if (bCheckRegion && !vApplicationIsDmaIdxPermitted(dmaIdx))
    {
        return FLCN_ERR_DMA_UNEXPECTED_DMAIDX;
    }
#endif // DMA_REGION_CHECK

    // Compute the actual FB / SYSMEM address of the first transfered byte.
    hi = pMemDesc->address.hi;
    lo = pMemDesc->address.lo + offset;
    if (lo < pMemDesc->address.lo)
    {
        if (hi < LW_U32_MAX)
        {
            hi += 1U;
        }
        else
        {
#ifdef SAFERTOS
            lwrtosHookError(lwrtosTaskGetLwrrentTaskHandle(), (LwS32) FLCN_ERR_ILWALID_ARGUMENT);
#else //SAFERTOS
            OS_HALT();
#endif //SAFERTOS
            return FLCN_ERR_ILWALID_ARGUMENT;
        }
    }

    //
    // Break the FB / SYSMEM address to DMB1, DMB, and DMA offset, assuring
    // that the DMA offset is minimal maximizing size of potential transfer.
    //
    base   = (lo >> 8) | (hi << 24);
#if PA_47_BIT_SUPPORTED
    base1  = (hi >> 8);
#endif // PA_47_BIT_SUPPORTED
    dmaOffset = (lo & 0xFFU);

    //
    // Both buffer addresses (source/destination) as well as the transfer
    // size ifself must be aligned to the minimal supported transfer size.
    //
    if (!VAL_IS_ALIGNED((dmaOffset | LW_PTR_TO_LWUPTR(pBuf) | numBytes),
                        DMA_XFER_SIZE_BYTES(bRead ?
                                            DMA_XFER_ESIZE_MIN_READ :
                                            DMA_XFER_ESIZE_MIN_WRITE)))
    {
        return FLCN_ERR_DMA_ALIGN;
    }

    // Critical section is required since we modify multiple Falcon SPRs.
    lwrtosENTER_CRITICAL();

#ifdef ON_DEMAND_PAGING_BLK
    {
        LwU32 uxTempIdx = 0;
        volatile LwU8 ucTempReadChar = 0;
        while (uxTempIdx < numBytes)
        {
            ucTempReadChar = ((volatile LwU8*)pBuf)[uxTempIdx];
            uxTempIdx++;
        }
        uxTempIdx = 0;
        while (uxTempIdx < numBytes)
        {
            ucTempReadChar = ((volatile LwU8*)pBuf)[uxTempIdx];
            uxTempIdx++;
        }
    }
#endif // ON_DEMAND_PAGING_BLK

    lwosDmaLockAcquire();

    lwosDmaExitSuspension();

    // Save and update 'DMB1', 'DMB', and 'CTX' falcon's SPRs.
#if PA_47_BIT_SUPPORTED
    falc_rspr(dmb1, DMB1);
    falc_wspr(DMB1, base1);
#endif // PA_47_BIT_SUPPORTED
    falc_rspr(dmb, DMB);
    falc_rspr(ctx, CTX);
    falc_wspr(DMB, base);
    falc_wspr(CTX, bRead ?
              DMA_SET_DMREAD_CTX(ctx, dmaIdx) :
              DMA_SET_DMWRITE_CTX(ctx, dmaIdx));

    // Start (multiple) DMA transfers, each being relative to programmed base.
    while (numBytes > 0U)
    {
        LwU32 xferSize;
        LwU32 temp;
        LwU8  encSize = 0;
        LwU32 pa;

        // Determine the MAX transfer size based on buffers' alignment.
        xferSize = dmaOffset | LW_PTR_TO_LWUPTR(pBuf) |
            DMA_XFER_SIZE_BYTES(bRead ? DMA_XFER_ESIZE_MAX_READ :
                                        DMA_XFER_ESIZE_MAX_WRITE);
        xferSize = LOWESTBIT(xferSize);

        // Reduce the MAX transfer size to not exceed remaining data.
        while (xferSize > numBytes)
        {
            xferSize >>= 1;
        }

        // Encode transfer size to the HW format: size = 2^(e + 2)
        // Note: The encSize < LW_U8_MAX is not really necessary, but it is there to appease Cert-C
        temp = xferSize >> 3;
        while (temp > 0U && encSize < LW_U8_MAX)
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
        pBuf       = ((LwU8 *)pBuf) + xferSize;
        numBytes  -= xferSize;
    }

    // Restore falcon's SPRs before exiting the critical section.
#if PA_47_BIT_SUPPORTED
    falc_wspr(DMB1, dmb1);
#endif // PA_47_BIT_SUPPORTED
    falc_wspr(DMB, dmb);
    falc_wspr(CTX, ctx);

    // Wait for the DMA completion and exit critical section.
#ifdef DMA_NACK_SUPPORTED
    //
    // If DMA NACKs are supported dmwait before leaving the critical section,
    // ensuring that the NACK was received by the correct dmaRead / dmaWrite.
    //
    falc_dmwait();
    if (vApplicationDmaNackCheckAndClear())
    {
        retval = FLCN_ERR_DMA_NACK;
    }
    lwrtosEXIT_CRITICAL();
#else // DMA_NACK_SUPPORTED
    // Note that the DMA operations need not be completed at this time.
    lwrtosEXIT_CRITICAL();
    falc_dmwait();
#endif // DMA_NACK_SUPPORTED

    lwosDmaLockRelease();

    return retval;
}
#endif  // UPROC_RISCV
