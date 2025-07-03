/* LWIDIA_COPYRIGHT_BEGIN                                                 */
/*                                                                        */
/* Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All       */
/* information contained herein is proprietary and confidential to LWPU */
/* Corporation.  Any use, reproduction, or disclosure without the written */
/* permission of LWPU Corporation is prohibited.                        */
/*                                                                        */
/* LWIDIA_COPYRIGHT_END                                                   */

#include "uphyif.h"
#include "core/include/massert.h"

const char * UphyIf::InterfaceToString(Interface interface)
{
    switch (interface)
    {
        case Interface::Pcie:
            return "PCIE";
        case Interface::LwLink:
            return "LwLink";
        case Interface::C2C:
            return "C2C";
        case Interface::All:
            break;
    }
    MASSERT(interface == Interface::All);
    return "All";
}

const char * UphyIf::VersionToString(Version version)
{
    switch (version)
    {
        case Version::V10:
            return "v1.0";
        case Version::V30:
            return "v3.0";
        case Version::V31:
            return "v3.1";
        case Version::V32:
            return "v3.2";
        case Version::V50:
            return "v5.0";
        case Version::V61:
            return "v6.1";
        case Version::UNKNOWN:
            break;;
    }
    MASSERT(version == Version::UNKNOWN);
    return "Unknown";
}

const char * UphyIf::RegBlockToString(RegBlock regblock)
{
    switch (regblock)
    {
        case RegBlock::CLN:
            return "CLN";
        case RegBlock::DLN:
            return "DLN";
        case RegBlock::ALL:
            return "All";
    }
    MASSERT(regblock == RegBlock::ALL);
    return "All";
}
