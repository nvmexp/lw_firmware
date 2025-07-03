# Emulation MClkSwitchTest Settings

# Before runing this test on emulaton, set the 'RMEnableClk' regkey to 0x20.
# Otherwise Resman returns an error indicating that MCLK is not programmable.
# Bit 5 (clkWhich_MClk) must be on to enable MCLK for emulation.
# LW_REG_STR_RM_ENABLE_CLK is the RM symbol for this regkey, which overrides OBJCLK::programmableDomains.

# This test is more robust that the previous MClkSwitchTest for emulation.
# See resman\sm2tests\tests\manual\clk\MClkSwitchTest-*.ini
# Specifically, that manual test would ask the user to select four POR frequencies.

# This CTP file does not contain any trials.
# It is included from other CTP files, generally emu.mclk.trial.*.ctp.
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific version of these partial p-states,
# create a file with the chip/family name in the middle.
# For example: emu.mclk.pstate.gm107.ctp or emu.mclk.pstate.gm10x.ctp



# Range with POR freq at the maximum
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClkSwitchTestPll
mclk.freq = +0%, -10%, -20%, -30%, -40%


# Bypass frequencies
# This partial p-state should be used with vbiosP8 or similar.
name=MClkSwitchTestBypass
mclk.freq = 270MHz, 324MHz, 405MHz, 540MHz, 810MHz


# MCLK at 90% of POR
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClk90Percent
mclk.freq = 90%:+FORCE_PLL:-FORCE_BYPASS

# MCLK at 80% of POR
# This partial p-state should be used with vbiosP0 or vbiosP1.
name=MClk80Percent
mclk.freq = 80%:+FORCE_PLL:-FORCE_BYPASS

# MCLK at 810MHz
# This partial p-state should be used with vbiosP5 or similar.
name=MClk810MHz
mclk.freq = 810MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 405MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk405MHz
mclk.freq = 405MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 324MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk324MHz
mclk.freq = 324MHz:+FORCE_BYPASS:-FORCE_PLL

# MCLK at 270MHz
# This partial p-state should be used with vbiosP8 or similar.
name=MClk270MHz
mclk.freq = 270MHz:+FORCE_BYPASS:-FORCE_PLL


