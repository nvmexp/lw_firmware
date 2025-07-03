/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_DMA_H
#define LIBLWRISCV_DMA_H

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/gcc_attrs.h>
#include <lwriscv/status.h>

#if (!LWRISCV_HAS_FBDMA) || (!LWRISCV_FEATURE_DMA)
#error "This header cannot be used on an engine which does not support FBDMA."
#endif // (!LWRISCV_HAS_FBDMA) || (!LWRISCV_FEATURE_DMA)

#if LWRISCV_HAS_FBIF
#include <liblwriscv/fbif.h>
/**
 * @brief The highest allowed value for the dma_idx argument.
 */
#define DMA_MAX_DMA_IDX (FBIF_NUM_APERTURES-1U)

#elif LWRISCV_HAS_TFBIF
#include <liblwriscv/tfbif.h>
/**
 * @brief The highest allowed value for the dma_idx argument.
 */
#define DMA_MAX_DMA_IDX (TFBIF_NUM_APERTURES-1U)

#else
#error "No FBIF or TFBIF"
#endif

/**
 * @brief The minimum block size which FBDMA can transfer. All addresses
 * and transfer sizes must be aligned to this.
 */
#define DMA_BLOCK_SIZE_MIN   0x4U

/**
 * @brief The maximum block size which FBDMA can transfer. The client can
 * request a larger transfer than this but the driver will internally break it up
 * into smaller transfers.
 */
#define DMA_BLOCK_SIZE_MAX   0x100U

/**
 * @brief DMA transfer between TCM PA and External memory. This is a synchronous transfer.
 * The function will block until the transfer has completed.
 *
 * @param[in] tcm_pa The RISCV PA for the TCM buffer. Must be aligned to DMA_BLOCK_SIZE_MIN,
 * but DMA_BLOCK_SIZE_MAX alignement is optimal.
 * @param[in] aperture_offset The external address (GPU PA or GPU VA) in the aperture
 * determined by dma_idx. The type of memory to access is determined by FBIF_APERTURE_CFG.memType
 * (on engines with FBIF) or by TFBIF_APERTURE_CFG.swid (on engines with TFBIF).
 * memutils_riscv_pa_to_target_offset can be used to translate a RISCV PA to a GPU PA/VA suitable for
 * this argument. Must be aligned to DMA_BLOCK_SIZE_MIN, but DMA_BLOCK_SIZE_MAX alignement
 * is optimal.
 * @param[in] size_bytes The number of bytes to transfer. Must have a granularity of
 * DMA_BLOCK_SIZE_MIN, but DMA_BLOCK_SIZE_MAX granularity is optimal.
 * @param[in] dma_idx Aperture index to use (0 to DMA_MAX_dma_idx)
 * @param[in] b_read_ext Transfer direction. true: External->TCM. false: TCM->External
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_OUT_OF_RANGE if tcm_pa is not within the range of the ITCM or DTCM or
 *         size_bytes, tcm_pa, or aperture_offset do not meet minimum alignment requirements.
 *         LWRV_ERR_DMA_NACK if there is a DMA NACK
 *         LWRV_ERR_GENERIC if there is a transfer error
 *
 * @pre The FBIF/TFBIF aperture referenced by dma_idx must have been configured by
 * fbifConfigureAperture or tfbifConfigureAperture. If the aperture is configured
 * to use a GPU VA, a context must be bound.
 */
GCC_ATTR_WARN_UNUSED LWRV_STATUS
fbdma_pa(uint64_t tcm_pa,
         uint64_t aperture_offset,
         uint64_t size_bytes,
         uint8_t  dma_idx,
         bool     b_read_ext
);

