# Basic Path Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#source_Field for details
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Test sweeps typical paths as defined in resman/kernel/clk/clk2/sd/clksdgm200.c

# Each entry has forces a path with the FORCE_XXX flag and then clears the other with a
# -FORCE_XXX (e.g. FORCE_BYPASS:-FORCE_PLL). This is to ensure that we don't end up with
# HYBRID mode

#=============================================================================================
name=CoreClk.Bypass
xbar2clk.freq   = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
xbar2clk.source = sppll0, sppll1
ltc2clk.freq    = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
ltc2clk.source  = sppll0, sppll1
gpc2clk.freq    = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
gpc2clk.source  = sppll0, sppll1
dispclk.freq    = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
dispclk.source  = sppll0, sppll1

name=CoreClk.Pll0
xbar2clk.freq   = 100%:FORCE_PLL:-FORCE_BYPASS
xbar2clk.source = sppll0
ltc2clk.freq    = 100%:FORCE_PLL:-FORCE_BYPASS
ltc2clk.source  = sppll0
gpc2clk.freq    = 100%:FORCE_PLL:-FORCE_BYPASS
gpc2clk.source  = sppll0

name=CoreClk.Pll1
xbar2clk.freq   = 100%:FORCE_PLL:-FORCE_BYPASS
xbar2clk.source = sppll1
ltc2clk.freq    = 100%:FORCE_PLL:-FORCE_BYPASS
ltc2clk.source  = sppll1
gpc2clk.freq    = 100%:FORCE_PLL:-FORCE_BYPASS
gpc2clk.source  = sppll1

# DISPCLK on the REF path cannot use SPPLL0/1 and SYSPLL as a source. This is because the
# VCO for DISPCLK require an input of 27MHz which is only available from the crystal

#=============================================================================================
name=RootClk.Bypass
pwrclk.freq    = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
pwrclk.source  = sppll0, sppll1
hub2clk.freq   = 50%:FORCE_BYPASS:-FORCE_PLL, 50%:FORCE_BYPASS:-FORCE_PLL
hub2clk.source = sppll0, sppll1

#=============================================================================================
name=Sys2Clk.Bypass
sys2clk.freq   = 50%:FORCE_BYPASS:-FORCE_PLL
sys2clk.source = sppll0

name=Sys2Clk.Pll0
sys2clk.freq   = 100%:FORCE_PLL:-FORCE_BYPASS
sys2clk.source = sppll0

name=Sys2Clk.Pll1
sys2clk.freq   = 100%:FORCE_PLL:-FORCE_BYPASS
sys2clk.source = sppll1

