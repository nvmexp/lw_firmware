/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

// #define FS_PRINTF(level, ...) fprintf(stderr, __VA_ARGS__)

#include "inforom/bitmap.h"
#include "inforom/fs.h"
#include "flash_access.h"
#include "lwmisc.h"
#include "oslib.h"

// A map link entry is a temporary struct inserted into the map table to
// make file linkage and identification of obsolete blocks easier. It does
// not persist in the map beyond the mounting phase.
typedef union
{
    LwU32 raw;
    struct
    {
        // block index in the file's sequential block list.
        LwU16 fileIndex;
        LwU8 version;
        LwU8 file : 7;
        LwU8 rw   : 1;
    } p;
} MAP_LINK_ENTRY;

typedef struct
{
    LwU32 lwrBlock;
    LwU32 prevBlock;
    LwU32 firstBlock;
    IFS_MAP *pMap;
    IFS_FILE *pFile;
} IFS_FILE_ITERATOR;

// Inforom file system related helper functions.
static FS_STATUS ifsBlockScan(IFS *, LwU32);
static FS_STATUS ifsBlockMove(IFS *, LwU32);
static FS_STATUS ifsSectorMoveLwrrentBlocks(IFS *, LwU32);
static FS_STATUS ifsLink(IFS *);
static FS_STATUS ifsFileLink(IFS *, IFS_FILE *, LwU16 *);
static FS_STATUS ifsFileGet(IFS *, const char *, LwBool, IFS_FILE **);
static LwBool ifsIsHeaderCorrupt(IFS_BLOCK_HEADER *);

// Inforom file related helper functions
static FS_STATUS ifsFileConstruct(IFS *, const char *, LwU32, IFS_FILE **);
static LwBool ifsFileNameMatches(const char *, const char *);
static FS_STATUS ifsFileWriteJournaled(IFS *, IFS_FILE_ITERATOR *, IFS_BLOCK *);

// general util functions
static FS_STATUS ifsGetCleanSectorCount(IFS *pFs, LwU32 *pCount);

void *ifsBufferBorrow(IFS *pFs)
{
    void *pPtr = NULL;
    int i;

    for (i = 0; i < IFS_BORROW_BLOCKS; i++)
    {
        pPtr = pFs->pBorrowBuffer[i];
        if (pPtr != NULL)
        {
            pFs->pBorrowBuffer[i] = NULL;
            break;
        }
    }
    FS_ASSERT(pPtr != NULL);
    return pPtr;
}

void ifsBufferReturn(IFS *pFs, void *pPtr)
{
    int i;

    if (pPtr == NULL)
        return;

    for (i = 0; i < IFS_BORROW_BLOCKS; i++)
    {
        if (pFs->pBorrowBuffer[i] == NULL)
        {
            pFs->pBorrowBuffer[i] = pPtr;
            return;
        }
    }
    FS_ASSERT(LW_FALSE);
}

static void
iteratorInit(IFS_FILE_ITERATOR *pIter, IFS_FILE *pFile, IFS_MAP *pMap)
{
    pIter->lwrBlock = pFile->firstBlock;
    pIter->prevBlock = IFS_MAP_ENTRY_CLEAN;
    pIter->firstBlock = pFile->firstBlock;
    pIter->pMap = pMap;
    pIter->pFile = pFile;
}

static LwU32
iteratorLwrrent(IFS_FILE_ITERATOR *pIter)
{
    return pIter->lwrBlock;
}

static void
iteratorNext(IFS_FILE_ITERATOR *pIter)
{
    if (pIter->lwrBlock == IFS_MAP_ENTRY_LISTEND)
        return;

    pIter->prevBlock = pIter->lwrBlock;
    ifsMapGet(pIter->pMap, pIter->lwrBlock, &pIter->lwrBlock);
}

static FS_STATUS
iteratorSeek(IFS_FILE_ITERATOR *pIter, LwU32 blocks)
{
    LwU32 lwr;

    // skip forward to requested offset
    for (lwr = 0; lwr < blocks; lwr++)
    {
        LwU32 ent = iteratorLwrrent(pIter);
        if (!MAP_IS_DATA_BLOCK(ent))
            return FS_ERR_ILWALID_STATE;
        if (ent == IFS_MAP_ENTRY_LISTEND)
            return FS_ERR_ILWALID_ARG;
        iteratorNext(pIter);
    }
    return 0;
}

static void
iteratorMove(IFS_FILE_ITERATOR *pIter, LwU32 dst)
{
    LwU32 next;

    ifsMapGet(pIter->pMap, pIter->lwrBlock, &next);
    ifsMapSet(pIter->pMap, dst, next);

    if (pIter->prevBlock == IFS_MAP_ENTRY_CLEAN)
    {
        pIter->firstBlock = dst;
        pIter->pFile->firstBlock = dst;
    }
    else
    {
        ifsMapSet(pIter->pMap, pIter->prevBlock, dst);
    }
    ifsMapSet(pIter->pMap, pIter->lwrBlock, IFS_MAP_ENTRY_DISCARD);
    pIter->lwrBlock = dst;
}

static void
iteratorSync(IFS_FILE_ITERATOR *pIter, LwU32 oldAddr, LwU32 newAddr)
{
    if (pIter->firstBlock == oldAddr)
        pIter->firstBlock = newAddr;
    if (pIter->lwrBlock == oldAddr)
        pIter->lwrBlock = newAddr;
    else if (pIter->prevBlock == oldAddr)
        pIter->prevBlock = newAddr;
}

/*!
 * @brief Constructs the FS class.
 */
IFS *
inforomFsConstruct(void *pFlashAccessor, LwU32 version, LwU32 gcSectorThreshold)
{
    IFS *pFs = NULL;
    LwU32 status = FS_OK;
    LwU32 i;

    status = fsAllocZeroMem((void **)&pFs, sizeof(IFS));
    if ((pFs == NULL) || (status != FS_OK)) 
        return NULL;

    pFs->pFlashAccessor = pFlashAccessor;
    pFs->version = version;
    
    pFs->sectorSize = IFS_SECTOR_SIZE;
    pFs->blocksPerSector = IFS_SECTOR_SIZE / IFS_BLOCK_SIZE;

    pFs->gcSyncThreshold = gcSectorThreshold;
    pFs->bEnableGC = LW_TRUE;

    pFs->bEnableWrites = LW_FALSE;

#if FS_OPTION_FLASH_TRACE_ENABLE
    pFs->bEnableFlashTracing = LW_TRUE;
#endif

    pFs->corruptList = IFS_MAP_ENTRY_LISTEND;

    for (i = 0; i < IFS_BORROW_BLOCKS; i++)
    {
        pFs->pBorrowBuffer[i] = pFs->borrowBufferBase + (i * IFS_BLOCK_SIZE);
    }


    return pFs;
}

