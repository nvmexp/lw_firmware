#include "flash_access.h"
#include "inforom/fs.h"

static LwU8 ifsCallwlateChecksum(IFS_BLOCK *pFsBlock)
{
    LwU8 *pBlockBuffer = (LwU8 *)pFsBlock;
    LwU32 i;
    LwU8 checksum = 0;

    for (i = 0; i < IFS_BLOCK_SIZE; i++)
    {
        if (i != IFS_BLOCK_CHECKSUM_OFFSET)
        {
            checksum += pBlockBuffer[i];
        }
    }

    return ((~checksum + 1) & 0xff);
}

FS_STATUS ifsBlockHeaderSanityCheck(IFS_BLOCK_HEADER *pHeader)
{
    LwU32 i;

    // Sanity check flags
    if (IFS_FLAGS_IS_CLEAN(pHeader->flags))
    {
        // For clean blocks, the entire header should be 0xff
        LwU8 *pHeaderBytes = (LwU8 *)pHeader;
        for (i = 0; i < IFS_BLOCK_HEADER_SIZE; i++)
        {
            if (pHeaderBytes[i] != 0xff)
                return FS_ERR_ILWALID_DATA;
        }
    }
    else
    {
        // Reserved space should always be zeroed in used blocks
        for (i = 0; i < sizeof(pHeader->reserved); i++)
        {
            if (pHeader->reserved[i] != 0x00)
                return FS_ERR_ILWALID_DATA;
        }

        // Size should not be any greater than the block size
        if (pHeader->size > IFS_BLOCK_SIZE)
            return FS_ERR_ILWALID_DATA;

        if (pHeader->flags == 0)
        {
            // No flags set must be a dummy RO block
            if ((pHeader->name[0] != 0) ||
                (pHeader->name[1] != 0) ||
                (pHeader->name[2] != 0) ||
                (pHeader->offset != 0)  ||
                (pHeader->size != IFS_BLOCK_SIZE) ||
                (pHeader->version != 0))
            {
                return FS_ERR_ILWALID_DATA;
            }
        }
        else
        {
            // Offset should always be a multiple of BLOCK_DATA_SIZE
            if ((pHeader->offset % IFS_BLOCK_DATA_SIZE) != 0)
                return FS_ERR_ILWALID_DATA;

            // Name should be capital ASCII chars
            if ((pHeader->name[0] < 'A') || (pHeader->name[0] > 'Z') ||
                (pHeader->name[1] < 'A') || (pHeader->name[1] > 'Z') ||
                (pHeader->name[2] < 'A') || (pHeader->name[2] > 'Z'))
            {
                return FS_ERR_ILWALID_DATA;
            }

            if (pHeader->size == 0)
                return FS_ERR_ILWALID_DATA;

            // RO blocks should never be obsolete, should always be
            // checksummed, and should always be version 0.
            if ((!IFS_FLAGS_IS_RW(pHeader->flags)) &&
                ((!IFS_FLAGS_IS_LWRRENT(pHeader->flags)) ||
                 (!IFS_FLAGS_IS_CHECKSUM_REQUIRED(pHeader->flags)) ||
                 (pHeader->version > 0)))
            {
                return FS_ERR_ILWALID_DATA;
            }
        }
    }

    return FS_OK;
}

FS_STATUS ifsBlockDataSanityCheck(IFS_BLOCK *pFsBlock, LwU8 *pChecksum)
{
    FS_STATUS status = FS_OK;
    LwU8 checksum;

    FS_ASSERT_AND_RETURN(pFsBlock != NULL, FS_ERR_ILWALID_ARG);

    if (IFS_FLAGS_IS_CHECKSUM_REQUIRED(pFsBlock->header.flags))
    {
        checksum = ifsCallwlateChecksum(pFsBlock);

        if (pChecksum != NULL)
        {
            *pChecksum = checksum;
        }

        if (checksum != pFsBlock->header.checksum)
        {
            FS_PRINTF(ERROR,
                "%c%c%c block at offset: %u failed checksum validation. Expected: %x Actual: %x\n",
                pFsBlock->header.name[0], pFsBlock->header.name[1],
                pFsBlock->header.name[2], pFsBlock->header.offset,
                pFsBlock->header.checksum, checksum);
            status = FS_ERR_ILWALID_DATA;
            goto done;
        }
    }

done:
    return status;
}

