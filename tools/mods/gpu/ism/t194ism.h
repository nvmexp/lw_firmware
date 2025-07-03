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

#ifndef INCLUDED_T194ISM_H
#define INCLUDED_T194ISM_H
#include "tegraism.h"
//------------------------------------------------------------------------------
// This class inherits from the TUxxxGpuIsm, with the following differences:
// - Use ISM controller version 1 not version 4
// - Support for LW_ISM_MINI_2clk_dbg_v3 ISM
// - Support for LW_ISM_aging_v2 ISM
//------------------------------------------------------------------------------
class T194Ism : public TegraIsm
{
public:
    T194Ism(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);

private:
    RC GetNoiseMeasClk(UINT32 *pClkKhz) override;
    RC TriggerISMExperiment() override;
};
#endif

