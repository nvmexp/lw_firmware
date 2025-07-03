# MCLK Sine Test
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test

# Test Specification
# Start with a normal POR frequency.
# Run until something breaks
name		= MemoryClockBreakingPoint
mclk.alpha	= 2500MHz
mclk.beta	= 50MHz
mclk.gamma	= 0.157
mclk.omega	= 0.5
iterations	= 1000


# Trial Specification
name        = MemoryClockBreakingPoint
test        = vbiosp0:MemoryClockBreakingPoint
tolerance   = 0.15, 0.25
rmapi       = clk
order       = sequence
prune       = auto

