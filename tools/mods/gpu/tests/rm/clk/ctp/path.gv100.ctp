# Basic Path Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#source_Field for details
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Pull in chip specific test parameters
include path.all.gv100

# Trial Specification
# Sine test replaces the values of the frequency domains it applies to,
# for domains not changed by sine test, we will inherit from PState 0

# Switching between the PLLs on P0 require us to first switch to a bypass path,
# so we will run some Core Bypass tests first, then the Core PLL0, some more
# Core Bypass and then Core PLL1
# The range of the Bypass tests are picked to cover all paths for all domains
name        = PathBasicCoreBypass1
test        = vbiosp0:CoreClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = clk
begin       = 0
end         = 1
prune       = auto

name        = PathBasicCorePll0
test        = vbiosp0:CoreClk.Pll0
tolerance   = 0.15, 0.25
rmapi       = clk
prune       = auto

name        = PathBasicCoreBypass2
test        = vbiosp0:CoreClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = clk
begin       = 2
end         = 4
prune       = auto

name        = PathBasicCorePll1
test        = vbiosp0:CoreClk.Pll1
tolerance   = 0.15, 0.25
rmapi       = clk
prune       = auto

name        = PathBasicCoreBypass3
test        = vbiosp0:CoreClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = clk
begin       = 5
end         = 8
prune       = auto


# The root clk bypass combinations
name        = PathBasicRootClkBypass
test        = vbiosp0:RootClk.Bypass
tolerance   = 0.15, 0.25
rmapi       = clk
prune       = auto


# The rest are here
name        = PathBasicSys2
test        = vbiosp0:Sys2Clk.Pll0,vbiosp0:Sys2Clk.Bypass,vbiosp0:Sys2Clk.Pll1
tolerance   = 0.15, 0.25
rmapi       = clk
prune       = auto

