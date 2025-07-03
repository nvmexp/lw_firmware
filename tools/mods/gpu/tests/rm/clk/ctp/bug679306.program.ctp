# Adapted from FermiClockStressTest  (diag/mods/gpu/tests/rm/clk/rmt_fermiclkstress.cpp)

# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Clocks 2.1: http://lwbugs/679306
# Clocks 2.0: http://lwbugs/1249060


# Get p-state definitions
include 	= bug679306.pstate

# Config+Program Trial
name		= Bug679306.program
test		= vbiosp0:por
rmapi		= clocks
order		= sequence

