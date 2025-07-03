# Overclocking Test
#
# This test checks for non-POR frequencies, both overclocked and intermediate.
# Bug 989753 is an example of a problem that would have been identified with this test.
#
# The target frequencies specified herein are not supported in hardware.
# Resman deals with this fact by choosing a reasonably close actual frequency.
#
# target:   the frequency requested by this test (or by other tools or the vbios)
# actual:   the frequency Resman chooses to program
# measured: the frequency as measured in hardware
#

#
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest
#



# Define OverClockP0 for this chip.
include overclock.pstate

#
# Trial Specification
#
name        = OverclockTest

#
# Start overclocking at P0 one domain at a time.
# dispclk, pwrclk, and xbar2clk are excluded due to interdomain dependencies.
#
test        = gpc2clk.OverClock.P0

#
# Since we are asking for unsupported frequencies, we tolerate a lot
# between the target v. actual.
#
tolerance   = 0.05, 0.20


rmapi       = clk
order       = sequence
prune       = auto
