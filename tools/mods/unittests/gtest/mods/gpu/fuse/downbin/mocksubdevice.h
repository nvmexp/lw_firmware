
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

#include "gpu/include/gpusbdev.h"
#include "gmock/gmock.h"

class MockGpuSubdevice : public GpuSubdevice
{
public:
    explicit MockGpuSubdevice()
        : GpuSubdevice(Gpu::LwDeviceId::SOC, &m_PciDevInfo)
    {
    }

    MOCK_CONST_METHOD0(GetUGpuMask, UINT32());
    MOCK_CONST_METHOD0(GetMaxUGpuCount, UINT32());
    MOCK_CONST_METHOD1(GetUGpuGpcMask, INT32(UINT32 ugpu));

    TestDevice* GetDevice() override { return this; }
    const TestDevice* GetDevice() const override { return this; }

    void SetUGpuEnableMask(UINT32 mask)
    {
        using ::testing::Return;
        using ::testing::AtLeast;
        EXPECT_CALL(*this, GetUGpuMask())
            .Times(AtLeast(1))
            .WillRepeatedly(Return(mask));
    }

    void SetMaxUGpuCount(UINT32 count)
    {
        using ::testing::Return;
        using ::testing::AtLeast;
        EXPECT_CALL(*this, GetMaxUGpuCount())
            .Times(AtLeast(1))
            .WillRepeatedly(Return(count));
    }

    void SetUGpuUnitMasks(const vector<UINT32>& ugpuMasks)
    {
        using ::testing::_;
        using ::testing::Ilwoke;
        using ::testing::AtLeast;
        EXPECT_CALL(*this, GetUGpuGpcMask(_))
            .Times(AtLeast(1))
            .WillRepeatedly(Ilwoke(
                        [=](UINT32 ugpu)
                        {
                            return ugpuMasks[ugpu];
                        }));
    }
private:
    void SetupBugs() override { }
    void PrintMonitorInfo() override { }
    PciDevInfo m_PciDevInfo = {};
};
