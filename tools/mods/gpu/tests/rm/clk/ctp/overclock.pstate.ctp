# Overclocking Test

# This test checks for non-POR frequencies, both overclocked and intermediate.
# Bug 989753 is an example of a problem that would have been identified with this test.

# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest


#
# In general, these partial p-states are for use with P0.
# The order shown here is the order we typically use in the VBIOS P-State Ordering Table.
# This is merely for colwenience; order here has no relevance.
# dispclk and pwrclk are excluded since they are not expected to work at these speeds.
#
name=sys2clk.OverClock.P0
sys2clk.freq = 1630MHz,1225MHz

name=hub2clk.OverClock.P0
hub2clk.freq = 1630MHz,1225MHz

name=legclk.OverClock.P0
legclk.freq  = 1630MHz,1225MHz

name=gpc2clk.OverClock.P0
gpc2clk.freq = 1630MHz,1225MHz

name=ltc2clk.OverClock.P0
ltc2clk.freq = 1630MHz,1225MHz

name=mclk.OverClock.P0
mclk.freq    = 1630MHz,1225MHz

# msdclk == lwdclk due to aliasing inside the RMPI.
name=msdclk.OverClock.P0
msdclk.freq  = 1630MHz,1225MHz

name=xbar2clk.OverClock.P0
xbar2clk.freq = 1630MHz,1225MHz


#
# In general, these partial p-states are for use with P8.
# The order shown here is the order we typically use in the VBIOS P-State Ordering Table.
# This is merely for colwenience; order here has no relevance.
#
name=sys2clk.OverClock.P8
sys2clk.freq = 816MHz:-FORCE_BYPASS:+FORCE_PLL

name=hub2clk.OverClock.P8
hub2clk.freq = 816MHz:-FORCE_BYPASS

name=legclk.OverClock.P8
legclk.freq  = 816MHz:-FORCE_BYPASS

name=gpc2clk.OverClock.P8
gpc2clk.freq = 816MHz:-FORCE_BYPASS:+FORCE_PLL

name=ltc2clk.OverClock.P8
ltc2clk.freq = 816MHz:-FORCE_BYPASS:+FORCE_PLL

name=mclk.OverClock.P8
mclk.freq    = 816MHz:-FORCE_BYPASS:+FORCE_PLL

# msdclk == lwdclk due to aliasing inside the RMPI.
name=msdclk.OverClock.P8
msdclk.freq  = 816MHz:-FORCE_BYPASS

name=dispclk.OverClock.P8
dispclk.freq = 816MHz:-FORCE_BYPASS:+FORCE_PLL

name=pwrclk.OverClock.P8
pwrclk.freq  = 816MHz:-FORCE_BYPASS

name=xbar2clk.OverClock.P8
xbar2clk.freq = 816MHz:-FORCE_BYPASS:+FORCE_PLL