// Writes aren't allowed in to the pre-init phase of RM so call this at a later time
FS_STATUS
inforomFsWriteEnable(IFS *pFs)
{
    FS_STATUS status;
    LwU32 lwrBlock;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);

    pFs->bEnableWrites = LW_TRUE;

    // Once write are enabled, ilwalidate blocks marked for deletion.

    for (lwrBlock = 0; lwrBlock < pFs->numBlocks; lwrBlock++)
    {
        LwU32 value = 0;

        ifsMapGet(pFs->pMap, lwrBlock, &value);
        if (value != IFS_MAP_ENTRY_DELETE)
            continue;

        status = ifsBlockIlwalidate(pFs, lwrBlock);
        if (status == FS_OK)
        {
            // Block has been ilwalidated on media, but not erased.
            ifsMapSet(pFs->pMap, lwrBlock, IFS_MAP_ENTRY_DISCARD);
        }
    }

    return FS_OK;
}

FS_STATUS
inforomFsDestruct(IFS *pFs)
{
    ifsFlashAccessDestruct(pFs);
    ifsMapDestroy(pFs->pMap);
    fsFreeMem(pFs->pBitmap);
    fsFreeMem(pFs);

    return FS_OK;
}

// Scan entire flash and build file system in memory from metadata in block headers.
FS_STATUS
inforomFsLoad(IFS *pFs, LwBool bReadOnly)
{
    IFS_SUPERBLOCK *pSuperBlock = NULL;
    FS_STATUS status = FS_OK;
    LwU32 lwrBlock;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);

    pFs->pMap = NULL;
    pFs->bEnableWrites = !bReadOnly;

    pSuperBlock = ifsBufferBorrow(pFs);

    status = readFlash(pFs->pFlashAccessor, 0, IFS_SUPERBLOCK_SIZE, (void *)pSuperBlock);
    if (status != FS_OK)
        goto done;

    // validate superblock

    // It's not really JFFS, this is a lie.
    if ((pSuperBlock->fsType[0] != 'J') ||
        (pSuperBlock->fsType[1] != 'F') ||
        (pSuperBlock->fsType[2] != 'F') ||
        (pSuperBlock->fsType[3] != 'S'))
    {
        FS_PRINTF(ERROR, "Invalid superblock tag\n");
        status = FS_ERR_ILWALID_STATE;
        goto done;
    }

    if (pSuperBlock->version == 0)
    {
        FS_PRINTF(ERROR, "Invalid superblock version: %u\n", pSuperBlock->version);
        status = FS_ERR_ILWALID_STATE;
        goto done;
    }

    if (pSuperBlock->version > IFS_VERSION_MAX)
    {
        FS_PRINTF(ERROR, "Unsupported file system version: %u\n", pSuperBlock->version);
        status = FS_ERR_NOT_SUPPORTED;
        goto done;
    }

    pFs->version = pSuperBlock->version;

    switch (pFs->version)
    {
    case 1:
        pFs->size = IFS_DEFAULT_SIZE;
        break;

    default:
        pFs->size = pSuperBlock->fsSize;
        break;
    }

    ifsBufferReturn(pFs, pSuperBlock);
    pSuperBlock = NULL;

    if (pFs->size == 0xFFFFFFFF)
    {
        FS_PRINTF(ERROR, "Invalid file system size\n");
        status = FS_ERR_ILWALID_STATE;
        goto done;
    }

    pFs->numBlocks = pFs->size / IFS_BLOCK_SIZE;

    status = ifsMapCreate(pFs->numBlocks, &pFs->pMap);
    if (status != FS_OK)
    {
        FS_PRINTF(ERROR, "Failed to allocate file table\n");
        goto done;
    }

    status = fsAllocZeroMem((void **)&pFs->pBitmap, FS_ROUND_UP_POW2(pFs->numBlocks, 64) / 8);
    if (status != FS_OK)
    {
        FS_PRINTF(ERROR, "Failed to allocate bitmap\n");
        goto done;
    }

    for (lwrBlock = 0; lwrBlock < pFs->numBlocks; lwrBlock++)
    {
        // Scan block
        status = ifsBlockScan(pFs, lwrBlock);
        if (status != FS_OK)
        {
            FS_PRINTF(ERROR, "Failed to scan blocks\n");
            goto done;
        }
    }

    // Due to a journaled write there is a chance that we can have duplicate
    // nodes i.e. valid nodes with same offset. Below we handle this scenario.
    status = ifsLink(pFs);
    if (status != FS_OK)
        goto done;

    // IFS_SECTOR_STATS stats = {};
    // for (LwU32 lwrSector = 0; lwrSector < pFs->numBlocks; lwrSector += IFS_BLOCKS_PER_SECTOR) {
    //     ifsGetSectorStats(pFs, lwrSector, LW_FALSE, &stats);
    // }

    // printf("Mounted filesystem\n");
    // printf("  total:    %u blocks\n", pFs->numBlocks);
    // printf("  used:     %u\n", stats.usedCount);
    // printf("  clean:    %u\n", stats.cleanCount);
    // printf("  discard:  %u\n", stats.discardCount);
    // printf("  reserved: %u\n", stats.reservedCount);
    // printf("  bad:      %u\n", stats.badCount);

done:
    ifsBufferReturn(pFs, pSuperBlock);
    if (status != FS_OK)
    {
        ifsMapDestroy(pFs->pMap);
        pFs->pMap = NULL;
        fsFreeMem(pFs->pBitmap);
        pFs->pBitmap = NULL;
    }
    return status;
}

FS_STATUS
inforomFsSetGCEnabled(IFS *pFs, LwBool bEnableGC)
{
    pFs->bEnableGC = bEnableGC;
    return FS_OK;
}

