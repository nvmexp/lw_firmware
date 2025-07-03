/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_LWL_SIMEBRIDGE_DEV_H
#define INCLUDED_LWL_SIMEBRIDGE_DEV_H

#include "lwl_sim_dev.h"
#include "gpu/include/device_interfaces.h"

namespace LwLinkDevIf
{
    //--------------------------------------------------------------------------
    //! \brief Abstraction for simulated lwlink devies
    class SimEbridgeDev final : public SimDev
                              , public DeviceInterfaces<Device::SIMEBRIDGE>::interfaces
    {
        public:
            SimEbridgeDev(Id i) : SimDev(i) { }
            ~SimEbridgeDev() { }

        private:
            // Make all public functions from the base class private
            // in order to forbid external access to this class except through
            // the base class pointer

            // Functions sorted alphabetically
            string DeviceIdString() const override { return "EBRIDGE"; }
            Device::LwDeviceId GetDeviceId() const override { return Device::SIMEBRIDGE; }
            Type GetType(void) const override { return TYPE_EBRIDGE; }
            bool IsSysmem() const override { return true; }

            string GetTypeString() const override { return "SimEbridge"; }

        private:
            TestDevice* GetDevice() override { return this; }
            const TestDevice* GetDevice() const override { return this; }
    };
};

#endif

