/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GAXXXISM_H
#define INCLUDED_GAXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// GA100 GPU ISM implementation.
class GAxxxGpuIsm : public GpuIsm
{
public:
    GAxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    ~GAxxxGpuIsm() override {}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

protected:
    bool    AllowIsmUnusedBits() override { return true; }
    RC      GetNoiseMeasClk(UINT32 *pClkKhz) override;

private:

};

#endif

