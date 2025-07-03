
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_limerock_dev.h"
#include "gpu/include/device_interfaces.h"

extern unique_ptr<Fuse> CreateLagunaFuse(TestDevice*);

namespace LwLinkDevIf
{
    //------------------------------------------------------------------------------
    //! \brief Abstraction for Limerock device
    //!
    class LagunaDev : public LimerockDev
    {
    public:
        LagunaDev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle,
            Device::LwDeviceId deviceId
        ) : LimerockDev(pLwSwitchLibif, i, handle, deviceId) {}
        virtual ~LagunaDev() = default;
    protected:
        RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
        RC HotReset(FundamentalResetOptions funResetOption) override;
        RC SetupFeatures() override
        {
            SetHasFeature(GPUSUB_SUPPORTS_FUSING_WO_DAUGHTER_CARD);
            SetHasFeature(GPUSUB_SUPPORTS_IFF_SW_FUSING);
            SetHasFeature(GPUSUB_HAS_MERGED_FUSE_MACROS);
            SetHasFeature(GPUSUB_SUPPORTS_LWLINK_OFF_TRANSITION);
            SetHasFeature(GPUSUB_LWLINK_L1_MODE_POR);
            SetHasFeature(GPUSUB_SUPPORTS_GEN5_PCIE);
            return RC::OK;
        }
    };

#if LWCFG(GLOBAL_LWSWITCH_IMPL_LS10)
    class LS10Dev final
        : public LagunaDev
        , public DeviceInterfaces<Device::LS10>::interfaces
    {
    public:
        LS10Dev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle
        ) : LagunaDev(pLwSwitchLibif, i, handle, Device::LS10) {}
        virtual ~LS10Dev() = default;
    protected:
        std::unique_ptr<Fuse> CreateFuse() override
        {
            return CreateLagunaFuse(this);
        }
        RC SetupBugs() override
        {
            SetHasBug(3464731);
            SetHasBug(3453601);
            return RC::OK;
        }
    private:
        string DeviceIdString() const override { return "LS10"; }
        Device::LwDeviceId GetDeviceId() const override { return Device::LS10; }
        Ism* GetIsm() override { return nullptr; }

        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }
    };

    class S000Dev final
        : public LagunaDev
        , public DeviceInterfaces<Device::S000>::interfaces
    {
    public:
        S000Dev
        (
            LibIfPtr pLwSwitchLibif,
            Id i,
            LibInterface::LibDeviceHandle handle
        ) : LagunaDev(pLwSwitchLibif, i, handle, Device::S000) {}
        virtual ~S000Dev() = default;
    private:
        string DeviceIdString() const override { return "S000"; }
        Device::LwDeviceId GetDeviceId() const override { return Device::S000; }
        Ism* GetIsm() override { return nullptr; }

        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }
    };
#endif
};