FS_STATUS
inforomFsRead
(
    IFS *pFs,
    const char pName[],
    LwU32 fileOffset,
    LwU32 size,
    LwU8 *pOutBuffer
)
{
    IFS_FILE_ITERATOR iter;
    IFS_FILE *pFile = NULL;
    IFS_BLOCK *pFsBlock = NULL;
    FS_STATUS status;
    LwU32 readSize = 0;
    LwU32 readOffset = 0;
    LwU32 skipBlocks;

    FS_ASSERT_AND_RETURN(pName != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pOutBuffer != NULL, FS_ERR_ILWALID_ARG);

    status = ifsFileGet(pFs, pName, LW_FALSE, &pFile);
    if (status != FS_OK)
        return status;

    // make sure we are not going over
    if (pFile->size < fileOffset + size)
    {
        FS_PRINTF(ERROR, "%s: Trying to read past limits of the file, size:0x%x\n",
                  __FUNCTION__, fileOffset + size);
        return FS_ERR_ILWALID_ARG;
    }

    pFsBlock = ifsBufferBorrow(pFs);
    if (pFsBlock == NULL)
        return FS_ERR_ILWALID_STATE;

    iteratorInit(&iter, pFile, pFs->pMap);

    // Find starting block.
    skipBlocks = fileOffset / IFS_BLOCK_DATA_SIZE;
    status = iteratorSeek(&iter, skipBlocks);
    if (status != FS_OK)
        goto error;

    readOffset = fileOffset % IFS_BLOCK_DATA_SIZE;

    while (size > 0)
    {
        INFOROM_FLASH_ACCESS *pFlashAccess = NULL;
        LwU32 blockOffset;
        LwU32 lwrBlock = 0;

        lwrBlock = iteratorLwrrent(&iter);

        if (lwrBlock == IFS_MAP_ENTRY_LISTEND)
        {
            status = FS_ERR_ILWALID_ARG;
            goto error;
        }

        if (!MAP_IS_DATA_BLOCK(lwrBlock))
        {
            FS_PRINTF(ERROR, "%s: Unexpected end of file list\n", __FUNCTION__);
            status = FS_ERR_ILWALID_STATE;
            goto error;
        }

        blockOffset = blockAddrToFlashOffset(lwrBlock);
        ifsFlashTracingStart(pFs, FLASHACCESSTYPE_READ, pName,
                             blockOffset, NULL, IFS_BLOCK_SIZE,
                             &pFlashAccess);
        status = readFlash(pFs->pFlashAccessor, blockOffset,
                           IFS_BLOCK_SIZE, (void*)pFsBlock);
        ifsFlashTracingEnd(pFs, (void*)pFsBlock, IFS_BLOCK_SIZE,
                           status, pFlashAccess);
        if (status != FS_OK)
            goto error;

        status = ifsBlockDataSanityCheck(pFsBlock, NULL);
        if (status != FS_OK)
            goto error;

        readSize = IFS_BLOCK_DATA_SIZE - readOffset;
        if (readSize > size)
            readSize = size;

        fsMemCopy(pOutBuffer, &pFsBlock->data[readOffset], readSize);

        fileOffset += readSize;
        pOutBuffer += readSize;
        size -= readSize;
        readOffset = 0;

        iteratorNext(&iter);
    }

error:
    ifsBufferReturn(pFs, pFsBlock);
    return status;
}

void
ifsGetSectorStats
(
    IFS *pFs,
    LwU32 sectorAddr,
    LwBool bClear,
    IFS_SECTOR_STATS *stats
)
{
    LwU32 i;

    if (bClear)
        fsMemSet(stats, 0, sizeof(*stats));

    for (i = 0; i < pFs->blocksPerSector; i++)
    {
        LwU32 value = 0;

        ifsMapGet(pFs->pMap, sectorAddr + i, &value);
        if (MAP_IS_DATA_BLOCK(value))
        {
            stats->usedCount++;
            continue;
        }

        switch (value)
        {
        case IFS_MAP_ENTRY_CLEAN:
            stats->cleanCount++;
            break;

        case IFS_MAP_ENTRY_BAD:
            stats->badCount++;
            break;

        case IFS_MAP_ENTRY_DISCARD:
        case IFS_MAP_ENTRY_DELETE:
            stats->discardCount++;
            break;

        case IFS_MAP_ENTRY_RESERVED:
            stats->reservedCount++;
            break;

        case IFS_MAP_ENTRY_LISTEND:
        default:
            stats->usedCount++;
            break;
        }
    }
}

static FS_STATUS
ifsClaimSectorSync(IFS *pFs)
{
    IFS_MAP_ITERATOR iter;
    INFOROM_FLASH_ACCESS *pFlashAccess = NULL;
    FS_STATUS status;
    LwU32 sectorAddr;
    const LwU32 stride = pFs->blocksPerSector;
    LwU32 blockState = IFS_MAP_ENTRY_CLEAN;
    LwU32 sparseAddress = IFS_MAP_ENTRY_ILWALID;
    LwU32 sparseUsedCount = IFS_BLOCKS_PER_SECTOR;
    LwU32 i;
    LwBool eraseRequired = LW_TRUE;

    if (!pFs->bEnableGC)
        return FS_ERR_NOT_SUPPORTED;

    // Find the next sector with discard blocks.
    // This sequence is less than ideal, but preserves the behavior
    // of the previous implementation.
    // Preferably start with sectors that are all discard blocks,
    // followed by mixed discard and written,
    // followed by mixed and empty


    // find sector with discarded blocks

    for (sectorAddr = ifsMapIterStart(&iter, pFs->pMap, pFs->gcSector);
         sectorAddr != IFS_MAP_ENTRY_ILWALID;
         sectorAddr = ifsMapIterNext(&iter, stride))
    {
        IFS_SECTOR_STATS stats;

        // Skip the sector lwrrently being written to.
        // This may not be necessary.
        if (sectorAddr == pFs->writeSector)
            continue;

        ifsGetSectorStats(pFs, sectorAddr, LW_TRUE, &stats);

        // Sector cannot be erased.
        if ((stats.badCount > 0) || (stats.reservedCount > 0))
            continue;

        // Found a sector to erase.
        if (stats.discardCount > 0)
            break;

        // Find a depopulated sector to move in case no suitable deletion sector found.
        if (stats.usedCount < sparseUsedCount)
        {
            sparseUsedCount = stats.usedCount;
            sparseAddress = sectorAddr;
        }
    }

    if (sectorAddr == IFS_MAP_ENTRY_ILWALID)
    {
        if (sparseAddress == IFS_MAP_ENTRY_ILWALID)
        {
            FS_PRINTF(ERROR, "No candidate sector to GC!\n");
            return FS_ERR_ILWALID_STATE;
        }
        sectorAddr = sparseAddress;
        if (sparseUsedCount == 0)
            eraseRequired = LW_FALSE;
    }

    status = ifsSectorMoveLwrrentBlocks(pFs, sectorAddr);
    if (status != FS_OK)
        return status;

    if (eraseRequired)
    {
        LwU32 offset;

        offset = blockAddrToFlashOffset(sectorAddr);
        ifsFlashTracingStart(pFs, FLASHACCESSTYPE_ERASE, "N/A", offset,
                             NULL, pFs->sectorSize, &pFlashAccess);
        status = eraseFlash(pFs->pFlashAccessor, offset, pFs->sectorSize);
        ifsFlashTracingEnd(pFs, NULL, pFs->sectorSize,
                           status, pFlashAccess);
        if (status != FS_OK)
        {
            FS_PRINTF(ERROR, "%s: Sector erase failed, rc:%d\n",
                      __FUNCTION__, status);
            blockState = IFS_MAP_ENTRY_BAD;
        }
    }

    for (i = 0; i < stride; i++)
    {
        ifsMapSet(pFs->pMap, sectorAddr + i, blockState);
    }

    pFs->gcSector = sectorAddr + stride;
    if (pFs->gcSector >= pFs->numBlocks)
        pFs->gcSector = 0;

    return FS_OK;
}

