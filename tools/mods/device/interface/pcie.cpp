/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "device/interface/pcie.h"

const char * Pcie::L1ClockPMSubstateToString(L1ClockPmSubstates state)
{
    switch (state)
    {
        case Pcie::L1_CLK_PM_DISABLED:
            return "Disabled";
        case Pcie::L1_CLK_PM_PLLPD:
            return "PLLPD";
        case Pcie::L1_CLK_PM_CPM:
            return "CPM";
        case Pcie::L1_CLK_PM_PLLPD_CPM:
            return "PLLPD/CPM";
        case Pcie::L1_CLK_PM_UNKNOWN:
            return "Unknown";
        default:
            break;
    }
    return "Invalid";
}
