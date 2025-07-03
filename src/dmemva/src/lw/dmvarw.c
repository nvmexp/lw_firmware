/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include "dmvarw.h"
#include "dmvaattrs.h"
#include "dmvadma.h"
#include "dmvaassert.h"
#include "dmvautils.h"
#include "dmvaregs.h"
#include "scp/inc/scp_internals.h"
#include <falcon-intrinsics.h>

void dummy(void);
asm("_dummy: ret;");
void dummyI(void);
asm("_dummyI: reti;");

void testCallRet(LwU32 addr, LwBool bInterrupt)
{
    UASSERT(aligned(addr, sizeof(LwU32)));

    if (bInterrupt)
    {
        LwU32 ie0 = 0;
        LwU32 ie1 = 0;
        LwU32 ie2 = 0;
        falc_sgetb_i((unsigned int *)&ie0, 16);
        falc_sgetb_i((unsigned int *)&ie1, 17);
        falc_sgetb_i((unsigned int *)&ie2, 18);
        falc_sputb_i(ie0, 20);
        falc_sputb_i(ie0, 21);
        falc_sputb_i(ie0, 22);
    }

    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, addr + sizeof(LwU32));

    if (bInterrupt)
    {
        dummyI();
    }
    else
    {
        dummy();
    }

    falc_wspr(SP, sp);
}

