/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "falcon.h"

class TuringGspFalcon : public Falcon
{
public:
    TuringGspFalcon(GpuSubdevice *pDev)
        : Falcon(pDev)
    {
    }
    ~TuringGspFalcon() = default;

    virtual UINT32 GetEngineBase() override;
    virtual UINT32 *GetUCodeData() const override;
    virtual UINT32 *GetUCodeHeader() const override;
    virtual UINT32 GetPatchLocation() const override;
    virtual UINT32 GetPatchSignature() const override;
    virtual UINT32 *GetSignature(bool debug) const override;
    virtual UINT32 SetPMC(UINT32 pmc, UINT32 state) override;
    virtual void EngineReset() override;
};
