/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_H
#define IFS_H

#ifdef __cplusplus
extern "C" {
#endif

// This is the internal inforom-fs common header
// The client API is in inforom.h

/* ------------------------ Includes --------------------------------------- */
#include "inforom_fs.h"
#include "inforom/block.h"
#include "inforom/flash.h"
#include "inforom/map.h"
#include "inforom/misc.h"

/* ------------------------ Macros ----------------------------------------- */
#define IFS_DEFAULT_SIZE       20480
#define IFS_SECTOR_SIZE        4096
#define IFS_VERSION            3
#define IFS_VERSION_MAX        IFS_VERSION
/* Garbage cleaning threshold is up to 2 sectors of clean space */
#define IFS_GC_SYNC_THRESHOLD  2
#define IFS_MAX_FILES          20

#define IFS_FILE_TYPE_ILWALID  0
#define IFS_FILE_TYPE_RO       1
#define IFS_FILE_TYPE_RW       2
#define IFS_FILE_TYPE_ZOMBIE   3

#define IFS_BLOCKS_PER_SECTOR  (IFS_SECTOR_SIZE / IFS_BLOCK_SIZE)
#define IFS_INITIAL_RESERVED_BLOCKS IFS_BLOCKS_PER_SECTOR

#define IFS_HASH_ENTRIES_COUNT 11

#define IFS_FLAGS_IS_LWRRENT(flags) ((flags & IFS_BLOCK_FLAGS_LWRRENT) ? LW_TRUE : LW_FALSE)

#define IFS_BLOCK_IS_LWRRENT(p) IFS_FLAGS_IS_LWRRENT(p->header.flags)

#define IFS_FLAGS_IS_CLEAN(flags) ((flags & IFS_BLOCK_FLAGS_CLEAN) ? LW_TRUE : LW_FALSE)

#define IFS_BLOCK_IS_CLEAN(p) IFS_FLAGS_IS_CLEAN(p->header.flags)

#define IFS_FLAGS_IS_RW(flags) ((flags & IFS_BLOCK_FLAGS_RW) ? LW_TRUE : LW_FALSE)

#define IFS_BLOCK_IS_RW(p) IFS_FLAGS_IS_RW(p->header.flags);

#define IFS_FLAGS_IS_CHECKSUM_REQUIRED(flags) \
    ((flags & IFS_BLOCK_FLAGS_NO_CSUM) ? LW_FALSE : LW_TRUE)

#define IFS_BLOCK_IS_CHECKSUM_REQUIRED(p) IFS_FLAGS_IS_CHECKSUM_REQUIRED(p->header.flags)

#define IFS_BLOCK_CONTAINS_OFFSET(pBlock, fileOffset) \
    (((fileOffset) >= (pBlock)->header.offset) &&     \
     ((fileOffset) < (pBlock)->header.offset + (pBlock)->header.size))

#define GET_OFFSET_WITHIN_BLOCK(pHeader, fileOffset) ((LwU8)((fileOffset) - (pHeader)->offset))

#define IFS_BORROW_BLOCKS 2

// File that is can be read by users
#define FILE_USABLE(f) (((f)->type == IFS_FILE_TYPE_RO) || ((f)->type == IFS_FILE_TYPE_RW))

/* ------------------------ Datatypes -------------------------------------- */
typedef struct IFS_FILE IFS_FILE;

typedef struct
{
    LwU32 usedCount;
    LwU32 cleanCount;
    LwU32 discardCount;
    LwU32 reservedCount;
    LwU32 badCount;
} IFS_SECTOR_STATS;

struct IFS_FILE
{
    // Name of the file.
    char name[IFS_FILE_NAME_SIZE + 1];

    // Size of file in the flash memory.
    LwU32 size;

    // Head block address in block table linked list.
    LwU32 firstBlock;

    // Unique ID for lookup.
    LwU32 id;

    // File type.
    LwU32 type;

    // Registered iterator.
    void *pIterator;
};

struct IFS
{
    // Pointer to flash device accessor class.
    void *pFlashAccessor;

    // File system version.
    LwU32 version;

    // File system size in bytes.
    LwU32 size;

    // Total number of blocks in file system.
    LwU32 numBlocks;

    // Size of sector in bytes.
    LwU32 sectorSize;

    // Number of blocks in each sector.
    LwU32 blocksPerSector;

    // Threshold for garbage cleaning
    LwU32 gcSyncThreshold;

    // Garbage cleaning is allowed or not.
    LwBool bEnableGC;

    // Whether flash access tracing should do something.
    LwBool bEnableFlashTracing;

    // Address of sector lwrrently used for writing.
    LwU32 writeSector;

    // Address of sector last used for garbage collection.
    LwU32 gcSector;

    // List of blocks from corrupted files, retained for analysis.
    // These blocks should be deleted last when GC is out of blocks.
    LwU32 corruptList;

    // Whether writes are enabled for FS.
    LwBool bEnableWrites;

    // Block allocation map
    IFS_MAP *pMap;

    // Bitmap for temporary use
    LwU64 *pBitmap;

    // Scratch blocks for borrow allocator
    void *pBorrowBuffer[IFS_BORROW_BLOCKS];
    LwU8 borrowBufferBase[IFS_BORROW_BLOCKS * IFS_BLOCK_SIZE];

    // Array of the files in file system.
    IFS_FILE files[IFS_MAX_FILES];

#ifdef FS_OPTION_FLASH_TRACING
    // Trace of flash accesses
    struct
    {
        INFOROM_FLASH_ACCESS firstEntries[FLASH_ACCESS_ARRAY_SIZE];
        INFOROM_FLASH_ACCESS lastEntries[FLASH_ACCESS_ARRAY_SIZE];
        LwU32 firstEntryIdx;
        LwU32 lastEntryIdx;
    } flashAccesses;
#endif
};

void ifsGetSectorStats(IFS *pFs, LwU32 sectorAddr, LwBool bClear, IFS_SECTOR_STATS *stats);

static inline LwU32 blockAddrToFlashOffset(LwU32 addr)
{
    return addr * IFS_BLOCK_SIZE;
}

FS_STATUS ifsGetBlockForWrite(IFS *pFs, LwU32 *pBlockAddr);


// Block operations

FS_STATUS ifsBlockRead(IFS *pFs, LwU32 blockAddr, IFS_BLOCK *pFsBlock);
FS_STATUS ifsBlockReadAndVerify(IFS *pFs, LwU32 blockAddr, LwU32 *pSize);
FS_STATUS ifsBlockReadHeader(IFS *pFs, LwU32 blockAddr, void *pHeader);
FS_STATUS ifsBlockHeaderSanityCheck(IFS_BLOCK_HEADER *);
FS_STATUS ifsBlockDataSanityCheck(IFS_BLOCK *pFsBlock, LwU8 *pChecksum);
FS_STATUS ifsBlockWrite(IFS *pFs, LwU32 blockAddr, void *pBlockBuffer);
FS_STATUS ifsBlockWriteJournaled(IFS *pFs, LwU32 lwrBlockAddr, IFS_BLOCK *pFsBlock,
                                 LwU32 *pNewBlockAddrOut);
FS_STATUS ifsBlockIlwalidate(IFS *pFs, LwU32 blockAddr);


// Reusable block buffer

void *ifsBufferBorrow(IFS *pFs);
void ifsBufferReturn(IFS *pFs, void *pPtr);

#ifdef __cplusplus
};
#endif

#endif
