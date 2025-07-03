# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: stress.gf119.ctp, stress.gf11x.ctp, stress.fermi.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.



# Partial P-State for most clock domains
name=defaults
dispclk.freq = 162MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 900MHz:-FORCE_BYPASS:+FORCE_PLL
gpc2clk.freq = 162MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 900MHz:-FORCE_BYPASS:+FORCE_PLL
xbar2clk.freq = 162MHz:+FORCE_BYPASS:-FORCE_PLL, 324MHz:+FORCE_BYPASS:-FORCE_PLL, 900MHz:-FORCE_BYPASS:+FORCE_PLL
pwrclk.freq = 162MHz, 324MHz, 810MHz

# Partial P-State for sys2clk
name=sys2clk
sys2clk.freq = 162MHz, 324MHz, 750MHz

# Partial P-State for hub2clk
name=hub2clk
hub2clk.freq = 162MHz, 324MHz, 810MHz

# Examples of Full P-States that could be used in a trial specification.
# The total number of p-states is the cartesion product of the
# VBIOS (always cardinality 1) and the partial p-states
# test=vbiosp0:hubclk		# 3 p-states	1 x 3     = |vbiosp0| x |hubclk|
# test=vbiosp0:legclk		# 4 p-states	1 x 4     = |vbiosp0| x |legclk|
# test=vbiosp0:hubclk:legclk	# 12 p-states	1 x 3 x 4 = |vbiosp0| x |hubclk| x |legclk|




# Trial specification for Stress Test
# Define two trials: one based on VBIOS P0 and the other based on VBIOS P8
# Use every combination of the pseudo-p-states inherited from the VBIOS p-states
# Run 1000 random iterations
name       = StressTest
test       = vbiosp0:defaults:sys2clk:hub2clk
rmapi      = clocks
order      = sequence
end        = 100
tolerance  = 0.05, 0.05


