/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef DMVA_SELWRITY_H
#define DMVA_SELWRITY_H

#include "dmvaattrs.h"
#include <stdarg.h>
#include <lwtypes.h>

#define HS_SHARED ATTR_OVLY(".ovl1Shared")

#define HS_ALIVE_MAGIC 0x39db97aa

#define LEVEL_MASK_NONE 0b000
#define LEVEL_MASK_ALL 0b111

#define SVEC_REG(base, size, s, e) ((base >> 8) | ((size | s) << 16) | (e << 17))

typedef LwU32 LevelMask;

extern LwU32 g_signature[4] ATTR_OVLY(".data") ATTR_ALIGNED(16);
extern LwU32 *g_pSignature;

typedef enum
{
    SEC_MODE_NS = 0,
    SEC_MODE_LS1 = 1,
    SEC_MODE_LS2 = 2,
    SEC_MODE_HS = 3,
    N_SEC_MODES,
} SecMode;

typedef enum
{
    HS_ALIVE = 0,         // (void)
    HS_SET_BLK_LVL,       // (LwU32 physicalBlockID, LwU32 newSelwrityLevel)
    HS_SET_BLK_LVL_IMPL,  // (LwU32 physicalBlockID, LwU32 newSelwrityLevel, DmInstrImpl impl)
    // SET_LVL can switch to NS/LS (level 0, 1, 2), but not HS (level 3)
    HS_SET_LVL,  // (LwU32 newUcodeSelwrityLevel)
    HS_ISSUE_DMA,
    HS_CSB_WR,   // (LwU32 *addr, LwU32 data)
    HS_CSB_RD,   // (LwU32 *addr)
    N_HS_FUNCS,  // Just a marker of the end of the enum.
} HsFuncId;

// Ucode with security level x can add permissions from ucode with security
// level y iff (addPermissions[x][y] == 1) and (x == 3 or blockMask[x] == 1).
const extern LwBool addPermissions[4][3];

// Ucode with security level x can remove permissions from ucode with security
// level y iff (removePermissions[x][y] == 1) and (x == 3 or blockMask[x] == 1).
const extern LwBool removePermissions[4][3];

SecMode getUcodeLevel(void);
LwU32 callHS(HsFuncId fId, ...);

LwU32 getExpectedMask(LwU32 oldMask, LwU32 attemptedMask, SecMode ucodeLevel, LwBool bUsingDmread);

void setDmemSelwrityExceptions(LwBool bEnable);

#endif
