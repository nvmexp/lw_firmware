# Emulation SDDR4 MClkSwitchTest when 2T is disabled
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Before runing this test on emulaton, set the 'RMEnableClk' regkey to 0x20.
# Otherwise Resman returns an error indicating that MCLK is not programmable.
# Bit 5 (clkWhich_MClk) must be on to enable MCLK for emulation.
# LW_REG_STR_RM_ENABLE_CLK is the RM symbol for this regkey, which overrides OBJCLK::programmableDomains.

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.sddr4.no2t[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.sddr4.no2t.emu.ctp or mclk.trial.sddr4.no2t.gm10x.ctp or mclk.trial.sddr4.no2t.emu.maxwell.ctp


# Get MClkSwitchTestPll and MClkSwitchTestBypass from mclk.pstate[.<platform>][.<family>].ctp
include = mclk.pstate


# Trial Specification
# Require that 2T training is disabled in the VBIOS.
# Use P0 as the basis for the PLL tests.
# Use P8 as the basis for the bypass paths.
# Iterate through all defined frequencies.
name    = MClkSwitchTest.sddr4.no2t
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
test    = vbiosp0:MClkSwitchTestPll, vbiosp8:MClkSwitchTestBypass
rmapi   = clk
ramtype = sddr4
disable = Training2t
disable = ThermalSlowdown
disable = BrokenFB

