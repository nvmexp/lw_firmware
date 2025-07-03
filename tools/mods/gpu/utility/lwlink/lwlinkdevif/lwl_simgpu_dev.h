/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWL_SIMGPU_DEV_H
#define INCLUDED_LWL_SIMGPU_DEV_H

#include "lwl_sim_dev.h"
#include "gpu/include/device_interfaces.h"
#include "gpu/include/testdevice.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for simulated lwlink devies
    class SimGpuDev final : public SimDev
                          , public DeviceInterfaces<Device::SIMGPU>::interfaces
    {
        public:
            SimGpuDev(Id i) : SimDev(i) { }
            ~SimGpuDev() { }

        private:
            // Make all public functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer

            // Functions sorted alphabetically
            string DeviceIdString() const override { return "SimGpu"; }
            RC GetAteIddq(UINT32 *pIddq, UINT32 *pVersion) override;
            RC GetAteRev(UINT32 *pAteRev) override;
            RC GetAteSpeedos(vector<UINT32> *pValues, UINT32 * pVersion) override;
            RC GetChipPrivateRevision(UINT32 *pPrivRev) override;
            RC GetChipRevision(UINT32 *pRev) override;
            RC GetChipSkuModifier(string *pStr) override;
            RC GetChipSkuNumber(string *pStr) override;
            RC GetChipTemps(vector<FLOAT32> *pTempsC) override;
            RC GetClockMHz(UINT32 *pClkMhz) override;
            Device::LwDeviceId GetDeviceId() const override { return Device::ILWALID_DEVICE; }
            RC GetFoundry(ChipFoundry *pFoundry) override;
            Type GetType(void) const override { return TYPE_LWIDIA_GPU; }
            string GetTypeString() const override { return "SimGpu"; }

        private:
            TestDevice* GetDevice() override { return this; }
            const TestDevice* GetDevice() const override { return this; }
    };
};

#endif

