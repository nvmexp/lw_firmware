/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GHXXXISM_H
#define INCLUDED_GHXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// Ampere GPU ISM implementation.
class GHxxxGpuIsm : public GpuIsm
{
public:
    GHxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    virtual ~GHxxxGpuIsm(){}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

protected:
    bool    AllowIsmUnusedBits() override { return true; }
    // TODO: This implementation assumes that it is the same as Ampere, however we need
    // to verify this at some point.
    RC      GetISMClkFreqKhz(UINT32 *clkSrcFreqKHz) override;
    RC      GetNoiseMeasClk(UINT32 *pClkKhz) override;
    INT32   GetISMOutDivForFreqCalc(PART_TYPES PartType,INT32 outDiv) override;

private:

};

#endif

