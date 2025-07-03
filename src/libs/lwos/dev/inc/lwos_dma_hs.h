/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_DMA_HS_H
#define LWOS_DMA_HS_H

/*!
 * @file lwos_dma_hs.h
 * This files holds the inline definitions for standard memory functions.
 * This header should only be included by heavy secure microcode.
 */

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * @brief   Use the Falcon's DMA controller to read a buffer of data from
 *          the given FB / SYSMEM address (DMA address) into DMEM.
 *
 * @copydoc dmaTransfer
 */
#define dmaRead_hs(_pDst, _pMemDesc, _offset, _numBytes)                         \
    dmaTransfer_hs(_pDst, _pMemDesc, _offset, _numBytes, LW_TRUE)

/*!
 * @brief   Use the Falcon's DMA controller to write a buffer of data in DMEM
 *          to given FB / SYSMEM address (DMA address).
 *
 * @copydoc dmaTransfer
 */
#define dmaWrite_hs(_pSrc, _pMemDesc, _offset, _numBytes)                        \
     dmaTransfer_hs(_pSrc, _pMemDesc, _offset, _numBytes, LW_FALSE)

/* ------------------------- Prototypes ------------------------------------- */
FLCN_STATUS dmaTransfer_hs(void *pBuf, RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes, LwBool bRead)
    GCC_ATTRIB_SECTION("imem_libDmaHs", "dmaTransfer_hs");

#endif // LWOS_DMA_HS_H
