/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LWOS_DMA_H
#define LWOS_DMA_H

/* ------------------------- System Includes -------------------------------- */
#include "flcnifcmn.h"
#include "lwuproc.h"
#include "lwos_dma_lock.h"

/* ------------------------- Application Includes --------------------------- */
/* ------------------------- Defines ---------------------------------------- */
/*!
 * The DMA transfer size is encoded in a 3-bit field within the DMA command
 * register.  This macro takes that 3-bit encoded-size (e) and colwerts it into
 * the real transfer-size in bytes. See the Falcon IAS for more information.
 */
#define DMA_XFER_SIZE_BYTES(e)          (LWBIT32((e) + 2U))

/*!
 * Falcon DMA supports transfer sizes of 4, 8, 16, 32, 64, 128, and 256 for
 * DMEM writes and 4, 8, 16, 32, 64, 128, and 256 for DMEM reads. The desired
 * size is specified with each DMA request through an operand encoded as
 * described above (@ref DMA_XFER_SIZE_BYTES).
 *
 * The following defines simply give the min and max encoded transfer sizes for
 * reads and writes.
 */
#define  DMA_XFER_ESIZE_MIN_READ        (0x00U) //  4-bytes
#define  DMA_XFER_ESIZE_MAX_READ        (0x06U) // 256-bytes
#define  DMA_XFER_ESIZE_MIN_WRITE       (0x00U) //   4-bytes
#define  DMA_XFER_ESIZE_MAX_WRITE       (0x06U) // 256-bytes

/*!
 * Define the minimum alignment required for any DMEM buffer used in a DMA-read
 * operation. The DMA read function (@ref dmaRead) will not read any data if
 * this alignment is not satisfied. Larger alignments are acceptable (and
 * desireable to achieve max. DMA performance) so long as they are multiples
 * of this value.
 */
#define DMA_MIN_READ_ALIGNMENT           \
    DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MIN_READ)

/*!
 * Maximum alignment/size of a single DMA read operation. Reads greater than
 * this in size must be batched.
 */
#define DMA_MAX_READ_ALIGNMENT           \
    DMA_XFER_SIZE_BYTES(DMA_XFER_ESIZE_MAX_WRITE)

/*!
 * @brief   Determine if a RM_FLCN_MEM_DESC structure points to NULL.
 */
#define DMA_ADDR_IS_ZERO(_pMemDesc)      \
    ((0U == (_pMemDesc)->address.lo) &&  \
     (0U == (_pMemDesc)->address.hi))

/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Function Prototypes ---------------------------- */
#define dmaRead(...) MOCKABLE(dmaRead)(__VA_ARGS__)
/*!
 * @brief   read a buffer from the given FB / SYSMEM address (DMA address) into DMEM.
 *
 * The Falcon DMA controller has a minimum transfer size of 16 / 4 bytes for
 * reads / writes respectively. The number of transfered bytes should be an
 * even multiple of this minimal value. Additionally, it is expected that the
 * given DMEM pointer as well as DMA addresses have same minimum alignment.
 * Zero bytes will be transfered if this requirement is not satisfied.
 *
 * @param[in]   pBuf         Pointer to the DMEM buffer.
 * @param[in]   pMemDesc     FB / SYSMEM surface descriptor.
 * @param[in]   offset       Offset of the first byte within the surface.
 * @param[in]   numBytes     The number of bytes to transfer.
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 * @return  FLCN_ERR_DMA_NACK
 *              on virtual address fault, if HW supported and enabled
 * @return  FLCN_ERR_DMA_UNEXPECTED_DMAIDX
 *              on trying to use a dmaIdx not permitted with these interfaces
 */
FLCN_STATUS dmaRead             (void *pBuf, RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes);

