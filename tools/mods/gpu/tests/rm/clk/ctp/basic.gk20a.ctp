# BasicSanityTest
# If this test fails, please file a bug and/or contact sw-gpu-rm-clocks-dev.
# https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# GK20A has no PStates and this test make use of the defaultsPState to test the POR freqs

name = gpc2clkTest
gpc2clk.freq = 144MHz:+FORCE_PLL, 216MHz:+FORCE_PLL, 360MHz:+FORCE_PLL, 504MHz:+FORCE_PLL, 648MHz:+FORCE_PLL, 792MHz:+FORCE_PLL, 936MHz:+FORCE_PLL, 1080MHz:+FORCE_PLL, 1224MHz:+FORCE_PLL, 1296MHz:+FORCE_PLL, 1368MHz:+FORCE_PLL, 1416MHz:+FORCE_PLL, 1512MHz:+FORCE_PLL, 1608MHz:+FORCE_PLL, 1704MHz:+FORCE_PLL, 1800MHz:+FORCE_PLL, 1848MHz:+FORCE_PLL, 1920MHz:+FORCE_PLL, 1968MHz:+FORCE_PLL, 2016MHz:+FORCE_PLL, 2064MHz:+FORCE_PLL

# Trial Specification
name        = BasicSanityTest
test        = initPState:gpc2clkTest
rmapi       = clk
order       = sequence