static inline ALWAYS_INLINE LwU32 dmva_stdB1(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.b %0 %1;" ::"r"(dest), "r"(*src));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_stdB2(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.b %0 %1 %2;" ::"r"(dest), "n"(0), "r"(*src));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_stdB3(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.b %0 %1 %2;" ::"r"(dest), "r"(0), "r"(*src));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_stdH1(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.h %0 %1;" ::"r"(dest), "r"(*(LwU16 *)src));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_stdH2(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.h %0 %1 %2;" ::"r"(dest), "n"(0), "r"(*(LwU16 *)src));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_stdH3(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.h %0 %1 %2;" ::"r"(dest), "r"(0), "r"(*(LwU16 *)src));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_stdW1(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.w %0 %1;" ::"r"(dest), "r"(*(LwU32 *)src));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_stdW2(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.w %0 %1 %2;" ::"r"(dest), "n"(0), "r"(*(LwU32 *)src));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_stdW3(LwU8 *dest, LwU8 *src)
{
    asm volatile("std.w %0 %1 %2;" ::"r"(dest), "r"(0), "r"(*(LwU32 *)src));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_lddB1(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.b %0 %1;" : "=r"(*dest) : "r"(src));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_lddB2(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.b %0 %1 %2;" : "=r"(*dest) : "r"(src), "r"(0));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_lddB3(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.b %0 %1 %2;" : "=r"(*dest) : "r"(src), "n"(0));
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_lddH1(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.h %0 %1;" : "=r"(*(LwU16 *)dest) : "r"(src));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_lddH2(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.h %0 %1 %2;" : "=r"(*(LwU16 *)dest) : "r"(src), "r"(0));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_lddH3(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.h %0 %1 %2;" : "=r"(*(LwU16 *)dest) : "r"(src), "n"(0));
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_lddW1(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.w %0 %1;" : "=r"(*(LwU32 *)dest) : "r"(src));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_lddW2(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.w %0 %1 %2;" : "=r"(*(LwU32 *)dest) : "r"(src), "r"(0));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_lddW3(LwU8 *dest, LwU8 *src)
{
    asm volatile("ldd.w %0 %1 %2;" : "=r"(*(LwU32 *)dest) : "r"(src), "n"(0));
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpB1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.b %0 %1;" ::"r"(*src), "r"(0));
    falc_wspr(SP, sp);
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpB2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.b %0 %1;" ::"r"(*src), "n"(0));
    falc_wspr(SP, sp);
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpH1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.h %0 %1;" ::"r"(*(LwU16 *)src), "r"(0));
    falc_wspr(SP, sp);
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpH2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.h %0 %1;" ::"r"(*(LwU16 *)src), "n"(0));
    falc_wspr(SP, sp);
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpW1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.w %0 %1;" ::"r"(*(LwU32 *)src), "r"(0));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_stdSpW2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest);
    asm volatile("stdsp.w %0 %1;" ::"r"(*(LwU32 *)src), "n"(0));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_push(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest + 4);
    asm volatile("push %0;" ::"r"(*(LwU32 *)src));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_pushm(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, dest + 4);
    asm volatile("mv.w a0 %0;" ::"r"(*(LwU32 *)src) : "a0");
    asm volatile("pushm a0;");
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpB1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.b %0 %1;" : "=r"(*dest) : "r"(0));
    falc_wspr(SP, sp);
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpB2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.b %0 %1;" : "=r"(*dest) : "n"(0));
    falc_wspr(SP, sp);
    return 1;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpH1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.h %0 %1;" : "=r"(*(LwU16 *)dest) : "r"(0));
    falc_wspr(SP, sp);
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpH2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.h %0 %1;" : "=r"(*(LwU16 *)dest) : "n"(0));
    falc_wspr(SP, sp);
    return 2;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpW1(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.w %0 %1;" : "=r"(*(LwU32 *)dest) : "r"(0));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_lddSpW2(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("lddsp.w %0 %1;" : "=r"(*(LwU32 *)dest) : "n"(0));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_pop(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("pop %0;" : "=r"(*(LwU32 *)dest));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_popm(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("popm a0 0;" :: : "a0");
    asm volatile("mv.w %0 a0;" : "=r"(*(LwU32 *)dest));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_popma(LwU8 *dest, LwU8 *src)
{
    LwU32 sp;
    falc_rspr(sp, SP);
    falc_wspr(SP, src);
    asm volatile("popma a0 0 0;" :: : "a0");
    asm volatile("mv.w %0 a0;" : "=r"(*(LwU32 *)dest));
    falc_wspr(SP, sp);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_dmwrite(LwU8 *dest, LwU8 *src)
{
    // first read the data (and put it into FB) using dmwrite
    dmvaIssueDma((LwU32)src, (LwU32)src, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_TO_FB,
                 IMPL_DM_RW);  // dmwrite
    // now read the data back from FB
    dmvaIssueDma((LwU32)dest, (LwU32)src, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_FROM_FB, IMPL_DM_RW);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_dmwrite2(LwU8 *dest, LwU8 *src)
{
    // first read the data (and put it into FB) using dmwrite
    dmvaIssueDma((LwU32)src, (LwU32)src, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_TO_FB,
                 IMPL_DM_RW2);  // dmwrite2
    // now read the data back from FB
    dmvaIssueDma((LwU32)dest, (LwU32)src, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_FROM_FB, IMPL_DM_RW2);
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_trapped_dmwrite(LwU8 *dest, LwU8 *src)
{
    // first read the data from src (and put it into SCP R5) using trapped dmwrite
    falc_scp_trap(TC_INFINITY);
    falc_trapped_dmwrite(falc_sethi_i((LwU32)src, SCP_R5));
    falc_dmwait();

    // now read the data back from SCP R5 using trapped dmread
    falc_trapped_dmread(falc_sethi_i((LwU32)dest, SCP_R5));
    falc_dmwait();
    falc_scp_trap(0);
    return 16;
}

static inline ALWAYS_INLINE LwU32 dmva_dmread(LwU8 *dest, LwU8 *src)
{
    // first put the src into fb
    dmvaIssueDma((LwU32)src, (LwU32)dest, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_TO_FB, IMPL_DM_RW);
    // now, write to dmem using dmread
    dmvaIssueDma((LwU32)dest, (LwU32)dest, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_FROM_FB,
                 IMPL_DM_RW);  // dmread
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_dmread2(LwU8 *dest, LwU8 *src)
{
    // first put the src into fb
    dmvaIssueDma((LwU32)src, (LwU32)dest, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_TO_FB, IMPL_DM_RW2);
    // now, write to dmem using dmread
    dmvaIssueDma((LwU32)dest, (LwU32)dest, sizeof(LwU32), 0, 0, 0, 0, DMVA_DMA_FROM_FB,
                 IMPL_DM_RW2);  // dmread2
    return 4;
}

static inline ALWAYS_INLINE LwU32 dmva_trapped_dmread(LwU8 *dest, LwU8 *src)
{
    // We need both trapped dmread and trapped dmwrite to move data
    // from DMEM to SCP to DMEM, so this has the same implementation
    return dmva_trapped_dmwrite(dest, src);
}

// TODO: MOVE ASSERTS TO THESE FUNCTIONS
// TODO: add read support for call, ret, reti, which read/write to/from SP?
LwU32 readDmemImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_READ_DMEM_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_lddB1(dest, src);
        case 1:
            return dmva_lddB2(dest, src);
        case 2:
            return dmva_lddB3(dest, src);
        case 3:
            return dmva_lddH1(dest, src);
        case 4:
            return dmva_lddH2(dest, src);
        case 5:
            return dmva_lddH3(dest, src);
        case 6:
            return dmva_lddW1(dest, src);
        case 7:
            return dmva_lddW2(dest, src);
        case 8:
            return dmva_lddW3(dest, src);
        default:
            halt();
    }
}

LwU32 writeDmemImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_WRITE_DMEM_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_stdB1(dest, src);
        case 1:
            return dmva_stdB2(dest, src);
        case 2:
            return dmva_stdB3(dest, src);
        case 3:
            return dmva_stdH1(dest, src);
        case 4:
            return dmva_stdH2(dest, src);
        case 5:
            return dmva_stdH3(dest, src);
        case 6:
            return dmva_stdW1(dest, src);
        case 7:
            return dmva_stdW2(dest, src);
        case 8:
            return dmva_stdW3(dest, src);
        default:
            halt();
    }
}

LwU32 readSpImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_READ_SP_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_lddSpB1(dest, src);
        case 1:
            return dmva_lddSpB2(dest, src);
        case 2:
            return dmva_lddSpH1(dest, src);
        case 3:
            return dmva_lddSpH2(dest, src);
        case 4:
            return dmva_lddSpW1(dest, src);
        case 5:
            return dmva_lddSpW2(dest, src);
        case 6:
            return dmva_pop(dest, src);
        case 7:
            return dmva_popm(dest, src);
        case 8:
            return dmva_popma(dest, src);
        default:
            halt();
    }
}

LwU32 writeSpImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_WRITE_SP_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_stdSpB1(dest, src);
        case 1:
            return dmva_stdSpB2(dest, src);
        case 2:
            return dmva_stdSpH1(dest, src);
        case 3:
            return dmva_stdSpH2(dest, src);
        case 4:
            return dmva_stdSpW1(dest, src);
        case 5:
            return dmva_stdSpW2(dest, src);
        case 6:
            return dmva_push(dest, src);
        case 7:
            return dmva_pushm(dest, src);
        default:
            halt();
    }
}

LwU32 readDmaImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_READ_DMA_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_dmwrite(dest, src);
        case 1:
            return dmva_dmwrite2(dest, src);
        case 2:
            return dmva_trapped_dmwrite(dest, src);
        default:
            halt();
    }
}

