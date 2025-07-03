/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2013,2015, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GMXXXISM_H
#define INCLUDED_GMXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
class GMxxxGpuIsm : public GpuIsm
{
public:
    GMxxxGpuIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    virtual ~GMxxxGpuIsm(){}
    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);
};

#endif

