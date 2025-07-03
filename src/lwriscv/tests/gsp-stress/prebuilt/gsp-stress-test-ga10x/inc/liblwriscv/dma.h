/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
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
#include <lwriscv/status.h>

#if (!LWRISCV_HAS_FBDMA) || (!LWRISCV_FEATURE_DMA)
#error "This header cannot be used on an engine which does not support FBDMA."
#endif // (!LWRISCV_HAS_FBDMA) || (!LWRISCV_FEATURE_DMA)

#define DMA_BLOCK_SIZE_MIN   0x4U
#define DMA_BLOCK_SIZE_MAX   0x100U

/*!
 * \brief DMA transfer between TCM PA and External memory
 *
 * @param[in] tcmPa The RISCV PA for the TCM buffer
 * @param[in] apertureOffset The external address (GPU PA or GPU VA) in the aperture
 *            determined by dmaIdx. This can be a GPU VA or GPU PA.
 * @param[in] sizeBytes the number of bytes to DMA
 * @param[in] dmaIdx Aperture index to use (0-7)
 * @param[in] bReadExt Transfer direction. true: External->TCM. false: TCM->External
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_OUT_OF_RANGE if tcmPa is not within the range of the ITCM or DTCM
 *         LWRV_ERR_ILWALID_ARGUMENT if sizeBytes, tcmPa, or apertureOffset do not meet
 *                                   minimum alignment requirements.
 *
 * On FBIF Engines:
 * - The type of address is determined by the FBIF_APERTURE_CFG.memType for this aperture.
 * - If it is a GPU VA, a context must be bound.
 * - To use a RISCV PA, it must first be translated to a GPU PA using riscvPaToTargetOffset.
 *
 * On TFBIF engines:
 * - The type of address is determined by TFBIF_APERTURE_CFG.swid.
 *
 * Limitations:
 * - Source/dest must have 4-byte alignment
 *   256-byte alignment is optimal
 * - Size must have 4-byte granularity
 *   256-byte granularity is optimal
 * - dGPU: If the dmaIdx is configured for VA, then caller is responsible for making
 *   sure a valid context is bound
 * - This is a synchronous transfer. The function will block until the transfer has completed.
 */
LWRV_STATUS
dmaPa(uint64_t tcmPa,
      uint64_t apertureOffset,
      uint64_t sizeBytes,
      uint8_t  dmaIdx,
      bool     bReadExt
);

#if LWRISCV_FEATURE_SCP
/*!
 * \brief DMA transfer from SCP to FB/Sysmem
 *
 * @param[in] apertureOffset The external address (GPU PA or GPU VA) in the aperture
 *            determined by dmaIdx. This can be a GPU VA or GPU PA. Must be aligned
              to SCP_REGISTER_SIZE.
 * @param[in] sizeBytes The number of bytes to DMA. This value must be a multiple of
              SCP_REGISTER_SIZE and should generally match the size used to configure
              the corresponding SCPDMA transfer.
 * @param[in] dmaIdx Aperture index to use (0-7)
 *
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if sizeBytes or apertureOffset do not meet
 *                                   minimum alignment requirements.
 *         LWRV_ERR_GENERIC if there is an error returned from the DMA hardware.
 *
 * This function uses the direct bypass shortlwt path from SCP:
 *      SCP registers --> SCPDMA --> FBDMA --> FB/SYSMEM
 *
 * The caller must first call into the SCP driver to configure SCPDMA for the transfer.
 */
LWRV_STATUS
dmaScpToExtmem(
    uint64_t apertureOffset,
    uint32_t sizeBytes,
    uint8_t  dmaIdx
);
#endif // LWRISCV_FEATURE_SCP

#endif // LIBLWRISCV_DMA_H
