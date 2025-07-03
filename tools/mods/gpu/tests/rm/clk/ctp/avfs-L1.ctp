# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest


# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: napll.gk110b.ctp, napll.gm10x.ctp, napll.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# AVFS-L1. The target of this test is to have L1 level coverage with regards to
# the AVFS feature i.e. to validate most paths for the voltage as well as frequency
# regime work.

# ADC override - The way this works is by just having a <clkdomain>.adcvolt value
# specified the voltage regime should be kicked in and the corresponding measured
# frequency should be from the LUT-entry corresponding to that voltage value.
# Also the regime Id set by the PMU should be Voltage-regime
name = adctestL1
gpc2clk.adcvolt = 550mV, 1100mV, 750mV, 1050mV, 700mV
sys2clk.adcvolt = 550mV, 1100mV, 750mV, 1050mV, 700mV
ltc2clk.adcvolt = 550mV, 1100mV, 750mV, 1050mV, 700mV
xbar2clk.adcvolt = 550mV, 1100mV, 750mV, 1050mV, 700mV

# LUT over-ride - When a frequency less than Fmax@Vmin is specified the frequency
# regime should kick-in. This would also verify the DVCO and SKIPPER functionality.
# Also the regime Id set by the PMU should be Frequency-regime or in case the
# requested frequency is less than the fixed frequency regime limit then it should
# be Fixed-frequency-regime
name = luttestL1
gpc2clk.adcvolt = 800mV, 1100mV, 700mV, 950mV, 550mV
gpc2clk.lutfreq = 2079MHz, 2650MHz, 1360MHz, 2600MHz, 800MHz
sys2clk.adcvolt = 800mV, 1100mV, 700mV, 950mV, 550mV
sys2clk.lutfreq = 2079MHz, 2650MHz, 1360MHz, 2600MHz, 800MHz
ltc2clk.adcvolt = 800mV, 1100mV, 700mV, 950mV, 550mV
ltc2clk.lutfreq = 2079MHz, 2650MHz, 1360MHz, 2600MHz, 800MHz
xbar2clk.adcvolt = 800mV, 1100mV, 700mV, 950mV, 550mV
xbar2clk.lutfreq = 2079MHz, 2650MHz, 1360MHz, 2600MHz, 800MHz

# Trial specification for adaptive VF scaling test
name       = AdaptiveVFSTestL1
test       = initPstate:adctestL1, initPstate:luttestL1
rmapi      = vfinject
order      = sequence
end        = 100
tolerance  = 0.05, 0.05