#define dmaWrite(...) MOCKABLE(dmaWrite)(__VA_ARGS__)
/*!
 * @brief   Write a buffer from DMEM into the given FB / SYSMEM address (DMA address).
 *
 * The Falcon DMA controller has a minimum transfer size of 16 / 4 bytes for
 * reads / writes respectively. The number of transfered bytes should be an
 * even multiple of this minimal value. Additionally, it is expected that the
 * given DMEM pointer as well as DMA addresses have same minimum alignment.
 * Zero bytes will be transfered if this requirement is not satisfied.
 *
 * @param[in]   pBuf         Pointer to the DMEM buffer.
 * @param[in]   pMemDesc     FB / SYSMEM surface descriptor.
 * @param[in]   offset       Offset of the first byte within the surface.
 * @param[in]   numBytes     The number of bytes to transfer.
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 * @return  FLCN_ERR_DMA_NACK
 *              on virtual address fault, if HW supported and enabled
 * @return  FLCN_ERR_DMA_UNEXPECTED_DMAIDX
 *              on trying to use a dmaIdx not permitted with these interfaces
 */
FLCN_STATUS dmaWrite            (void *pBuf, RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes);

#define dmaReadUnrestricted(...) MOCKABLE(dmaReadUnrestricted)(__VA_ARGS__)
/*!
 * @brief   read a buffer from the given FB / SYSMEM address (DMA address) into DMEM, skipping the DMA index check.
 *
 * This function is identical to dmaRead, except that it does not check whether the specified dmaIdx is permitted.
 * 
 * The Falcon DMA controller has a minimum transfer size of 16 / 4 bytes for
 * reads / writes respectively. The number of transfered bytes should be an
 * even multiple of this minimal value. Additionally, it is expected that the
 * given DMEM pointer as well as DMA addresses have same minimum alignment.
 * Zero bytes will be transfered if this requirement is not satisfied.
 *
 * @param[in]   pBuf         Pointer to the DMEM buffer.
 * @param[in]   pMemDesc     FB / SYSMEM surface descriptor.
 * @param[in]   offset       Offset of the first byte within the surface.
 * @param[in]   numBytes     The number of bytes to transfer.
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 * @return  FLCN_ERR_DMA_NACK
 *              on virtual address fault, if HW supported and enabled
 */
FLCN_STATUS dmaReadUnrestricted (void *pBuf, RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes);

#define dmaWriteUnrestricted(...) MOCKABLE(dmaWriteUnrestricted)(__VA_ARGS__)
/*!
 * @brief   Write a buffer from DMEM into the given FB / SYSMEM address (DMA address), skipping the DMA index check.
 *
 * This function is identical to dmaWrite, except that it does not check whether the specified dmaIdx is permitted.
 * 
 * The Falcon DMA controller has a minimum transfer size of 16 / 4 bytes for
 * reads / writes respectively. The number of transfered bytes should be an
 * even multiple of this minimal value. Additionally, it is expected that the
 * given DMEM pointer as well as DMA addresses have same minimum alignment.
 * Zero bytes will be transfered if this requirement is not satisfied.
 *
 * @param[in]   pBuf         Pointer to the DMEM buffer.
 * @param[in]   pMemDesc     FB / SYSMEM surface descriptor.
 * @param[in]   offset       Offset of the first byte within the surface.
 * @param[in]   numBytes     The number of bytes to transfer.
 *
 * @return  FLCN_OK
 *              on full transfer (bytesTransferred == numBytes)
 * @return  FLCN_ERR_DMA_ALIGN
 *              on no transfer (bytesTransferred == 0)
 * @return  FLCN_ERR_DMA_NACK
 *              on virtual address fault, if HW supported and enabled
 */
FLCN_STATUS dmaWriteUnrestricted(void *pBuf, RM_FLCN_MEM_DESC *pMemDesc, LwU32 offset, LwU32 numBytes);

#ifdef DMA_NACK_SUPPORTED
extern LwBool vApplicationDmaNackCheckAndClear(void);
#endif
extern LwBool vApplicationIsDmaIdxPermitted(LwU8 dmaIdx);

#endif // LWOS_DMA_H
