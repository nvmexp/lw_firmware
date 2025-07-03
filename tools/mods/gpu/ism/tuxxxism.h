/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TUXXXISM_H
#define INCLUDED_TUXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// Turing GPU ISM implementation.
class TUxxxGpuIsm : public GpuIsm
{
public:
    TUxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    virtual ~TUxxxGpuIsm(){}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

protected:
    virtual bool    AllowIsmUnusedBits(){ return true; }
    virtual RC      GetNoiseMeasClk(UINT32 *pClkKhz);

private:

};

#endif

