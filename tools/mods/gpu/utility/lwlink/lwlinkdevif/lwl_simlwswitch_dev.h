/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWL_SIMLWSWITCH_DEV_H
#define INCLUDED_LWL_SIMLWSWITCH_DEV_H

#include "lwl_sim_dev.h"
#include "gpu/utility/thermalthrottling.h"
#include "gpu/include/device_interfaces.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for simulated lwlink devies
    class SimLwSwitchDev : public SimDev
                         , public ThermalThrottling
    {
        public:
            SimLwSwitchDev(Id i);
            virtual ~SimLwSwitchDev() { }

        private:
            // Make all public functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer

            // Functions sorted alphabetically
            RC GetAteDvddIddq(UINT32 *pIddq) override;
            RC GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override;
            RC GetAteRev(UINT32 *pAteRev) override;
            RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override;
            RC GetChipPrivateRevision(UINT32 *pPrivRev) override;
            RC GetChipRevision(UINT32 *pRev) override;
            RC GetChipSkuModifier(string *pStr) override;
            RC GetChipSkuNumber(string *pStr) override;
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
            RC GetClockMHz(UINT32 *pClkMhz) override;
            RC GetFoundry(ChipFoundry *pFoundry) override;
            Fuse* GetFuse() override { return nullptr; }
            Type GetType(void) const override { return TYPE_LWIDIA_LWSWITCH; }
            RC GetVoltageMv(UINT32 *pMv) override;
            bool IsEndpoint() const override { return false; }

            RC ReadHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                vector<UINT32> *pResult
            ) override { return OK; }

            RC WriteHost2Jtag
            (
                UINT32 Chiplet,
                UINT32 Instruction,
                UINT32 ChainLength,
                const vector<UINT32> &InputData
            ) override { return OK; }

            string GetTypeString() const override { return "SimLwSwitch"; }

        private: // Interface Implemenations
            // ThermalThrottling
            RC DoStartThermalThrottling(UINT32 onCount, UINT32 offCount) override { return OK; }
            RC DoStopThermalThrottling() override { return OK; }
    };

    class SimWillowDev final : public SimLwSwitchDev
                             , public DeviceInterfaces<Device::SIMLWSWITCH>::interfaces
    {
    public:
        SimWillowDev(Id i) : SimLwSwitchDev(i) {}
        virtual ~SimWillowDev() = default;
    private:
        string DeviceIdString() const override { return "SVNP01"; }
        Device::LwDeviceId GetDeviceId() const override { return Device::SVNP01; }

        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }
    };

    class SimLimerockDev final : public SimLwSwitchDev
                               , public DeviceInterfaces<Device::SIMLR10>::interfaces
    {
    public:
        SimLimerockDev(Id i) : SimLwSwitchDev(i) {}
        virtual ~SimLimerockDev() = default;
    private:
        string DeviceIdString() const override { return "LR10"; }
        Device::LwDeviceId GetDeviceId() const override
        {
#if LWCFG(GLOBAL_LWSWITCH_IMPL_LR10)
                return Device::LR10;
#else
                return  Device::ILWALID_DEVICE;
#endif
        }

        TestDevice* GetDevice() override { return this; }
        const TestDevice* GetDevice() const override { return this; }
    };
};

#endif

