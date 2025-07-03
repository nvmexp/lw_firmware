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

#ifndef INCLUDED_XP_MODS_KD_H
#define INCLUDED XP_MODS_KD_H

//! Stub class so dependent code can compile for QNX.
namespace Xp
{
    class MODSKernelDriver
    {
        public:
            MODSKernelDriver() = default;
            ~MODSKernelDriver() = default;
            /// Informs whether /dev/mods is open.
            /// If /dev/mods is not open, /dev/mem can still be open on CheetAh.
            bool           IsOpen() const;
            /// Informs whether /dev/mem is open.
            bool           IsDevMemOpen() const;
            /// Opens the MODS kernel driver - /dev/mods.
            /// On CheetAh, if /dev/mods is unavailable, it will open /dev/mem instead.
            RC             Open(const char* exePath);
            /// Closes the connection to the driver.
            RC             Close();
            /// Returns version of the MODS kernel driver (if open).
            UINT32         GetVersion() const;
            /// Returns kernel version, as reported by the driver.
            RC             GetKernelVersion(UINT64* pVersion);
            /// Issues an ioctl to the MODS kernel driver.
            int            Ioctl(unsigned long fn, void* param) const;
    };
}

namespace Xp
{
    MODSKernelDriver* GetMODSKernelDriver();
}

#endif
