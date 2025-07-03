# Emulation GDDR5 MClkSwitchTest
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Before runing this test on emulaton, set the 'RMEnableClk' regkey to 0x20.
# Otherwise Resman returns an error indicating that MCLK is not programmable.
# Bit 5 (clkWhich_MClk) must be on to enable MCLK for emulation.
# LW_REG_STR_RM_ENABLE_CLK is the RM symbol for this regkey, which overrides OBJCLK::programmableDomains.

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.gddr5[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.gddr5.emu.ctp or mclk.trial.gddr5.gm10x.ctp or mclk.trial.gddr5.emu.maxwell.ctp


# Get MClkSwitchTestPll and MClkSwitchTestBypass from mclk.pstate[.<platform>][.<family>].ctp
include = mclk.pstate


# Trial Specification
# Use P0 as the basis for the DRAMPLL (reference path) tests.
# Use P8 as the basis for the REFMPLL (bypass path) tests.
# RM does not support DRAMPLL=>DRAMPLL transitions for GDDR5.  See bug 1201446.
name    = MClkSwitchTest.gddr5
test    = vbiosp0, vbiosp8, vbiosp5, vbiosp8
test    = vbiosp0:MClk90Percent, vbiosp8:MClk270MHz
test    = vbiosp0:MClk80Percent, vbiosp8:MClk324MHz
rmapi   = clk
ramtype = gddr5
disable = ThermalSlowdown
disable = BrokenFB

