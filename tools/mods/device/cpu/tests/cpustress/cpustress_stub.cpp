/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "cpustress.h"

class CpuStressStub : public CpuStress
{
public:
    CpuStressStub() = default;
    virtual ~CpuStressStub() = default;

    RC Setup() override { return RC::UNSUPPORTED_DEVICE; }
    RC Run() override { return RC::UNSUPPORTED_DEVICE; }
    RC Cleanup() override { return RC::UNSUPPORTED_DEVICE; }
};

//-----------------------------------------------------------------------------
unique_ptr<CpuStress> MakeCpuStress()
{
    return make_unique<CpuStressStub>();
}
