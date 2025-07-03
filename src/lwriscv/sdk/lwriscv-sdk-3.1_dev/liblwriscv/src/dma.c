/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    dma.c
 * @brief   DMA driver
 *
 * Used to load data from FB to IMEM/DMEM and vice-versa.
 */

#include <stdint.h>
#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/dma.h>
#include <liblwriscv/io.h>

#include <liblwriscv/print.h>

#if LWRISCV_FEATURE_SCP
#include <liblwriscv/scp.h>

#ifndef SCP_REGISTER_SIZE
#error "SCP_REGISTER_SIZE not defined!"
#endif

_Static_assert(SCP_REGISTER_SIZE % DMA_BLOCK_SIZE_MIN == 0U,
    "Minimum SCPDMA transfer size is not aligned to minimum FBDMA block size!");
#endif

#if (LWRISCV_IMEM_SIZE == 0U) || (LWRISCV_DMEM_SIZE == 0U)
#error "LWRISCV_IMEM_SIZE and LWRISCV_DMEM_SIZE must be configured"
#endif

/**
 * @brief Create a LW_PRGNLCL_FALCON_DMATRFCMD register value for a dma transfer
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
 * @param[in] b_read_ext Transfer direction. true: External->TCM. false: TCM->External
 * @param[out] p_dma_cmd The LW_PRGNLCL_FALCON_DMATRFCMD register value to use for this transfer.
 * @param[out] p_tcm_offset The offset of tcm_pa from the start of its TCM.
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if tcm_pa is not a valid TCM address or if size_bytes, tcm_offset,
 * or aperture_offset are not properly aligned.
 */
static LWRV_STATUS
create_dma_command(
    uint64_t tcm_pa,
    uint64_t size_bytes,
    uint8_t  dma_idx,
    bool     b_read_ext,
    uint32_t *p_dma_cmd,
    uint32_t *p_tcm_offset
)
{
    LWRV_STATUS status = LWRV_OK;
    bool b_tcm_imem = false;
    uint32_t dma_cmd = 0;
    uint32_t tcm_offset = 0;
    uint64_t tcm_buf_end;

    // overflow check for tcm_pa + size_bytes
    if ((UINT64_MAX - tcm_pa) < size_bytes)
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }
    else
    {
        tcm_buf_end = tcm_pa + size_bytes;
    }

    // tcm_pa is in ITCM or DTCM?
    if ((tcm_pa >= LW_RISCV_AMAP_IMEM_START) &&
        (tcm_buf_end < (LW_RISCV_AMAP_IMEM_START + (uint64_t)LWRISCV_IMEM_SIZE)))
    {
        b_tcm_imem = true;
        tcm_offset = (uint32_t)((tcm_pa - LW_RISCV_AMAP_IMEM_START) & 0xFFFFFFFFU);
    }
    else if ((tcm_pa >= LW_RISCV_AMAP_DMEM_START) &&
        (tcm_buf_end < (LW_RISCV_AMAP_DMEM_START + (uint64_t)LWRISCV_DMEM_SIZE)))
    {
        tcm_offset = (uint32_t)((tcm_pa - LW_RISCV_AMAP_DMEM_START) & 0xFFFFFFFFU);
    }
    else
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    if (b_tcm_imem)
    {
        dma_cmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _TRUE, dma_cmd);
    }
    else
    {
        dma_cmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE, dma_cmd);
        dma_cmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SET_DMTAG, _TRUE, dma_cmd);
    }

    if (b_read_ext) // Ext -> TCM
    {
        dma_cmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _FALSE, dma_cmd);
    }
    else // TCM -> Ext
    {
        dma_cmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, dma_cmd);
    }

    if (dma_idx <= DMA_MAX_DMA_IDX)
    {
        dma_cmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dma_idx, dma_cmd);
    }
    else
    {
        status = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    *p_dma_cmd = dma_cmd;
    *p_tcm_offset = tcm_offset;

out:
    return status;
}

