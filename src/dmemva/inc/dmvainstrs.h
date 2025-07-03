/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_INSTRS_H
#define DMVA_INSTRS_H

#include <lwtypes.h>
#include <falcon-intrinsics.h>
#include "dmvaselwrity.h"
#include "dmvaassert.h"

typedef struct PACKED
{
    LwU8      zeros      : 8;  /*  7:0  */
    LwU16     tag        : 16; /* 23:8  */
    LevelMask levelMask  : 3;  /* 26:24 */
    LwU32     unused1    : 1;
    LwBool    valid      : 1;  /* 28:28 */
    LwBool    pending    : 1;  /* 29:29 */
    LwBool    dirty      : 1;  /* 30:30 */
    LwU32     unused2    : 1;
} dmblk_t;

typedef struct PACKED
{
    LwU32     blkIdx     : 20; /* 19:0  */
    LwBool    miss       : 1;  /* 20:20 */
    LwBool    multihit   : 1;  /* 21:21 */
    LwU32     unused1    : 2;  /* 23:22 */
    LevelMask levelMask  : 3;  /* 26:24 */
    LwU32     unused2    : 1;
    LwBool    valid      : 1;  /* 28:28 */
    LwBool    pending    : 1;  /* 29:29 */
    LwBool    dirty      : 1;  /* 30:30 */
    LwBool    outOfBound : 1;  /* 31:31 */
} dmtag_t;

typedef enum
{
    DM_IMPL_INSTRUCTION = 0,
    DM_IMPL_DMCTL,
    N_DM_IMPLS,
} DmInstrImpl;

HS_SHARED void dmva_dmlvl(LwU32 blk, LevelMask blockMask);
void dmlvlAtUcodeLevelImpl(LwU32 physBlk, LevelMask blockMask, SecMode ucodeLevel,
                           DmInstrImpl impl);

void dmilwImpl(LwU32 physBlk, DmInstrImpl impl);
void dmcleanImpl(LwU32 physBlk, DmInstrImpl impl);
dmblk_t dmblkImpl(LwU32 physBlk, DmInstrImpl impl);
dmtag_t dmtagImpl(LwU32 va, DmInstrImpl impl);
void setdtagImpl(LwU32 va, LwU32 physBlk, DmInstrImpl impl);

// compiles to just dmblk
UNUSED ALWAYS_INLINE static inline dmblk_t dmva_dmblk(LwU32 blockIndex)
{
    CASSERT(sizeof(dmblk_t) == sizeof(LwU32));
    union
    {
        LwU32 integer;
        dmblk_t structure;
    } d;
    falc_dmblk(&d.integer, blockIndex);
    return d.structure;
}

// compiles to just dmtag
UNUSED ALWAYS_INLINE static inline dmtag_t dmva_dmtag(LwU32 va)
{
    CASSERT(sizeof(dmtag_t) == sizeof(LwU32));
    union
    {
        LwU32 integer;
        dmtag_t structure;
    } d;
    falc_dmtag(&d.integer, va);
    return d.structure;
}

// compiles to just imread2
UNUSED ALWAYS_INLINE static inline unsigned dmva_imread2(unsigned int fbOffset,
                                                         unsigned int imemSizeOffset)
{
    unsigned int tag;
    asm volatile(" imread2 %0 %1 %2; "
                 : "=r"(tag)
                 : "r"(fbOffset), "r"(imemSizeOffset)
#ifndef _FALCON_NO_MEMORY_CLOBBER
                 : "memory"
#endif
                 );
    return (tag);
}

#endif
