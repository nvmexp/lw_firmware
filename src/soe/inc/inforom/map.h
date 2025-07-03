/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_MAP_H
#define IFS_MAP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"
#include "inforom/status.h"

// InfoROM block map
// Each entry in the table maps to one block on the storage medium.
// Files are maintained as linked lists, where each entry contains
// the index of the next entry or a terminator tag.
// Entries that are not part of a file contain tags that describe
// the state of the associated blocks.

// Map entry tags

// Clean - block is erased and ready to be written.
#define IFS_MAP_ENTRY_CLEAN          0xffffffff

// Reserved - block is not available for file operations.
// This is used for blocks in the superblock sector.
#define IFS_MAP_ENTRY_RESERVED       0xfffffffe

// Discard - block is obsolete and ready for erasure.
#define IFS_MAP_ENTRY_DISCARD        0xfffffffd

// Delete - effectively the same as discard, but ilwalidation
// of the media block is recommended.
#define IFS_MAP_ENTRY_DELETE         0xfffffffc

// Bad - This block experienced IO errors and should
// not be used.
#define IFS_MAP_ENTRY_BAD            0xfffffffb

// Invalid - This value should never appear in the table
// It is used as an indicator of an invalid entry.
#define IFS_MAP_ENTRY_ILWALID        0xfffffff1

// List End - This entry is the final entry in a linked
// list of entries.
#define IFS_MAP_ENTRY_LISTEND        0xfffffff0

// The lowest tag value, inclusive of list end.
// Values below this are valid data addresses.
#define IFS_MAP_ENTRY_TAGS           0xfffffff0

// Note that the entry containing IFS_MAP_ENTRY_LISTEND
// points to a valid data block.
#define MAP_IS_DATA_BLOCK(b) ((b) <= IFS_MAP_ENTRY_TAGS)
#define IFS_MAP_ENT_IS_TAG(b) (b >= IFS_MAP_ENTRY_TAGS)

typedef struct
{
    LwU32 numEntries;
    LwU32 entry[];
} IFS_MAP;

// A linear iterator that handles looping around the table.
typedef struct
{
    LwU32 lwrAddr;
    LwU32 firstAddr;
    IFS_MAP *pMap;
} IFS_MAP_ITERATOR;

// Create a block map with 'numEntries' entries.
FS_STATUS ifsMapCreate(LwU32 numEntries, IFS_MAP **pMapOut);

// Retrieve the value of entry at 'addr'
FS_STATUS ifsMapGet(IFS_MAP *pMap, LwU32 addr, LwU32 *pValue);

// Update the value of entry at 'addr'
FS_STATUS ifsMapSet(IFS_MAP *pMap, LwU32 addr, LwU32 value);

// Deallocate the map pointer.
void ifsMapDestroy(IFS_MAP *pMap);

// This must be the first call to begin usage of the iterator.
// Returns the block address of the first item starting from 'firstAddr'
LwU32 ifsMapIterStart(IFS_MAP_ITERATOR *pIter, IFS_MAP *pMap, LwU32 firstAddr);

// Subsequent iterations should call this, where 'stride' is the number of
// blocks to skip. If iterator has looped and reached the 'firstAddr' provided
// to ifsMapIterStart(), IFS_MAP_ENTRY_ILWALID will be returned.
LwU32 ifsMapIterNext(IFS_MAP_ITERATOR *pIter, LwU32 stride);


#ifdef __cplusplus
};
#endif

#endif // IFS_MAP_H
