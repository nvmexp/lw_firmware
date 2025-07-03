/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_MEM_H
#define DMVA_MEM_H

#include "dmvaselwrity.h"
#include "dmvacommon.h"  // contains BlockStatus declaration
#include "dmvarand.h"
#include <lwtypes.h>

#define BLOCK_WIDTH 8
#define BLOCK_SIZE (1 << BLOCK_WIDTH)

extern LwU32 fbBufBlkIdxDmem;
extern LwU32 fbBufBlkIdxImem;

LwU32 getDmemTagWidth(void);
LwU32 getImemTagWidth(void);
LwU32 dmemcVal(LwU32 address, LwBool bAincw, LwBool bAincr, LwBool bSetTag, LwBool bSetLvl,
               LwBool bVa);
BlockStatus getBlockStatus(LwU32 physBlockId);
void setBlockStatus(BlockStatus blockStatus, LwU32 addr, LwU32 physBlockId);
void resetBlockStatus(LwU32 blkId);
void resetAllBlockStatus(void);
LwU32 trimVa(LwU32 addr);
LwU32 trimTag(LwU32 tag);
HS_SHARED LwU32 getDmemSize(void);  // returns the size of physical dmem in units of 256 bytes
HS_SHARED LwU32 getImemSize(void);  // returns the size of physical imem in units of 256 bytes
LwU32 getDmemVaSize(void);          // returns the size of virtual dmem in units of 256 bytes
LwU32 getImemVaSize(void);          // returns the size of virtual dmem in units of 256 bytes
LwU32 maxVa(void);  // Maximum addressable byte in VA. One less than the number of bytes in VA.
void resetDmvactl(void);

// ctx is unchanged so you can verify with the same ctx later
void fillMem(RNGctx ctx, LwU8 *addr, LwU32 nBytes);

// returns the number of verified bytes
LwU32 verifyMem(RNGctx ctx, const LwU8 *addr, LwU32 nBytes);

#endif
