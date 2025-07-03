/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file clk_gf117.cpp
//! \brief Implementation of GF117 clk hal
//!

#include "clkbase.h"
#include "core/include/memcheck.h"

ClockUtilGF117::ClockUtilGF117(LwU32 hClient, LwU32 hDev, LwU32 hSubDev)
    :ClockUtilGF100(hClient, hDev, hSubDev)
{
}

//!
//! @brief Gets mask of all programmable domains
//!
//! @return Mask of all programmable domains
//!----------------------------------------------------------------------------
LwU32 ClockUtilGF117::GetProgrammableDomains()
{
    if (!m_ProgClkDomains)
    {
        m_ProgClkDomains = ClockUtil::GetProgrammableDomains();

        //
        // (Bug 802804) Remove HostClk from list of programmable domains
        // as for GF117, RM is unable to program it to any non-OSM freq
        //
        m_ProgClkDomains &= ~(LW2080_CTRL_CLK_DOMAIN_HOSTCLK);

        // Programmable domains should not be empty!
        MASSERT(m_ProgClkDomains);
    }

    return m_ProgClkDomains;
}
