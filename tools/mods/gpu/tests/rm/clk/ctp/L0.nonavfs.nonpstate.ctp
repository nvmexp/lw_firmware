# Test to verify non-AVFS clock domains
# This is different than other L0/L1 vflwrve based tests that they have just
# POR values but this one targets most of the achievable frequencies on non-avfs
# clock domains even if they are fixed clock domains like pwrclk.
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest
# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

name=dispclk
dispclk.freq = 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 648MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1080MHz:+FORCE_BYPASS:-FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL, 310MHz:-FORCE_BYPASS:+FORCE_PLL, 462MHz:-FORCE_BYPASS:+FORCE_PLL, 550MHz:-FORCE_BYPASS:+FORCE_PLL, 810MHz:-FORCE_BYPASS:+FORCE_PLL, 920MHz:-FORCE_BYPASS:+FORCE_PLL, 1010MHZ:-FORCE_BYPASS:+FORCE_PLL, 1290MHz:-FORCE_BYPASS:+FORCE_PLL, 1430MHz:-FORCE_BYPASS:+FORCE_PLL, 1620MHz:-FORCE_BYPASS:+FORCE_PLL

name=hubclk
hubclk.freq = 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 648MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1080MHz:+FORCE_BYPASS:-FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL

name=pwrclk
pwrclk.freq = 324MHz:+FORCE_BYPASS:-FORCE_PLL, 405MHz:+FORCE_BYPASS:-FORCE_PLL, 540MHz:+FORCE_BYPASS:-FORCE_PLL, 648MHz:+FORCE_BYPASS:-FORCE_PLL, 810MHz:+FORCE_BYPASS:-FORCE_PLL, 1080MHz:+FORCE_BYPASS:-FORCE_PLL, 1620MHz:+FORCE_BYPASS:-FORCE_PLL

# Trial specification
name       = NonAvfsClockTest
test       = initpstate:dispclk, initpstate:hubclk, initpstate:pwrclk
rmapi      = clocks
order      = random
end        = 100
tolerance  = 0.01, 0.01
disable    = thermalslowdown