static FS_STATUS
ifsSectorMoveLwrrentBlocks(IFS *pFs, LwU32 sectorAddr)
{
    FS_STATUS status = FS_OK;
    LwU32 i;

    for (i = 0; i < pFs->blocksPerSector; i++)
    {
        LwU32 blockAddr = sectorAddr + i;
        LwU32 value = 0;

        status = ifsMapGet(pFs->pMap, blockAddr, &value);
        if (status != FS_OK)
            goto out;

        if (!MAP_IS_DATA_BLOCK(value))
            continue;

        status = ifsBlockMove(pFs, blockAddr);
        if (status != FS_OK)
            goto out;
    }

out:
    return FS_OK;
}

static FS_STATUS
ifsBlockMove(IFS *pFs, LwU32 blockAddr)
{
    IFS_FILE_ITERATOR iter;
    IFS_BLOCK *pFsBlock = NULL;
    IFS_FILE *pFile = NULL;
    FS_STATUS status = FS_OK;
    LwU32 newBlockAddr;

    pFsBlock = ifsBufferBorrow(pFs);
    if (pFsBlock == NULL)
        return FS_ERR_ILWALID_STATE;

    status = readFlash(pFs->pFlashAccessor, blockAddrToFlashOffset(blockAddr),
                       IFS_BLOCK_SIZE, (void *)pFsBlock);
    if (status != FS_OK)
        goto out;

    status = ifsBlockWriteJournaled(pFs, blockAddr, pFsBlock, &newBlockAddr);
    if (status != FS_OK)
        goto out;

    // Fix links for this file.
    status = ifsFileGet(pFs, pFsBlock->header.name, LW_TRUE, &pFile);
    if (status != FS_OK)
        goto out;

    iteratorInit(&iter, pFile, pFs->pMap);
    for ( ; ; )
    {
        LwU32 lwrBlock = iteratorLwrrent(&iter);

        if (lwrBlock == blockAddr)
        {
            iteratorMove(&iter, newBlockAddr);
            break;
        }
        if (IFS_MAP_ENT_IS_TAG(lwrBlock))
        {
            // block not found in file.
            status = FS_ERR_ILWALID_STATE;
            goto out;
        }
        iteratorNext(&iter);
    }

    // If an iterator is registered, update it to reflect the moved block.
    if (pFile->pIterator != NULL)
    {
        iteratorSync(pFile->pIterator, blockAddr, newBlockAddr);
    }

out:
    ifsBufferReturn(pFs, pFsBlock);
    return status;
}

FS_STATUS
ifsGetBlockForWrite(IFS *pFs, LwU32 *pBlockAddr)
{
    LwU32 firstAddr = pFs->writeSector;
    LwU32 sectorAddr;
    LwU32 firstCleanSector = IFS_MAP_ENTRY_ILWALID;
    LwBool bLooped = LW_FALSE;

    // This loop emulates the allocation behavior of the v2 FS code.
    // It presumes that there is one active writing sector which is
    // linearly written.
    // It attempts to find and fill partially filled sectors first,
    // then selecting erased sectors and allocating linearly within them.

    for (sectorAddr = firstAddr; ; sectorAddr += pFs->blocksPerSector)
    {
        LwU32 i;
        LwU32 entry = 0;
        LwU32 firstCleanAddr = IFS_MAP_ENTRY_ILWALID;
        LwU32 cleanCount = 0;

        if (sectorAddr >= pFs->numBlocks)
        {
            // Wrap around
            sectorAddr = 0;
            bLooped = LW_TRUE;
        }

        // Failed to locate writeable block
        if ((sectorAddr == firstAddr) && bLooped)
            break;

        for (i = 0; i < pFs->blocksPerSector; i++)
        {
            LwU32 lwrBlock = sectorAddr + i;

            ifsMapGet(pFs->pMap, lwrBlock, &entry);
            if (entry == IFS_MAP_ENTRY_CLEAN)
            {
                cleanCount++;
                if (firstCleanAddr == IFS_MAP_ENTRY_ILWALID)
                    firstCleanAddr = lwrBlock;
            }
        }

        if (cleanCount == 0)
            continue;
        if (cleanCount < pFs->blocksPerSector)
        {
            // Found partially written sector.
            // There should be only one of these on systems with v2 allocators.
            // Hopefully that's true here. Fill it linearly.
            pFs->writeSector = sectorAddr;
            *pBlockAddr = firstCleanAddr;
            return FS_OK;
        }
        else
        {
            if (firstCleanSector == IFS_MAP_ENTRY_ILWALID)
                firstCleanSector = sectorAddr;
        }
    }

    if (firstCleanSector == IFS_MAP_ENTRY_ILWALID)
        return FS_ERR_INSUFFICIENT_RESOURCES;

    pFs->writeSector = firstCleanSector;
    *pBlockAddr = firstCleanSector;
    return FS_OK;
}

