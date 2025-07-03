# Emulation GDDR6 MClkSwitchTest
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.gddr6[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.gddr6.emu.ctp or mclk.trial.gddr6.gm10x.ctp or mclk.trial.gddr6.emu.maxwell.ctp


# Get MClkSwitchTestPll and MClkSwitchTestBypass from mclk.pstate[.<platform>][.<family>].ctp
include = mclk.pstate


# Trial Specification
# Use P0 as the basis for the DRAMPLL (reference path) tests.
# Use P8 as the basis for the REFMPLL (bypass path) tests.
name    = MClkSwitchTest.gddr6
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk270MHz
test    = vbiosp0:MClk80Percent, vbiosp8:MClk324MHz
rmapi   = clk
disable = ThermalSlowdown
disable = BrokenFB