/**
 * @brief DMA transfer between TCM PA and External memory. This is an asynchronous transfer.
 * this function does not wait for completion of DMA. It returns right away, allowing the
 * client to call dmaPaAsync again and queue up more requests.
 *
 * The client should call dmaWaitCompletion to ensure that all DMAs have completed.
 *
 * @param[in] tcm_pa The RISCV PA for the TCM buffer. Must be aligned to DMA_BLOCK_SIZE_MIN,
 * but DMA_BLOCK_SIZE_MAX alignement is optimal.
 * @param[in] aperture_offset The external address (GPU PA or GPU VA) in the aperture
 * determined by dma_idx. The type of memory to access is determined by FBIF_APERTURE_CFG.memType
 * (on engines with FBIF) or by TFBIF_APERTURE_CFG.swid (on engines with TFBIF).
 * memutils_riscv_pa_to_target_offset can be used to translate a RISCV PA to a GPU PA/VA suitable for
 * this argument. Must be aligned to DMA_BLOCK_SIZE_MIN, but DMA_BLOCK_SIZE_MAX alignement
 * is optimal.
 * @param[in] size_bytes The number of bytes to transfer. Must have a granularity of
 * DMA_BLOCK_SIZE_MIN, but DMA_BLOCK_SIZE_MAX granularity is optimal.
 * @param[in] dma_idx Aperture index to use (0-7).
 * @param[in] b_read_ext Transfer direction. true: External->TCM. false: TCM->External.
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_OUT_OF_RANGE if tcm_pa is not within the range of the ITCM or DTCM or
 *         size_bytes, tcm_pa, or aperture_offset do not meet minimum alignment requirements.
 *
 * @pre The FBIF/TFBIF aperture referenced by dma_idx must have been configured by
 * fbifConfigureAperture or tfbifConfigureAperture. If the aperture is configured
 * to use a GPU VA, a context must be bound.
 */
GCC_ATTR_WARN_UNUSED LWRV_STATUS
fbdma_pa_async(uint64_t tcm_pa,
      uint64_t aperture_offset,
      uint64_t size_bytes,
      uint8_t  dma_idx,
      bool     b_read_ext
);

/**
 * @brief Wait for all DMA transfers to complete
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_GENERIC if DMA HW returned an error
 *         LWRV_ERR_DMA_NACK if DMA engine has returned a NACK
 *
 * @pre Should be called after dmaPaAsync to ensure that DMAs have completed.
 */
GCC_ATTR_WARN_UNUSED LWRV_STATUS
fbdma_wait_completion(void);

#if LWRISCV_FEATURE_SCP
/**
 * @brief DMA transfer from SCP to FB/Sysmem. This function uses the direct bypass
 * shortlwt path from SCP: SCP registers --> SCPDMA --> FBDMA --> FB/SYSMEM
 *
 * @param[in] aperture_offset The external address (GPU PA or GPU VA) in the aperture
 *            determined by dma_idx. This can be a GPU VA or GPU PA. Must be aligned
              to SCP_REGISTER_SIZE.
 * @param[in] size_bytes The number of bytes to DMA. This value must be a multiple of
              SCP_REGISTER_SIZE and should generally match the size used to configure
              the corresponding SCPDMA transfer.
 * @param[in] dma_idx Aperture index to use (0 to DMA_MAX_dma_idx)
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if size_bytes or aperture_offset do not meet
 *                                   minimum alignment requirements.
 *         LWRV_ERR_DMA_NACK if there is a DMA NACK
 *         LWRV_ERR_GENERIC if there is an error returned from the DMA hardware.
 *
 * @pre The FBIF/TFBIF aperture referenced by dma_idx must have been configured by
 * fbifConfigureAperture or tfbifConfigureAperture. If the aperture is configured
 * to use a GPU VA, a context must be bound. The caller must first call into the SCP
 * driver to configure SCPDMA for the transfer.
 */
GCC_ATTR_WARN_UNUSED LWRV_STATUS
fbdma_scp_to_extmem(
    uint64_t aperture_offset,
    uint64_t size_bytes,
    uint8_t  dma_idx
);

#endif // LWRISCV_FEATURE_SCP

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/dma_legacy.h>
#endif

#endif // LIBLWRISCV_DMA_H
