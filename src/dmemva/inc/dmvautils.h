/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_UTILS_H
#define DMVA_UTILS_H

#include "dmvaselwrity.h"
#include <lwtypes.h>
#include <lwmisc.h>

#define SUPPRESS_UNUSED_WARNING(var) ((void)(var))

#define CAT_I(a, b) a##b
#define CAT(a, b) CAT_I(a, b)

#define POW2(x) BIT(x)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

// align down to nearest multiple of alignment
LwU32 align(LwU32 addr, LwU32 alignment);
// align up to nearest multiple of alignment
LwU32 alignUp(LwU32 addr, LwU32 alignment);
// is addr aligned to alignment?
LwBool aligned(LwU32 addr, LwU32 alignment);
LwU32 bitsToEncode(LwU32 x);
HS_SHARED LwBool isPow2(LwU32 x);
HS_SHARED LwU32 log_2(LwU32 x);

void wait(void);
HS_SHARED NORETURN void halt(void);

void rmBarrier(void);

// these functions syncronize to rm before a read/write
LwU32 rmRead32(void);
void rmWrite32(LwU32 data);

LwU32 rmRequestRead(LwU32 addr);
void rmRequestWrite(LwU32 addr, LwU32 data);

void stxAtUcodeLevel(LwU32 *addr, LwU32 data, SecMode ucodeLevel);
LwU32 ldxAtUcodeLevel(LwU32 *addr, SecMode ucodeLevel);

typedef enum
{
    IMPL_CSB = 0,
    IMPL_PRIV,
    N_REG_RW_IMPLS
} DmvaRegRwImpl;

#define DMVAREGWR_IMPL(reg, val, impl)            \
    do                                            \
    {                                             \
        if ((impl) == IMPL_CSB)                   \
        {                                         \
            DMVAREGWR(reg, val);                  \
        }                                         \
        else if ((impl) == IMPL_PRIV)             \
        {                                         \
            rmRequestWrite(PRIVADDR(reg), (val)); \
        }                                         \
    } while (LW_FALSE)

#define DMVAREGRD_IMPL(reg, impl) \
    ((impl) == IMPL_CSB ? DMVAREGRD(reg) : rmRequestRead(PRIVADDR(reg)))

void setStackOverflowProtection(LwU32 bound, LwBool bEnable);
void enableDMEMaperture(void);
void setupCtxDma(void);
void programDmcya(void);

#endif
