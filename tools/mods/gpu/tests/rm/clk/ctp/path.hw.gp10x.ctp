# Basic Path Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#source_Field for details
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Pull in chip specific test parameters
include path.all

# Trial Specification
# Sine test replaces the values of the frequency domains it applies to,
# for domains not changed by sine test, we will inherit from PState 0

# Switching between the PLLs on P0 require us to first switch to a bypass path,
# so we will run some Core Bypass tests first, then the Core PLL0, some more
# Core Bypass and then Core PLL1
# The range of the Bypass tests are picked to cover all paths for all domains
name        = PathBasicCoreBypass1
test        = initpstate:CoreClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = vfinject
begin       = 0
end         = 1
prune       = auto

name        = PathBasicCorePll
test        = initpstate:CoreClk.Pll
tolerance   = 0.15, 0.25
rmapi       = vfinject
prune       = auto

name        = PathBasicCoreBypass2
test        = initpstate:CoreClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = vfinject
begin       = 2
end         = 4
prune       = auto

# The root clk bypass combinations
name        = PathBasicRootClkBypass
test        = initpstate:RootClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = vfinject
prune       = auto

