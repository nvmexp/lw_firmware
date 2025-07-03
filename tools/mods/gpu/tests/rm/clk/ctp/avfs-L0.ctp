# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest


# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: napll.gk110b.ctp, napll.gm10x.ctp, napll.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# AVFS-L0. The target of this test is to have L0 level coverage with regards to
# the AVFS feature i.e. to validate the basic functionality of the ADC over-ride
# works as well as the LUT over-ride works.

# ADC over-ride - The way this works is by just having a <clkdomain>.adcvolt value
# specified the voltage regime should be kicked in and the corresponding measured
# frequency should be from the LUT-entry corresponding to that voltage value.
name = adctestL0
gpc2clk.adcvolt = 800mV
sys2clk.adcvolt = 800mV
ltc2clk.adcvolt = 800mV
xbar2clk.adcvolt = 800mV

# LUT over-ride - When a frequency less than Fmax@Vmin is specified the frequency
# regime should kick-in. This would also verify the DVCO and SKIPPER functionality.
name = luttestL0
gpc2clk.adcvolt = 950mV
gpc2clk.lutfreq = 2200MHz
sys2clk.adcvolt = 950mV
sys2clk.lutfreq = 2200MHz
ltc2clk.adcvolt = 950mV
ltc2clk.lutfreq = 2200MHz
xbar2clk.adcvolt = 950mV
xbar2clk.lutfreq = 2200MHz

# Trial specification for adaptive VF scaling test
name       = AdaptiveVFSTestL0
test       = initPstate:luttestL0, initPstate:adctestL0
rmapi      = vfinject
order      = sequence
end        = 100
tolerance  = 0.05, 0.05