FS_STATUS ifsBlockReadHeader(IFS *pFs, LwU32 blockAddr, void *pHeader)
{
    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pHeader != NULL, FS_ERR_ILWALID_ARG);

    return readFlash(pFs->pFlashAccessor, blockAddrToFlashOffset(blockAddr),
                     IFS_BLOCK_HEADER_SIZE, pHeader);
}

FS_STATUS ifsBlockRead(IFS *pFs, LwU32 blockAddr, IFS_BLOCK *pFsBlock)
{
    FS_STATUS status;
    LwU32 blockOffset = blockAddrToFlashOffset(blockAddr);

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pFsBlock != NULL, FS_ERR_ILWALID_ARG);

    status = readFlash(pFs->pFlashAccessor, blockOffset, IFS_BLOCK_SIZE, (void *)pFsBlock);
    if (status != FS_OK)
        return status;

    return ifsBlockHeaderSanityCheck(&pFsBlock->header);
}

FS_STATUS ifsBlockReadAndVerify(IFS *pFs, LwU32 blockAddr, LwU32 *pSize)
{
    IFS_BLOCK *pFsBlock = NULL;
    FS_STATUS status;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);

    pFsBlock = ifsBufferBorrow(pFs);
    if (pFsBlock == NULL)
        return FS_ERR_ILWALID_STATE;

    status = ifsBlockRead(pFs, blockAddr, pFsBlock);
    if (status != FS_OK)
        goto out;

    status = ifsBlockDataSanityCheck(pFsBlock, NULL);
    if ((status == FS_OK) && (pSize != NULL))
        *pSize = pFsBlock->header.size;

out:
    ifsBufferReturn(pFs, pFsBlock);
    return status;
}

FS_STATUS ifsBlockIlwalidate(IFS *pFs, LwU32 blockAddr)
{
    FS_STATUS status = FS_OK;
    LwU32 offset = 0;
    LwU8 flags = 0;

    FS_ASSERT_AND_RETURN((pFs != NULL), FS_ERR_ILWALID_ARG);

    offset = blockAddrToFlashOffset(blockAddr);
    offset += offsetof(IFS_BLOCK_HEADER, flags);

    // Retrieve the existing flags to retain the RW bit.
    status = readFlash(pFs->pFlashAccessor, offset, sizeof(flags), &flags);
    if (status != FS_OK)
        goto out_error;

    // Clear the Current bit to mark block as obsolete.
    flags &= ~IFS_BLOCK_FLAGS_LWRRENT;

    status = writeFlash(pFs->pFlashAccessor, offset, sizeof(flags), &flags);
    if (status != FS_OK)
        goto out_error;
    return status;

out_error:
    FS_PRINTF(ERROR, "Failed to ilwalidate block at 0x%x, rc:%d\n", offset, status);
    return status;
}

FS_STATUS ifsBlockWrite(IFS *pFs, LwU32 blockAddr, void *pBlockBuffer)
{
    FS_STATUS status = FS_OK;
    LwU32 blockOffset = blockAddrToFlashOffset(blockAddr);

    FS_ASSERT_AND_RETURN(pBlockBuffer != NULL, FS_ERR_ILWALID_ARG);

    status = writeFlash(pFs->pFlashAccessor, blockOffset,
                        IFS_BLOCK_SIZE, pBlockBuffer);
    if (status != FS_OK)
        FS_PRINTF(ERROR, "Failed to write block at offset: 0x%x, rc:%d\n",
                  blockOffset, status);

    return status;
}

FS_STATUS ifsBlockWriteJournaled
(
    IFS *pFs,
    LwU32 lwrBlockAddr,
    IFS_BLOCK *pFsBlock,
    LwU32 *pNewBlockAddrOut
)
{
    FS_STATUS status = FS_OK;
    LwU32 blockAddr = 0;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pFsBlock != NULL, FS_ERR_ILWALID_ARG);

    pFsBlock->header.version++;
    pFsBlock->header.checksum = ifsCallwlateChecksum(pFsBlock);

    status = ifsGetBlockForWrite(pFs, &blockAddr);
    if (status != FS_OK)
        goto out;

    status = ifsBlockWrite(pFs, blockAddr, pFsBlock);
    if (status != FS_OK)
        goto out;

    status = ifsBlockIlwalidate(pFs, lwrBlockAddr);
    // TODO: this is recoverable, the new block is written and has a higher version number.
    if (status != FS_OK)
        goto out;

    if (pNewBlockAddrOut != NULL)
        *pNewBlockAddrOut = blockAddr;

out:
    return status;
}
