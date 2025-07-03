# HBM MClkSwitchTest
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.hbm[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gp100, gp10x, pascal).
# For example: mclk.trial.hbm.emu.ctp or mclk.trial.hbm.gp100.ctp or mclk.trial.hbm.emu.pascal.ctp


# Get MClkSwitchTestPll and MClkSwitchTestBypass from mclk.pstate[.<platform>][.<family>].ctp
include = mclk.pstate


# Trial Specification
# Use P0 as the basis for the DRAMPLL (reference path) tests.
# Use P8 as the basis for the REFMPLL (bypass path) tests.
name    = MClkSwitchTest.hbm
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk90Percent
test    = vbiosp0:MClk80Percent, vbiosp8:MClk80Percent
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk90Percent
test    = vbiosp0:MClk80Percent, vbiosp8:MClk80Percent
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk90Percent
test    = vbiosp0:MClk80Percent, vbiosp8:MClk80Percent
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk90Percent
test    = vbiosp0:MClk80Percent, vbiosp8:MClk80Percent
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass

tolerance = 0.05, 0.20
rmapi     = clk
disable   = ThermalSlowdown
disable   = BrokenFB
order     = random

#
# Trial Specification
# Restore to a low, safe P-State before ending the test or else issues may
# occur during cleanup (especially MCLK timeouts). This is because UCT or MODS
# may try to restore to a different P-State at the end of the test.
#
name = RestoreP8
test = vbiosp8

tolerance = 0.05, 0.20
rmapi     = perf
order     = sequence
prune     = auto
