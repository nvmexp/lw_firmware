# Base Sine Test
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test for details on Sine Test
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# GP104 Freq Swings from P0 to P8
# dispclk:  1080000KHz - 405000KHz
# mclk:     4004000KHz - 405000KHz (for GDDR5)
# hub2clk:  1080000KHz - 810000KHz
# pwrclk:   540000KHz stationary

# Parameter for basic sine test, test will swing the freq between max and min as listed above.
# Test starts at middle frequency and makes sure that the peak/trough values are on the max/min.
# Decay rate is set to 0 to produce oscillations within the min/max envelope
name           = SineBasic
dispclk.alpha  = 743MHz
dispclk.beta   = 337MHz
dispclk.omega  = 0
dispclk.gamma  = 0.785
dispclk.flags  = -FORCE_BYPASS:+FORCE_PLL

hub2clk.alpha  = 945MHz
hub2clk.beta   = 135MHz
hub2clk.omega  = 0
hub2clk.gamma  = 8.709

iterations = 10