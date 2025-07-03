# BasicSanityTest
# Adapted from diag/mods/gpu/tests/rm/utility/clk/clk_gf100.cpp:493 (ClockUtilGF100::ClockSanityTest)
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: basic.pstate.gf119.ctp, basic.pstate.gf11x.ctp, basic.pstate.fermi.ctp
# You can also create version specific to emulation, etc.
# Examples: basic.pstate.emu.ctp, basic.pstate.emu.maxwell.ctp


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
# Excluding power clock, legclk and LWD clock from the ctp files as:
# 1) Power clock is fixed on GP100
# 2) As per a dislwssion with Abhishek, LWDCLK is fixed to 600 MHz at boot time and
# changes only with SYS2CLK(50% of SYS2CLK)
# 3)legclk isn't a valid domain for GP100(at least).
# As we have the DIST_MODE set to 1X_PLL for HUB2CLK on GP100 testing 2*(known 1620 divisor frequencies)
hub2clk.freq    = 540MHz:+FORCE_BYPASS:-FORCE_PLL, 648MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1080MHz:+FORCE_BYPASS:-FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL
