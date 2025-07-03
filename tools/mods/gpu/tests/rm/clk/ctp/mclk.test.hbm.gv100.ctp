# HBM MCLK switch test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Before runing this test on non-silicon platforms, set the 'RMEnableClk' regkey
# to 0x20 to enable MCLK as a programmable domain.

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# To make a chip-specific or platform-specific version of this trial,
# create a file named as mclk.trial.hbm[.<platform>][.<family>].ctp
# where <platform> specifies the platform type (emu, hw, rtl) and
# where <family> specifies the chip and/or family (e.g. gm107, gm10x, maxwell).
# For example: mclk.trial.hbm.emu.ctp or mclk.trial.hbm.gm10x.ctp or mclk.trial.hbm.emu.maxwell.ctp


name=Hbm.mclk
mclk.freq    = 810MHz, 1000MHz, 1200MHz

name        = MClkSwitchTest.Hbm
test        = initPstate:Hbm.mclk
tolerance   = 0.05, 0.10
rmapi       = clk
disable     = ThermalSlowdown
disable     = BrokenFB
order       = random
prune       = auto
