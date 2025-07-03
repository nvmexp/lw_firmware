/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_sim_dev.h"
#include "gpu/include/device_interfaces.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for simulated lwlink devies
    class TrexDev : public SimDev
                  , public DeviceInterfaces<Device::TREX>::interfaces
    {
        public:
            TrexDev(Id i) : SimDev(i) {}
            virtual ~TrexDev() { }

        private:
            // Functions sorted alphabetically
            string DeviceIdString() const override { return "Trex"; }
            Device::LwDeviceId GetDeviceId() const override { return Device::TREX; }
            Type GetType(void) const override { return TYPE_TREX; }
            string GetTypeString() const override { return "TREX"; };

        private:
            TestDevice* GetDevice() override { return this; }
            const TestDevice* GetDevice() const override { return this; }
    };
};