static FS_STATUS
ifsGetCleanSectorCount(IFS *pFs, LwU32 *pCount)
{
    LwU32 sectorCount = 0;
    LwU32 blockCount = 0;
    LwU32 sectorAddr;
    LwU32 stride;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);

    stride = pFs->blocksPerSector;

    for (sectorAddr = 0; sectorAddr < pFs->numBlocks; sectorAddr += stride)
    {
        LwU32 i;
        LwBool bClean = LW_TRUE;

        for (i = 0; i < stride; i++)
        {
            LwU32 entry = 0;

            ifsMapGet(pFs->pMap, sectorAddr + i, &entry);
            if (entry == IFS_MAP_ENTRY_CLEAN)
                blockCount++;
            else
                bClean = LW_FALSE;
        }
        if (bClean)
            sectorCount++;
    }

    // If there are partially filled sectors increment sector count to
    // delay GC until they are consumed.
    if (blockCount > (sectorCount * stride))
        sectorCount++;

    *pCount = sectorCount;
    return FS_OK;
}

FS_STATUS
inforomFsIsGCNeeded(IFS *pFs, LwBool *pIsGcNeeded)
{
    FS_STATUS status = FS_OK;
    LwU32 count = 0;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pIsGcNeeded != NULL, FS_ERR_ILWALID_ARG);

    status = ifsGetCleanSectorCount(pFs, &count);
    if (status != FS_OK)
        return status;

    // While writeable sector count is not below threshold, GC can be deferred.
    *pIsGcNeeded = (count <= pFs->gcSyncThreshold);
    return status;
}

FS_STATUS
inforomFsDoGC(IFS *pFs)
{
    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    return ifsClaimSectorSync(pFs);
}

/*!
 * @brief Perform a journaled write i.e. write new block and then ilwalidate
 *        old block.
 */
static FS_STATUS
ifsFileWriteJournaled(IFS *pFs, IFS_FILE_ITERATOR *pIter, IFS_BLOCK *pFsBlock)
{
    INFOROM_FLASH_ACCESS *pFlashAccess = NULL;
    IFS_FILE *pFile = NULL;
    FS_STATUS status = FS_OK;
    LwU32 newBlockAddr = 0;
    LwU32 nextBlock = 0;
    LwU32 lwrBlock;
    LwBool bIsGcRequired = LW_FALSE;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pIter != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pFsBlock != NULL, FS_ERR_ILWALID_ARG);

    // check if GC is needed. Claim a sector if so.
    status = inforomFsIsGCNeeded(pFs, &bIsGcRequired);
    if (status != FS_OK)
        goto out;

    if (bIsGcRequired)
    {
        status = ifsClaimSectorSync(pFs);
        if (status != FS_OK)
            goto out;
    }

    // The contents of pIter may change due to GC moving blocks.
    // The map is updated, but we must avoid stale local
    // addresses from before the move.

    pFile = pIter->pFile;
    lwrBlock = iteratorLwrrent(pIter);

    ifsFlashTracingStart(pFs, FLASHACCESSTYPE_WRITE, pFile->name,
                         blockAddrToFlashOffset(lwrBlock), pFsBlock,
                         IFS_BLOCK_SIZE, &pFlashAccess);
    status = ifsBlockWriteJournaled(pFs, lwrBlock, pFsBlock, &newBlockAddr);
    ifsFlashTracingEnd(pFs, NULL, IFS_BLOCK_SIZE, status, pFlashAccess);
    if (status != FS_OK)
        goto out;

    // New block is written and old block ilwalidated on disk.
    // Update iterator and map structures to reflect this.

    iteratorMove(pIter, newBlockAddr);
    lwrBlock = iteratorLwrrent(pIter);


    // update file size

    // All blocks are fully utilized except the last block.
    // If last block, trim file size and add new block size.

    status = ifsMapGet(pFs->pMap, lwrBlock, &nextBlock);
    if (status != FS_OK)
        goto out;

    if (nextBlock == IFS_MAP_ENTRY_LISTEND)
    {
        LwU32 trim = pFile->size % IFS_BLOCK_DATA_SIZE;
        if (trim == 0)
            trim = IFS_BLOCK_DATA_SIZE;

        pFile->size = pFile->size - trim + pFsBlock->header.size;
    }

out:
    return status;
}

