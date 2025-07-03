/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "lwl_libif.h"
#include "core/include/utility.h"
#include "gpu/utility/lwlink/lwlinkdevif/lwl_ibmnpu_dev.h"
#include <map>

namespace LwLinkDevIf
{
    class IbmNpuLibIf;

    //------------------------------------------------------------------------------
    //! \brief Abstraction for IBM NPU P9'+ device
    //!
    class IbmP9PDev : public IbmNpuDev
                    , public DeviceInterfaces<Device::IBMP9P>::interfaces
    {
        public:
            IbmP9PDev
            (
                LibIfPtr pIbmNpuLibif,
                Id i,
                const vector<LibInterface::LibDeviceHandle> &handles
            ) : IbmNpuDev(pIbmNpuLibif, i, handles) { }
            virtual ~IbmP9PDev() { }

            RC Initialize() override;
            RC Shutdown() override;
        private:
            TestDevice* GetDevice() override { return this; }
            const TestDevice* GetDevice() const override { return this; }
            string DeviceIdString() const override { return "IBMP9P"; }
            Device::LwDeviceId GetDeviceId() const override { return Device::IBMP9P; }
            UINT32 GetMaxLinks() const override { return LwLink::GetMaxLinks(); }

            bool     m_Initialized = false;      //!< True if the device was initialized
    };
};
