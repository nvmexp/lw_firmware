/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/types.h"
#include "core/include/framebuf.h"

namespace Memory
{
    namespace Gddr
    {
        using RamVendorId   = FrameBuffer::RamVendorId;
        using RamProtocol   = FrameBuffer::RamProtocol;

        //!
        //! \brief A specific model of GDDR
        //!
        struct Model
        {
            RamVendorId vendorId    = RamVendorId::RamVendorUnknown;
            RamProtocol ramProtocol = RamProtocol::RamUnknown;
            string vendorName;
            string protocolName;

            Model() {}
            Model
            (
                RamVendorId vendorId,
                RamProtocol ramProtocol,
                string vendorName,
                string protocolName
            ) : vendorId(vendorId),
                ramProtocol(ramProtocol),
                vendorName(vendorName),
                protocolName(protocolName)
            {}
        };
    } // namespace Gddr
} // namespace Memory
