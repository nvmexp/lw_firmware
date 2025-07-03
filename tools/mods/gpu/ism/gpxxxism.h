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

#ifndef INCLUDED_GPXXXISM_H
#define INCLUDED_GPXXXISM_H

#include "gpu/include/gpuism.h"

//------------------------------------------------------------------------------
// Pascal+ GPU ISM implementation.
class GPxxxGpuIsm : public GpuIsm
{
public:
    GPxxxGpuIsm(GpuSubdevice *pGpu,vector<Ism::IsmChain>* pTable);
    virtual ~GPxxxGpuIsm(){}

    static vector<IsmChain>* GetTable(GpuSubdevice *pGpu);

protected:

private:
    //------------------------------------------------------------------------------
    // Allow unused bits at the start/end of the chain and between macros. This is
    // now going to be standard for the IEEE-1500 jtag process.
    virtual bool    AllowIsmUnusedBits(){ return true; }
    virtual RC      GetNoiseMeasClk(UINT32 *pClkKhz);
};

#endif

