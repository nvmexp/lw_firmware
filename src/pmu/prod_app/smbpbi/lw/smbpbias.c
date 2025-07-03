/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   smbpbias.c
 * @brief  SMBus Post-Box Interface, Asynchronous request support
 *
 * This file contains auxiliary functions for SMBPBI, that are only required,
 * when PMU_SMBPBI_ASYNC_REQUESTS is enabled.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "lwostask.h"
#include "lwuproc.h"
#include "oob/smbpbi_shared.h"
#include "main.h" // For PmuInitArgs

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objsmbpbi.h"

/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Properties of the internal state registers
 */
#define SMBPBI_REG_FILE_SHARED          /* registers shared between RM/PMU */   \
                    (BIT(LW_MSGBOX_CMD_ARG2_REG_INDEX_SCRATCH_PAGE) |           \
                    BIT(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING))
#define SMBPBI_REG_FILE_EXT_WRITABLE    /* registers writable by the RM */      \
                    BIT(LW_MSGBOX_CMD_ARG2_REG_INDEX_EVENTS_PENDING)
#define SMBPBI_REG_IS_LOCAL(r)     ((BIT(r) & SMBPBI_REG_FILE_SHARED) == 0)
#define SMBPBI_REG_IS_CACHEABLE(r) (SMBPBI_REG_IS_LOCAL(r) ||                   \
                            ((BIT(r) & SMBPBI_REG_FILE_EXT_WRITABLE) == 0))

ct_assert((LW_MSGBOX_CMD_ARG2_REG_INDEX_MAX + 1) == LW_SMBPBI_STATE_REGISTER_FILE_SIZE);

/* ------------------------- Private Functions ------------------------------ */
static LwU8 s_smbpbiHostMemWriteCopy(LwBool bWrite, LwU32 dst, LwU32 from, LwU32 szBytes)
    GCC_ATTRIB_NOINLINE();

/*!
 * @brief Handles DMA write/copy from/to SMBPBI host side buffers
 *
 * @param[in]   bWrite
 *      LW_TRUE  - write (memset) operation
 *      LW_FALSE - copy (memcpy) operation
 * @param[in]   dst
 *      word offset into the destination surface
 * @param[in]   from
 *      for write operation - the word to write
 *      for copy operation  - byte offset into the source surface
 * @param[in]   szBytes
 *      byte count
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      writte/copied successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      DMA malfunction
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_DATA
 *      source buffer wraps around
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG1
 *      destination buffer wraps around
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      destination and source buffers overlap
 */
static LwU8
s_smbpbiHostMemWriteCopy
(
    LwBool  bWrite,
    LwU32   dst,
    LwU32   from,
    LwU32   szBytes
)
{
    LwU8       dmaBufSpace[SMBPBI_IFR_XFER_SIZE + DMA_MIN_READ_ALIGNMENT - 1];
    LwU8      *dmaBuf = PMU_ALIGN_UP_PTR(dmaBufSpace, DMA_MIN_READ_ALIGNMENT);
    LwU8      *pWrite;
    LwU8      *pRead  = NULL;  // pacify gcc
    LwU32      szRead = 0;
    LwU32      dmaWSize;
    LwU8       off;
    LwU8       status = LW_MSGBOX_CMD_STATUS_ERR_MISC;

    if (bWrite)
    {
        for (pWrite = dmaBuf; pWrite < (dmaBuf + SMBPBI_IFR_XFER_SIZE);
                                                            pWrite += sizeof(LwU32))
        {
            *(LwU32 *)pWrite = from;
        }
    }
    else
    {
        if (from + szBytes > LW_MSGBOX_SCRATCH_BUFFER_SIZE)
        {
            // source buffer may not wrap around
            return LW_MSGBOX_CMD_STATUS_ERR_DATA;
        }

        if (dst + szBytes > LW_MSGBOX_SCRATCH_BUFFER_SIZE)
        {
            // destination buffer may not wrap around
            return LW_MSGBOX_CMD_STATUS_ERR_ARG1;
        }

        if (((dst >= from) && (dst < from + szBytes)) ||
            ((from > dst) && (from < dst + szBytes)))
        {
            // source and destination buffers may not overlap
            return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
        }

        from += PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH);
    }

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    {
        while (szBytes > 0)
        {
            if (szRead == 0)
            {
                szRead = SMBPBI_IFR_XFER_SIZE;
                pRead = dmaBuf;

                if (!bWrite)
                {
                    off = from % DMA_MIN_READ_ALIGNMENT;
                    szRead -= off;
                    pRead += off;
                    if (FLCN_OK != dmaRead(dmaBuf, &PMU_SMBPBI_HOST_MEM_SURFACE(),
                                           from - off, SMBPBI_IFR_XFER_SIZE))
                    {
                        break;  // return status
                    }

                    from += SMBPBI_IFR_XFER_SIZE - off;
                }
            }

            dmaWSize = LW_MIN(szRead, szBytes);
            dmaWSize = LW_MIN(dmaWSize, LW_MSGBOX_SCRATCH_BUFFER_SIZE - dst);

            if (FLCN_OK != dmaWrite(pRead, &PMU_SMBPBI_HOST_MEM_SURFACE(),
                                    PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH) +
                                    dst, dmaWSize))
            {
                break;  // return status
            }

            szRead -= dmaWSize;
            szBytes -= dmaWSize;
            pRead += dmaWSize;
            dst = (dst + dmaWSize) % LW_MSGBOX_SCRATCH_BUFFER_SIZE; // dst wraps around
        }
    }
    lwosDmaLockRelease();

    if (szBytes == 0)
    {
        status = LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    return status;
}

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Dispatch SMBPBI scratch memory request to the appropriate exelwtor ftn
 *
 * @param[in]   cmd
 *      SMBPBI command register
 * @param[in, out]  pData
 *      pointer to SMBPBI data register
 *
 * @return
 *      bubbles up status from the exelwtor
 *      LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED - scratch memory not configured
 */
