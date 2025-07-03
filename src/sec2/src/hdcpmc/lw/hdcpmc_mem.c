/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#if (SEC2CFG_FEATURE_ENABLED(SEC2TASK_HDCPMC))
/* ------------------------- System Includes -------------------------------- */
#include "lwuproc.h"
#include "sec2sw.h"
#include "rmsec2cmdif.h"

/* ------------------------- Application Includes --------------------------- */
#include "lwos_dma.h"
#include "lwos_dma_hs.h"
#include "hdcpmc/hdcpmc_mem.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Size of DMA transfers while in HS mode.
 */
#define HDCP_DMA_TRANSFER_SIZE                                              256

/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Reads data from the frame buffer.
 *
 * The function uses the virtual DMA index to perform the read
 * (RM_SEC2_DMAIDX_VIRT).
 *
 * @param [out] pBuffer     Pointer to the buffer to read the data into. Must be
 *                          aligned to @ref HDCP_DMA_ALIGNMENT.
 * @param [in]  fbOffset    Offset within the FB the data is stored at. Must be
 *                          aligned to @ref HDCP_DMA_ALIGNMENT.
 * @param [in]  size        The size in bytes of the data. Must be aligned to
 *                          @ref HDCP_DMA_ALIGNMENT.
 *
 * @return FLCN_OK if the data was successfully read from the FB;
 *         FLCN_ERROR otherwise.
 */
FLCN_STATUS
hdcpDmaRead
(
    void   *pBuffer,
    LwU32   fbOffset,
    LwU32   size
)
{
    RM_FLCN_MEM_DESC memdesc;
    FLCN_STATUS      status = FLCN_OK;
    LwU32            dmaSize;
    LwU16            dmaOffset = 0;

    // Verify alignment of buffers and size.
    if (((LwU32)pBuffer % HDCP_DMA_ALIGNMENT) ||
        (size % HDCP_DMA_ALIGNMENT))
    {
        return FLCN_ERROR;
    }

    while (size > 0)
    {
        dmaSize = LW_MIN(size, HDCP_DMA_BUFFER_SIZE);

        // Initialize the memory descriptor
        memdesc.address.lo = (fbOffset << 8) + dmaOffset;
        memdesc.address.hi = fbOffset >> 24;
        memdesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, dmaSize) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_VIRT);

        status = dmaRead((void*)pBuffer + dmaOffset, &memdesc, 0, dmaSize);
        if (FLCN_OK != status)
        {
            break;
        }
        size        -= dmaSize;
        dmaOffset   += dmaSize;
    }

    return status;
}

/*!
 * @brief Writes data to the frame buffer.
 *
 * The function uses the virtual DMA index to perform the write
 * (RM_SEC2_DMAIDX_VIRT).
 *
 * @param [in]  pBuffer     Pointer to the data.
 * @param [in]  fbOffset    Offset withing the FB to store the data at.
 * @param [in]  size        The size in bytes of the data.
 *
 * @return FLCN_OK if the data was successfully written to the FB;
 *         FLCN_ERROR otherwise.
 */
FLCN_STATUS
hdcpDmaWrite
(
    const void *pBuffer,
    LwU32       fbOffset,
    LwU32       size
)
{
    RM_FLCN_MEM_DESC memdesc;
    FLCN_STATUS      status     = FLCN_OK;
    LwU32            dmaSize    = 0;
    LwU16            dmaOffset  = 0;

    // Verify alignment of buffers and size.
    if (((LwU32)pBuffer % HDCP_DMA_ALIGNMENT) ||
        (size % HDCP_DMA_ALIGNMENT))
    {
        return FLCN_ERROR;
    }

    while (size > 0)
    {
        dmaSize = LW_MIN(size, HDCP_DMA_BUFFER_SIZE);

        // Initialize the memory descriptor
        memdesc.address.lo = (fbOffset << 8) + dmaOffset;
        memdesc.address.hi = fbOffset >> 24;
        memdesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, dmaSize) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_VIRT);

        status = dmaWrite((void*)pBuffer + dmaOffset, &memdesc, 0, dmaSize);
        if (FLCN_OK != status)
        {
            break;
        }

        size          -= dmaSize;
        dmaOffset     += dmaSize;
    }

    return status;
}

