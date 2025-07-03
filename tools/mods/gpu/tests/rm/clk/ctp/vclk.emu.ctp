# Emulation VClkTest
# See https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest

# Before runing this test on emulaton, set the 'RMEnableClk' regkey to 0x78000000.
# Otherwise Resman returns an error indicating that MCLK is not programmable.
# Bit 27,28,29 and 30 (clkWhich_VClk(i) for i = 0,1,2,3) must be on to enable VCLK for emulation.
# LW_REG_STR_RM_ENABLE_CLK is the RM symbol for this regkey, which overrides OBJCLK::programmableDomains.

# If this test fails, please file a bug or contact sw-gpu-rm-clocks-dev.

# Partial P-State for pclk. The %age are chosen to cover most of the N/SDM/PL switch possibilities. We keep M constant for pclk.
# The very close frequency switches will just change the SDM value while keeping N/PL constants. ex. 100 +/- 1%
# The next close frequency switches will change N and/or SDM value while keeping PL constant. ex 100 +/- 5%
# 100 +/- 40% will also make PL value to change.
name=pclk0
pclk0.freq = 1620MHz:-FORCE_BYPASS:+FORCE_PLL, 462MHz:-FORCE_BYPASS:+FORCE_PLL, 1010MHZ:-FORCE_BYPASS:+FORCE_PLL

name=pclk1
pclk1.freq = 1232MHz:-FORCE_BYPASS:+FORCE_PLL, 960MHz:-FORCE_BYPASS:+FORCE_PLL, 660MHZ:-FORCE_BYPASS:+FORCE_PLL

name=pclk2
pclk2.freq = 800MHz:-FORCE_BYPASS:+FORCE_PLL, 226MHz:-FORCE_BYPASS:+FORCE_PLL, 1568MHZ:-FORCE_BYPASS:+FORCE_PLL

name=pclk3
pclk3.freq = 26MHz:-FORCE_BYPASS:+FORCE_PLL, 1450MHz:-FORCE_BYPASS:+FORCE_PLL, 720MHZ:-FORCE_BYPASS:+FORCE_PLL

# Trial specification
name       = vclktest
test       = vbiosp0:pclk0, vbiosp5:pclk1, vbiosp8:pclk2, vbiosp0:pclk3
rmapi      = clocks
order      = random
end        = 100
tolerance  = 0.01, 0.01
disable    = thermalslowdown
