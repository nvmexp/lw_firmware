
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_DMA_LEGACY_H
#define LIBLWRISCV_DMA_LEGACY_H

/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

#define dmaPa(tcm_pa, aperture_offset, size_bytes, dma_idx, b_read_ext) \
    fbdma_pa((tcm_pa), (aperture_offset), (size_bytes), (dma_idx), (b_read_ext))

#define dmaPaAsync(tcm_pa, aperture_offset, size_bytes, dma_idx, b_read_ext)\
    fbdma_pa_async((tcm_pa), (aperture_offset), (size_bytes), (dma_idx), (b_read_ext))

#define dmaWaitCompletion(void) \
    fbdma_wait_completion() 

#define dmaScpToExtmem(aperture_offset, size_bytes, dma_idx) \
    fbdma_scp_to_extmem((aperture_offset), (size_bytes), (dma_idx))

#endif // LIBLWRISCV_DMA_LEGACY_H