static FS_STATUS
ifsFileWrite
(
    IFS *pFs,
    IFS_FILE *pFile,
    LwU32 fileOffset,
    LwU32 size,
    LwU8 *pInBuffer
)
{
    IFS_FILE_ITERATOR iter;
    IFS_BLOCK *pFsBlock = NULL;
    FS_STATUS status;
    LwU32 bytesToWrite = size;
    LwU32 numBlocks;
    LwU32 i;

    if (size == 0)
        return FS_OK;

    if (pFile->firstBlock == IFS_MAP_ENTRY_LISTEND)
    {
        // File has no current blocks. This should never happen.
        return FS_ERR_ILWALID_STATE;
    }

    pFsBlock = ifsBufferBorrow(pFs);
    if (pFsBlock == NULL)
        return FS_ERR_ILWALID_STATE;

    iteratorInit(&iter, pFile, pFs->pMap);
    pFile->pIterator = &iter;

    // Step ahead to the desired block.
    // Find starting block.
    numBlocks = fileOffset / IFS_BLOCK_DATA_SIZE;
    status = iteratorSeek(&iter, numBlocks);
    if (status != FS_OK)
        goto out;

    while(bytesToWrite != 0)
    {
        IFS_BLOCK_HEADER *pHeader = NULL;
        LwU8 *pBlockData = NULL;
        LwU32 writeSize;
        LwU32 blockOffset;
        LwU32 lwrBlock;
        LwU8 fileOffsetInBlock;
        LwBool bChanged = LW_FALSE;
        LwBool bAllocReqd = LW_FALSE;

        lwrBlock = iteratorLwrrent(&iter);
        if (lwrBlock == IFS_MAP_ENTRY_LISTEND)
        {
            status = FS_ERR_ILWALID_ARG;
            goto out;
        }
        if (!MAP_IS_DATA_BLOCK(lwrBlock))
        {
            status = FS_ERR_ILWALID_STATE;
            goto out;
        }

        fsMemSet(pFsBlock, 0, IFS_BLOCK_SIZE);

        blockOffset = blockAddrToFlashOffset(lwrBlock);
        status = readFlash(pFs->pFlashAccessor, blockOffset, IFS_BLOCK_SIZE, (void *)pFsBlock);
        if (status != FS_OK)
            goto out;

        pHeader = &pFsBlock->header;

        // sanity check block's data
        if ((pHeader->offset == 0xFFFF) || (fileOffset < pHeader->offset))
        {
            FS_PRINTF(ERROR, "%s: Invalid block offset:0x%x at flash offset: 0x%x\n",
                      __FUNCTION__, pHeader->offset, blockOffset);
            status = FS_ERR_ILWALID_STATE;
            goto out;
        }

        if (ifsIsHeaderCorrupt(pHeader))
        {
            FS_PRINTF(ERROR, "%s: Old block header is corrupted at offset: 0x%x!\n",
                      __FUNCTION__, blockOffset);
            status = FS_ERR_ILWALID_STATE;
            goto out;
        }

        /* offset in the block where file's data starts */
        fileOffsetInBlock = GET_OFFSET_WITHIN_BLOCK(pHeader, fileOffset);
        if ((pHeader->size == 0) || (pHeader->size < fileOffsetInBlock))
        {
            FS_PRINTF(ERROR,
                      "%s: Invalid header size:0x%x with fileOffsetInBlock:0x%x\n",
                      __FUNCTION__, pHeader->size, fileOffsetInBlock);
            status = FS_ERR_ILWALID_STATE;
            goto out;
        }

        writeSize = FS_MIN(bytesToWrite, ((LwU32)pHeader->size - fileOffsetInBlock));
        pBlockData = &pFsBlock->data[fileOffsetInBlock];
        for (i = 0; i < writeSize; i++)
        {
            LwU8 newByte;
            LwU8 lwrrByte;

            //
            // if pHeader happens to contain garbage data, there is chance
            // that we may accidentally overflow the allocated buffer
            //
            if ((i + fileOffsetInBlock) >= IFS_BLOCK_DATA_SIZE)
            {
                FS_PRINTF(ERROR,
                          "Trying to access past block data size. "
                          "The header likely contains a bad offset: %d fileOffset: %d\n",
                          pHeader->offset, fileOffset);
                status = FS_ERR_ILWALID_STATE;
                goto out;
            }

            lwrrByte = pBlockData[i];
            newByte = pInBuffer[i];
            if ((newByte ^ lwrrByte) != 0)
            {
                bChanged = LW_TRUE;

                // if any bit has changed from 0 to 1 then new block allocation is
                // required for block that doesn't require checksum.
                if (((newByte ^ lwrrByte) & newByte) != 0)
                {
                    bAllocReqd = LW_TRUE;
                }

                // update block data buffer
                pBlockData[i] = pInBuffer[i];
            }
        }

        if (bChanged)
        {
            if (IFS_FLAGS_IS_CHECKSUM_REQUIRED(pHeader->flags))
                bAllocReqd = LW_TRUE;

            if (bAllocReqd)
            {
                status = ifsFileWriteJournaled(pFs, &iter, pFsBlock);
                if (status != FS_OK)
                {
                    FS_PRINTF(ERROR, "Writing block to file failed, rc:%d\n", status);
                    goto out;
                }
            }
            else
            {
                status = ifsBlockWrite(pFs, lwrBlock, pFsBlock);
                if (status != FS_OK)
                {
                    FS_PRINTF(ERROR, "Writing block to file failed, rc:%d\n", status);
                    goto out;
                }
            }
        }

        fileOffset += writeSize;
        pInBuffer += writeSize;
        bytesToWrite -= writeSize;

        iteratorNext(&iter);
    }

out:
    ifsBufferReturn(pFs, pFsBlock);
    pFile->pIterator = NULL;
    return status;
}

FS_STATUS
inforomFsWrite
(
    IFS *pFs,
    const char pName[],
    LwU32 fileOffset,
    LwU32 size,
    LwU8 *pInBuffer
)
{
    IFS_FILE *pFile = NULL;
    FS_STATUS status;

    FS_ASSERT_AND_RETURN(pName != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pInBuffer != NULL, FS_ERR_ILWALID_ARG);
    FS_ASSERT_AND_RETURN(pFs->bEnableWrites, FS_ERR_ILWALID_STATE);

    status = ifsFileGet(pFs, pName, LW_FALSE, &pFile);
    if (status != FS_OK)
    {
        return status;
    }

    // make sure we are not going over
    if (pFile->size < fileOffset + size)
    {
        FS_PRINTF(ERROR, "%s: Write out of bounds, size:0x%x\n",
                  __FUNCTION__, fileOffset + size);
        return FS_ERR_ILWALID_ARG;
    }

    if (pFile->type == IFS_FILE_TYPE_RO)
    {
        FS_PRINTF(ERROR, "%s: Trying to write to a RO file!\n", __FUNCTION__);
        status = FS_ERR_NOT_SUPPORTED;
    }
    else
    {
        status = ifsFileWrite(pFs, pFile, fileOffset, size, pInBuffer);
    }

    return status;
}

FS_STATUS
inforomFsGetFileSize(IFS *pFs, const char pName[], LwU32 *pSize)
{
    FS_STATUS status;
    IFS_FILE *pFile = NULL;

    status = ifsFileGet(pFs, pName, LW_FALSE, &pFile);
    if (status != FS_OK)
    {
        return status;
    }

    *pSize = pFile->size;

    return FS_OK;
}

FS_STATUS
inforomFsGetFileType(IFS *pFs, const char pName[], LwU32 *pType)
{
    FS_STATUS status;
    IFS_FILE *pFile = NULL;

    status = ifsFileGet(pFs, pName, LW_FALSE, &pFile);
    if (status != FS_OK)
    {
        return status;
    }

    *pType = pFile->type;

    return FS_OK;
}

FS_STATUS
inforomFsGetFileList(IFS *pFs, char (*pList)[IFS_FILE_NAME_SIZE], LwU32 *pListSize)
{
    LwU32 i;
    LwU32 listIndex = 0;

    for (i = 0; i < IFS_MAX_FILES; i++)
    {
        IFS_FILE *pFile;
        LwU32 j;

        pFile = &pFs->files[i];
        if (!FILE_USABLE(pFile))
            continue;

        for (j = 0; j < IFS_FILE_NAME_SIZE; j++)
        {
            pList[listIndex][j] = pFile->name[j];
        }
        listIndex++;
    }

    *pListSize = listIndex;
    return FS_OK;
}

/*!
 * @brief Check if the files' names match.
 */
static LwBool
ifsFileNameMatches
(
    const char pFileName[IFS_FILE_NAME_SIZE],
    const char pName[IFS_FILE_NAME_SIZE]
)
{
    if ((pFileName[0] == pName[0]) &&
        (pFileName[1] == pName[1]) &&
        (pFileName[2] == pName[2]))
    {
        return LW_TRUE;
    }

    return LW_FALSE;
}

