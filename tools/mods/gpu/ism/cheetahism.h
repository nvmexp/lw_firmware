/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_TEGRAISM_H
#define INCLUDED_TEGRAISM_H

#include "core/include/platform.h"
#include "cheetah/include/teggpurgown.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpuism.h"

vector<Ism::IsmChain>* GetTegraIsmTable(GpuSubdevice *pGpu);
//------------------------------------------------------------------------------
// This class only provides CheetAh specific support for ISMs. Any GPU type of
// support must be provided by one of the GpuIsm classes.
// 
class TegraIsm : public GpuIsm
{
public:
    TegraIsm(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);
    virtual ~TegraIsm();

    bool    AllowIsmTrailingBit() override { return true; }
    bool    AllowIsmUnusedBits() override { return true; }

    // Global enable for all ISMs
    RC      EnableISMs(bool bEnable);
    RC      GetISMClkFreqKhz(UINT32 *clkSrcFreqKHz);
    bool    GetISMsEnabled() override { return m_bISMsEnabled; }
    UINT32  GetSupportedChainSubtypes() { return Ism::CHAIN_SUBTYPE_MASK; }

private:
    bool    m_bISMsEnabled = false;
    TegraGpuRailGateOwner m_GpuRailGate;
};


#define CREATE_TEGRA_ISM_FUNCTION(Class)                          \
    GpuIsm *Create##Class(GpuSubdevice *pGpu)                     \
    {                                                             \
        if (!Platform::IsTegra())                                 \
            return nullptr;                                       \
        return new Class(pGpu, GetTegraIsmTable(pGpu));  \
    }

#endif
