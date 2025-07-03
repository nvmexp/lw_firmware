/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "tegraism.h"

class T234Ism : public TegraIsm
{
    public:
        T234Ism(GpuSubdevice *pGpu, vector<Ism::IsmChain>* pTable);

    private:
        RC GetNoiseMeasClk(UINT32 *pClkKhz) override;
        RC TriggerISMExperiment() override;
};