/**
 * @brief Kick off a DMA transfer between TCM PA and External memory
 *
 * @param[in] tcm_offset The offset of the TCM address from the start of that TCM.
 * @param[in] aperture_offset The external address (GPU PA or GPU VA) in the aperture
 *            determined by dma_idx. This can be a GPU VA or GPU PA.
 * @param[in] size_bytes The number of bytes to DMA.
 * @param[in] dma_cmd The value to write into the LW_PRGNLCL_FALCON_DMATRFCMD register,
 * excluding the size field. size will be determined inside this function.
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_ILWALID_ARGUMENT if aperture_offset is not a valid address or size_bytes, tcm_pa,
 *          or aperture_offset do not meet minimum alignment requirements.
 *
 * @pre The FBIF/TFBIF aperture referenced by dma_idx must have been configured by
 * fbifConfigureAperture or tfbifConfigureAperture. If the aperture is configured
 * to use a GPU VA, a context must be bound.
 */
static LWRV_STATUS
fbdma_xfer(
    uint32_t tcm_offset,
    uint64_t aperture_offset,
    uint64_t size_bytes,
    uint32_t dma_cmd
)
{
    LWRV_STATUS err = LWRV_OK;
    uint32_t dma_block_size;
    uint8_t dma_enc_block_size;
    uint32_t lwrrent_tcm_offset = tcm_offset;
    uint64_t bytes_remaining = size_bytes;
    uint32_t dma_cmd_with_block_size = dma_cmd;
    uint32_t fb_offset;

    if (((aperture_offset | size_bytes | tcm_offset) & (DMA_BLOCK_SIZE_MIN - 1U)) != 0U)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    // When REQUIRE_CTX = true, DMAs without a CTX bound will report idle immediately but won't actually be issued
    localWrite(LW_PRGNLCL_FALCON_DMACTL,
        DRF_DEF(_PRGNLCL, _FALCON_DMACTL, _REQUIRE_CTX, _FALSE));

    //
    // Break up aperture_offset into a base/offset to be used in DMATRFBASE, TRFFBOFFS
    // Note: We cannot use lwrrent_tcm_offset for both TRFMOFFS and TRFFBOFFS because this
    // would require us to callwlate a DMATRFBASE value by subtracting
    // (aperture_offset - lwrrent_tcm_offset) and this may result in a misaligned value
    // which we cannot program into DMATRFBASE.
    //
    fb_offset = (uint32_t) (aperture_offset & 0xFFU);

    // Check for overflows (CERT-C INT30 violation in lwrrent_tcm_offset += dma_block_size; fb_offset += dma_block_size;)
    if (((UINT64_MAX - lwrrent_tcm_offset) < size_bytes) ||
        ((UINT64_MAX - fb_offset) < size_bytes))
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    localWrite(LW_PRGNLCL_FALCON_DMATRFBASE, (uint32_t)((aperture_offset >> 8U) & 0xffffffffU));
#ifdef LW_PRGNLCL_FALCON_DMATRFBASE1
    localWrite(LW_PRGNLCL_FALCON_DMATRFBASE1, (uint32_t)(((aperture_offset >> 8U) >> 32U) & 0xffffffffU));
#else //LW_PRGNLCL_FALCON_DMATRFBASE1
    if ((((aperture_offset >> 8U) >> 32U) & 0xffffffffU) != 0U)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }
#endif //LW_PRGNLCL_FALCON_DMATRFBASE1

    while (bytes_remaining != 0U)
    {
        // Wait if we're full
        while (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, _TRUE, localRead(LW_PRGNLCL_FALCON_DMATRFCMD)))
        {
        }

        localWrite(LW_PRGNLCL_FALCON_DMATRFMOFFS, lwrrent_tcm_offset); // This is fine as long as caller does error checking.
        localWrite(LW_PRGNLCL_FALCON_DMATRFFBOFFS, fb_offset);

        // Determine the largest pow2 block transfer size of the remaining data
        dma_block_size = LOWESTBIT(lwrrent_tcm_offset | fb_offset | DMA_BLOCK_SIZE_MAX);

        // Reduce the dma_block_size to not exceed remaining data.
        while (dma_block_size > bytes_remaining)
        {
            dma_block_size >>= 1U;
        }

        // Colwert the pow2 block size to block size encoding
        dma_enc_block_size = (uint8_t)BIT_IDX_32(dma_block_size / DMA_BLOCK_SIZE_MIN);
        dma_cmd_with_block_size = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, dma_enc_block_size, dma_cmd);

        localWrite(LW_PRGNLCL_FALCON_DMATRFCMD, dma_cmd_with_block_size);

        bytes_remaining -= dma_block_size;
        lwrrent_tcm_offset += dma_block_size;
        fb_offset += dma_block_size;
    }

out:
    return err;
}

