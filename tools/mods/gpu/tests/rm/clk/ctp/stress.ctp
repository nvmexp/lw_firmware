# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: stress.gf119.ctp, stress.gf11x.ctp, stress.fermi.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.



# Partial P-State for most clock domains
name=defaults
dispclk.freq = 162MHz, 324MHz, 900MHz
gpc2clk.freq = 162MHz, 324MHz, 900MHz
xbar2clk.freq = 162MHz, 324MHz, 900MHz
pwrclk.freq = 162MHz, 324MHz, 900MHz
msdclk.freq = 162MHz, 324MHz, 900MHz

# Partial P-State for hostclk
name=hostclk
hostclk.freq = 417MHz, 277MHz, 648MHz, 540MHz, 462.857MHz, 405MHz, 360MHz, 324MHz, 294.545MHz, 270MHz, 249.231MHz, 231.429MHz, 216MHz, 202.5MHz, 190.588MHz, 180MHz, 170.526MHz, 162MHz, 154.288MHz, 147.273MHz, 140.870MHz, 135MHz, 129.6MHz, 27.615MHz, 120MHz, 115.714MHz, 111.724MHz, 108MHz, 104.516MHz, 101.250MHz, 98.182MHz, 95.294MHz, 90MHz, 85.263MHz, 81MHz, 77.143MHz, 73.636MHz, 70.435MHz, 67.5MHz, 64.8MHz, 62.308MHz, 60MHz, 57.857MHz, 55.862MHz, 54MHz, 52.258MHz

# Partial P-State for mclk
name=mclk
mclk.freq = 162MHz, 324MHz, 800MHz

# Partial P-State for sys2clk
name=sys2clk
sys2clk.freq = 162MHz, 324MHz, 750MHz

# Partial P-State for hub2clk
name=hub2clk
hub2clk.freq = 162MHz, 324MHz, 750MHz

# Partial P-State for legclk
name=legclk
legclk.freq = 162MHz, 324MHz, 600MHz, +20%


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
test       = vbiosp0:defaults:hostclk:sys2clk:hub2clk:legclk, vbiosp8:defaults:hostclk:sys2clk:hub2clk:legclk
rmapi      = clocks
order      = random
end        = 1000


