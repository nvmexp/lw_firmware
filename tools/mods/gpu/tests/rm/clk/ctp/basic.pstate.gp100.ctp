# BasicSanityNonPstateTest
# Adapted from diag/mods/gpu/tests/rm/utility/clk/clk_gf100.cpp:493 (ClockUtilGF100::ClockSanityTest)
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# You can make chip/family/platform-specific versions of this test,
# but your probably don't want to.  You'd probably prefer making
# chip/family/platform-specific versions of basic.pstate.ctp.
# Examples: basic.pstate.gf119.ctp, basic.pstate.gf11x.ctp, basic.pstate.fermi.ctp
# Examples: basic.pstate.emu.ctp, basic.pstate.emu.maxwell.ctp, basic.pstate.rtl.gf20x.ctp
# The 'include' directive automatically loads chip/family/platform-specific profiles.

name=BasicSanity.dispclk
dispclk.freq    = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 350MHz:-FORCE_BYPASS:+FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 470MHz:-FORCE_BYPASS:+FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 800MHz:-FORCE_BYPASS:+FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1374MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:-FORCE_BYPASS:+FORCE_PLL

name=BasicSanity.gpc2clk
core.volt       = 1100mV
gpc2clk.freq    = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1530MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2670MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

name=BasicSanity.xbar2clk
xbar2clk.freq   = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1486MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2710MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

name=BasicSanity.ltc2clk
ltc2clk.freq    = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1287MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2348MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

name=BasicSanity.sys2clk
sys2clk.freq    = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 750MHz:-FORCE_BYPASS:+FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2348MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

#hub2clk and pwrclk don't have PLLs
name=BasicSanity.other
hub2clk.freq    = 50%:+FORCE_BYPASS:-FORCE_PLL
legclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
pwrclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL
# msdclk==lwdclk in the RMAPI
msdclk.freq     = 50%:+FORCE_BYPASS:-FORCE_PLL

