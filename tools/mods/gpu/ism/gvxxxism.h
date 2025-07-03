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

#ifndef INCLUDED_GVXXXISM_H
#define INCLUDED_GVXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// Volta+ GPU ISM implementation.
class GVxxxGpuIsm : public GpuIsm
{
public:
    GVxxxGpuIsm(GpuSubdevice *pGpu,vector<Ism::IsmChain>* pTable);
    virtual ~GVxxxGpuIsm(){}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);
protected:
    virtual bool    AllowIsmUnusedBits(){ return true; }
    virtual RC      GetNoiseMeasClk(UINT32 *pClkKhz);

private:

};

#endif