LwU32 writeDmaImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    UASSERT(implId < N_WRITE_DMA_IMPLS);
    switch (implId)
    {
        case 0:
            return dmva_dmread(dest, src);
        case 1:
            return dmva_dmread2(dest, src);
        case 2:
            return dmva_trapped_dmread(dest, src);
        default:
            halt();
    }
}

LwU32 readImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    if (implId < N_READ_DMEM_IMPLS)
    {
        return readDmemImpl(dest, src, implId);
    }
    else if ((implId -= N_READ_DMEM_IMPLS) < N_READ_SP_IMPLS)
    {
        return readSpImpl(dest, src, implId);
    }
    else if ((implId -= N_READ_SP_IMPLS) < N_READ_DMA_IMPLS)
    {
        return readDmaImpl(dest, src, implId);
    }
    else
    {
        halt();  // implId too large
    }
}

LwU32 writeImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    if (implId < N_WRITE_DMEM_IMPLS)
    {
        return writeDmemImpl(dest, src, implId);
    }
    else if ((implId -= N_WRITE_DMEM_IMPLS) < N_WRITE_SP_IMPLS)
    {
        return writeSpImpl(dest, src, implId);
    }
    else if ((implId -= N_WRITE_SP_IMPLS) < N_WRITE_DMA_IMPLS)
    {
        return writeDmaImpl(dest, src, implId);
    }
    else
    {
        halt();  // implId too large
    }
}

LwU32 accessImpl(LwU8 *dest, LwU8 *src, LwU32 implId)
{
    if (implId < N_READ_IMPLS)
    {
        return readImpl(dest, src, implId);
    }
    else if ((implId -= N_READ_IMPLS) < N_WRITE_IMPLS)
    {
        return writeImpl(dest, src, implId);
    }
    else
    {
        halt();  // implId too large
    }
}

LwU32 nImplsInFamily(LwBool bRead, RwImplType implType)
{
    if (bRead)
    {
        switch (implType)
        {
            case RW_IMPL_DMEM:
                return N_READ_DMEM_IMPLS;
            case RW_IMPL_SP:
                return N_READ_SP_IMPLS;
            case RW_IMPL_DMA:
                return N_READ_DMA_IMPLS;
            default:
                halt();
        }
    }
    else
    {
        switch (implType)
        {
            case RW_IMPL_DMEM:
                return N_WRITE_DMEM_IMPLS;
            case RW_IMPL_SP:
                return N_WRITE_SP_IMPLS;
            case RW_IMPL_DMA:
                return N_WRITE_DMA_IMPLS;
            default:
                halt();
        }
    }
}

LwU32 rwImplFamily(LwU8 *dest, LwU8 *src, LwU32 implId, LwBool bRead, RwImplType implType)
{
    if (bRead)
    {
        switch (implType)
        {
            case RW_IMPL_DMEM:
                return readDmemImpl(dest, src, implId);
            case RW_IMPL_SP:
                return readSpImpl(dest, src, implId);
            case RW_IMPL_DMA:
                return readDmaImpl(dest, src, implId);
            default:
                halt();
        }
    }
    else
    {
        switch (implType)
        {
            case RW_IMPL_DMEM:
                return writeDmemImpl(dest, src, implId);
            case RW_IMPL_SP:
                return writeSpImpl(dest, src, implId);
            case RW_IMPL_DMA:
                return writeDmaImpl(dest, src, implId);
            default:
                halt();
        }
    }
}