/*!
 * @brief Reads data from the frame buffer in HS mode.
 *
 * The function uses the virtual DMA index to perform the read
 * (RM_SEC2_DMAIDX_VIRT).
 *
 * @param [out] pBuffer     Pointer to the buffer to read the data into.
 * @param [in]  fbOffset    Offset within the FB the data is stored at.
 * @param [in]  size        The size in bytes of the data.
 *
 * @return FLCN_OK if the data was successfully read from the FB;
 *         FLCN_ERROR otherwise.
 */
FLCN_STATUS
hdcpDmaRead_hs
(
    void   *pBuffer,
    LwU32   fbOffset,
    LwU32   size
)
{
    RM_FLCN_MEM_DESC memdesc;
    FLCN_STATUS      status = FLCN_OK;
    LwU32            dmaSize;
    LwU16            dmaOffset = 0;

    // Verify alignment of buffers and size.
    if (((LwU32)pBuffer % HDCP_DMA_ALIGNMENT) ||
        (size % HDCP_DMA_ALIGNMENT))
    {
        return FLCN_ERROR;
    }

    while (size > 0)
    {
        dmaSize = LW_MIN(size, HDCP_DMA_BUFFER_SIZE);

        // Initialize the memory descriptor
        memdesc.address.lo = (fbOffset << 8) + dmaOffset;
        memdesc.address.hi = fbOffset >> 24;
        memdesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, dmaSize) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_SEC2_DMAIDX_VIRT);

        status = dmaRead_hs((void*)pBuffer + dmaOffset, &memdesc, 0, dmaSize);
        if (FLCN_OK != status)
        {
            break;
        }
        size        -= dmaSize;
        dmaOffset   += dmaSize;
    }

    return status;
}

/*!
 * @brief Writes data to the frame buffer in HS mode.
 *
 * The function uses the virtual DMA index to perform the write
 * (RM_SEC2_DMAIDX_VIRT).
 *
 * @param [in]  pBuffer     Pointer to the data.
 * @param [in]  fbOffset    Offset withing the FB to store the data at.
 * @param [in]  size        The size in bytes of the data.
 *
 * @return FLCN_OK if the data was successfully written to the FB;
 *         FLCN_ERROR otherwise.
 */
FLCN_STATUS
hdcpDmaWrite_hs
(
    const void *pBuffer,
    LwU32       fbOffset,
    LwU32       size
)
{
    RM_FLCN_MEM_DESC memdesc;
    FLCN_STATUS      status     = FLCN_OK;
    LwU32            dmaSize    = 0;
    LwU16            dmaOffset  = 0;

    // Verify alignment of buffers and size.
    if (((LwU32)pBuffer % HDCP_DMA_ALIGNMENT) ||
        (size % HDCP_DMA_ALIGNMENT))
    {
        return FLCN_ERROR;
    }

    while (size > 0)
    {
        dmaSize = LW_MIN(size, HDCP_DMA_BUFFER_SIZE);

        // Initialize the memory descriptor
        memdesc.address.lo = (fbOffset << 8) + dmaOffset;
        memdesc.address.hi = fbOffset >> 24;
        memdesc.params = DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, dmaSize) |
                         DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX,
                                 RM_SEC2_DMAIDX_VIRT);

        status = dmaWrite_hs((void*)pBuffer + dmaOffset, &memdesc, 0, dmaSize);
        if (FLCN_OK != status)
        {
            break;
        }

        size        -= dmaSize;
        dmaOffset   += dmaSize;
    }

    return status;
}
#endif
