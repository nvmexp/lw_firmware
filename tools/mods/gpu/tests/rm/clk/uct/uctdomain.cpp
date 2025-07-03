/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "core/include/platform.h"
#include "uctdomain.h"

const uct::DomainNameMap uct::DomainNameMap::freq = uct::DomainNameMap::initialize
    ("gpcclk",          LW2080_CTRL_CLK_DOMAIN_GPCCLK     )
    ("xbarclk",         LW2080_CTRL_CLK_DOMAIN_XBARCLK    )
    ("sysclk",          LW2080_CTRL_CLK_DOMAIN_SYSCLK     )
    ("hubclk",          LW2080_CTRL_CLK_DOMAIN_HUBCLK     )
    ("mclk",            LW2080_CTRL_CLK_DOMAIN_MCLK       )
    ("hostclk",         LW2080_CTRL_CLK_DOMAIN_HOSTCLK    )
    ("dispclk",         LW2080_CTRL_CLK_DOMAIN_DISPCLK    )
    ("pclk0",           LW2080_CTRL_CLK_DOMAIN_PCLK0      )
    ("pclk1",           LW2080_CTRL_CLK_DOMAIN_PCLK1      )
    ("pclk2",           LW2080_CTRL_CLK_DOMAIN_PCLK2      )
    ("pclk3",           LW2080_CTRL_CLK_DOMAIN_PCLK3      )
    ("xclk",            LW2080_CTRL_CLK_DOMAIN_XCLK       )
    ("gpc2clk",         LW2080_CTRL_CLK_DOMAIN_GPC2CLK    )
    ("ltc2clk",         LW2080_CTRL_CLK_DOMAIN_LTC2CLK    )
    ("xbar2clk",        LW2080_CTRL_CLK_DOMAIN_XBAR2CLK   )
    ("sys2clk",         LW2080_CTRL_CLK_DOMAIN_SYS2CLK    )
    ("hub2clk",         LW2080_CTRL_CLK_DOMAIN_HUB2CLK    )
    ("legclk",          LW2080_CTRL_CLK_DOMAIN_LEGCLK     )
    ("utilsclk",        LW2080_CTRL_CLK_DOMAIN_UTILSCLK   )
    ("pwrclk",          LW2080_CTRL_CLK_DOMAIN_PWRCLK     )
    ("lwdclk",          LW2080_CTRL_CLK_DOMAIN_LWDCLK     )     // Alias: MSDCLK in Kepler, LWDCLK in Maxwell and after
    ("pciegenclk",      LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK );    // Instead of frequencies, use link speed (i.e. 1, 2, or 3)

// Voltage domains.
const uct::DomainNameMap uct::DomainNameMap::volt = uct::DomainNameMap::initialize
    ("lwvdd",           CLK_UCT_VOLT_DOMAIN_LOGIC   )
    ("sram",            CLK_UCT_VOLT_DOMAIN_SRAM    )
    ("msvdd",           CLK_UCT_VOLT_DOMAIN_MSVDD   );

