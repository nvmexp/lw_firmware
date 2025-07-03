/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef IFS_BLOCK_H
#define IFS_BLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lwtypes.h"

// On-media structures.

#define IFS_BLOCK_FLAGS_CLEAN    (1u << 0)
#define IFS_BLOCK_FLAGS_LWRRENT  (1u << 1)
#define IFS_BLOCK_FLAGS_RW       (1u << 2)
#define IFS_BLOCK_FLAGS_NO_CSUM  (1u << 3)

#pragma pack(1)

typedef struct
{
    LwU8 flags;
    char name[3];
    LwU16 offset;
    LwU8 size;
    LwU8 version;
    LwU8 checksum;
    LwU8 reserved[7];
} IFS_BLOCK_HEADER;

#define IFS_BLOCK_HEADER_SIZE           16
#define IFS_BLOCK_SIZE                  128
#define IFS_BLOCK_DATA_SIZE             (IFS_BLOCK_SIZE - IFS_BLOCK_HEADER_SIZE)
#define IFS_BLOCK_CHECKSUM_OFFSET       8

typedef struct
{
    IFS_BLOCK_HEADER header;
    LwU8 data[IFS_BLOCK_DATA_SIZE];
} IFS_BLOCK;

typedef struct
{
    char fsType[4];
    LwU32 version;
    LwU32 fsSize;
    LwU8 reserved[116];
} IFS_SUPERBLOCK;

#define IFS_SUPERBLOCK_SIZE sizeof(IFS_SUPERBLOCK)

#pragma pack()

#ifdef __cplusplus
}
#endif

#endif /* IFS_BLOCK_H */
