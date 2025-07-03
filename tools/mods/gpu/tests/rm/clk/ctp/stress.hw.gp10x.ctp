# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# To make a chip-specific version of this test,
# create a file with the chip/family name in the middle.
# Examples: stress.gf119.ctp, stress.gf11x.ctp, stress.fermi.ctp

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.



# Partial P-State for most clock domains
name=dispclk
dispclk.freq = 270MHz:+FORCE_BYPASS:-FORCE_PLL, 350MHz:-FORCE_BYPASS:+FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 470MHz:-FORCE_BYPASS:+FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 720MHz:-FORCE_BYPASS:+FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 934MHz:-FORCE_BYPASS:+FORCE_PLL, 1080MHz:-FORCE_BYPASS:+FORCE_PLL, 1252MHz:-FORCE_BYPASS:+FORCE_PLL, 1324MHz:-FORCE_BYPASS:+FORCE_PLL, 1432MHz:-FORCE_BYPASS:+FORCE_PLL, 1561MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:-FORCE_BYPASS:+FORCE_PLL

# Partial P-State for hub2clk
name=hub2clk
hub2clk.freq = 162MHz, 324MHz, 540MHz, 648MHz, 720MHz, 810MHz, 1080MHz, 1296MHz

name=mclk
mclk.freq    = 405MHz, 540MHz, 702MHz, 963MHz, 1114MHz, 1232MHz, 1692MHz, 1836MHz, 2016MHz, 2576MHz, 3003MHz, 3342MHz, 3772MHz, 3904MHz, 4004MHz


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
test       = initPstate:dispclk:hub2clk:mclk
rmapi      = vfinject
order      = random
end        = 1000

