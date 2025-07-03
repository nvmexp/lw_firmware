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

# GP104 Freq Swings from P0 to P8
# mclk:     5005000KHz - 405000KHz (for GDDR5X)
# Avoiding Frequencies between 1300MHz to 1620MHz as per bug 1720686

name=Gddr5x.mclk
mclk.freq    = 405MHz, 540MHz, 734MHz, 972MHz, 1104MHz, 1274MHz, 1683MHz, 1782MHz, 1895MHz, 2072MHz, 2312MHz, 2600MHz, 2754MHz, 3003MHz, 3278MHz, 3382MHz, 3421MHz, 3687MHz, 3824MHz, 4004MHz, 4123MHz, 4356MHz, 4472MHz, 4621MHz, 4734MHz, 4902MHz, 5005MHz

name        = MClkSwitchTest.gddr5x
test        = initPstate:Gddr5x.mclk
tolerance   = 0.05, 0.10
rmapi       = vfinject
ramtype     = gddr5x
disable     = ThermalSlowdown
disable     = BrokenFB
order       = random
prune       = auto
