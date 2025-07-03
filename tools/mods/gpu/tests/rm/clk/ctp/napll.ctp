# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/Noise_Aware_PLL

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: napll.gk110b.ctp, napll.gm10x.ctp, napll.maxwell.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# Partial P-State for gpc2clk. The %age are chosen to cover most of the N/SDM/PL switch possibilities. We keep M constant for gpc2clk.
# The very close frequency switches will just change the SDM value while keeping N/PL constants. ex. 100 +/- 1%
# The next close frequency switches will change N and/or SDM value while keeping PL constant. ex 100 +/- 5%
# 100 +/- 40% will also make PL value to change.
name=gpc2clk
gpc2clk.freq = -50%:-FORCE_BYPASS:+FORCE_PLL, -40%:-FORCE_BYPASS:+FORCE_PLL, -31%:-FORCE_BYPASS:+FORCE_PLL, -23%:-FORCE_BYPASS:+FORCE_PLL, -16%:-FORCE_BYPASS:+FORCE_PLL, -10%:-FORCE_BYPASS:+FORCE_PLL, -8%:-FORCE_BYPASS:+FORCE_PLL, -6%:-FORCE_BYPASS:+FORCE_PLL, -5%:-FORCE_BYPASS:+FORCE_PLL, -3%:-FORCE_BYPASS:+FORCE_PLL, -1%:-FORCE_BYPASS:+FORCE_PLL, 100%:-FORCE_BYPASS:+FORCE_PLL, +1%:-FORCE_BYPASS:+FORCE_PLL, +3%:-FORCE_BYPASS:+FORCE_PLL, +5%:-FORCE_BYPASS:+FORCE_PLL, +6%:-FORCE_BYPASS:+FORCE_PLL, +8%:-FORCE_BYPASS:+FORCE_PLL, +10%:-FORCE_BYPASS:+FORCE_PLL, +16%:-FORCE_BYPASS:+FORCE_PLL, +23%:-FORCE_BYPASS:+FORCE_PLL, +31%:-FORCE_BYPASS:+FORCE_PLL, +40%:-FORCE_BYPASS:+FORCE_PLL, +50%:-FORCE_BYPASS:+FORCE_PLL


# Trial specification for noise-aware pll test
name       = NoiseAwarePllTest
test       = vbiosp0:gpc2clk, vbiosp5:gpc2clk, vbiosp8:gpc2clk
rmapi      = clocks
order      = random
end        = 100
tolerance  = 0.01, 0.01
disable    = thermalslowdown

