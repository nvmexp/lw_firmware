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
sys2clk.freq = +5%:-FORCE_BYPASS

name=hub2clk.OverClock.P0
hub2clk.freq = +5%

name=gpc2clk.OverClock.P0
gpc2clk.freq = +5%:-FORCE_BYPASS

name=ltc2clk.OverClock.P0
ltc2clk.freq = +5%:-FORCE_BYPASS

name=mclk.OverClock.P0
mclk.freq    = +5%:-FORCE_BYPASS

name=xbar2clk.OverClock.P0
xbar2clk.freq = +5%:-FORCE_BYPASS


