# HBM MCLK switch test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-team

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.hbm[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.hbm.emu.ctp or mclk.trial.hbm.gm10x.ctp or mclk.trial.hbm.emu.maxwell.ctp


name=Hbm.mclk
mclk.freq   = 6800MHz, 6200MHz, 7001MHz, 5500MHz, 405MHz, 5800MHz, 6500MHz, 5001MHz, 810MHz, 6001MHz, 10001MHz, 9001MHz, 8001MHz

name        = MClkSwitchTest.Hbm
test        = initPstate:Hbm.mclk
tolerance   = 0.05, 0.10
rmapi       = pmu
disable     = ThermalSlowdown
disable     = BrokenFB
order       = random
prune       = auto
