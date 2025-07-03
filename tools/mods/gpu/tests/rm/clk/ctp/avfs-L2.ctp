# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: napll.gk110b.ctp, napll.gm10x.ctp, napll.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# AVFS-L2. The target of this test is to have L2 level coverage with regards to
# the AVFS feature i.e. stress testing all the possible combinations of VF points#
# The way this works is by just having a <clkdomain>.adcvolt value set to 'all'.
# In that case a set of random frequencies would be generated and corresponding
# voltage values picked and ADC override specified with it.
name = avfsL2
gpc2clk.lutfreq = all
sys2clk.lutfreq = all
xbar2clk.lutfreq = all
ltc2clk.lutfreq = all

# Trial specification for adaptive VF scaling test
name       = AdaptiveVFSTestL2
test       = initPstate:avfsL2
rmapi      = vfinject
order      = random
end        = 800
tolerance  = 0.08, 0.08

