# MCLK switch test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-team

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.hbm[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.hbm.emu.ctp or mclk.trial.hbm.gm10x.ctp or mclk.trial.hbm.emu.maxwell.ctp


name=mclk
mclk.freq   = 405MHz, 810MHz, 405MHz, 405MHz, 810MHz, 810MHz

name        = MClkSwitchTest
test        = initPstate:mclk
tolerance   = 0.05, 0.10
rmapi       = pmu
disable     = ThermalSlowdown
disable     = BrokenFB
order       = random
prune       = auto
