# Base Sine Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test for details on Sine Test
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# GM206 Freq Swings from P0 to P8
# dispclk:  1080000KHz - 540000KHz
# gpc2clk:  2000000KHz - 810000KHz
# ltc2clk:  2000000KHz - 405000KHz
# xbar2clk: 2000000KHz - 1000000KHz
# sys2clk:  1600000KHz - 1066000KHz
# hub2clk:  1080000KHz - 810000KHz
# pwrclk:   324000KHz stationary

# Parameter for basic sine test, test will swing the freq between max and min as listed above.
# Test starts at middle frequency and makes sure that the peak/trough values are on the max/min.
# Decay rate is set to 0 to produce oscillations within the min/max envelope
name           = SineBasic
dispclk.alpha  = 810MHz
dispclk.beta   = 270MHz
dispclk.omega  = 0
dispclk.gamma  = 0.785

gpc2clk.alpha  = 1405MHz
gpc2clk.beta   = 595MHz
gpc2clk.omega  = 0
gpc2clk.gamma  = 1.271

ltc2clk.alpha  = 1202.5MHz
ltc2clk.beta   = 797.5MHz
ltc2clk.omega  = 0
ltc2clk.gamma  = 2.056

xbar2clk.alpha  = 1500MHz
xbar2clk.beta   = 500MHz
xbar2clk.omega  = 0
xbar2clk.gamma  = 3.327

sys2clk.alpha  = 1333MHz
sys2clk.beta   = 267MHz
sys2clk.omega  = 0
sys2clk.gamma  = 5.383

hub2clk.alpha  = 945MHz
hub2clk.beta   = 135MHz
hub2clk.omega  = 0
hub2clk.gamma  = 8.709

iterations = 10