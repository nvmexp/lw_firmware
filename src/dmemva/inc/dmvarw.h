/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_RW_H
#define DMVA_RW_H

#include <lwtypes.h>

// SP implementations have different behavior with exceptions
// dmread/dmwrite implementations only use PA
#define N_READ_DMEM_IMPLS  9
#define N_WRITE_DMEM_IMPLS 9
#define N_READ_SP_IMPLS    9
#define N_WRITE_SP_IMPLS   8
#define N_READ_DMA_IMPLS   3
#define N_WRITE_DMA_IMPLS  3

#define N_READ_IMPLS       (N_READ_DMEM_IMPLS  + N_READ_SP_IMPLS  + N_READ_DMA_IMPLS )
#define N_WRITE_IMPLS      (N_WRITE_DMEM_IMPLS + N_WRITE_SP_IMPLS + N_WRITE_DMA_IMPLS)
#define N_ACCESS_IMPLS     (N_READ_IMPLS + N_WRITE_IMPLS)

void testCallRet(LwU32 addr, LwBool bInterrupt);

typedef enum
{
    RW_IMPL_DMEM = 0,
    RW_IMPL_SP,
    RW_IMPL_DMA,
    N_RW_IMPLS,
} RwImplType;

LwU32 readDmemImpl(  LwU8 *dest, LwU8 *src, LwU32 implId );
LwU32 writeDmemImpl( LwU8 *dest, LwU8 *src, LwU32 implId );
LwU32 readSpImpl(    LwU8 *dest, LwU8 *src, LwU32 implId );
LwU32 writeSpImpl(   LwU8 *dest, LwU8 *src, LwU32 implId );
LwU32 readDmaImpl(   LwU8 *dest, LwU8 *src, LwU32 implId );
LwU32 writeDmaImpl(  LwU8 *dest, LwU8 *src, LwU32 implId );

//
// Aggegate versions of the above functions for colwenience.
//

// Only reads from ldd.x, lddsp.x, pop, popm
LwU32 readImpl(      LwU8 *dest, LwU8 *src, LwU32 implId );
// Only writes from std.x, stdsp.x, push, pusm, pushma
LwU32 writeImpl(     LwU8 *dest, LwU8 *src, LwU32 implId );
// Everything
LwU32 accessImpl(    LwU8 *dest, LwU8 *src, LwU32 implId );

LwU32 nImplsInFamily(LwBool bRead, RwImplType implType);
LwU32 rwImplFamily(LwU8 *dest, LwU8 *src, LwU32 implId, LwBool bRead, RwImplType implType);

#endif