LwU8
smbpbiHostMemOp
(
    LwU32  cmd,
    LwU32 *pData
)
{
    LwU32   szBytes;
    LwU8    status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
    LwBool  bWrite = LW_FALSE;
    LwU32   from = DRF_VAL(_MSGBOX, _DATA, _COPY_SRC_OFFSET, *pData) * sizeof(LwU32);
    LwU8    arg1 = DRF_VAL(_MSGBOX, _CMD, _ARG1, cmd);
    LwU8    arg2 = DRF_VAL(_MSGBOX, _CMD, _ARG2, cmd);
    LwU32   dst  = arg1 * sizeof(LwU32);
    LwU32   pageReg;            //< bank register from the register file
    LwU32   pageOffsetRead;     //< source bank offset
    LwU32   pageOffsetWrite;    //< destination bank offset

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    if (smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_SCRATCH_PAGE, &pageReg)
                    != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        pageReg = 0;
    }

    pageOffsetRead = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_SRC, pageReg)
                    * LW_MSGBOX_SCRATCH_PAGE_SIZE;
    pageOffsetWrite = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_DST, pageReg)
                    * LW_MSGBOX_SCRATCH_PAGE_SIZE;

    from += pageOffsetRead;
    szBytes = (arg2 + 1) * sizeof(LwU32);

    switch (DRF_VAL(_MSGBOX, _CMD, _OPCODE, cmd))
    {
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_READ:
        {
            status = smbpbiHostMemRead_SCRATCH(dst + pageOffsetRead,
                                               sizeof(LwU32), (LwU8 *)pData);
            break;
        }
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_WRITE:
        {
            bWrite = LW_TRUE;
            from = *pData;  // data word to set the memory block to
            // no break
        }
        case LW_MSGBOX_CMD_OPCODE_SCRATCH_COPY:
        {
            status = s_smbpbiHostMemWriteCopy(bWrite,
                                             dst + pageOffsetWrite, from, szBytes);
            break;
        }
    }

    return status;
}

/*!
 * @brief   Write a word into  a register in the SMBPBI register file
 *
 * @param[in] reg
 *      register ID (index)
 * @param[in] data
 *      the value to be written
 *
 * @return LW_MSGBOX_CMD_STATUS_SUCCESS
 *      written successfully
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      no such register
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      DMA malfunction
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *      register file not configured
 */
LwU8
smbpbiRegWr
(
    LwU8  reg,
    LwU32 data
)
{
    FLCN_STATUS status = FLCN_OK;

    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    if (reg >= LW_SMBPBI_STATE_REGISTER_FILE_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    if (reg < SMBPBI_REG_CACHE_SIZE)
    {
        Smbpbi.regFile[reg] = data;
    }

    if (!SMBPBI_REG_IS_LOCAL(reg))
    {
        status = dmaWrite(&data, &PMU_SMBPBI_HOST_MEM_SURFACE(),
                          PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_REGFILE) +
                          reg * sizeof(LwU32),
                          sizeof(LwU32));
    }

    return (status == FLCN_OK) ? LW_MSGBOX_CMD_STATUS_SUCCESS :
                                 LW_MSGBOX_CMD_STATUS_ERR_MISC;
}

/*!
 * @brief   Read a register from the SMBPBI register file
 *
 * @param[in] reg
 *      register ID (index)
 * @param[in, out] pData
 *      pointer to the read value
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      no such register
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED
 *      register file not configured
 *
 * @return other
 *      bubbled up status from the lower level
 */
LwU8
smbpbiRegRd
(
    LwU8  reg,
    LwU32 *pData
)
{
    if (!LW_MSGBOX_CAP_IS_AVAILABLE(Smbpbi.config.cap, 2, _NUM_SCRATCH_BANKS))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_NOT_SUPPORTED;
    }

    if (reg >= LW_SMBPBI_STATE_REGISTER_FILE_SIZE)
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    if ((reg < SMBPBI_REG_CACHE_SIZE) && SMBPBI_REG_IS_CACHEABLE(reg))
    {
        *pData = Smbpbi.regFile[reg];
        return LW_MSGBOX_CMD_STATUS_SUCCESS;
    }

    return smbpbiHostMemRead_REGFILE(reg * sizeof(LwU32),
                                     sizeof(LwU32), (LwU8 *)pData);
}

