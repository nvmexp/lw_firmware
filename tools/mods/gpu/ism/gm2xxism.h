/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017, 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_GM2XXISM_H
#define INCLUDED_GM2XXISM_H

#ifndef INCLUDED_GMXXXISMISM_H
#include "gpu/include/gpuism.h"
#endif

//------------------------------------------------------------------------------
class GM2xxGpuIsm : public GpuIsm
{
public:
    GM2xxGpuIsm(GpuSubdevice *pGpu,vector<Ism::IsmChain> *pTable);
    virtual ~GM2xxGpuIsm(){}

    static vector<IsmChain> * GetTable(GpuSubdevice *pGpu);

private:

    RC GetNoiseMeasClk      //!< Get the slowest clock that could be running a noise circuit
    (
        UINT32 *pClkKhz     //!< address of location to place the clock value in KHz
    );
};

#endif

