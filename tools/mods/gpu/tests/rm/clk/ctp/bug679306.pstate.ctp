# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Clocks 2.1: http://lwbugs/679306
# Clocks 2.0: http://lwbugs/1249060


# Plan of Record: Kepler+ chips should run at these frequencies
name		    = por
gpc2clk.freq	= 1630MHz:force_pll,1630MHz:force_pll,1225MHz:force_pll,405MHz:force_bypass,202.5MHz:force_bypass,900MHz:force_pll,1360MHz:force_pll,950MHz:force_pll,810MHz:force_bypass

# Above POR: We don't expect Kepler to be able to actually run at these frequencies
name		    = hyper
gpc2clk.freq	= 2046MHz:force_pll,2046MHz:force_pll,2300MHz:force_pll,2046MHz:force_pll