#if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS))
/*!
 * @brief   Colwert an 8-bit ptr to scratch memory within the
 *          specified bank to a byte ptr into the entire
 *          scratch memory array.
 *
 * @param[in] ptr
 *      32-bit word index within the bank
 * @param[in] bank_type
 *      _READ or _WRITE bank
 * @param[out] pOffset
 *      byte offset into the scratch memory
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      invalid bank type specified
 *
 * @return other
 *      bubbled up status from the lower level
 */
LwU8
smbpbiScratchPtr2Offset
(
    LwU8                        ptr,
    SMBPBI_SCRATCH_BANK_TYPE    bank_type,
    LwU32                       *pOffset
)
{
    LwU32   bankReg;
    LwU32   offset;
    LwU8    status;

    status = smbpbiRegRd(LW_MSGBOX_CMD_ARG2_REG_INDEX_SCRATCH_PAGE, &bankReg);
    if (status != LW_MSGBOX_CMD_STATUS_SUCCESS)
    {
        goto smbpbiScratchPtr2Offset_exit;
    }

    switch (bank_type)
    {
        case SMBPBI_SCRATCH_BANK_READ:
        {
            offset = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_SRC, bankReg);
            break;
        }
        case SMBPBI_SCRATCH_BANK_WRITE:
        {
            offset = DRF_VAL(_MSGBOX, _DATA, _SCRATCH_PAGE_DST, bankReg);
            break;
        }
        default:
        {
            PMU_TRUE_BP();
            status = LW_MSGBOX_CMD_STATUS_ERR_MISC;
            goto smbpbiScratchPtr2Offset_exit;
        }
    }

    *pOffset = (offset * LW_MSGBOX_SCRATCH_PAGE_SIZE) + (ptr * sizeof(LwU32));

smbpbiScratchPtr2Offset_exit:
    return status;
}

/*!
 * @brief   Write a buffer contents into the scratch space
 *
 * @param[in] offset
 *      byte offset into the scratch memory
 * @param[in] pBuf
 *      buffer ptr
 * @param[in] size
 *      buffer size
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_MISC
 *      no bytes were written by the inferior function
 *
 * @return other
 *      bubbled up status from the lower level
 */
static LwU8
s_smbpbiScratchWrite
(
    LwU32   offset,
    void    *pBuf,
    LwU32   size
)
{
    LwU8    dmaBufSpace[SMBPBI_IFR_XFER_SIZE + DMA_MIN_READ_ALIGNMENT - 1];
    LwU8    *dmaBuf = PMU_ALIGN_UP_PTR(dmaBufSpace, DMA_MIN_READ_ALIGNMENT);
    LwU8    dmaSize;

    // Its recommended to use DMA section when task is doing back to back DMA.
    lwosDmaLockAcquire();
    while (size != 0)
    {
        dmaSize = LW_MIN(size, SMBPBI_IFR_XFER_SIZE);
        memcpy(dmaBuf, pBuf, dmaSize);

        if (FLCN_OK != dmaWrite(pBuf, &Smbpbi.config.memdescInforom,
                PMU_SMBPBI_HOST_MEM_OFFSET(PMU_SMBPBI_HOST_MEM_TYPE_SCRATCH) +
                offset, dmaSize))
        {
            break;  // return status
        }

        size -= dmaSize;
        pBuf = (LwU8 *)pBuf + dmaSize;
        offset += dmaSize;
    }
    lwosDmaLockRelease();

    return size == 0 ? LW_MSGBOX_CMD_STATUS_SUCCESS
                        : LW_MSGBOX_CMD_STATUS_ERR_MISC;
}

/*!
 * @brief   Read from scratch memory into a buffer, or
 *          write a buffer contents into the scratch memory
 *
 * @param[in] offset
 *      byte offset into the scratch memory
 * @param[in] pBuf
 *      buffer ptr
 * @param[in] size
 *      buffer size
 * @param[in] bWrite
 *      write/read
 *
 * @return LW_MSGBOX_CMD_STATUS_ERR_ARG2
 *      scratch memory size exceeded
 *
 * @return other
 *      bubbled up status from the lower level
 */
LwU8
smbpbiScratchRW
(
    LwU32   offset,
    void    *pBuf,
    LwU32   size,
    LwBool  bWrite
)
{

    if ((offset >= LW_MSGBOX_SCRATCH_BUFFER_SIZE) ||
        ((offset + size) > LW_MSGBOX_SCRATCH_BUFFER_SIZE))
    {
        return LW_MSGBOX_CMD_STATUS_ERR_ARG2;
    }

    return bWrite ?
        s_smbpbiScratchWrite(offset, pBuf, size) :
        smbpbiHostMemRead_SCRATCH(offset, size, pBuf);
}
#endif  // PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_BUNDLED_REQUESTS)