static FS_STATUS
ifsLink(IFS *pFs)
{
    LwU16 *pFileBlockList = NULL;
    LwU32 i;
    FS_STATUS status = FS_OK;

    FS_ASSERT_AND_RETURN(pFs != NULL, FS_ERR_ILWALID_ARG);

    status = fsAllocMem((void **)&pFileBlockList, pFs->numBlocks * sizeof(LwU16));
    FS_ASSERT_AND_RETURN(status == FS_OK, status);

    for (i = 0; i < IFS_MAX_FILES; i++)
    {
        IFS_FILE *pFile = &pFs->files[i];
        if (pFile->type != IFS_FILE_TYPE_ILWALID)
        {
            fsMemSet(pFileBlockList, 0, pFs->numBlocks * sizeof(LwU16));
            status = ifsFileLink(pFs, pFile, pFileBlockList);
            if (status != FS_OK)
            {
                FS_PRINTF(ERROR, "Failed to link file, rc: %d\n", status);
                break;
            }
        }
    }

    fsFreeMem(pFileBlockList);
    return status;
}

// ifsFileLink() gathers the set of blocks associated with a file into an
// list in pFileBlockList, ordered by index into the file. Duplicate
// blocks are pruned, and holes in the file are detected.
// The list is then written as a linked list into the block map.

static FS_STATUS
ifsFileLink(IFS *pFs, IFS_FILE *pFile, LwU16 *pFileBlockList)
{
    IFS_BLOCK_HEADER mediaHeader;
    LwU32 fileBlocks = 0;
    LwU32 lwrBlock;
    LwU32 prevBlock;
    LwU32 fileLwrsor;
    FS_STATUS status;
    LwBool bEmptySeen;
    LwBool bMissingBlocks;

    // Scan the block table, copy nodes into temp array and remove stale nodes.

    // This function uses the value zero in pFileBlockList to indicate no block
    // exists in the current index. Zero cannot be a valid file block value,
    // it is the address of the superblock.

    for (lwrBlock = 0; lwrBlock < pFs->numBlocks; lwrBlock++)
    {
        MAP_LINK_ENTRY linkEntry;
        MAP_LINK_ENTRY prevEntry;
        LwU32 index;
        LwU32 size = 0;

        // The bitmap is set for map entries that contain MAP_LINK_ENTRY values.
        if (!ifsBitmapIsSet(pFs->pBitmap, lwrBlock))
            continue;

        status = ifsMapGet(pFs->pMap, lwrBlock, &linkEntry.raw);
        FS_ASSERT_AND_RETURN(status == FS_OK, FS_ERR_ILWALID_STATE);

        if (linkEntry.p.file != pFile->id)
            continue;

        ifsBitmapClear(pFs->pBitmap, lwrBlock, 1);

        status = ifsBlockReadAndVerify(pFs, lwrBlock, &size);
        if (status == FS_ERR_ILWALID_DATA)
        {
            // Current block failed verification, mark for discard.
            ifsMapSet(pFs->pMap, lwrBlock, IFS_MAP_ENTRY_DISCARD);
            continue;
        }
        else if (status != FS_OK)
        {
            return status;
        }

        index = linkEntry.p.fileIndex;
        prevBlock = pFileBlockList[index];
        if (prevBlock == 0)
        {
            pFileBlockList[index] = (LwU16)lwrBlock;
            continue;
        }

        // Another version of this block exists.

        status = ifsMapGet(pFs->pMap, prevBlock, &prevEntry.raw);
        FS_ASSERT_AND_RETURN(status == FS_OK, FS_ERR_ILWALID_STATE);

        if (linkEntry.p.version > prevEntry.p.version)
        {
            // Replace previous block with new version.
            pFileBlockList[index] = (LwU16)lwrBlock;
            // Mark previous block for discard
            status = ifsMapSet(pFs->pMap, prevBlock, IFS_MAP_ENTRY_DISCARD);
        }
        else
        {
            // Keep previous block. Mark current block for discard.
            status = ifsMapSet(pFs->pMap, lwrBlock, IFS_MAP_ENTRY_DISCARD);
        }
        FS_ASSERT_AND_RETURN(status == FS_OK, FS_ERR_ILWALID_STATE);
    }

    // Compute how many blocks were added.
    // Scan for holes in the file due to missing blocks.
    bMissingBlocks = LW_FALSE;
    bEmptySeen = LW_FALSE;
    fileBlocks = 0;
    for (fileLwrsor = 0; fileLwrsor < pFs->numBlocks; fileLwrsor++)
    {
        if (pFileBlockList[fileLwrsor] == 0)
        {
            bEmptySeen = LW_TRUE;
        }
        else
        {
            if (bEmptySeen)
                bMissingBlocks = LW_TRUE;
            fileBlocks++;
        }
    }

    if (fileBlocks == 0)
    {
        pFile->type = IFS_FILE_TYPE_ILWALID;
        return FS_OK;
    }

    if (bMissingBlocks)
    {
        // File has missing blocks.
        FS_PRINTF(ERROR, "File %c%c%c is corrupted.\n",
                  pFile->name[0], pFile->name[1], pFile->name[2]);

        if (pFs->version < 3)
        {
            LwU32 index = 0;

            // fsv2 compat
            // keep the file but flag as inaccessible

            pFile->type = IFS_FILE_TYPE_ZOMBIE;

            // compact the nodes for our linked list. This throws off the file offsets,
            // but this file should never be read by this code, so it doesn't matter.

            for (fileLwrsor = 0; fileLwrsor < pFs->numBlocks; fileLwrsor++)
            {
                if (pFileBlockList[fileLwrsor] == 0)
                    continue;

                pFileBlockList[index] = pFileBlockList[fileLwrsor];
                index++;
            }
            // fall through to normal file creation.
        }
        else
        {
            LwU32 firstBlock = IFS_MAP_ENTRY_ILWALID;

            // Remove file
            pFile->type = IFS_FILE_TYPE_ILWALID;

            // Move all blocks to the corrupt list.
            prevBlock = IFS_MAP_ENTRY_ILWALID;
            for (fileLwrsor = 0; fileLwrsor < pFs->numBlocks; fileLwrsor++)
            {
                lwrBlock = pFileBlockList[fileLwrsor];

                if (lwrBlock == 0)
                    continue;

                if (prevBlock == IFS_MAP_ENTRY_ILWALID)
                {
                    firstBlock = lwrBlock;
                    prevBlock = lwrBlock;
                    continue;
                }

                status = ifsMapSet(pFs->pMap, prevBlock, lwrBlock);
                if (status != FS_OK)
                    return status;
                prevBlock = lwrBlock;
            }
            if (prevBlock != IFS_MAP_ENTRY_ILWALID)
            {
                // Link tail of list to existing corrupt block list head.
                status = ifsMapSet(pFs->pMap, prevBlock, pFs->corruptList);
                if (status != FS_OK)
                    return status;

                // Point corrupt list head at head of list.
                pFs->corruptList = firstBlock;
            }
            return FS_OK;
        }
    }

    pFile->firstBlock = pFileBlockList[0];
    pFile->size = 0;

    // Colwert temp array into linked list in block table.

    for (fileLwrsor = 0; fileLwrsor < fileBlocks - 1; fileLwrsor++)
    {
        status = ifsMapSet(pFs->pMap, pFileBlockList[fileLwrsor], pFileBlockList[fileLwrsor + 1]);
        if (status != FS_OK)
            return status;
        pFile->size += IFS_BLOCK_DATA_SIZE;
    }
    lwrBlock = pFileBlockList[fileLwrsor];
    status = ifsMapSet(pFs->pMap, lwrBlock, IFS_MAP_ENTRY_LISTEND);
    if (status != FS_OK)
        return status;

    // Read last block header to find its size.
    status = ifsBlockReadHeader(pFs, lwrBlock, &mediaHeader);
    if (status != FS_OK)
        return status;
    pFile->size += mediaHeader.size;

    return FS_OK;
}

