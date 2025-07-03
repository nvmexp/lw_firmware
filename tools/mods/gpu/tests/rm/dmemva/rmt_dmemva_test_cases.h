/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef DMEMVA_TEST_CASES_H
#define DMEMVA_TEST_CASES_H
#include "lwtypes.h"

class GpuSubdevice;

class FalconInfo
{
public:
    enum FALCON_INSTANCE {
        FALCON_SEC2 = 0,
        FALCON_PMU,
        FALCON_LWDEC,
        FALCON_GSP,
    };

    virtual FALCON_INSTANCE getEngineId(void) const = 0;
    virtual const char *getFalconName(void) const = 0;
    virtual LwU32 getEngineBase(void) const = 0;
    virtual LwU32 *getUcodeData(void) const = 0;
    virtual LwU32 *getUcodeHeader(void) const = 0;
    virtual LwU32 *getSignature(bool debug = true) const = 0;
    virtual LwU32 getPatchLocation(void) const = 0;
    virtual LwU32 getPatchSignature(void) const = 0;
    virtual LwU32 setPMC(LwU32 lwPmc, LwU32 state) const = 0;
    virtual void engineReset(GpuSubdevice* pSubdev) const = 0;

    static const FalconInfo *getFalconInfo(GpuSubdevice *pSubdev, enum FALCON_INSTANCE instance);
    static void ucodePatchSignature(LwU32 *pImg, LwU32 *pSig, LwU32 patchLoc);
private:
    static const FalconInfo *getPascalFalconInfo(enum FALCON_INSTANCE instance);
    static const FalconInfo *getVoltaFalconInfo(enum FALCON_INSTANCE instance);
    static const FalconInfo *getTuringFalconInfo(enum FALCON_INSTANCE instance);
};

#endif
