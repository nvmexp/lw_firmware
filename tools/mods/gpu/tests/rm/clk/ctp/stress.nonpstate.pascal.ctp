# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: stress.gf119.ctp, stress.gf11x.ctp, stress.fermi.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.



# Partial P-State for most clock domains
name=defaults
dispclk.freq = 26MHz:-FORCE_BYPASS:+FORCE_PLL, 270MHz:+FORCE_BYPASS:-FORCE_PLL, 470MHz:-FORCE_BYPASS:+FORCE_PLL, 800MHz:-FORCE_BYPASS:+FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1174MHz:-FORCE_BYPASS:+FORCE_PLL, 1464MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:-FORCE_BYPASS:+FORCE_PLL
gpc2clk.freq = 55MHz:-FORCE_BYPASS:+FORCE_PLL, 270MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1100MHz:-FORCE_BYPASS:+FORCE_PLL, 1530MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2670MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL
xbar2clk.freq = 55MHz:-FORCE_BYPASS:+FORCE_PLL, 270MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 990MHz:-FORCE_BYPASS:+FORCE_PLL, 1486MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2710MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

# Partial P-State for sys2clk
name=sys2clk
sys2clk.freq = 55MHz:-FORCE_BYPASS:+FORCE_PLL, 270MHz:+FORCE_BYPASS:-FORCE_PLL, 750MHz:-FORCE_BYPASS:+FORCE_PLL, 1340MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2348MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

# Partial P-State for ltc2clk
name=ltc2clk
ltc2clk.freq = 55MHz:-FORCE_BYPASS:+FORCE_PLL, 270MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1287MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 1700MHz:-FORCE_BYPASS:+FORCE_PLL, 2348MHz:-FORCE_BYPASS:+FORCE_PLL, 3400MHz:-FORCE_BYPASS:+FORCE_PLL

# Partial P-State for hub2clk
name=hub2clk
hub2clk.freq = 162MHz, 324MHz, 750MHz


# Examples of Full P-States that could be used in a trial specification.
# The total number of p-states is the cartesion product of the
# VBIOS (always cardinality 1) and the partial p-states
# test=vbiosp0:hubclk           # 3 p-states    1 x 3     = |vbiosp0| x |hubclk|
# test=vbiosp0:legclk           # 4 p-states    1 x 4     = |vbiosp0| x |legclk|
# test=vbiosp0:hubclk:legclk    # 12 p-states   1 x 3 x 4 = |vbiosp0| x |hubclk| x |legclk|




# Trial specification for Stress Test
# Define trials based on VBIOS P0
# Use every combination of the pseudo-p-states inherited from the VBIOS p-states
# Run 1000 random iterations
name       = StressTest
test       = initPstate:defaults, initPstate:sys2clk, initPstate:hub2clk
rmapi      = clocks
order      = random
end        = 1000