static FS_STATUS
ifsFileGet(IFS *pFs, const char pName[], LwBool bHidden, IFS_FILE **ppFile)
{
    LwU32 i;

    FS_ASSERT_AND_RETURN(pName != NULL, FS_ERR_ILWALID_ARG);

    for (i = 0; i < IFS_MAX_FILES; i++)
    {
        IFS_FILE *pFile = &pFs->files[i];

        if (pFile->type == IFS_FILE_TYPE_ILWALID)
            continue;
        if ((!bHidden) && (pFile->type == IFS_FILE_TYPE_ZOMBIE))
            continue;

        if (ifsFileNameMatches(pFile->name, pName))
        {
            *ppFile = pFile;
            return FS_OK;
        }
    }

    return FS_ERR_FILE_NOT_FOUND;
}

static FS_STATUS
ifsFileConstruct
(
    IFS *pFs,
    const char pName[IFS_FILE_NAME_SIZE],
    LwU32 type,
    IFS_FILE **ppFile
)
{
    FS_STATUS status = FS_OK;
    IFS_FILE *pFile = NULL;
    LwU32 id;

    // find unused file
    for (id = 0; id < IFS_MAX_FILES; id++)
    {
        if (pFs->files[id].type == IFS_FILE_TYPE_ILWALID)
            break;
    }

    if (id == IFS_MAX_FILES)
        return FS_ERR_NO_FREE_MEM;

    pFile = &pFs->files[id];

    fsMemCopy(pFile->name, pName, IFS_FILE_NAME_SIZE);
    pFile->name[IFS_FILE_NAME_SIZE] = '\0';
    pFile->type = type;
    pFile->id = id;
    *ppFile = pFile;

    return status;
}

/*!
 * @brief Checks if the reserved region is zeroed out.
 *      If the region is non-zero, the block is corrupted.
 */
static LwBool
ifsIsHeaderCorrupt(IFS_BLOCK_HEADER *pHeader)
{
    LwU32 size = sizeof(pHeader->reserved);
    LwU8 expectedPattern = 0x00;
    LwBool bCorrupted = LW_FALSE;
    LwU32 i;

    for (i = 0; i < size; i++)
    {
        if (pHeader->reserved[i] != expectedPattern)
        {
            bCorrupted = LW_TRUE;
            break;
        }
    }

    return bCorrupted;
}

static FS_STATUS
ifsBlockScan(IFS *pFs, LwU32 blockAddr)
{
    IFS_BLOCK_HEADER header;
    MAP_LINK_ENTRY linkEntry;
    IFS_FILE *pFile = NULL;
    FS_STATUS status;

    if (blockAddr == 0)
    {
        // Mark superblock block as reserved.
        ifsMapSet(pFs->pMap, blockAddr, IFS_MAP_ENTRY_RESERVED);
        return FS_OK;
    }

    status = ifsBlockReadHeader(pFs, blockAddr, &header);
    if (status != FS_OK)
        return status;

    if (IFS_FLAGS_IS_CLEAN(header.flags))
    {
        ifsMapSet(pFs->pMap, blockAddr, IFS_MAP_ENTRY_CLEAN);
        return FS_OK;
    }

    // Detect corrupted header
    status = ifsBlockHeaderSanityCheck(&header);
    if (status != FS_OK)
    {
        // Header is corrupted but we don't know if the block is bad until we try writing.
        ifsMapSet(pFs->pMap, blockAddr, IFS_MAP_ENTRY_DISCARD);
        return FS_OK;
    }

    if (!IFS_FLAGS_IS_LWRRENT(header.flags))
    {
        // Obsolete block
        status = ifsMapSet(pFs->pMap, blockAddr, IFS_MAP_ENTRY_DISCARD);
        goto out;
    }

    status = ifsFileGet(pFs, header.name, LW_TRUE, &pFile);
    if (status != FS_OK)
    {
        // This is the first node we are encountering for this file so
        // create corresponding file.
        LwU32 type;

        type = IFS_FLAGS_IS_RW(header.flags) ? IFS_FILE_TYPE_RW : IFS_FILE_TYPE_RO;

        status = ifsFileConstruct(pFs, header.name, type, &pFile);
        if (status != FS_OK)
            goto out;
    }

    linkEntry.p.rw = IFS_FLAGS_IS_RW(header.flags) ? 1 : 0;
    linkEntry.p.file = (LwU8)pFile->id;
    linkEntry.p.version = header.version;
    linkEntry.p.fileIndex = header.offset / IFS_BLOCK_DATA_SIZE;

    ifsBitmapSet(pFs->pBitmap, blockAddr, 1);
    status = ifsMapSet(pFs->pMap, blockAddr, linkEntry.raw);

out:
    return status;
}