LWRV_STATUS
fbdma_pa
(
    uint64_t tcm_pa,
    uint64_t aperture_offset,
    uint64_t size_bytes,
    uint8_t  dma_idx,
    bool     b_read_ext
)
{
    LWRV_STATUS ret;
    uint32_t tcm_offset;
    uint32_t dma_cmd;

    ret = create_dma_command(tcm_pa, size_bytes, dma_idx, b_read_ext, &dma_cmd, &tcm_offset);
    if (ret != LWRV_OK)
    {
        goto out;
    }

    // Nothing to copy, bail after checks
    if (size_bytes == 0U)
    {
        ret = LWRV_OK;
        goto out;
    }

    ret = fbdma_xfer(tcm_offset, aperture_offset, size_bytes, dma_cmd);
    if (ret != LWRV_OK)
    {
        goto out;
    }

    ret = fbdma_wait_completion();

out:
    return ret;
}

LWRV_STATUS
fbdma_pa_async(uint64_t tcm_pa,
      uint64_t aperture_offset,
      uint64_t size_bytes,
      uint8_t  dma_idx,
      bool     b_read_ext
)
{
    LWRV_STATUS ret;
    uint32_t tcm_offset;
    uint32_t dma_cmd;

    ret = create_dma_command(tcm_pa, size_bytes, dma_idx, b_read_ext, &dma_cmd, &tcm_offset);
    if (ret != LWRV_OK)
    {
        goto out;
    }

    // Nothing to copy, bail after checks
    if (size_bytes == 0U)
    {
        ret = LWRV_OK;
        goto out;
    }

    ret = fbdma_xfer(tcm_offset, aperture_offset, size_bytes, dma_cmd);

out:
    return ret;
}

/**
 * @brief Wait for all DMA transfers to complete
 *
 * @return LWRV_OK on success.
 *         LWRV_ERR_GENERIC if DMA HW returned an error.
 *         LWRV_ERR_DMA_NACK if DMA engine has returned a NACK.
 *
 * @pre Should be called after dmaPaAsync to ensure that DMAs have completed.
 */
LWRV_STATUS
fbdma_wait_completion(void)
{
    LWRV_STATUS err = LWRV_OK;
    uint32_t dmatrfcmd;
    uint32_t dmainfo;

    // Wait for completion of any remaining transfers
    do {
        dmatrfcmd = localRead(LW_PRGNLCL_FALCON_DMATRFCMD);
    }
    while (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _FALSE, dmatrfcmd));

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _ERROR, _TRUE, dmatrfcmd))
    {
        err = LWRV_ERR_GENERIC;
        goto out;
    }

    dmainfo = localRead(LW_PRGNLCL_FALCON_DMAINFO_CTL);
    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _TRUE, dmainfo))
    {
        // Received DMA NACK, clear it and return error
        dmainfo = FLD_SET_DRF(_PRGNLCL, _FALCON_DMAINFO_CTL, _INTR_ERR_COMPLETION, _CLR, dmainfo);
        localWrite(LW_PRGNLCL_FALCON_DMAINFO_CTL, dmainfo);
        err = LWRV_ERR_DMA_NACK;
        goto out;
    }

out:
    return err;
}

#if LWRISCV_FEATURE_SCP
LWRV_STATUS
fbdma_scp_to_extmem(
    uint64_t aperture_offset,
    uint64_t size_bytes,
    uint8_t dma_idx
)
{
    LWRV_STATUS ret;

    // Nothing to copy
    if (size_bytes == 0U)
    {
        return LWRV_OK;
    }

    if ((((size_bytes | aperture_offset) & (SCP_REGISTER_SIZE - 1U)) != 0U) ||
        (dma_idx > DMA_MAX_DMA_IDX))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    //
    // Set dma_cmd:
    // - write to Extmem
    // - enable direct bypass shortlwt path from SCP
    // - dma_idx
    //
    uint32_t dma_cmd = DRF_DEF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SEC, 2U) |
                DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dma_idx);

    //
    // call fbdma_xfer with tcm_offset=0, since this value won't get used anyways:
    // Data is sourced from SCP.
    //
    ret = fbdma_xfer(0U, aperture_offset, size_bytes, dma_cmd);
    if (ret != LWRV_OK)
    {
        return ret;
    }

    return fbdma_wait_completion();
}
#endif // LWRISCV_FEATURE_SCP
