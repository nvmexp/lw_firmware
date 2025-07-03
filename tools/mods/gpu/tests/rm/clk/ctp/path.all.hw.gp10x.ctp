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
dispclk.freq    = 100%:FORCE_BYPASS:-FORCE_PLL, 100%:FORCE_BYPASS:-FORCE_PLL
dispclk.source  = sppll0, sppll1

name=CoreClk.Pll
dispclk.freq    = 100%:-FORCE_BYPASS:+FORCE_PLL

# DISPCLK on the REF path cannot use SPPLL0/1 and SYSPLL as a source. This is because the
# VCO for DISPCLK require an input of 27MHz which is only available from the crystal

#=============================================================================================
name=RootClk.Bypass
pwrclk.freq    = 100%:FORCE_BYPASS:-FORCE_PLL, 100%:FORCE_BYPASS:-FORCE_PLL
pwrclk.source  = sppll0, sppll1
hub2clk.freq   = 100%:FORCE_BYPASS:-FORCE_PLL, 100%:FORCE_BYPASS:-FORCE_PLL
hub2clk.source = sppll0, sppll1

#=============================================================================================

