# BasicSanityTest
# Adapted from diag/mods/gpu/tests/rm/utility/clk/clk_gf100.cpp:493 (ClockUtilGF100::ClockSanityTest)
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: basic.pstate.gf119.ctp, basic.pstate.gf11x.ctp, basic.pstate.fermi.ctp
# You can also create version specific to emulation, etc.
# Examples: basic.pstate.emu.ctp, basic.pstate.emu.maxwell.ctp



# The original sanity test sets all the frequencies to half value and uses ALT path
name=BasicSanity.HalfFreqBypass
mclk.freq       = 50%:+FORCE_BYPASS:-FORCE_PLL
dispclk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
gpc2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
xbar2clk.freq   = 50%:+FORCE_BYPASS:-FORCE_PLL
ltc2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
sys2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
hub2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
legclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
pwrclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
# msdclk==lwdclk in the RMAPI
msdclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL

name=BasicSanity.FullFreqPLL
mclk.freq       = 100%:-FORCE_BYPASS:+FORCE_PLL
dispclk.freq    = 100%:-FORCE_BYPASS:+FORCE_PLL
gpc2clk.freq    = 100%:-FORCE_BYPASS:+FORCE_PLL
xbar2clk.freq   = 100%:-FORCE_BYPASS:+FORCE_PLL
ltc2clk.freq    = 100%:-FORCE_BYPASS:+FORCE_PLL
sys2clk.freq    = 100%:-FORCE_BYPASS:+FORCE_PLL
#hub2clk and pwrclk don't have PLLs
hub2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
pwrclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
legclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
msdclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
