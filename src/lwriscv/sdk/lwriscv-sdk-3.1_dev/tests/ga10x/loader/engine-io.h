/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef ENGINEIO_H
#define ENGINEIO_H

#include <lwtypes.h>
#include "gpu-io.h"

typedef struct {
    const char *name;
    unsigned falconBase;
    unsigned riscvBase;
    int  pmc_bit; // -1 - not possible
    unsigned fbif_ctl; // address of FBIF_CTL register
    unsigned fbif_ctl2; // address of FBIF_CTL2 register
    // Non-set
} EngineInfo;

typedef struct EngineInstance
{
    Gpu *pGpu;
    EngineInfo *pInfo;
    LwU32 imemSize;
    LwU32 dmemSize;
    LwBool bOldNetlist;
} Engine;

Engine *engineGet(Gpu *pGpu, const char *name);
Engine *enginePut(Engine *pEngine);

LwU32 engineRead(Engine *pEngine, LwU32 reg);
void engineWrite(Engine *pEngine, LwU32 reg, LwU32 val);
#define falconRead engineRead
#define falconWrite engineWrite

LwU32 riscvRead(Engine *pEngine, LwU32 reg);
void riscvWrite(Engine *pEngine, LwU32 reg, LwU32 val);

// Resets engine *and* SE
LwBool engineReset(Engine *pEngine);
LwBool engineIsActive(Engine *pEngine);
LwBool engineIsInIcd(Engine *pEngine);

LwBool engineReadImem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer);
LwBool engineReadDmem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer);
LwBool engineWriteImem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer);
LwBool engineWriteDmem(Engine *pEngine, LwU32 offset, unsigned len, void *pBuffer);

LwU32 engineReadImemWord(Engine *pEngine, LwU32 offset);
LwU32 engineReadDmemWord(Engine *pEngine, LwU32 offset);
LwBool engineWriteImemWord(Engine *pEngine, LwU32 offset, LwU32 value);
LwBool engineWriteDmemWord(Engine *pEngine, LwU32 offset, LwU32 value);

#endif // ENGINEIO_H
