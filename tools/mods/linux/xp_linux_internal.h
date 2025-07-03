/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2015,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef WE_NEED_LINUX_XP
#error "This file can only be included in Linux XP files"
#endif

namespace Xp
{
    namespace LinuxInternal
    {
        RC OpenDriver(const vector<string> &args);
        RC CloseModsDriver();
        RC AcpiDsmEval
        (
            UINT16 domain,
            UINT16 bus,
            UINT08 dev,
            UINT08 func,
            UINT32 Function,
            UINT32 DSMMXMSubfunction,
            UINT32 *pInOut,
            UINT16 *pSize
        );

        enum class AcpiPowerMethod
        {
            AcpiOff,
            AcpiOn,
            AcpiStatus
        };

        RC AcpiRootPortEval
        (
            UINT16 domain,
            UINT16 bus,
            UINT08 device,
            UINT08 function,
            AcpiPowerMethod method,
            UINT32 *pStatus
        );
    }
}
