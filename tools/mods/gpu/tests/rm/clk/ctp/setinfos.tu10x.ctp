# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest


# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: setinfos.gk110b.ctp, setinfos.gm10x.ctp, setinfos.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# SetInfos: The target of this test is to set clock frequency for diffrent domains.
# only one freqency can be targeted at a time for a single domain.

name = SetInfos
gpcclk.freq = 1600MHz

# Trial specification for setinfos test
name       = SetInfosTest
test       = InitPstate:SetInfos
rmapi      = pmu
order      = sequence
end        = 100
tolerance  = 0.05, 0.05
